#include "Renderer.h"
#include "CodexPetFrames.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace sw {
namespace {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr auto background = rgb(7, 12, 16);
constexpr auto panel = rgb(17, 27, 33);
constexpr auto frame = rgb(36, 53, 62);
constexpr auto selectedPanel = rgb(16, 42, 42);
constexpr auto iconPanel = rgb(23, 39, 48);
constexpr auto ink = rgb(236, 247, 246);
constexpr auto muted = rgb(154, 176, 186);
constexpr auto mint = rgb(116, 233, 200);
constexpr auto amber = rgb(247, 185, 117);
constexpr auto blue = rgb(163, 188, 253);
constexpr int center = 233;
constexpr int tileWidth = rows.front().w;
constexpr int tileHeight = rows.front().h;

enum class Icon { Car, Bike, Settings, Wifi, Source, Haptics, Charge, Refresh, Codex };

void label(lgfx::LovyanGFX& canvas, const char* value, int x, int y, const lgfx::IFont* font,
           uint16_t color, lgfx::textdatum_t datum = lgfx::middle_center, float scale = 1) {
    canvas.setFont(font);
    canvas.setTextSize(scale);
    canvas.setTextDatum(datum);
    canvas.setTextColor(color);
    canvas.drawString(value, x, y);
}

void icon(lgfx::LovyanGFX& canvas, Icon shape, int x, int y, uint16_t color, float scale = 1) {
    const auto line = [&](int x1, int y1, int x2, int y2) {
        canvas.drawWideLine(x + x1 * scale, y + y1 * scale, x + x2 * scale, y + y2 * scale,
                            1.2f * scale, color);
    };
    switch (shape) {
    case Icon::Car:
        canvas.drawRoundRect(x - 14, y - 2, 29, 11, 3, color);
        line(-11, -2, -7, -9);
        line(-7, -9, 7, -9);
        line(7, -9, 11, -2);
        line(-10, 8, -10, 11);
        line(10, 8, 10, 11);
        canvas.fillCircle(x - 9, y + 3, 1, color);
        canvas.fillCircle(x + 9, y + 3, 1, color);
        break;
    case Icon::Bike:
        canvas.fillArc(x - 13 * scale, y + 5 * scale, 8 * scale, 6 * scale, 0, 360, color);
        canvas.fillArc(x + 13 * scale, y + 5 * scale, 8 * scale, 6 * scale, 0, 360, color);
        line(-13, 5, -7, -7);
        line(-14, -7, -5, -7);
        line(-13, 5, 1, 5);
        canvas.fillRoundRect(x - 4 * scale, y - 8 * scale, 13 * scale, 6 * scale, 2 * scale, color);
        canvas.fillRoundRect(x - 5 * scale, y, 9 * scale, 7 * scale, 2 * scale, color);
        line(13, 5, 7, -11);
        line(7, -11, 12, -11);
        break;
    case Icon::Settings:
        for (int i = -1; i <= 1; ++i) {
            const int bar = i * 8;
            line(-12, bar, 12, bar);
            canvas.fillCircle(i == 0 ? x - 4 : x + 5, y + bar, 3, color);
        }
        break;
    case Icon::Wifi:
        for (int radius = 8; radius <= 18; radius += 5)
            canvas.fillArc(x, y + 7, radius, radius - 2, 225, 315, color);
        canvas.fillCircle(x, y + 7, 2, color);
        break;
    case Icon::Source:
        line(-11, -5, 11, -5);
        line(11, -5, 6, -10);
        line(11, -5, 6, 0);
        line(11, 6, -11, 6);
        line(-11, 6, -6, 1);
        line(-11, 6, -6, 11);
        break;
    case Icon::Haptics:
        canvas.drawRoundRect(x - 6, y - 11, 13, 23, 3, color);
        line(-11, -5, -11, 5);
        line(11, -5, 11, 5);
        break;
    case Icon::Charge:
        canvas.fillTriangle(x + 3, y - 13, x - 9, y + 2, x + 2, y + 2, color);
        canvas.fillTriangle(x - 3, y + 13, x + 9, y - 2, x - 2, y - 2, color);
        break;
    case Icon::Refresh:
        canvas.fillArc(x, y, 11, 9, 30, 330, color);
        line(10, -5, 13, -12);
        line(10, -5, 3, -6);
        break;
    case Icon::Codex:
        canvas.drawRoundRect(x - 13, y - 10, 27, 21, 6, color);
        line(-7, -4, -3, 0);
        line(-3, 0, -7, 4);
        line(1, 4, 7, 4);
        break;
    }
}
constexpr uint32_t animationDuration(const codexPet::Frame* frames, size_t count) {
    uint32_t duration = 0;
    for (size_t i = 0; i < count; ++i) duration += frames[i].holdMs;
    return duration;
}

const codexPet::Frame& frameAt(const codexPet::Frame* frames, size_t count, uint32_t elapsed, bool repeat) {
    const uint32_t duration = animationDuration(frames, count);
    uint32_t phase = repeat ? elapsed % duration : std::min(elapsed, duration - 1);
    size_t index = 0;
    while (phase >= frames[index].holdMs) phase -= frames[index++].holdMs;
    return frames[index];
}

} // namespace

