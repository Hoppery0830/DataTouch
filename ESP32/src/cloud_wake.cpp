#include "cloud_wake.h"
#include "cloud_upload.h"
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <atomic>
#include <cstring>
#include <ctime>

#if __has_include("wake_config.local.h")
#include "wake_config.local.h"
#else
#include "wake_config.example.h"
#endif

namespace {
std::atomic<bool> pending{false};

void socketEvent(WStype_t type, uint8_t *data, size_t length) {
    if (type == WStype_CONNECTED) Serial.println("[WAKE] WSS connected");
    if (type == WStype_DISCONNECTED) Serial.println("[WAKE] WSS disconnected");
    if (type == WStype_TEXT && length == 4 && memcmp(data, "wake", 4) == 0) {
        pending.store(true);
    }
}

void listenForWake(void *) {
    WebSocketsClient client;
    String currentId;
    uint32_t nextConfigCheck = 0;
    bool configured = false;
    for (;;) {
        if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
            if (configured) client.disconnect();
            configured = false;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (!configured || (int32_t)(millis() - nextConfigCheck) >= 0) {
            const String id = cloudUploadGetConfig().deviceId;
            nextConfigCheck = millis() + 1000;
            if (!configured || id != currentId) {
                if (configured) client.disconnect();
                currentId = id;
                if (id.isEmpty() || id.length() > 64 ||
                    strspn(id.c_str(), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != id.length()) {
                    configured = false;
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    continue;
                }
                // The WebSockets library leaves its client-certificate pointers
                // uninitialized when this overload is used. Initialize them
                // explicitly to null so the TLS client does not parse a bogus
                // 0xA5A5A5A5 certificate pointer.
                client.beginSslWithClientKey(WAKE_HOST, 443, "/device", WAKE_CA_PEM,
                                              nullptr, nullptr, "");
                client.setAuthorization("Bearer " WAKE_DEVICE_KEY);
                // WebSocketsClient stores this pointer for the later TLS
                // handshake. Keep the header in a String with task lifetime.
                static String deviceHeader;
                deviceHeader = String("X-Device-Id: ") + id;
                client.setExtraHeaders(deviceHeader.c_str());
                client.onEvent(socketEvent);
                client.setReconnectInterval(5000);
                client.enableHeartbeat(10000, 3000, 2);
                configured = true;
            }
        }
        client.loop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
}

void cloudWakeInit() {
    if (!strlen(WAKE_HOST) || strlen(WAKE_DEVICE_KEY) != 64 || !strlen(WAKE_CA_PEM)) {
        Serial.println("[WAKE] WSS not configured; using HTTPS polling");
        return;
    }
    // Keep this task isolated from the main cloud task while the WebSocket TLS
    // client performs its handshake.
    if (xTaskCreatePinnedToCore(listenForWake, "cloudWake", 16384, nullptr, 1, nullptr, 0) != pdPASS) {
        Serial.println("[WAKE] Could not create task; using HTTPS polling");
    }
}

bool cloudWakeTake() {
    return pending.exchange(false);
}
