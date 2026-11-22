#pragma once
#include "../core/Application.h"
#include <cstddef>

namespace sw {
enum class CodexActivity : uint8_t {
    Idle, Working, Complete, Interrupted, Unavailable, Disconnected
};
enum class CodexTransport : uint8_t { None, Usb, Lan };
enum class CodexMode : uint8_t { Auto, Usb, Wifi };
struct CodexStatus {
    CodexActivity activity;
    CodexTransport transport;
    uint64_t deviceId = 0;
    const char* deviceName = "";
    unsigned deviceNumber = 0;
    unsigned deviceCount = 0;
};
inline constexpr uint32_t codexHeartbeatTimeoutMs = 8000;
inline constexpr uint32_t codexAutoKnockIntervalMs = 180;
inline constexpr uint32_t codexAutoKnockPauseMs = 2700;
inline constexpr size_t codexMaxLanDevices = 8;
inline constexpr size_t codexDeviceNameLength = 16;
using CodexDeviceName = std::array<char, codexDeviceNameLength + 1>;
inline constexpr Rect codexPetButton{99, 86, 268, 176};
inline constexpr Rect codexStatusCard{100, 266, 266, 63};
inline constexpr Rect codexTransportButton{273, 269, 85, 28};
inline constexpr Rect codexDeviceButton{100, 298, 266, 31};
inline constexpr Rect codexHomeButton{113, 54, 60, 30};
inline constexpr Rect codexAutoMuyuButton{202, 381, 149, 39};
enum class CodexControl : uint8_t {
    Home, Pet,
#define SW_PET_ACTION(id, title, clip, key, x, y, width) id,
#include "CodexPetActions.def"
#undef SW_PET_ACTION
    AutoMuyu, Device, Transport, Count
};
enum class PetGesture : uint8_t {
    Wave,
#define SW_PET_ACTION(id, title, clip, key, x, y, width) id,
#include "CodexPetActions.def"
#undef SW_PET_ACTION
    Count
};
enum class PetWorkout : uint8_t { BenchPress, LatPulldown, GobletSquat, Count };
struct CodexActionButton {
    CodexControl control;
    PetGesture gesture;
    Rect area;
    const char* label;
    char diagnostic;
};
inline constexpr CodexActionButton codexActionButtons[]{
#define SW_PET_ACTION(id, title, clip, key, x, y, width) {CodexControl::id, PetGesture::id, {x, y, width, 39}, title, key},
#include "CodexPetActions.def"
#undef SW_PET_ACTION
};
constexpr Rect codexDiagnosticTarget(char command) {
    for (const auto& action : codexActionButtons) {
        if (action.diagnostic == command) return action.area;
    }
    if (command == 'u') return codexAutoMuyuButton;
    if (command == 'd') return codexDeviceButton;
    if (command == 't') return codexTransportButton;
    return {};
}

class CodexApp final : public Application {
public:
    using RandomWord = uint32_t (*)();
    explicit CodexApp(RandomWord randomWord) : randomWord_(randomWord) {}

    bool receive(char command, uint32_t now) {
        if (command < '0' || command > '4') return false;
        usb_ = {static_cast<CodexActivity>(command - '0'), now};
        return true;
    }

    bool receiveLan(uint64_t id, const CodexDeviceName& name, char command, uint64_t sequence, uint32_t now) {
        if (id == 0 || command < '0' || command > '4') return false;

        auto slot = lan_.size();
        for (size_t i = 0; i < lan_.size(); ++i) {
            const auto& device = lan_[i];
            if (device.id == id) {
                if (device.heartbeat.fresh(now) && sequence <= device.sequence) return false;
                slot = i;
                break;
            }
            if (slot == lan_.size() && !device.heartbeat.fresh(now)) slot = i;
        }
        if (slot == lan_.size()) return false;

        auto& device = lan_[slot];
        device = {id, sequence, name, {static_cast<CodexActivity>(command - '0'), now}};
        device.name.back() = '\0';
        return true;
    }

    void disconnectLan() {
        lan_ = {};
        selectedLan_ = lan_.size();
    }

    void tick(uint32_t now) {
        now_ = now;
        if (!usb_.fresh(now)) usb_ = {};
        for (auto& device : lan_) {
            if (!device.heartbeat.fresh(now)) device.heartbeat = {};
        }
        selectedLan_ = activeDevice(now);
        if (selected_ == CodexControl::Device && status(now).deviceCount < 2) selected_ = CodexControl::Home;
    }

    CodexStatus status(uint32_t now) const {
        const auto active = activeDevice(now);
        if (mode_ != CodexMode::Usb && active < lan_.size()) {
            const auto& device = lan_[active];
            CodexStatus status{device.heartbeat.activity, CodexTransport::Lan, device.id, device.name.data()};
            for (size_t i = 0; i < lan_.size(); ++i) {
                if (!lan_[i].heartbeat.fresh(now)) continue;
                ++status.deviceCount;
                if (i == active) status.deviceNumber = status.deviceCount;
            }
            return status;
        }
        if (mode_ != CodexMode::Wifi && usb_.fresh(now)) return {usb_.activity, CodexTransport::Usb};
        return {CodexActivity::Disconnected, CodexTransport::None};
    }

    CodexMode mode() const { return mode_; }