struct Renderer::RowStyle {
    Icon icon;
    uint16_t accent;
    bool selected = false;
    const char* value = "";
    bool interactive = true;
};

bool Renderer::begin() {
    tile_.setColorDepth(16);
    tile_.setPsram(true);
    if (!tile_.createSprite(tileWidth, tileHeight)) {
        tile_.setPsram(false);
        if (!tile_.createSprite(tileWidth, tileHeight)) return false;
        Serial.println("[warning] UI tile uses internal RAM");
    }
    Serial.println("[ui] reusable 320x76 RGB565 tile ready (48640 bytes)");
    return true;
}

bool Renderer::cacheChanged(size_t index, const char* key) {
    if (strcmp(key, keys_[index]) == 0) return false;
    snprintf(keys_[index], sizeof(keys_[index]), "%s", key);
    return true;
}

bool Renderer::prepareTile(size_t index, const char* key) {
    if (!cacheChanged(index, key)) return false;
    tile_.fillSprite(background);
    return true;
}

void Renderer::beginPaint() {
    if (!painting_) {
        display_.startWrite();
        painting_ = true;
    }
    ++paintCount_;
}

void Renderer::presentTile(const Rect& area) {
    beginPaint();
    if (area.h < tileHeight) display_.setClipRect(area.x, area.y, area.w, area.h);
    tile_.pushSprite(area.x, area.y);
    if (area.h < tileHeight) display_.clearClipRect();
}

void Renderer::row(size_t index, const Rect& area, const char* title, const char* detail, const RowStyle& style) {
    char key[sizeof(keys_[0])];
    snprintf(key, sizeof(key), "%s|%s|%s|%d", title, detail, style.value, style.selected);
    if (!prepareTile(index, key)) return;

    const int mid = area.h / 2;
    const int iconSize = std::min(46, area.h - 16);
    tile_.fillRoundRect(0, 1, area.w, area.h - 2, 18, style.selected ? selectedPanel : panel);
    tile_.drawRoundRect(0, 1, area.w, area.h - 2, 18, style.selected ? mint : frame);
    tile_.fillRoundRect(37 - iconSize / 2, mid - iconSize / 2, iconSize, iconSize, 14, iconPanel);
    icon(tile_, style.icon, 37, mid, style.accent);

    const bool hasValue = style.value[0] != '\0';
    const int maxTitleWidth = hasValue ? 166 : (style.interactive ? 203 : 232);
    tile_.setFont(&lgfx::fonts::FreeSansBold12pt7b);
    tile_.setTextSize(1);
    const int titleWidth = tile_.textWidth(title);
    const float scale = titleWidth > maxTitleWidth ? float(maxTitleWidth) / titleWidth : 1.0f;
    label(tile_, title, 74, mid - 11, &lgfx::fonts::FreeSansBold12pt7b, ink, lgfx::middle_left, scale);
    label(tile_, detail, 74, mid + 15, &lgfx::fonts::FreeSans9pt7b, muted, lgfx::middle_left);
    if (hasValue) {
        label(tile_, style.value, 301, mid - 11, &lgfx::fonts::FreeSansBold9pt7b, style.accent, lgfx::middle_right);
    } else if (style.interactive) {
        const auto color = style.selected ? mint : muted;
        tile_.drawWideLine(292, mid - 5, 298, mid + 1, 1.2f, color);
        tile_.drawWideLine(298, mid + 1, 292, mid + 7, 1.2f, color);
    }
    presentTile(area);
}

