#include "regional_uniformity_v2.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <new>

namespace {

constexpr int context_side = 1024;
constexpr int pool_start = 72;
constexpr int pool_end = 440;
constexpr int pool_side = pool_end - pool_start;
constexpr int area_count = 15;
constexpr double pi = 3.14159265358979323846;
constexpr double intercept = 72.74604902853203;

constexpr std::array<double, 5> sigmas{0.5, 0.75, 1.0, 2.0, 4.0};
constexpr std::array<double, 6> calibration_weights{
    7.446789334292563, 0.0, 0.0, 144.1503105491112,
    281.948753468757, 8.117013328024164
};
constexpr std::array<double, 6> calibration_min{
    0.0157753966550791, 0.0091044414642232, 0.0059332974811,
    0.0018830059100074, 0.0011258498544924, 0.0130714318432298
};
constexpr std::array<double, 6> calibration_max{
    0.1642419199981958, 0.1225264428493026, 0.0981017481502146,
    0.0520868188003473, 0.0343811578125464, 0.2024892965317557
};

// All regions with nonzero observation weight fit inside this crop,
// including the largest (s=16) disk's complete footprint.
const cv::Rect high_roi(2 * pool_start, 2 * pool_start, 2 * pool_side, 2 * pool_side);

struct SpatialWeights {
    cv::Mat envelope_mask;
    cv::Mat envelope_denominator;
    cv::Mat centers;
    double variance_correction;
};

struct AreaWindow {
    cv::Mat kernel;
    double log_area;
};

double disk_pixel_weight(int x, int y, double radius) {
    int covered = 0;
    for (int sy = 0; sy < 8; ++sy) {
        const double dy = y + (sy + 0.5) / 8.0 - 0.5;
        for (int sx = 0; sx < 8; ++sx) {
            const double dx = x + (sx + 0.5) / 8.0 - 0.5;
            covered += dx * dx + dy * dy <= radius * radius;
        }
    }
    return covered;
}

std::array<AreaWindow, area_count> make_area_windows() {
    std::array<AreaWindow, area_count> windows;
    for (int index = 0; index < area_count; ++index) {
        const int side = index + 2;
        const double area = 6.0 * side * side;
        const double radius = std::sqrt(area / pi);
        const int extent = static_cast<int>(std::ceil(radius + 0.5));
        cv::Mat kernel(2 * extent + 1, 2 * extent + 1, CV_64F);
        for (int y = -extent; y <= extent; ++y) {
            double *row = kernel.ptr<double>(y + extent);
            for (int x = -extent; x <= extent; ++x) {
                row[x + extent] = disk_pixel_weight(x, y, radius);
            }
        }
        kernel /= cv::sum(kernel)[0];
        windows[index] = {kernel, std::log(area)};
    }
    return windows;
}

cv::Mat circular_weights(int size, double center, double boundary) {
    cv::Mat weights(size, size, CV_64F);
    for (int y = 0; y < size; ++y) {
        double *row = weights.ptr<double>(y);
        const double dy = y - center;
        for (int x = 0; x < size; ++x) {
            const double dx = x - center;
            row[x] = std::min(1.0, std::max(0.0, boundary - std::sqrt(dx * dx + dy * dy)));
        }
    }
    return weights;
}

SpatialWeights make_spatial_weights() {
    SpatialWeights weights;
    weights.envelope_mask = circular_weights(context_side, 511.5, 370.5);
    cv::Mat blurred;
    cv::GaussianBlur(weights.envelope_mask, blurred, cv::Size(193, 193), 32.0, 0.0,
                     cv::BORDER_CONSTANT);
    // high_roi is entirely inside the positive support of the blurred mask.
    weights.envelope_denominator = blurred(high_roi).clone();
    cv::max(weights.envelope_denominator, 1e-20, weights.envelope_denominator);
    weights.centers = circular_weights(pool_side, 255.5 - pool_start, 160.5);
    weights.centers /= cv::sum(weights.centers)[0];
    weights.variance_correction = 1.0 - weights.centers.dot(weights.centers);
    return weights;
}

cv::Mat sample_context(const uint8_t *data, int width, int height, size_t stride,
                       double scale, double sigma, int radius) {
    cv::Mat gray(height, width, CV_32F);
    for (int y = 0; y < height; ++y) {
        const uint8_t *input = data + static_cast<size_t>(y) * stride;
        float *row = gray.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            row[x] = static_cast<float>(input[x]) / 255.0f;
        }
    }
    if (radius > 0) {
        const int side = 2 * radius + 1;
        cv::GaussianBlur(gray, gray, cv::Size(side, side), sigma, 0.0, cv::BORDER_REFLECT_101);
    }
    const cv::Point2f center((width - 1) / 2.0f, (height - 1) / 2.0f);
    cv::Mat matrix = cv::getRotationMatrix2D(center, 0.0, scale);
    matrix.at<double>(0, 2) += 511.5 - center.x;
    matrix.at<double>(1, 2) += 511.5 - center.y;
    cv::Mat sampled;
    cv::warpAffine(gray, sampled, matrix, cv::Size(context_side, context_side),
                   cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
    return sampled;
}

cv::Mat pool_envelope(cv::Mat field, const SpatialWeights &weights) {
    cv::multiply(field, weights.envelope_mask, field);
    cv::GaussianBlur(field, field, cv::Size(193, 193), 32.0, 0.0, cv::BORDER_CONSTANT);
    cv::Mat inner = field(high_roi);
    cv::divide(inner, weights.envelope_denominator, inner);
    cv::Mat pooled;
    cv::resize(inner, pooled, cv::Size(pool_side, pool_side), 0.0, 0.0, cv::INTER_AREA);
    return pooled;
}

double weighted_variance(const cv::Mat &means, const cv::Mat &weights, double mean) {
    double variance = 0.0;
    for (int y = 0; y < means.rows; ++y) {
        const double *row = means.ptr<double>(y);
        const double *center_weights = weights.ptr<double>(y);
        for (int x = 0; x < means.cols; ++x) {
            const double difference = row[x] - mean;
            variance += center_weights[x] * difference * difference;
        }
    }
    return variance;
}

std::array<cv::Mat, 3> make_tensor_moments(const cv::Mat &gray, double sigma) {
    const int side = 2 * static_cast<int>(std::ceil(3 * sigma)) + 1;
    const cv::Size kernel_size(side, side);
    cv::Mat blurred, dx, dy;
    cv::GaussianBlur(gray, blurred, kernel_size, sigma, 0.0, cv::BORDER_REFLECT_101);
    cv::Scharr(blurred, dx, CV_32F, 1, 0, 1.0, 0.0, cv::BORDER_REFLECT_101);
    cv::Scharr(blurred, dy, CV_32F, 0, 1, 1.0, 0.0, cv::BORDER_REFLECT_101);
    blurred.release();
    dx *= 1.0 / 16.0;
    dy *= 1.0 / 16.0;
    std::array<cv::Mat, 3> moments;
    cv::multiply(dx, dx, moments[0]);
    cv::multiply(dx, dy, moments[1]);
    cv::multiply(dy, dy, moments[2]);
    dx.release();
    dy.release();
    for (cv::Mat &moment : moments) {
        cv::GaussianBlur(moment, moment, kernel_size, sigma, 0.0, cv::BORDER_REFLECT_101);
    }
    return moments;
}

double integrate(const std::array<double, area_count> &curve,
                 const std::array<AreaWindow, area_count> &windows) {
    double integral = 0.0;
    for (int index = 1; index < area_count; ++index) {
        const double step = windows[index].log_area - windows[index - 1].log_area;
        integral += 0.5 * step * (curve[index] + curve[index - 1]);
    }
    return integral / (windows.back().log_area - windows.front().log_area);
}

int calculate_differences(const cv::Mat &gray, double (&features)[6]) {
    const SpatialWeights weights = make_spatial_weights();
    const auto windows = make_area_windows();
    std::array<double, area_count> structure_normalizers, luminance_variances;
    cv::Mat field, regional_means;
    gray.convertTo(field, CV_64F);
    const cv::Mat luminance = pool_envelope(field, weights);
    field.release();

    for (int index = 0; index < area_count; ++index) {
        cv::filter2D(luminance, regional_means, CV_64F, windows[index].kernel,
                     cv::Point(-1, -1), 0.0, cv::BORDER_CONSTANT);
        const double mean = weights.centers.dot(regional_means);
        if (mean <= 1e-6) {
            return REGIONAL_UNIFORMITY_INSUFFICIENT_LUMINANCE;
        }
        const double mean_squared = mean * mean;
        luminance_variances[index] = weighted_variance(regional_means, weights.centers, mean)
            / (weights.variance_correction * mean_squared);
        structure_normalizers[index] = 3.0 * weights.variance_correction
            * mean_squared * mean_squared;
    }
    features[5] = std::sqrt(integrate(luminance_variances, windows));

    // Stream the scales and components so the complete 15-channel field is
    // never materialized. The variance of each component is independent.
    for (size_t scale = 0; scale < sigmas.size(); ++scale) {
        const auto moments = make_tensor_moments(gray, 2.0 * sigmas[scale]);
        std::array<double, area_count> variances{};
        for (int component = 0; component < 3; ++component) {
            if (component == 0) {
                cv::add(moments[0], moments[2], field, cv::noArray(), CV_64F);
            } else if (component == 1) {
                cv::subtract(moments[0], moments[2], field, cv::noArray(), CV_64F);
            } else {
                moments[1].convertTo(field, CV_64F, 2.0);
            }
            const cv::Mat pooled = pool_envelope(field, weights);
            field.release();
            for (int index = 0; index < area_count; ++index) {
                cv::filter2D(pooled, regional_means, CV_64F, windows[index].kernel,
                             cv::Point(-1, -1), 0.0, cv::BORDER_CONSTANT);
                const double mean = weights.centers.dot(regional_means);
                variances[index] += weighted_variance(regional_means, weights.centers, mean);
            }
        }
        for (int index = 0; index < area_count; ++index) {
            variances[index] /= structure_normalizers[index];
        }
        features[scale] = std::sqrt(std::sqrt(integrate(variances, windows)));
    }
    return REGIONAL_UNIFORMITY_OK;
}

}  // namespace

