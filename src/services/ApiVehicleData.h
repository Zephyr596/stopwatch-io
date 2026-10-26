#pragma once
#include "ApiCache.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
namespace sw {
class ApiVehicleDataProvider final : public VehicleDataProvider {
public:
    bool begin();
    void update(uint32_t now) override;
    bool getState(VehicleState& state) const override { state = cache_.state(); return state.valid; }
    bool isMock() const override { return false; }
    bool healthy() const override { return cache_.error() == ApiError::None; }
    const char* status() const override { return cache_.status(now_); }
    void refresh() { requested_ = true; }
    ApiError error() const { return cache_.error(); }
    uint32_t stackFree() const { return task_ ? uxTaskGetStackHighWaterMark(task_) : 0; }
private:
    struct Result { VehicleState state; ApiError error = ApiError::None; };
    static void worker(void* context);
    Result fetch();
    ApiCache cache_;
    QueueHandle_t requests_ = nullptr, results_ = nullptr;
    TaskHandle_t task_ = nullptr;
    bool pending_ = false, requested_ = true, attempted_ = false;
    uint32_t lastAttempt_ = 0, now_ = 0;
};
// Explicit source selection. Network failures never silently substitute fictional data.
class VehicleDataService final : public VehicleDataProvider {
public:
    VehicleDataService(MockVehicleDataProvider& mock, ApiVehicleDataProvider& api, bool defaultApi = false) : mock_(mock), api_(api), mockMode_(!defaultApi) {}
    void update(uint32_t now) override { if (mockMode_) mock_.update(now); else api_.update(now); }
    bool getState(VehicleState& s) const override { return mockMode_ ? mock_.getState(s) : api_.getState(s); }
    bool isMock() const override { return mockMode_; }
    bool healthy() const override { return mockMode_ || api_.healthy(); }
    const char* status() const override { return mockMode_ ? "MOCK" : api_.status(); }
    void toggleSource() { mockMode_ = !mockMode_; if (!mockMode_) api_.refresh(); }
    void action() { if (mockMode_) mock_.toggleCharging(); else api_.refresh(); }
private:
    MockVehicleDataProvider& mock_;
    ApiVehicleDataProvider& api_;
    bool mockMode_ = true;
};
}
