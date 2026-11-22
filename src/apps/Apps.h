#pragma once
#include "../core/Application.h"
#include "../services/VehicleData.h"
namespace sw {
struct LaunchItem { AppId id; const char* title; const char* subtitle; };
inline constexpr std::array<LaunchItem, launcherRows.size()> launchItems{{
    {AppId::Tesla, "Tesla", "Vehicle dashboard"},
    {AppId::Motorcycle, "Motorcycle", "Coming soon"},
    {AppId::Settings, "Settings", "Wi-Fi, data & haptics"},
    {AppId::Codex, "Codex", "Pet & chat status"}
}};
class Launcher final : public Application {
public:
    void onEnter() override {}
    void onExit() override {}
    void onUpdate(uint32_t) override {}
    AppId onInput(const InputEvent& e) override {
        if (e.type == InputType::Next) selected_ = (selected_ + 1) % launchItems.size();
        if (e.type == InputType::Confirm) return launchItems[selected_].id;
        if (e.type == InputType::Tap) {
            for (size_t i = 0; i < launcherRows.size(); ++i) {
                if (launcherRows[i].contains(e.x, e.y)) { selected_ = i; return launchItems[i].id; }
            }
        }
        return AppId::None;
    }
    size_t selected() const { return selected_; }
private:
    size_t selected_ = 0;
};
class TeslaApp final : public Application {
public:
    using Toggle = void (*)(void*);
    TeslaApp(VehicleDataProvider& provider, Toggle toggle, void* context)
        : provider_(provider), toggle_(toggle), context_(context) {}
    void onEnter() override { selected_ = 0; }
    void onExit() override {}
    void onUpdate(uint32_t now) override { provider_.update(now); if (!provider_.getState(state_)) state_.valid = false; }
    AppId onInput(const InputEvent& e) override {
        if (e.type == InputType::Next) selected_ = 1 - selected_;
        if (e.type == InputType::Confirm) {
            if (selected_ == 1) return AppId::Home;
            if (toggle_) toggle_(context_);
        }
        if (e.type == InputType::Tap && rows[2].contains(e.x, e.y) && toggle_) toggle_(context_);
        return AppId::None;
    }
    const VehicleState& state() const { return state_; }
    bool isMock() const { return provider_.isMock(); }
    bool healthy() const { return provider_.healthy(); }
    const char* status() const { return provider_.status(); }
    size_t selected() const { return selected_; }
private:
    VehicleDataProvider& provider_;
    Toggle toggle_;
    void* context_;
    VehicleState state_;
    size_t selected_ = 0;
};
class MotorcycleApp final : public Application {
public:
    void onEnter() override {}
    void onExit() override {}
    void onUpdate(uint32_t) override {}
    AppId onInput(const InputEvent& e) override { return e.type == InputType::Confirm ? AppId::Home : AppId::None; }
};
class SettingsApp final : public Application {
public:
    using ToggleSource = void (*)(void*);
    SettingsApp(ToggleSource toggle = nullptr, void* context = nullptr) : toggle_(toggle), context_(context) {}
    void onEnter() override { selected_ = 0; }
    void onExit() override {}
    void onUpdate(uint32_t) override {}
    AppId onInput(const InputEvent& e) override {
        if (e.type == InputType::Next) selected_ = (selected_ + 1) % (toggle_ ? 3 : 2);
        if (e.type == InputType::Confirm) {
            if (selected_ == 1) return AppId::Home;
            if (selected_ == 2 && toggle_) toggle_(context_);
            else haptics_ = !haptics_;
        }
        if (e.type == InputType::Tap && rows[1].contains(e.x, e.y) && toggle_) toggle_(context_);
        if (e.type == InputType::Tap && rows[2].contains(e.x, e.y)) haptics_ = !haptics_;
        return AppId::None;
    }
    bool haptics() const { return haptics_; }
    size_t selected() const { return selected_; }
private:
    ToggleSource toggle_;
    void* context_;
    bool haptics_ = false; // Explicit opt-in; never alter charging / regulator settings.
    size_t selected_ = 0;
};
} // namespace sw
