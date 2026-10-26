#include "ApiVehicleData.h"
#include "VehicleJson.h"
#include <Config.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctime>
namespace sw {
bool ApiVehicleDataProvider::begin() {
    if (!SW_API_URL[0]) return true; // No endpoint: no task or queues needed.
    requests_ = xQueueCreate(1, sizeof(uint8_t));
    results_ = xQueueCreate(1, sizeof(Result));
    if (!requests_ || !results_ || xTaskCreate(worker, "vehicle-http", 12288, this, 1, &task_) != pdPASS) {
        if (requests_) vQueueDelete(requests_);
        if (results_) vQueueDelete(results_);
        requests_ = results_ = nullptr;
        cache_.fail(ApiError::Worker);
        Serial.println("[api] worker allocation failed");
        return false;
    }
    cache_.fail(ApiError::Waiting);
    return true;
}
void ApiVehicleDataProvider::update(uint32_t now) {
    now_ = now;
    if (!SW_API_URL[0]) { cache_.fail(ApiError::NotConfigured); return; }
    if (!task_) return;
    Result result;
    if (xQueueReceive(results_, &result, 0) == pdTRUE) {
        pending_ = false;
        if (result.error == ApiError::None) cache_.accept(result.state);
        else cache_.fail(result.error);
        Serial.printf("[api] result=%s revision=%u\n", errorLabel(result.error), cache_.state().revision);
    }
    if (WiFi.status() != WL_CONNECTED) { cache_.fail(ApiError::Offline); return; }
    // Manual refresh is bounded too; no faster than once every 2 seconds.
    const uint32_t interval = requested_ ? 2000 : 5000;
    if (!pending_ && (!attempted_ || uint32_t(now - lastAttempt_) >= interval)) {
        const uint8_t request = 1;
        if (xQueueSend(requests_, &request, 0) == pdTRUE) {
            pending_ = attempted_ = true;
            requested_ = false;
            lastAttempt_ = now;
        }
    }
}
void ApiVehicleDataProvider::worker(void* context) {
    auto& self = *static_cast<ApiVehicleDataProvider*>(context);
    uint8_t request;
    for (;;) {
        if (xQueueReceive(self.requests_, &request, portMAX_DELAY) == pdTRUE) {
            const auto result = self.fetch();
            xQueueOverwrite(self.results_, &result);
        }
    }
}
ApiVehicleDataProvider::Result ApiVehicleDataProvider::fetch() {
    Result result;
    const bool https = strncmp(SW_API_URL, "https://", 8) == 0;
    if ((!https && (strncmp(SW_API_URL, "http://", 7) != 0 || !SW_ALLOW_HTTP_TEST || SW_API_TOKEN[0])) ||
        (https && !SW_API_CA[0])) { result.error = ApiError::TlsConfig; return result; }
    if (https && time(nullptr) < 1700000000) { result.error = ApiError::Clock; return result; }
    WiFiClient plain;
    WiFiClientSecure secure;
    if (https) { secure.setCACert(SW_API_CA); secure.setHandshakeTimeout(5); }
    WiFiClient& client = https ? static_cast<WiFiClient&>(secure) : plain;
    HTTPClient http;
    http.setConnectTimeout(3000);
    http.setTimeout(2500);
    http.setReuse(false);
    http.useHTTP10(true); // Endpoint must send a Content-Length; no chunked bodies.
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!http.begin(client, SW_API_URL)) { result.error = ApiError::Transport; return result; }
    if (SW_API_TOKEN[0]) http.addHeader("Authorization", String("Bearer ") + SW_API_TOKEN);
    http.addHeader("Accept", "application/json");
    const uint32_t start = millis();
    const int code = http.GET();
    if (code != 200) {
        result.error = code < 0 ? ApiError::Transport : ApiError::Http;
    } else {
        const int length = http.getSize();
        char body[1537];
        size_t used = 0;
        if (length <= 0 || length > 1536) result.error = ApiError::Payload;
        else {
            auto& stream = http.getStream();
            while (used < size_t(length) && uint32_t(millis() - start) < 7000) {
                const int available = stream.available();
                if (available > 0) {
                    const int count = stream.read(reinterpret_cast<uint8_t*>(body + used), size_t(length) - used);
                    if (count > 0) used += size_t(count);
                } else if (!stream.connected()) break;
                else vTaskDelay(pdMS_TO_TICKS(5));
            }
            body[used] = '\0';
            if (used != size_t(length)) result.error = ApiError::Transport;
            else if (!decodeVehicle(body, used, millis(), result.state)) result.error = ApiError::Payload;
            else {
                // Conservative: include full request time in upstream data age.
                const uint32_t elapsed = millis() - start;
                result.state.dataAgeMs = elapsed > UINT32_MAX - result.state.dataAgeMs ? UINT32_MAX : result.state.dataAgeMs + elapsed;
                if (!https && result.state.source != VehicleSource::Fixture) result.error = ApiError::Payload;
            }
        }
    }
    http.end();
    return result;
}
}
