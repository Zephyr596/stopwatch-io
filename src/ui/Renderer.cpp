#include "Renderer.h"
#include <cstdio>
#include <cstring>
namespace sw {
namespace {
constexpr uint16_t black = 0x0000, panel = 0x0841, white = 0xFFFF, muted = 0x9CF3, green = 0x4F37;
}
bool Renderer::begin() {
    tile_.setColorDepth(16);
    tile_.setPsram(true);
    if (!tile_.createSprite(320, 76)) {
        tile_.setPsram(false);
        if (!tile_.createSprite(320, 76)) return false;
        Serial.println("[warning] UI tile uses internal RAM");
    }
    Serial.println("[ui] reusable 320x76 RGB565 tile ready (48640 bytes)");
    return true;
}
void Renderer::text(const char* value, int y, int size, uint16_t color) {
    tile_.setTextDatum(middle_center);
    tile_.setTextSize(size);
    tile_.setTextColor(color);
    tile_.drawString(value, 160, y);
}
void Renderer::card(size_t index, const char* title, const char* subtitle, bool selected, int size, int progress) {
    char key[160];
    snprintf(key, sizeof(key), "%s|%s|%d|%d|%d", title, subtitle, selected, size, progress);
    if (!strcmp(key, keys_[index])) return;
    snprintf(keys_[index], sizeof(keys_[index]), "%s", key);
    tile_.fillSprite(black);
    tile_.fillRoundRect(0, 0, 320, 76, 12, panel);
    if (selected) tile_.drawRoundRect(0, 0, 320, 76, 12, green);
    text(title, 26, size, white);
    text(subtitle, 58, 2, muted);
    if (progress >= 0) {
        tile_.fillRoundRect(20, 69, 280, 4, 2, muted);
        if (progress > 0) tile_.fillRoundRect(20, 69, 280 * progress / 100, 4, 2, green);
    }
    tile_.pushSprite(rows[index].x, rows[index].y);
    ++paintCount_;
}
void Renderer::footer(bool selected) {
    if (footerSelected_ == int(selected)) return;
    footerSelected_ = selected;
    ++paintCount_;
    display_.fillRoundRect(homeButton.x, homeButton.y, homeButton.w, homeButton.h, 14, selected ? green : panel);
    display_.setTextDatum(middle_center);
    display_.setTextSize(2);
    display_.setTextColor(selected ? black : white);
    display_.drawString("HOME", 233, 394);
}
void Renderer::render(AppId page, const Launcher& launcher, const TeslaApp& tesla,
                      const SettingsApp& settings, const DeviceStatus& device, const Network& network, uint32_t now) {
    if (page != page_ || mock_ != tesla.isMock()) {
        mock_ = tesla.isMock();
        page_ = page;
        ++paintCount_;
        for (auto& key : keys_) key[0] = '\0';
        footerSelected_ = -1;
        display_.fillScreen(black); // Only on application transitions.
        display_.setTextDatum(middle_center);
        display_.setTextColor(page == AppId::Tesla ? green : muted);
        display_.setTextSize(2);
        const char* title = page == AppId::Home ? "STOPWATCH" : page == AppId::Tesla ? (mock_ ? "TESLA / DEMO" : "TESLA / API") :
                            page == AppId::Settings ? "DEVICE" : "MOTORCYCLE";
        display_.drawString(title, 233, 65);
        if (page == AppId::Tesla) {
            display_.setTextSize(1);
            display_.drawString(mock_ ? "B / TAP STATUS: CHARGE or DRIVE" : "B / TAP STATUS: REFRESH", 233, 357);
        }
        if (page == AppId::Home) {
            display_.setTextSize(1);
            display_.drawString("A: NEXT    B: OPEN", 233, 391);
        }
    }
    char title[64], detail[96];
    switch (page) {
    case AppId::Home:
        for (size_t i = 0; i < launchItems.size(); ++i)
            card(i, launchItems[i].title, launchItems[i].subtitle, launcher.selected() == i, 2);
        break;
    case AppId::Tesla: {
        const auto& s = tesla.state();
        const bool valid = s.valid && !stale(s, now);
        const bool live = valid && tesla.healthy() && s.speedValid && s.connection == VehicleConnection::Online;
        if (s.valid) {
            snprintf(title, sizeof(title), "%.0f%% BATTERY", s.batteryPercent);
            const char* range = s.rangeKind == RangeKind::Rated ? "RATED" : s.rangeKind == RangeKind::Ideal ? "IDEAL" : "EST.";
            snprintf(detail, sizeof(detail), "%s %.0f km%s", range, s.rangeKm, valid ? "" : " / CACHE");
        } else {
            snprintf(title, sizeof(title), "--%% BATTERY");
            snprintf(detail, sizeof(detail), "DATA UNAVAILABLE / STALE");
        }
        card(0, title, detail, false, 3, s.valid ? int(s.batteryPercent) : -1);
        if (live) snprintf(title, sizeof(title), "%.0f", s.speedKmh);
        else snprintf(title, sizeof(title), "--");
        card(1, title, mock_ ? "km/h / SIMULATED" : s.source == VehicleSource::Fixture ? "km/h / TEST FIXTURE" : live ? "km/h" : "km/h / NOT LIVE", false, 5);
        if (s.valid) snprintf(detail, sizeof(detail), "%s / %lus ago", s.source == VehicleSource::Mock ? "MOCK" : s.source == VehicleSource::Fixture ? "TEST" : "API", (unsigned long)(ageMs(s, now) / 1000));
        else snprintf(detail, sizeof(detail), "No vehicle data");
        const char* status = valid && tesla.healthy() && s.connection == VehicleConnection::Online ? (s.charging ? "CHARGING" : !s.speedValid ? "SPEED UNKNOWN" : s.speedKmh > 0 ? "DRIVING" : "PARKED") : tesla.status();
        card(2, status, detail, tesla.selected() == 0, strlen(status) > 16 ? 2 : 3);
        footer(tesla.selected() == 1);
        break;
    }
    case AppId::Settings:
        if (device.batteryPercent >= 0) snprintf(detail, sizeof(detail), "%s B:%d%%", network.online() ? network.address() : "No IP", device.batteryPercent);
        else snprintf(detail, sizeof(detail), "%s B:--", network.online() ? network.address() : "No IP");
        card(0, network.label(), detail, false, 2);
        card(1, mock_ ? "SOURCE: MOCK" : "SOURCE: API", "A x2 / B or TAP: change", settings.selected() == 2, 2);
        card(2, settings.haptics() ? "HAPTIC ON" : "HAPTIC OFF", "B / TAP: toggle (not saved)", settings.selected() == 0, 2);
        footer(settings.selected() == 1);
        break;
    case AppId::Motorcycle:
        card(0, "MOTORCYCLE", "Riding assistant", false, 2);
        card(1, "COMING SOON", "No GPS / CAN / OBD", false, 2);
        card(2, "OFFLINE READY", "B / Hold A: HOME", false, 2);
        footer(true);
        break;
    default: break;
    }
}
} // namespace sw
