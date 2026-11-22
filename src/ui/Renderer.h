#pragma once
#include <M5GFX.h>
#include "../apps/Apps.h"
#include "../apps/CodexApp.h"
#include "../hardware/Device.h"
#include "../networking/Network.h"
namespace sw {
class Renderer {
public:
    explicit Renderer(M5GFX& display) : display_(display), tile_(&display) {}
    bool begin();
    uint32_t paintCount() const { return paintCount_; }
    void render(AppId page, const Launcher&, const TeslaApp&, const SettingsApp&, const CodexApp&,
                const DeviceStatus&, const Network&, uint32_t now);
private:
    struct RowStyle;
    void beginPaint();
    bool cacheChanged(size_t index, const char* key);
    bool prepareTile(size_t index, const char* key);
    void presentTile(const Rect&);
    void row(size_t index, const Rect&, const char* title, const char* detail, const RowStyle&);
    void deviceBattery(int percent);
    void vehicleBatteryRing(int percent, bool cached);
    void button(const Rect&, const char* title, bool selected, bool enabled = true);
    void autoMuyuButton(bool selected, bool active);
    void footer(bool selected);
    void renderTesla(const TeslaApp&, uint32_t now);
    void renderCodex(const CodexApp&, const Network&, uint32_t now);
    void renderDice(unsigned face, bool rolling);
    void renderKnocks(uint32_t count);
    M5GFX& display_;
    M5Canvas tile_;
    AppId page_ = AppId::None;
    char keys_[launcherRows.size()][160]{};
    const char* modeLabel_ = "";
    int devicePercent_ = -2;
    int batterySegments_ = -1;
    bool batteryCached_ = false;
    bool painting_ = false;
    int footerSelected_ = -1;
    CodexActivity codexActivity_ = CodexActivity::Disconnected;
    CodexTransport codexTransport_ = CodexTransport::None;
    uint64_t codexDeviceId_ = 0;
    uint32_t petAnimationStart_ = 0;
    uint32_t petFrameOffset_ = UINT32_MAX;
    uint32_t seenGestures_ = 0;
    bool playingGesture_ = false;
    uint32_t paintCount_ = 0;
};
} // namespace sw
