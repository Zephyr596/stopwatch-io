#include "apps/Apps.h"
#include "apps/CodexApp.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <string_view>
#include <vector>
using namespace sw;
namespace {
uint32_t diceEntropy = 0;
uint32_t nextSample() { return diceEntropy++; }

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
    for (size_t i = 0; i < launchItems.size(); ++i) home.onInput({InputType::Next});
    assert(home.selected() == 0);
    for (size_t i = 0; i < launcherRows.size(); ++i) {
        assert(home.onInput({InputType::Tap, 233, int16_t(launcherRows[i].y + 20)}) == launchItems[i].id);
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
void codexActivity() {
    CodexApp app(nextSample);
    const CodexDeviceName laptop{"Laptop"};
    const auto start = std::numeric_limits<uint32_t>::max() - 10;
    assert(app.status(start).transport == CodexTransport::None);
    assert(app.receive('1', start) && app.status(start + 1).activity == CodexActivity::Working);
    assert(!app.receive('s', start + 2));
    assert(app.status(start + 7999).transport == CodexTransport::Usb);
    assert(app.status(start + 8000).activity == CodexActivity::Disconnected);
    assert(app.receive('2', 8000) && app.status(8000).activity == CodexActivity::Complete);
    assert(app.receiveLan(1, laptop, '1', 10, 8001));
    assert(app.status(8001).transport == CodexTransport::Lan);
    assert(app.status(8001).activity == CodexActivity::Working);
    assert(app.receive('4', 8002) && app.status(8002).transport == CodexTransport::Lan);
    app.disconnectLan();
    assert(app.status(8002).activity == CodexActivity::Unavailable);
    assert(app.status(8002).transport == CodexTransport::Usb);
    assert(app.receiveLan(1, laptop, '1', 11, 9000));
    assert(app.receive('2', 10000));
    assert(app.status(17000).transport == CodexTransport::Usb);
    assert(app.status(18000).activity == CodexActivity::Disconnected);
    assert(app.onInput({InputType::Confirm}) == AppId::Home);
    app.tick(18000);
    for (unsigned index = 0; index < unsigned(CodexControl::Device); ++index) app.onInput({InputType::Next});
    assert(app.selected() == CodexControl::Transport);
    app.onInput({InputType::Next});
    assert(app.selected() == CodexControl::Home);
}
void codexDevices() {
    CodexApp app(nextSample);
    const CodexDeviceName first{"Main PC"}, second{"Laptop"};
    const auto switchTap = InputEvent{InputType::Tap, codexDeviceButton.x + 10, codexDeviceButton.y + 10};
    assert(app.receiveLan(11, first, '1', 1000, 100));
    assert(app.receiveLan(22, second, '2', 30, 200));
    app.tick(200);
    auto current = app.status(200);
    assert(current.deviceId == 11 && current.deviceNumber == 1 && current.deviceCount == 2);
    assert(std::string_view(current.deviceName) == "Main PC");
    app.onInput(switchTap);
    current = app.status(200);
    assert(current.deviceId == 22 && current.deviceNumber == 2 && current.activity == CodexActivity::Complete);
    assert(std::string_view(current.deviceName) == "Laptop");

    assert(app.receiveLan(11, first, '0', 1001, 400));
    assert(!app.receiveLan(22, second, '3', 30, 410));
    assert(!app.receiveLan(22, second, '9', 31, 420));
    app.tick(420);
    assert(app.status(420).deviceId == 22 && app.status(420).activity == CodexActivity::Complete);
    for (unsigned index = 0; index < unsigned(CodexControl::Device); ++index) app.onInput({InputType::Next});
    assert(app.selected() == CodexControl::Device);
    assert(app.onInput({InputType::Confirm}) == AppId::None);
    assert(app.status(420).deviceId == 11 && app.status(420).activity == CodexActivity::Idle);
    app.onInput(switchTap);
    assert(app.status(420).deviceId == 22);

    app.tick(8200);
    assert(app.status(8200).deviceId == 11 && app.status(8200).deviceCount == 1);
    assert(app.selected() == CodexControl::Home);
    assert(app.receiveLan(22, second, '3', 1, 8201));  // A restarted bridge can rejoin.
    app.tick(8201);
    assert(app.status(8201).deviceId == 11 && app.status(8201).deviceCount == 2);
    app.onInput(switchTap);
    assert(app.status(8201).deviceId == 22 && app.status(8201).activity == CodexActivity::Interrupted);
    app.disconnectLan();
    assert(app.status(8201).transport == CodexTransport::None && app.status(8201).deviceCount == 0);

    for (size_t i = 0; i < codexMaxLanDevices; ++i) assert(app.receiveLan(i + 1, first, '0', 1, 9000));
    assert(!app.receiveLan(99, second, '0', 1, 9001));
    assert(app.status(9001).deviceCount == codexMaxLanDevices);
    assert(app.receiveLan(99, second, '0', 2, 17000));
    assert(app.status(17000).deviceId == 99 && app.status(17000).deviceCount == 1);
}
void codexTransportSwitch() {
    CodexApp app(nextSample);
    const CodexDeviceName first{"Main PC"}, second{"Laptop"};
    const InputEvent deviceTap{InputType::Tap, codexDeviceButton.x + 10, codexDeviceButton.y + 10};
    const InputEvent transportTap{InputType::Tap, codexTransportButton.x + 10, codexTransportButton.y + 10};
    assert(app.mode() == CodexMode::Auto);
    assert(app.receive('1', 100));
    assert(app.status(100).transport == CodexTransport::Usb);
    assert(app.receiveLan(11, first, '2', 1, 100));
    assert(app.receiveLan(22, second, '3', 1, 100));
    app.tick(100);
    assert(app.mode() == CodexMode::Auto && app.status(100).deviceId == 11);

    app.onInput(deviceTap);
    app.onInput(transportTap);
    assert(app.mode() == CodexMode::Wifi && app.status(100).deviceId == 22);
    app.onInput(transportTap);
    assert(app.mode() == CodexMode::Usb && app.status(100).transport == CodexTransport::Usb);
    assert(app.status(100).activity == CodexActivity::Working && app.status(100).deviceCount == 0);
    app.onInput(deviceTap);
    app.onInput(transportTap);
    assert(app.mode() == CodexMode::Auto && app.status(100).deviceId == 22);

    app.disconnectLan();
    app.tick(101);
    assert(app.mode() == CodexMode::Auto && app.status(101).transport == CodexTransport::Usb);
    app.onInput(transportTap);
    assert(app.mode() == CodexMode::Wifi && app.status(101).transport == CodexTransport::None);
    assert(app.receiveLan(22, second, '2', 1, 200));
    app.tick(200);
    assert(app.mode() == CodexMode::Wifi && app.status(200).deviceId == 22);
    for (unsigned index = 0; index < unsigned(CodexControl::Device); ++index) app.onInput({InputType::Next});
    assert(app.selected() == CodexControl::Transport);
    assert(app.onInput({InputType::Confirm}) == AppId::None);
    assert(app.mode() == CodexMode::Usb && app.status(200).transport == CodexTransport::Usb);
    app.tick(8100);
    assert(app.mode() == CodexMode::Usb && app.status(8100).transport == CodexTransport::None);
    app.onInput(transportTap);
    assert(app.mode() == CodexMode::Auto && app.status(8100).deviceId == 22);
}
void companionNavigation() {
    std::vector<int> calls;
    Probe home(1, calls);
    CodexApp codex(nextSample);
    const auto tap = [](Rect area) {
        return InputEvent{InputType::Tap, int16_t(area.x + area.w / 2), int16_t(area.y + area.h / 2)};
    };
    AppManager manager({{{AppId::Home, "home", &home}, {AppId::Codex, "codex", &codex, codexHomeButton}}});
    assert(manager.switchTo(AppId::Codex));
    manager.input(tap(codexPetButton));
    assert(codex.gesture() == PetGesture::Wave && codex.plays(PetGesture::Wave) == 1);

    const char commands[]{'p', 'k', 'g', 'j', 'l'};
    static_assert(std::size(commands) == std::size(codexActionButtons));
    for (size_t index = 0; index < std::size(commands); ++index) {
        const auto& action = codexActionButtons[index];
        assert(action.diagnostic == commands[index]);
        manager.input(tap(codexDiagnosticTarget(commands[index])));
        assert(codex.plays(action.gesture) == 1 && codex.gesture() == action.gesture);
    }
    assert(codex.plays(PetGesture::Wave) == 1 && codex.gestures() == 1 + std::size(commands));

    diceEntropy = 6;  // The first two out-of-range samples are rejected.
    for (unsigned expected = 1; expected <= 6; ++expected) {
        manager.input(tap(codexDiagnosticTarget('g')));
        assert(codex.diceResult() == expected);
    }
    assert(codex.plays(PetGesture::Dice) == 7 && codex.plays(PetGesture::Muyu) == 1);

    manager.input({InputType::Next});
    manager.input({InputType::Confirm});
    assert(codex.gesture() == PetGesture::Wave && codex.plays(PetGesture::Wave) == 2);
    for (const auto& action : codexActionButtons) {
        manager.input({InputType::Next});
        assert(codex.selected() == action.control);
        const auto previous = codex.plays(action.gesture);
        manager.input({InputType::Confirm});
        assert(codex.plays(action.gesture) == previous + 1 && codex.gesture() == action.gesture);
    }
    assert(codex.plays(PetGesture::Muyu) == 2);
    manager.input({InputType::Next});
    assert(codex.selected() == CodexControl::AutoMuyu);
    manager.input({InputType::Confirm});
    assert(codex.autoMuyu() && codex.plays(PetGesture::Muyu) == 3);
    manager.input(tap(codexHomeButton));
    assert(manager.current() == AppId::Home && !codex.autoMuyu());
}
void autoMuyuAndWorkout() {
    CodexApp app(nextSample);
    const auto tap = [&](char command) {
        const auto area = codexDiagnosticTarget(command);
        app.onInput({InputType::Tap, int16_t(area.x + area.w / 2), int16_t(area.y + area.h / 2)});
    };
    const auto start = std::numeric_limits<uint32_t>::max() - 25;
    const uint32_t firstRepeat = start + codexAutoKnockIntervalMs;
    app.tick(start);
    tap('u');
    assert(app.autoMuyu() && app.plays(PetGesture::Muyu) == 1);
    app.onUpdate(firstRepeat - 1);
    assert(app.plays(PetGesture::Muyu) == 1);
    app.onUpdate(firstRepeat);
    assert(app.plays(PetGesture::Muyu) == 2);
    app.onUpdate(firstRepeat);
    assert(app.plays(PetGesture::Muyu) == 2);
    app.tick(firstRepeat);
    diceEntropy = 6;  // The workout picker rejects the same extra samples as Dice.
    for (const auto expected : {PetWorkout::BenchPress, PetWorkout::LatPulldown, PetWorkout::GobletSquat,
                                PetWorkout::BenchPress, PetWorkout::LatPulldown, PetWorkout::GobletSquat}) {
        tap('l');
        assert(app.gesture() == PetGesture::Workout && app.workout() == expected && app.autoMuyu());
    }
    assert(app.plays(PetGesture::Workout) == 6 && app.diceResult() == 0);
    app.onUpdate(firstRepeat + codexAutoKnockPauseMs - 1);
    assert(app.plays(PetGesture::Muyu) == 2);
    app.onUpdate(firstRepeat + codexAutoKnockPauseMs);
    assert(app.gesture() == PetGesture::Muyu && app.plays(PetGesture::Muyu) == 3);
    tap('u');
    assert(!app.autoMuyu());
    app.onUpdate(firstRepeat + 10000);
    assert(app.plays(PetGesture::Muyu) == 3);
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
    for (const auto& r : launcherRows) check(r);
    check(codexPetButton);
    check(codexStatusCard);
    check(codexDeviceButton);
    check(codexTransportButton);
    check(codexHomeButton);
    check(codexAutoMuyuButton);
    for (const auto& action : codexActionButtons) check(action.area);
    check(homeButton);
}
}
int main() {
    lifecycle(); launcherAndApps(); provider(); providerFailure(); codexActivity(); codexDevices(); codexTransportSwitch();
    companionNavigation(); autoMuyuAndWorkout(); geometry();
    puts("PASS: lifecycle, app input, mock data, Codex transport, dice, auto Muyu, workouts, circular layout");
}
