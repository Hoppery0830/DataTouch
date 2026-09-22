// utils/fmt.js
// 通用格式化工具（M1）：时间格式化 / 数值显示 / 四维归一化值换算
// 约定（见 docs/小程序与微信云开发落地方案.md §0.3、§3.1）：
//   softness / smoothness / roughness / rebound 为 STM32 归一化后的 [0,1]（metricsNormalized=true）
//   composite 为 0-100 的综合评分
//   小程序端只做「显示」，不重算权威评分

/** 数字补零：9 -> "09" */
function pad2(n) {
  const v = Number(n) || 0;
  return v < 10 ? "0" + v : "" + v;
}

/**
 * 把任意时间形态解析为 Date 对象
 * 兼容：Date / number(ms) / 纯数字字符串(ms) / ISO 字符串 / "2026-09-06 10:21:03"
 */
function toDate(value) {
  if (!value && value !== 0) return null;
  if (value instanceof Date) return isNaN(value.getTime()) ? null : value;
  if (typeof value === "number") {
    // 秒级时间戳兜底（< 1e12 视为秒）
    const ms = value < 1e12 ? value * 1000 : value;
    const d = new Date(ms);
    return isNaN(d.getTime()) ? null : d;
  }
  if (typeof value === "string") {
    const s = value.trim();
    if (!s) return null;
    if (/^\d+$/.test(s)) return toDate(Number(s));
    // iOS 不支持 "YYYY-MM-DD HH:mm:ss"，统一替换成 "YYYY/MM/DD HH:mm:ss"
    const normalized = s.indexOf("T") >= 0 ? s : s.replace(/-/g, "/");
    const d = new Date(normalized);
    return isNaN(d.getTime()) ? null : d;
  }
  return null;
}

/**
 * 格式化时间：YYYY-MM-DD HH:mm(:ss)
 * @param {*} value 任意时间形态
 * @param {boolean} withSec 是否带秒，默认 false
 * @param {string} empty 空值占位
 */
function formatTime(value, withSec, empty) {
  const d = toDate(value);
  if (!d) return empty === undefined ? "--" : empty;
  let out = d.getFullYear() + "-" + pad2(d.getMonth() + 1) + "-" + pad2(d.getDate());
  out += " " + pad2(d.getHours()) + ":" + pad2(d.getMinutes());
  if (withSec) out += ":" + pad2(d.getSeconds());
  return out;
}

/** 列表用短时间：MM-DD HH:mm */
function formatShortTime(value, empty) {
  const d = toDate(value);
  if (!d) return empty === undefined ? "--" : empty;
  return (
    pad2(d.getMonth() + 1) + "-" + pad2(d.getDate()) + " " +
    pad2(d.getHours()) + ":" + pad2(d.getMinutes())
  );
}

/** 数字 -> 定点字符串，非法值返回占位 */
function formatNumber(value, digits, empty) {
  if (value === undefined || value === null || value === "") {
    return empty === undefined ? "--" : empty;
  }
  const n = Number(value);
  if (!isFinite(n)) return empty === undefined ? "--" : empty;
  return n.toFixed(digits === undefined ? 2 : digits);
}

/** 0-100 评分 -> 一位小数（列表/详情大数字用） */
function formatScore(value, empty) {
  if (value === undefined || value === null || value === "") {
    return empty === undefined ? "--" : empty;
  }
  const n = Number(value);
  if (!isFinite(n)) return empty === undefined ? "--" : empty;
  return n.toFixed(1);
}

/** clamp 到 [0,1] */
function clamp01(v) {
  const n = Number(v);
  if (!isFinite(n)) return 0;
  if (n < 0) return 0;
  if (n > 1) return 1;
  return n;
}

/**
 * 四维 [0,1] 归一化值 -> 展示整数 0-100
 * 云库里若误存了 0-100 的量纲（> 1），这里做一次宽容处理，避免显示成 "6200"
 */
function metricToPercent(value) {
  const n = Number(value);
  if (!isFinite(n)) return 0;
  const x = n > 1.0000001 ? n / 100 : n; // 容忍历史/误存数据
  return Math.round(clamp01(x) * 100);
}

module.exports = {
  pad2,
  toDate,
  formatTime,
  formatShortTime,
  formatNumber,
  formatScore,
  clamp01,
  metricToPercent,
};