int regional_uniformity_v2_measure(const uint8_t *gray, int width, int height, size_t stride,
                                   regional_uniformity_result *result) {
    if (gray == nullptr || result == nullptr || width <= 0 || height <= 0 ||
        width >= 32767 || height >= 32767 || stride < static_cast<size_t>(width)) {
        return REGIONAL_UNIFORMITY_INVALID_INPUT;
    }
    const int short_side = std::min(width, height);
    const double scale = 896.0 / short_side;
    const double sigma = 0.5 * std::sqrt(std::max(1.0 / (scale * scale) - 1.0, 0.0));
    const int radius = static_cast<int>(std::ceil(3.0 * sigma));
    const double available_radius = ((short_side - 1) * scale / 2.0
        - std::sqrt(2.0) * (radius + 1) * scale) / 2.0;
    if (scale > 1.0 || available_radius < 220.0) {
        return REGIONAL_UNIFORMITY_INSUFFICIENT_CONTEXT;
    }

    try {
        regional_uniformity_result measured{};
        const cv::Mat context = sample_context(gray, width, height, stride, scale, sigma, radius);
        const int status = calculate_differences(context, measured.differences);
        if (status != REGIONAL_UNIFORMITY_OK) {
            return status;
        }
        measured.score = intercept;
        measured.within_feature_calibration_ranges = 1;
        for (size_t index = 0; index < calibration_weights.size(); ++index) {
            const double difference = measured.differences[index];
            if (!std::isfinite(difference)) {
                return REGIONAL_UNIFORMITY_COMPUTE_ERROR;
            }
            measured.score -= calibration_weights[index] * difference;
            measured.within_feature_calibration_ranges &= (
                difference >= calibration_min[index] && difference <= calibration_max[index]
            );
        }
        *result = measured;
        return REGIONAL_UNIFORMITY_OK;
    } catch (const std::bad_alloc &) {
        return REGIONAL_UNIFORMITY_NO_MEMORY;
    } catch (const cv::Exception &error) {
        return error.code == cv::Error::StsNoMem
            ? REGIONAL_UNIFORMITY_NO_MEMORY : REGIONAL_UNIFORMITY_COMPUTE_ERROR;
    }
}
