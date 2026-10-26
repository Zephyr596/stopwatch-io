#pragma once
#include <M5GFX.h>
#include "../apps/Apps.h"
#include "../hardware/Device.h"
#include "../networking/Network.h"
namespace sw {
class Renderer {
public:
    explicit Renderer(M5GFX& display) : display_(display), tile_(&display) {}
    bool begin();
    uint32_t paintCount() const { return paintCount_; }
    void render(AppId page, const Launcher&, const TeslaApp&, const SettingsApp&, const DeviceStatus&, const Network&, uint32_t now);
private:
    void text(const char* value, int y, int size, uint16_t color);
    void card(size_t index, const char* title, const char* subtitle, bool selected, int size = 3, int progress = -1);
    void footer(bool selected);
    M5GFX& display_;
    M5Canvas tile_;
    AppId page_ = AppId::None;
    bool mock_ = true;
    char keys_[3][160]{};
    int footerSelected_ = -1;
    uint32_t paintCount_ = 0;
};
} // namespace sw