void Renderer::deviceBattery(int percent) {
    if (devicePercent_ == percent) return;
    devicePercent_ = percent;
    beginPaint();
    char value[8] = "--";
    if (percent >= 0) snprintf(value, sizeof(value), "%u%%", unsigned(std::min(percent, 100)));
    display_.fillRect(185, 28, 96, 27, background);
    display_.drawRoundRect(200, 35, 22, 12, 2, muted);
    display_.fillRect(223, 39, 2, 4, muted);
    if (percent > 0) display_.fillRect(203, 38, std::max(1, 16 * std::min(percent, 100) / 100), 6, mint);
    label(display_, value, 231, 41, &lgfx::fonts::FreeSans9pt7b, muted, lgfx::middle_left);
}

void Renderer::vehicleBatteryRing(int percent, bool cached) {
    constexpr int segments = 36;
    const int filled = std::max(0, percent) * segments / 100;
    if (batterySegments_ == filled && batteryCached_ == cached) return;
    beginPaint();
    for (int i = 0; i < segments; ++i) {
        if (batterySegments_ >= 0 && cached == batteryCached_ &&
            (i < batterySegments_) == (i < filled)) continue;
        const int angle = 144 + i * 7;
        const auto color = i < filled ? (cached ? muted : mint) : frame;
        display_.fillArc(center, center, 214, 207, angle, angle + 4, color);
    }
    batterySegments_ = filled;
    batteryCached_ = cached;
}

void Renderer::button(const Rect& area, const char* title, bool selected, bool enabled) {
    beginPaint();
    const bool highlighted = selected && enabled;
    display_.fillRoundRect(area.x, area.y, area.w, area.h, area.h / 2, highlighted ? mint : panel);
    if (!highlighted) display_.drawRoundRect(area.x, area.y, area.w, area.h, area.h / 2, frame);
    const lgfx::IFont* font = &lgfx::fonts::FreeSansBold9pt7b;
    if (static_cast<unsigned char>(title[0]) >= 0x80) font = &lgfx::fonts::efontCN_16_b;
    display_.setFont(font);
    display_.setTextSize(1);
    const int textWidth = display_.textWidth(title);
    const int available = area.w - 10;
    const float scale = textWidth > available ? float(available) / textWidth : 1.0f;
    label(display_, title, area.x + area.w / 2, area.y + area.h / 2,
          font, highlighted ? background : enabled ? ink : muted, lgfx::middle_center, scale);
}

void Renderer::autoMuyuButton(bool selected, bool active) {
    beginPaint();
    const auto& area = codexAutoMuyuButton;
    const auto fill = selected ? mint : active ? selectedPanel : panel;
    const auto edge = (selected || active) ? mint : frame;
    display_.fillRoundRect(area.x, area.y, area.w, area.h, area.h / 2, fill);
    display_.drawRoundRect(area.x, area.y, area.w, area.h, area.h / 2, edge);
    const int y = area.y + area.h / 2;
    label(display_, "无限功德模式", area.x + 11, y, &lgfx::fonts::efontCN_16_b,
          selected ? background : ink, lgfx::middle_left);
    const int trackX = area.x + area.w - 37;
    display_.fillRoundRect(trackX, y - 7, 26, 14, 7, selected ? selectedPanel : active ? mint : iconPanel);
    display_.fillCircle(trackX + (active ? 19 : 7), y, 4, selected ? mint : active ? background : muted);
}