    CodexControl selected() const { return selected_; }
    uint32_t plays(PetGesture gesture) const { return plays_[unsigned(gesture)]; }
    uint32_t gestures() const { return gestures_; }
    unsigned diceResult() const { return diceResult_; }
    PetGesture gesture() const { return gesture_; }
    uint32_t gestureStarted() const { return gestureStarted_; }
    PetWorkout workout() const { return workout_; }
    bool autoMuyu() const { return autoMuyu_; }

    void onEnter() override { selected_ = CodexControl::Home; }
    void onUpdate(uint32_t now) override {
        if (!autoMuyu_) return;
        const auto interval = gesture_ == PetGesture::Muyu ? codexAutoKnockIntervalMs : codexAutoKnockPauseMs;
        if (uint32_t(now - gestureStarted_) >= interval) play(PetGesture::Muyu, now);
    }
    void onExit() override { autoMuyu_ = false; }
    AppId onInput(const InputEvent& event) override {
        if (event.type == InputType::Next) {
            selected_ = static_cast<CodexControl>((unsigned(selected_) + 1) % unsigned(CodexControl::Count));
            if (selected_ == CodexControl::Device && status(now_).deviceCount < 2) selected_ = CodexControl::Transport;
        } else if (event.type == InputType::Confirm) {
            if (selected_ == CodexControl::Home) return AppId::Home;
            if (selected_ == CodexControl::Pet) play(PetGesture::Wave, now_);
            else if (selected_ == CodexControl::AutoMuyu) toggleAutoMuyu();
            else if (selected_ == CodexControl::Device) switchDevice();
            else if (selected_ == CodexControl::Transport) switchTransport();
            else {
                for (const auto& action : codexActionButtons) {
                    if (selected_ == action.control) play(action.gesture, now_);
                }
            }
        } else if (event.type == InputType::Tap) {
            if (codexPetButton.contains(event.x, event.y)) play(PetGesture::Wave, now_);
            else if (codexAutoMuyuButton.contains(event.x, event.y)) toggleAutoMuyu();
            else if (codexDeviceButton.contains(event.x, event.y)) switchDevice();
            else if (codexTransportButton.contains(event.x, event.y)) switchTransport();
            else {
                for (const auto& action : codexActionButtons) {
                    if (action.area.contains(event.x, event.y)) play(action.gesture, now_);
                }
            }
        }
        return AppId::None;
    }

private:
    void toggleAutoMuyu() {
        autoMuyu_ = !autoMuyu_;
        if (autoMuyu_) play(PetGesture::Muyu, now_);
    }
    void play(PetGesture gesture, uint32_t now) {
        gesture_ = gesture;
        gestureStarted_ = now;
        ++gestures_;
        ++plays_[unsigned(gesture)];
        if (gesture == PetGesture::Dice || gesture == PetGesture::Workout) {
            unsigned sample;
            do { sample = randomWord_() & 7u; } while (sample >= 6);
            if (gesture == PetGesture::Dice) diceResult_ = sample + 1;
            else workout_ = static_cast<PetWorkout>(sample % unsigned(PetWorkout::Count));
        }
    }

    struct Heartbeat {
        CodexActivity activity = CodexActivity::Disconnected;
        uint32_t receivedAt = 0;
        bool fresh(uint32_t now) const {
            return activity != CodexActivity::Disconnected &&
                   uint32_t(now - receivedAt) < codexHeartbeatTimeoutMs;
        }
    };
    struct LanDevice {
        uint64_t id = 0;
        uint64_t sequence = 0;
        CodexDeviceName name{};
        Heartbeat heartbeat;
    };
    size_t nextDevice(size_t active, uint32_t now) const {
        const size_t first = active == lan_.size() ? 0 : (active + 1) % lan_.size();
        for (size_t offset = 0; offset < lan_.size(); ++offset) {
            const auto slot = (first + offset) % lan_.size();
            if (lan_[slot].heartbeat.fresh(now)) return slot;
        }
        return lan_.size();
    }
    size_t activeDevice(uint32_t now) const {
        if (selectedLan_ < lan_.size() && lan_[selectedLan_].heartbeat.fresh(now)) return selectedLan_;
        return nextDevice(selectedLan_, now);
    }
    void switchDevice() {
        if (status(now_).deviceCount > 1) selectedLan_ = nextDevice(activeDevice(now_), now_);
    }
    void switchTransport() {
        switch (mode_) {
        case CodexMode::Auto: mode_ = CodexMode::Wifi; break;
        case CodexMode::Wifi: mode_ = CodexMode::Usb; break;
        case CodexMode::Usb: mode_ = CodexMode::Auto; break;
        }
    }

    Heartbeat usb_;
    std::array<LanDevice, codexMaxLanDevices> lan_{};
    size_t selectedLan_ = codexMaxLanDevices;
    uint32_t now_ = 0;
    CodexMode mode_ = CodexMode::Auto;
    CodexControl selected_ = CodexControl::Home;
    PetGesture gesture_ = PetGesture::Wave;
    RandomWord randomWord_;
    std::array<uint32_t, unsigned(PetGesture::Count)> plays_{};
    uint32_t gestures_ = 0;
    uint32_t gestureStarted_ = 0;
    unsigned diceResult_ = 0;
    PetWorkout workout_ = PetWorkout::BenchPress;
    bool autoMuyu_ = false;
};
} // namespace sw
