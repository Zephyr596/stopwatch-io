#include "Device.h"
#include "../apps/CodexApp.h"
namespace sw {
namespace {
extern const uint8_t muyuWav[] asm("_binary_assets_codex_muyu_wav_start");
extern const uint8_t muyuWavEnd[] asm("_binary_assets_codex_muyu_wav_end");
extern const uint8_t sutraWav[] asm("_binary_assets_codex_muyu_100_wav_start");
extern const uint8_t sutraWavEnd[] asm("_binary_assets_codex_muyu_100_wav_end");
extern const uint8_t smokeWav[] asm("_binary_assets_codex_smoke_wav_start");
extern const uint8_t smokeWavEnd[] asm("_binary_assets_codex_smoke_wav_end");
extern const uint8_t whiskyWav[] asm("_binary_assets_codex_whisky_wav_start");
extern const uint8_t whiskyWavEnd[] asm("_binary_assets_codex_whisky_wav_end");
extern const uint8_t diceWav[] asm("_binary_assets_codex_dice_wav_start");
extern const uint8_t diceWavEnd[] asm("_binary_assets_codex_dice_wav_end");
extern const uint8_t workoutWav[] asm("_binary_assets_codex_workout_wav_start");
extern const uint8_t workoutWavEnd[] asm("_binary_assets_codex_workout_wav_end");

struct WavClip {
    const uint8_t* start;
    const uint8_t* end;
};
struct PetSound {
    PetGesture gesture;
    const char* name;
    WavClip clip;
};
constexpr PetSound petSounds[]{
    {PetGesture::Smoke, "smoke", {smokeWav, smokeWavEnd}},
    {PetGesture::Whisky, "whisky", {whiskyWav, whiskyWavEnd}},
    {PetGesture::Dice, "dice", {diceWav, diceWavEnd}},
    {PetGesture::Muyu, "muyu", {muyuWav, muyuWavEnd}},
    {PetGesture::Workout, "workout", {workoutWav, workoutWavEnd}},
};
constexpr WavClip sutra{sutraWav, sutraWavEnd};
constexpr uint8_t effectsChannel = 0;
constexpr uint8_t sutraChannel = 1;
constexpr uint8_t maxSpeakerVolume = 255;
}
bool Device::begin() {
    Serial.begin(115200); // Do not wait for USB connection.
    Serial.println("[boot] StopWatch / apps + read-only vehicle API");
    auto cfg = M5.config();
    cfg.internal_mic = false;
    cfg.internal_spk = true;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    M5.begin(cfg);
    // Current official driver exposes 468x468 for the 466x466 visible panel.
    ready_ = M5.getBoard() == m5::board_t::board_M5StopWatch &&
             M5.Display.width() >= 466 && M5.Display.height() >= 466;
    M5.BtnA.setHoldThresh(700);
    M5.BtnB.setHoldThresh(700);
    M5.Speaker.setVolume(maxSpeakerVolume);
    M5.Speaker.setAllChannelVolume(maxSpeakerVolume);
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
void Device::playPetSound(PetGesture gesture, bool playSutra) {
    if (!ready_) return;
    for (const auto& sound : petSounds) {
        if (sound.gesture != gesture) continue;
        const auto& clip = playSutra ? sutra : sound.clip;
        const auto bytes = reinterpret_cast<uintptr_t>(clip.end) - reinterpret_cast<uintptr_t>(clip.start);
        auto& speaker = M5.Speaker;
        // Later actions can replace an effect while the passage continues.
        const uint8_t channel = playSutra ? sutraChannel : effectsChannel;
        if (playSutra) speaker.stop(effectsChannel);
        const bool queued = speaker.playWav(clip.start, bytes, 1, channel, true);
        Serial.printf("[%s] audio=%d sutra=%d volume=%u channel_volume=%u\n",
                      sound.name, queued, playSutra, unsigned(speaker.getVolume()),
                      unsigned(speaker.getChannelVolume(channel)));
        return;
    }
}
} // namespace sw
