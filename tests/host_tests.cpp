#include "apps/Apps.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>
using namespace sw;
namespace {
struct Probe : Application {
    Probe(int id, std::vector<int>& calls) : id(id), calls(calls) {}
    int id, updates = 0, inputs = 0;
    std::vector<int>& calls;
    AppId request = AppId::None;
    void onEnter() override { calls.push_back(id); }
    void onExit() override { calls.push_back(-id); }
    void onUpdate(uint32_t) override { ++updates; }
    AppId onInput(const InputEvent&) override { ++inputs; return request; }
};
void lifecycle() {
    std::vector<int> calls;
    Probe home(1, calls), tesla(2, calls), moto(3, calls);
    AppManager manager({{{AppId::Home, "home", &home}, {AppId::Tesla, "tesla", &tesla},
                        {AppId::Motorcycle, "moto", &moto}, {AppId::Settings, "missing", nullptr}}});
    manager.update(0);
    manager.input({InputType::Confirm});
    assert(!manager.switchTo(AppId::None));
    assert(manager.switchTo(AppId::Home));
    assert(!manager.switchTo(AppId::Home));
    assert(!manager.switchTo(AppId::Settings));
    assert(calls == std::vector<int>({1}));
    home.request = AppId::Tesla;
    manager.input({InputType::Confirm});
    assert(calls == std::vector<int>({1, -1, 2}));
    manager.update(42);
    assert(tesla.updates == 1 && home.updates == 0);
    manager.input({InputType::Home});
    assert(calls == std::vector<int>({1, -1, 2, -2, 1}));
    assert(tesla.inputs == 0); // Global Home never leaks into an app.
    for (int i = 0; i < 10000; ++i) {
        assert(manager.switchTo(AppId::Tesla));
        manager.input({InputType::Tap, 233, 394});
        assert(manager.current() == AppId::Home);
    }
    assert(calls.size() == 40005);
}
void launcherAndApps() {
    Launcher home;
    home.onEnter();
    assert(home.onInput({InputType::Confirm}) == AppId::Tesla);
    for (int i = 0; i < 3; ++i) home.onInput({InputType::Next});
    assert(home.selected() == 0);
    for (size_t i = 0; i < rows.size(); ++i) {
        assert(home.onInput({InputType::Tap, 233, int16_t(rows[i].y + 20)}) == launchItems[i].id);
    }
    assert(home.onInput({InputType::Tap, 0, 0}) == AppId::None);
    assert(!rows[0].contains(rows[0].x + rows[0].w, rows[0].y));
    SettingsApp settings;
    settings.onEnter();
    assert(!settings.haptics());
    settings.onInput({InputType::Confirm});
    assert(settings.haptics());
    settings.onInput({InputType::Next});
    assert(settings.onInput({InputType::Confirm}) == AppId::Home);
    settings.onExit(); settings.onEnter();
    assert(settings.haptics() && settings.selected() == 0);
    settings.onInput({InputType::Tap, 233, 300});
    assert(!settings.haptics());
    MockVehicleDataProvider mock;
    TeslaApp tesla(mock, [](void* p) { static_cast<MockVehicleDataProvider*>(p)->toggleCharging(); }, &mock);
    tesla.onEnter(); tesla.onUpdate(0);
    assert(tesla.state().valid && !tesla.state().charging);
    tesla.onInput({InputType::Confirm}); tesla.onUpdate(1);
    assert(tesla.state().charging && tesla.state().speedKmh == 0);
    tesla.onInput({InputType::Tap, 233, 300}); tesla.onUpdate(2);
    assert(!tesla.state().charging);
    tesla.onInput({InputType::Next});
    assert(tesla.onInput({InputType::Confirm}) == AppId::Home);
    tesla.onExit(); tesla.onEnter();
    assert(tesla.selected() == 0);
}
void providerFailure() {
    struct Unavailable final : VehicleDataProvider {
        void update(uint32_t) override {}
        bool getState(VehicleState& state) const override {
            state.valid = true; // A failed fetch must never leave a valid UI snapshot.
            return false;
        }
    } unavailable;
    TeslaApp tesla(unavailable, nullptr, nullptr);
    tesla.onEnter(); tesla.onUpdate(0);
    assert(!tesla.state().valid);
    tesla.onInput({InputType::Confirm});
    tesla.onInput({InputType::Tap, 233, 300});
    assert(!tesla.state().valid);
}
void provider() {
    MockVehicleDataProvider mock;
    VehicleState state;
    assert(!mock.getState(state));
    assert(stale(state, 0));
    mock.update(0); assert(mock.getState(state));
    assert(state.revision == 1 && state.rangeKm == state.batteryPercent * 4.5f);
    mock.update(999); mock.getState(state); assert(state.revision == 1);
    mock.update(1000); mock.getState(state); assert(state.revision == 2);
    assert(!stale(state, 5999) && stale(state, 6000));
    mock.toggleCharging(); mock.update(1001); mock.getState(state);
    assert(state.charging && state.speedKmh == 0 && state.revision == 3);
    for (uint32_t now = 2001; now < 4000001; now += 1000) {
        mock.update(now); mock.getState(state);
        assert(state.batteryPercent >= 0 && state.batteryPercent <= 100);
        assert(state.speedKmh == 0 && state.rangeKm <= 450);
    }
    assert(state.batteryPercent == 100);
    mock.toggleCharging();
    for (uint32_t now = 4000001; now < 16000001; now += 1000) {
        mock.update(now); mock.getState(state);
        assert(state.batteryPercent >= 0 && state.batteryPercent <= 100);
        assert(state.speedKmh >= 0 && state.speedKmh <= 80);
    }
    assert(state.batteryPercent == 0);
    MockVehicleDataProvider rollover;
    const uint32_t start = std::numeric_limits<uint32_t>::max() - 499;
    rollover.update(start);
    rollover.update(499); rollover.getState(state); assert(state.revision == 1);
    rollover.update(500); rollover.getState(state); assert(state.revision == 2);
    assert(!stale(state, 5499) && stale(state, 5500));
}
void geometry() {
    // Every interactive rectangle is within the round 466px viewport, with margin.
    auto check = [](const Rect& r) {
        for (int x : {int(r.x), int(r.x + r.w)})
            for (int y : {int(r.y), int(r.y + r.h)})
                assert((x - 233) * (x - 233) + (y - 233) * (y - 233) < 223 * 223);
    };
    for (const auto& r : rows) check(r);
    check(homeButton);
}
}
int main() {
    lifecycle(); launcherAndApps(); provider(); providerFailure(); geometry();
    puts("PASS: lifecycle, 10000 switches, input routing, launcher, settings, mock cadence/ranges, stale state, millis rollover, circular layout");
}