void Renderer::footer(bool selected) {
    if (footerSelected_ == int(selected)) return;
    footerSelected_ = selected;
    button(homeButton, "Home", selected);
}

void Renderer::renderTesla(const TeslaApp& tesla, uint32_t now) {
    const auto& state = tesla.state();
    const bool mock = tesla.isMock();
    const bool healthy = tesla.healthy();
    const bool fresh = !stale(state, now);
    const bool cached = state.valid && (!fresh || !healthy);
    const bool online = fresh && healthy && state.connection == VehicleConnection::Online;
    const bool live = online && state.speedValid;
    const char* mode = mock ? "DEMO" : state.source == VehicleSource::Fixture ? "TEST FIXTURE" : "API";
    if (strcmp(mode, modeLabel_) != 0) {
        modeLabel_ = mode;
        beginPaint();
        const int width = state.source == VehicleSource::Fixture && !mock ? 140 : 68;
        display_.fillRect(center - 70, 89, 140, 23, background);
        display_.fillRoundRect(center - width / 2, 89, width, 23, 11, panel);
        label(display_, mode, center, 100, &lgfx::fonts::FreeSansBold9pt7b,
              !mock && state.source == VehicleSource::Api ? mint : amber);
    }

    char speed[12] = "--";
    if (live) snprintf(speed, sizeof(speed), "%.0f", state.speedKmh);
    if (prepareTile(0, speed)) {
        label(tile_, speed, tileWidth / 2, 35, &lgfx::fonts::FreeSansBold24pt7b,
              live ? ink : muted, lgfx::middle_center, 1.5f);
        label(tile_, "km/h", tileWidth / 2, 66, &lgfx::fonts::FreeSans9pt7b, muted);
        presentTile({rows[0].x, 113, tileWidth, tileHeight});
    }

    char charge[12] = "--", range[18] = "--", key[64];
    const int percent = state.valid ? std::clamp(int(state.batteryPercent + 0.5f), 0, 100) : -1;
    const char* rangeType = state.rangeKind == RangeKind::Rated ? "Rated range"
                          : state.rangeKind == RangeKind::Ideal ? "Ideal range" : "Est. range";
    if (state.valid) {
        snprintf(charge, sizeof(charge), "%d%%", percent);
        snprintf(range, sizeof(range), "%.0f km", state.rangeKm);
    }
    vehicleBatteryRing(percent, cached);
    snprintf(key, sizeof(key), "%s|%s|%s|%d", charge, range, rangeType, cached);
    if (prepareTile(1, key)) {
        tile_.fillRoundRect(0, 4, tileWidth, 68, 18, panel);
        tile_.drawFastVLine(tileWidth / 2, 18, 40, frame);
        label(tile_, charge, 80, 29, &lgfx::fonts::FreeSansBold18pt7b, cached ? muted : mint);
        label(tile_, "Battery", 80, 55, &lgfx::fonts::FreeSans9pt7b, muted);
        label(tile_, range, 240, 29, &lgfx::fonts::FreeSansBold12pt7b, ink);
        label(tile_, rangeType, 240, 55, &lgfx::fonts::FreeSans9pt7b, muted);
        presentTile(rows[1]);
    }

    const char* status = tesla.status();
    if (online) {
        status = state.charging ? "Charging" : !state.speedValid ? "Speed unknown"
                                 : state.speedKmh > 0 ? "Driving" : "Parked";
    }
    const char* action = mock ? (state.charging ? "Simulate drive" : "Simulate charge") : "Refresh";
    char detail[48];
    if (state.valid) {
        unsigned long age = ageMs(state, now) / 1000;
        const char unit = age >= 3600 ? 'h' : age >= 60 ? 'm' : 's';
        if (unit == 'h') age /= 3600;
        if (unit == 'm') age /= 60;
        snprintf(detail, sizeof(detail), "%s / %lu%c ago", action, age, unit);
    } else {
        snprintf(detail, sizeof(detail), "%s / No data", action);
    }
    row(2, rows[2], status, detail, {state.charging && online ? Icon::Charge : Icon::Refresh,
                            state.charging && online ? amber : mint, tesla.selected() == 0});
    footer(tesla.selected() == 1);
}

