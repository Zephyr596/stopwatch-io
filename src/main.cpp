#include "hardware/Device.h"
#include <Config.h>
#include "ui/Renderer.h"
#include "networking/Network.h"
#include "services/ApiVehicleData.h"
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace {
sw::Device device;
sw::Launcher launcher;
sw::MockVehicleDataProvider mock;
sw::Network network;
sw::ApiVehicleDataProvider api;
sw::VehicleDataService data(mock, api, SW_DEFAULT_API);
sw::TeslaApp tesla(data, [](void* p) { static_cast<sw::VehicleDataService*>(p)->action(); }, &data);
sw::MotorcycleApp motorcycle;
sw::SettingsApp settings([](void* p) { static_cast<sw::VehicleDataService*>(p)->toggleSource(); }, &data);
sw::AppManager manager({{{sw::AppId::Home, "Launcher", &launcher},
                        {sw::AppId::Tesla, "Tesla", &tesla},
                        {sw::AppId::Motorcycle, "Motorcycle", &motorcycle},
                        {sw::AppId::Settings, "Settings", &settings}}},
                       [](const char* name) { Serial.printf("[app] enter %s\n", name); });
sw::Renderer renderer(M5.Display);
bool started = false;
uint32_t lastRender = 0;
}
void setup() {
    if (!device.begin()) return;
    if (!renderer.begin()) {
        Serial.println("[error] UI buffer allocation failed");
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setCursor(100, 220);
        M5.Display.print("UI memory error");
        return;
    }
    network.begin();
    api.begin();
    started = manager.switchTo(sw::AppId::Home);
    Serial.println("[boot] ready; A next / B confirm / hold A home; Wi-Fi asynchronous; source from local config");
}
void loop() {
    const uint32_t now = millis();
    network.update(now);
    data.update(now);
    sw::InputEvent event{};
    bool hasInput = device.poll(now, event);
    // Bounded USB diagnostics use the same input path as physical controls.
    // Real input wins. No serial connection is required for normal operation.
    if (!hasInput && Serial.available()) {
        const char command = char(Serial.read());
        if (command == 'a' || command == 'b' || command == 'h') {
            event = {command == 'a' ? sw::InputType::Next : command == 'b' ? sw::InputType::Confirm : sw::InputType::Home};
            hasInput = true;
        } else if (command == 'w') {
            network.reconnect();
        } else if (command == 'm' || command == 'r') {
            if (data.isMock() != (command == 'm')) data.toggleSource();
        } else if (command == 's') {
            sw::VehicleState state;
            data.getState(state);
            Serial.printf("[state] app=%u launch=%u valid=%d charging=%d speed=%.0f revision=%u heap=%u min=%u paints=%u wifi=%d mock=%d age=%u api_error=%u worker_stack=%u live=%d source=%u clock=%lu\n",
                          unsigned(manager.current()), unsigned(launcher.selected()), state.valid, state.charging, state.speedKmh,
                          state.revision, ESP.getFreeHeap(), ESP.getMinFreeHeap(), renderer.paintCount(), network.online(), data.isMock(),
                          sw::ageMs(state, now), unsigned(api.error()), api.stackFree(), state.valid && !sw::stale(state, now) && data.healthy() && state.speedValid && state.connection == sw::VehicleConnection::Online, unsigned(state.source), static_cast<unsigned long>(time(nullptr)));
        }
    }
    if (started) {
        if (hasInput) {
            Serial.printf("[input] type=%u x=%d y=%d\n", unsigned(event.type), event.x, event.y);
            manager.input(event);
            device.feedback(now, settings.haptics());
        }
        manager.update(now);
        if (uint32_t(now - lastRender) >= 33) {
            lastRender = now;
            renderer.render(manager.current(), launcher, tesla, settings, device.status(), network, now);
        }
    }
    vTaskDelay(1); // Yield one scheduler tick, not a UI timing delay.
}
