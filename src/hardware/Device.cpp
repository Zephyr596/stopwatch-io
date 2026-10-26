#include "Device.h"
namespace sw {
bool Device::begin() {
    Serial.begin(115200); // Do not wait for USB connection.
    Serial.println("[boot] StopWatch / apps + read-only vehicle API");
    auto cfg = M5.config();
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    M5.begin(cfg);
    // Current official driver exposes 468x468 for the 466x466 visible panel.
    ready_ = M5.getBoard() == m5::board_t::board_M5StopWatch &&
             M5.Display.width() >= 466 && M5.Display.height() >= 466;
    M5.BtnA.setHoldThresh(700);
    M5.BtnB.setHoldThresh(700);
    sample();
    Serial.printf("[hardware] ready=%d board=%d display=%dx%d touch=%d psram=%u battery=%d\n",
                  ready_, int(M5.getBoard()), M5.Display.width(), M5.Display.height(),
                  M5.Touch.isEnabled(), ESP.getPsramSize(), status_.batteryPercent);
    if (!ready_) Serial.println("[error] StopWatch display detection failed; app startup disabled");
    if (!M5.Touch.isEnabled()) Serial.println("[warning] Touch unavailable; use A/B");
    if (!ESP.getPsramSize()) Serial.println("[warning] PSRAM unavailable");
    return ready_;
}
void Device::sample() {
    const int battery = ready_ ? M5.Power.getBatteryLevel() : -1;
    status_.batteryPercent = battery >= 0 && battery <= 100 ? battery : -1;
    status_.freeHeap = ESP.getFreeHeap();
    status_.minHeap = ESP.getMinFreeHeap();
    status_.freePsram = ESP.getFreePsram();
}
bool Device::poll(uint32_t now, InputEvent& event) {
    M5.update();
    if (vibrating_ && uint32_t(now - vibrationStart_) >= 35) {
        M5.Power.setVibration(0);
        vibrating_ = false;
    }
    if (uint32_t(now - lastSample_) >= 5000) { lastSample_ = now; sample(); }
    if (uint32_t(now - lastLog_) >= 30000) {
        lastLog_ = now;
        Serial.printf("[health] heap=%u min=%u psram=%u battery=%d\n", status_.freeHeap,
                      status_.minHeap, status_.freePsram, status_.batteryPercent);
    }
    if (!ready_) return false;
    // One event per update. Home has priority; hold release cannot produce a click.
    if (M5.BtnA.wasHold()) { event = {InputType::Home}; return true; }
    if (M5.BtnA.wasClicked()) { event = {InputType::Next}; return true; }
    if (M5.BtnB.wasClicked()) { event = {InputType::Confirm}; return true; }
    for (uint8_t i = 0; i < M5.Touch.getCount(); ++i) {
        const auto& touch = M5.Touch.getDetail(i);
        if (touch.wasClicked()) { event = {InputType::Tap, touch.x, touch.y}; return true; }
    }
    return false;
}
void Device::feedback(uint32_t now, bool enabled) {
    if (!ready_) return;
    if (!enabled) {
        if (vibrating_) M5.Power.setVibration(0);
        vibrating_ = false;
        return;
    }
    M5.Power.setVibration(100);
    vibrating_ = true;
    vibrationStart_ = now;
}
} // namespace sw
