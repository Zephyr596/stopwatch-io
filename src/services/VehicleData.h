#pragma once
#include <cstdint>
namespace sw {
// Time is monotonic milliseconds, NOT a Unix timestamp. No wall clock is needed offline.
enum class VehicleSource : uint8_t { Mock, Api, Fixture };
enum class RangeKind : uint8_t { Estimated, Rated, Ideal };
enum class VehicleConnection : uint8_t { Online, Offline, Asleep };
struct VehicleState {
    float batteryPercent = 72.0f;
    float rangeKm = 324.0f;
    float speedKmh = 0.0f;
    bool charging = false;
    bool valid = false;
    uint32_t updatedAtMs = 0;
    uint32_t revision = 0;
    uint32_t dataAgeMs = 0;
    uint64_t observedAt = 0;
    VehicleSource source = VehicleSource::Mock;
    RangeKind rangeKind = RangeKind::Estimated;
    VehicleConnection connection = VehicleConnection::Online;
    bool speedValid = true;
};
class VehicleDataProvider {
public:
    virtual ~VehicleDataProvider() = default;
    virtual void update(uint32_t nowMs) = 0;
    virtual bool getState(VehicleState& state) const = 0;
    virtual bool healthy() const { return true; }
    virtual bool isMock() const { return true; }
    virtual const char* status() const { return "MOCK"; }
};
class MockVehicleDataProvider final : public VehicleDataProvider {
public:
    void toggleCharging() { state_.charging = !state_.charging; pending_ = true; }
    void update(uint32_t now) override {
        if (state_.valid && !pending_ && uint32_t(now - state_.updatedAtMs) < 1000) return;
        pending_ = false;
        // Bounded deterministic triangle speed profile; charging always means parked.
        phase_ = (phase_ + 1) % 40;
        state_.speedKmh = state_.charging ? 0.0f : float((phase_ <= 20 ? phase_ : 40 - phase_) * 4);
        state_.batteryPercent += state_.charging ? 0.10f : -0.01f;
        if (state_.batteryPercent > 100.0f) state_.batteryPercent = 100.0f;
        if (state_.batteryPercent < 0.0f) state_.batteryPercent = 0.0f;
        state_.rangeKm = state_.batteryPercent * 4.5f;
        state_.valid = true;
        state_.updatedAtMs = now;
        ++state_.revision;
    }
    bool getState(VehicleState& state) const override { state = state_; return state_.valid; }
private:
    VehicleState state_;
    uint8_t phase_ = 0;
    bool pending_ = true;
};
inline uint32_t ageMs(const VehicleState& state, uint32_t now) {
    const uint32_t elapsed = now - state.updatedAtMs;
    return elapsed > UINT32_MAX - state.dataAgeMs ? UINT32_MAX : elapsed + state.dataAgeMs;
}
inline bool stale(const VehicleState& state, uint32_t now) {
    return !state.valid || ageMs(state, now) >= (state.source == VehicleSource::Mock ? 5000u : 15000u);
}
} // namespace sw