void Renderer::renderCodex(const CodexApp& codex, const Network& network, uint32_t now) {
    using namespace codexPet;
    const auto status = codex.status(now);
    struct Appearance {
        const char* status;
        uint16_t color;
        const Frame* frames;
        size_t count;
        bool repeat;
    };
    static constexpr Appearance appearances[]{
        {"Ready", mint, idle, std::size(idle), true},
        {"Working", blue, working, std::size(working), true},
        {"Done", mint, complete, std::size(complete), false},
        {"Interrupted", amber, interrupted, std::size(interrupted), false},
        {"Status unavailable", muted, idle, 1, false},
        {"Connecting", muted, idle, 1, false}
    };
    const auto& appearance = appearances[static_cast<unsigned>(status.activity)];
    if (status.activity != codexActivity_ || status.transport != codexTransport_ ||
        status.deviceId != codexDeviceId_ || petFrameOffset_ == UINT32_MAX) {
        codexActivity_ = status.activity;
        codexTransport_ = status.transport;
        codexDeviceId_ = status.deviceId;
        petAnimationStart_ = now;
    }

    const bool switchable = status.deviceCount > 1;
    const bool deviceSelected = switchable && codex.selected() == CodexControl::Device;
    const bool transportSelected = codex.selected() == CodexControl::Transport;
    const auto mode = codex.mode();
    const char* transport = mode == CodexMode::Auto ? "Auto" : mode == CodexMode::Usb ? "USB" : "Wi-Fi";
    const char* activeLink = mode != CodexMode::Auto ? "" : status.transport == CodexTransport::Lan ? "Wi-Fi" :
                             status.transport == CodexTransport::Usb ? "USB" : "";
    const char* waiting = mode == CodexMode::Usb ? "Waiting for USB bridge" :
                          !network.online() ? "Connecting to Wi-Fi" :
                          mode == CodexMode::Wifi ? "Waiting for Wi-Fi bridge" : "Waiting for PC bridge";
    const char* deviceName = status.transport == CodexTransport::Lan ? status.deviceName :
                             status.transport == CodexTransport::Usb ? "PC bridge" : waiting;
    char deviceKey[96];
    snprintf(deviceKey, sizeof(deviceKey), "%s|%s|%s|%s|%u|%u|%d|%d", appearance.status, transport, activeLink, deviceName,
             status.deviceNumber, status.deviceCount, deviceSelected, transportSelected);
    if (prepareTile(2, deviceKey)) {
        const auto& area = codexStatusCard;
        const auto& control = codexTransportButton;
        tile_.fillRoundRect(0, 0, area.w, area.h, 18, deviceSelected ? selectedPanel : panel);
        tile_.drawRoundRect(0, 0, area.w, area.h, 18, deviceSelected ? mint : frame);
        const int controlX = control.x - area.x;
        const int controlY = control.y - area.y;
        const int statusY = controlY + control.h / 2;
        const int dividerY = codexDeviceButton.y - area.y;
        const int deviceY = dividerY + codexDeviceButton.h / 2;
        tile_.fillCircle(19, statusY, 5, appearance.color);
        tile_.setFont(&lgfx::fonts::FreeSansBold12pt7b);
        tile_.setTextSize(1);
        const int statusWidth = tile_.textWidth(appearance.status);
        const int statusSpace = controlX - 43;
        const float statusScale = statusWidth > statusSpace ? float(statusSpace) / statusWidth : 1.0f;
        label(tile_, appearance.status, 34, statusY, &lgfx::fonts::FreeSansBold12pt7b, ink, lgfx::middle_left, statusScale);
        tile_.fillRoundRect(controlX, controlY, control.w, control.h, 13, transportSelected ? mint : iconPanel);
        tile_.drawRoundRect(controlX, controlY, control.w, control.h, 13, transportSelected ? mint : frame);
        label(tile_, transport, controlX + 31, statusY, &lgfx::fonts::FreeSansBold9pt7b, transportSelected ? background : ink);
        icon(tile_, Icon::Source, controlX + 66, statusY, transportSelected ? background : mint, 0.56f);
        tile_.drawFastHLine(14, dividerY, area.w - 28, frame);
        const int nameWidth = switchable || *activeLink ? 166 : area.w - 32;
        tile_.setFont(&lgfx::fonts::FreeSans9pt7b);
        tile_.setTextSize(1);
        const int textWidth = tile_.textWidth(deviceName);
        const float scale = textWidth > nameWidth ? float(nameWidth) / textWidth : 1.0f;
        label(tile_, deviceName, 16, deviceY, &lgfx::fonts::FreeSans9pt7b, muted, lgfx::middle_left, scale);
        if (switchable) {
            char position[8];
            snprintf(position, sizeof(position), "%u/%u", status.deviceNumber, status.deviceCount);
            label(tile_, position, area.w - 47, deviceY, &lgfx::fonts::FreeSansBold9pt7b, mint, lgfx::middle_right);
            icon(tile_, Icon::Source, area.w - 26, deviceY, mint, 0.65f);
        } else if (*activeLink) {
            label(tile_, activeLink, area.w - 15, deviceY, &lgfx::fonts::FreeSans9pt7b, mint, lgfx::middle_right);
        }
        presentTile(area);
    }

    const auto gestures = codex.gestures();
    if (seenGestures_ != gestures) {
        seenGestures_ = gestures;
        playingGesture_ = true;
    }
    struct GestureAnimation { const Frame* frames; size_t count; };
    static constexpr GestureAnimation gestureAnimations[]{
        {waving, std::size(waving)},
#define SW_PET_ACTION(id, title, clip, key, x, y, w) {clip, std::size(clip)},
#include "../apps/CodexPetActions.def"
#undef SW_PET_ACTION
    };
    static_assert(std::size(gestureAnimations) == unsigned(PetGesture::Count));
    struct WorkoutAnimation { GestureAnimation animation; const char* name; };
    static constexpr WorkoutAnimation workouts[]{
        {{benchPress, std::size(benchPress)}, "卧推"},
        {{latPulldown, std::size(latPulldown)}, "拉背"},
        {{gobletSquat, std::size(gobletSquat)}, "练腿"},
    };
    static_assert(std::size(workouts) == unsigned(PetWorkout::Count));

    const bool showDice = codex.gesture() == PetGesture::Dice;
    const bool showMuyu = codex.gesture() == PetGesture::Muyu;
    const bool showWorkout = codex.gesture() == PetGesture::Workout;
    const auto& workout = workouts[unsigned(codex.workout())];
    auto gesture = gestureAnimations[unsigned(codex.gesture())];
    if (showWorkout) gesture = workout.animation;
    if (showMuyu && codex.autoMuyu()) gesture = {rapidKnocking, std::size(rapidKnocking)};
    const uint32_t gestureElapsed = now - codex.gestureStarted();
    if (playingGesture_ && gestureElapsed >= animationDuration(gesture.frames, gesture.count))
        playingGesture_ = false;

    const bool compact = showDice || showMuyu;
    const char* layout = showDice ? "dice" : showMuyu ? "muyu" : showWorkout ? "workout" : "pet";
    if (cacheChanged(1, layout)) {
        const auto& area = codexPetButton;
        beginPaint();
        display_.fillRect(area.x, area.y, area.w, area.h, background);
        petFrameOffset_ = UINT32_MAX;
        keys_[0][0] = '\0';
        keys_[3][0] = '\0';
    }
    const auto& pose = playingGesture_ ? frameAt(gesture.frames, gesture.count, gestureElapsed, false) :
        (showMuyu || showWorkout) ? gesture.frames[gesture.count - 1] :
        frameAt(appearance.frames, appearance.count, now - petAnimationStart_, appearance.repeat);
    if (pose.offset != petFrameOffset_) {
        petFrameOffset_ = pose.offset;
        beginPaint();
        const float scale = showWorkout ? 1.0f : compact ? 0.84f : 0.88f;
        const int poseX = compact ? 104 : center - int((showWorkout ? workoutWidth : width) * scale) / 2;
        display_.drawPng(data + pose.offset, pose.bytes, poseX, compact ? 94 : 87, 0, 0, 0, 0, scale);
    }
    if (showDice) {
        constexpr unsigned rollingFaces[]{3, 5, 2, 6, 1, 4};
        const unsigned face = playingGesture_ ? rollingFaces[(gestureElapsed / 90) % std::size(rollingFaces)] : codex.diceResult();
        renderDice(face, playingGesture_);
    }
    if (showMuyu) renderKnocks(codex.plays(PetGesture::Muyu));
    if (showWorkout && cacheChanged(3, workout.name)) {
        beginPaint();
        display_.fillRoundRect(center - 44, 235, 88, 25, 12, panel);
        display_.drawRoundRect(center - 44, 235, 88, 25, 12, frame);
        label(display_, workout.name, center, 247, &lgfx::fonts::efontCN_16_b, amber);
    }

    char selectionKey[12];
    snprintf(selectionKey, sizeof(selectionKey), "%u|%d", unsigned(codex.selected()), codex.autoMuyu());
    if (cacheChanged(0, selectionKey)) {
        button(codexHomeButton, "Home", codex.selected() == CodexControl::Home);
        for (const auto& action : codexActionButtons) {
            button(action.area, action.label, codex.selected() == action.control);
        }
        autoMuyuButton(codex.selected() == CodexControl::AutoMuyu, codex.autoMuyu());
        const auto& area = codexPetButton;
        display_.drawRoundRect(area.x, area.y, area.w, area.h, 24,
                               codex.selected() == CodexControl::Pet ? mint : background);
    }
}

