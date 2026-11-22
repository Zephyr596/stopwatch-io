#pragma once
#include <M5Unified.h>
#include "../core/Application.h"
namespace sw {
enum class PetGesture : uint8_t;

struct DeviceStatus {
    int batteryPercent = -1;
    uint32_t freeHeap = 0, minHeap = 0, freePsram = 0;
};
class Device {
public:
    bool begin();
    bool poll(uint32_t now, InputEvent& event);
    void feedback(uint32_t now, bool enabled);
    void playPetSound(PetGesture gesture, bool playSutra);
    const DeviceStatus& status() const { return status_; }
    bool ready() const { return ready_; }
private:
    void sample();
    bool ready_ = false, vibrating_ = false;
    uint32_t vibrationStart_ = 0, lastSample_ = 0, lastLog_ = 0;
    DeviceStatus status_;
};
} // namespace sw
