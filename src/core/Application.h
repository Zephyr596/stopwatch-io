#pragma once
#include <array>
#include <cstdint>

namespace sw {
enum class AppId : uint8_t { Home, Tesla, Motorcycle, Settings, None };
enum class InputType : uint8_t { Next, Confirm, Home, Tap };
struct InputEvent { InputType type; int16_t x = 0, y = 0; };
struct Rect {
    int16_t x, y, w, h;
    constexpr bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};
inline constexpr Rect homeButton{153, 374, 160, 40};
inline constexpr std::array<Rect, 3> rows{{{73, 110, 320, 76}, {73, 190, 320, 76}, {73, 270, 320, 76}}};
class Application {
public:
    virtual ~Application() = default;
    virtual void onEnter() = 0;
    virtual void onUpdate(uint32_t nowMs) = 0;
    virtual void onExit() = 0;
    virtual AppId onInput(const InputEvent&) = 0;
};
struct AppEntry { AppId id; const char* name; Application* app; };
class AppManager {
public:
    using SwitchLog = void (*)(const char*);
    AppManager(const std::array<AppEntry, 4>& registry, SwitchLog log = nullptr)
        : registry_(registry), log_(log) {}
    bool switchTo(AppId id) {
        if (id == current_) return false;
        for (const auto& entry : registry_) {
            if (entry.id != id || !entry.app) continue;
            if (active_) active_->onExit();
            active_ = entry.app;
            current_ = id;
            active_->onEnter();
            if (log_) log_(entry.name);
            return true;
        }
        return false;
    }
    void input(const InputEvent& event) {
        if (!active_) return;
        if (event.type == InputType::Home ||
            (current_ != AppId::Home && event.type == InputType::Tap && homeButton.contains(event.x, event.y))) {
            switchTo(AppId::Home);
            return;
        }
        const auto next = active_->onInput(event);
        if (next != AppId::None) switchTo(next);
    }
    void update(uint32_t now) { if (active_) active_->onUpdate(now); }
    AppId current() const { return current_; }
private:
    std::array<AppEntry, 4> registry_;
    SwitchLog log_;
    Application* active_ = nullptr;
    AppId current_ = AppId::None;
};
} // namespace sw