void Renderer::renderDice(unsigned face, bool rolling) {
    char key[8];
    snprintf(key, sizeof(key), "%u|%d", face, rolling);
    if (!cacheChanged(3, key)) return;
    beginPaint();
    constexpr int x = 306;
    const int y = rolling && (face & 1) ? 148 : 152;
    display_.fillRect(257, 96, 108, 162, background);
    display_.fillRoundRect(x - 47, y - 44, 94, 96, 20, frame);
    display_.fillRoundRect(x - 47, y - 48, 94, 96, 20, rgb(244, 232, 212));
    constexpr uint16_t pips[]{0x010, 0x101, 0x111, 0x145, 0x155, 0x16D};
    for (int i = 0; i < 9; ++i) {
        if (pips[face - 1] & (1 << i)) display_.fillCircle(x + (i % 3 - 1) * 24, y + (i / 3 - 1) * 24, 7, background);
    }
    if (rolling) label(display_, "Rolling", x, 226, &lgfx::fonts::FreeSansBold9pt7b, muted);
    else {
        const char result[]{char('0' + face), '\0'};
        label(display_, result, x, 227, &lgfx::fonts::FreeSansBold24pt7b, mint);
    }
}

void Renderer::renderKnocks(uint32_t count) {
    char value[12];
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(count));
    if (!cacheChanged(3, value)) return;
    beginPaint();
    display_.fillRoundRect(257, 130, 102, 110, 20, panel);
    display_.drawRoundRect(257, 130, 102, 110, 20, frame);
    label(display_, "Knocks", 308, 155, &lgfx::fonts::FreeSansBold9pt7b, muted);
    const float scale = std::min(1.0f, 3.1f / strlen(value));
    label(display_, value, 308, 199, &lgfx::fonts::FreeSansBold24pt7b, amber, lgfx::middle_center, scale);
}

