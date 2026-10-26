#include "Network.h"
#include <WiFi.h>
#include <Config.h>
#include <ctime>
namespace sw {
void Network::begin() {
    configured_ = SW_WIFI_SSID[0] != '\0';
    if (!configured_) { Serial.println("[wifi] not configured"); return; }
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false); // One owner for bounded retry cadence.
    WiFi.setHostname("m5-stopwatch");
    WiFi.begin(SW_WIFI_SSID, SW_WIFI_PASSWORD);
    attempting_ = true;
    lastAttempt_ = millis();
    Serial.println("[wifi] connecting");
}
void Network::reconnect() {
    if (!configured_) return;
    WiFi.disconnect(false, false);
    attempting_ = false;
    lastAttempt_ = millis();
    Serial.println("[wifi] diagnostic disconnect; reconnect in 30s");
}
const char* Network::label() const {
    return online_ ? "WIFI CONNECTED" : !configured_ ? "WIFI UNCONFIGURED" : attempting_ ? "WIFI CONNECTING" : "WIFI OFFLINE";
}
void Network::update(uint32_t now) {
    if (!configured_) return;
    if (scanning_) {
        const int count = WiFi.scanComplete();
        if (count >= 0) {
            bool exact = false, caseMatch = false;
            for (int i = 0; i < count; ++i) {
                const String ssid = WiFi.SSID(i);
                exact |= ssid == SW_WIFI_SSID;
                caseMatch |= ssid.equalsIgnoreCase(SW_WIFI_SSID);
            }
            Serial.printf("[wifi] scan networks=%d target_found=%d case_insensitive_match=%d\n", count, exact, caseMatch);
            WiFi.scanDelete();
            scanning_ = false;
        } else if (count == WIFI_SCAN_FAILED) { scanning_ = false; Serial.println("[wifi] scan failed"); }
    }
    if (scanRequested_ && uint32_t(now - lastAttempt_) >= 15500) {
        scanRequested_ = false;
        scanned_ = true;
        scanning_ = WiFi.scanNetworks(true, false, false, 120) == WIFI_SCAN_RUNNING;
        if (!scanning_) Serial.println("[wifi] diagnostic scan could not start");
    }
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected != online_) {
        online_ = connected;
        if (online_) {
            snprintf(address_, sizeof(address_), "%s", WiFi.localIP().toString().c_str());
            attempting_ = false;
            scanRequested_ = false;
            // Asynchronous clock sync needed for HTTPS certificate validity, never awaited by UI.
            configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
            Serial.printf("[wifi] connected ip=%s rssi=%d\n", address_, WiFi.RSSI());
        } else {
            snprintf(address_, sizeof(address_), "No connection");
            Serial.println("[wifi] disconnected");
        }
    }
    if (online_) return;
    if (attempting_ && uint32_t(now - lastAttempt_) >= 15000) {
        WiFi.disconnect(false, false);
        attempting_ = false;
        Serial.println("[wifi] timeout; retry scheduled");
        // Diagnostic scan during retry cooldown; never wait for results in the UI loop.
        scanRequested_ = !scanned_;
    }
    if (!attempting_ && !scanning_ && uint32_t(now - lastAttempt_) >= 30000) {
        WiFi.begin(SW_WIFI_SSID, SW_WIFI_PASSWORD);
        lastAttempt_ = now;
        attempting_ = true;
        Serial.println("[wifi] reconnecting");
    }
}
}