void Renderer::render(AppId page, const Launcher& launcher, const TeslaApp& tesla,
                      const SettingsApp& settings, const CodexApp& codex,
                      const DeviceStatus& device, const Network& network, uint32_t now) {
    if (page != page_) {
        page_ = page;
        for (auto& key : keys_) key[0] = '\0';
        modeLabel_ = "";
        devicePercent_ = -2;
        batterySegments_ = -1;
        footerSelected_ = -1;
        petFrameOffset_ = UINT32_MAX;
        playingGesture_ = false;
        beginPaint();
        display_.fillScreen(background);
        display_.drawCircle(center, center, 222, frame);
        switch (page) {
        case AppId::Home:
            label(display_, "StopWatch", center, 77, &lgfx::fonts::FreeSansBold18pt7b, ink);
            display_.fillRoundRect(148, 380, 23, 24, 6, panel);
            display_.fillRoundRect(250, 380, 23, 24, 6, panel);
            label(display_, "A", 159, 392, &lgfx::fonts::FreeSansBold9pt7b, mint);
            label(display_, "B", 261, 392, &lgfx::fonts::FreeSansBold9pt7b, mint);
            label(display_, "Next", 179, 392, &lgfx::fonts::FreeSans9pt7b, muted, lgfx::middle_left);
            label(display_, "Open", 281, 392, &lgfx::fonts::FreeSans9pt7b, muted, lgfx::middle_left);
            break;
        case AppId::Tesla:
            label(display_, "Tesla", center, 63, &lgfx::fonts::FreeSansBold18pt7b, ink);
            break;
        case AppId::Settings:
            label(display_, "Settings", center, 77, &lgfx::fonts::FreeSansBold18pt7b, ink);
            break;
        case AppId::Motorcycle:
            label(display_, "Motorcycle", center, 79, &lgfx::fonts::FreeSansBold18pt7b, ink);
            display_.fillCircle(center, 192, 75, panel);
            display_.drawCircle(center, 192, 75, frame);
            icon(display_, Icon::Bike, center, 192, amber, 2.8f);
            label(display_, "Coming soon", center, 303, &lgfx::fonts::FreeSansBold12pt7b, ink);
            label(display_, "Riding dashboard", center, 329, &lgfx::fonts::FreeSans9pt7b, muted);
            break;
        case AppId::Codex:
            label(display_, "Codex", center, 65, &lgfx::fonts::FreeSansBold18pt7b, ink);
            break;
        default: break;
        }
    }

    switch (page) {
    case AppId::Home: {
        constexpr Icon icons[]{Icon::Car, Icon::Bike, Icon::Settings, Icon::Codex};
        constexpr uint16_t accents[]{mint, amber, blue, blue};
        deviceBattery(device.batteryPercent);
        for (size_t i = 0; i < launchItems.size(); ++i)
            row(i, launcherRows[i], launchItems[i].title, launchItems[i].subtitle,
                {icons[i], accents[i], launcher.selected() == i});
        break;
    }
    case AppId::Tesla:
        renderTesla(tesla, now);
        break;
    case AppId::Settings:
        deviceBattery(device.batteryPercent);
        row(0, rows[0], "Wi-Fi", network.online() ? network.address() : network.label(),
            {Icon::Wifi, network.online() ? mint : muted, false, "", false});
        row(1, rows[1], "Data source", tesla.isMock() ? "Simulated vehicle data" : "Read-only vehicle data",
            {Icon::Source, blue, settings.selected() == 2, tesla.isMock() ? "Mock" : "API"});
        row(2, rows[2], "Haptics", "Button and touch feedback",
            {Icon::Haptics, settings.haptics() ? mint : muted, settings.selected() == 0,
             settings.haptics() ? "On" : "Off"});
        footer(settings.selected() == 1);
        break;
    case AppId::Motorcycle:
        footer(true);
        break;
    case AppId::Codex:
        renderCodex(codex, network, now);
        break;
    default: break;
    }
    if (painting_) {
        display_.endWrite();
        painting_ = false;
    }
}
} // namespace sw
