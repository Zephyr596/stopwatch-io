#include "services/VehicleJson.h"
#include "services/ApiCache.h"
#include "apps/Apps.h"
#include <cassert>
#include <string>
#include <cstdio>
using namespace sw;
const char* good = R"({"schema_version":1,"source":"teslamate","battery_percent":78,"range_km":351.5,"speed_kmh":42,"charging":false,"observed_at":1793000000,"data_age_ms":1000,"vehicle_state":"online","range_kind":"rated","speed_valid":true})";
int main() {
    VehicleState state;
    assert(decodeVehicle(good, strlen(good), 100, state));
    assert(state.source == VehicleSource::Api && state.valid && state.speedKmh == 42);
    assert(ageMs(state, 200) == 1100);
    assert(!stale(state, 14099) && stale(state, 14100));
    for (const char* bad : {"", "null", "[]", "{}", "{", "{\"a\":{\"b\":{\"c\":1}}}"}) {
        assert(!decodeVehicle(bad, strlen(bad), 100, state));
        assert(state.speedKmh == 42);
    }
    const auto invalid = [&](const char* field, const char* value) {
        StaticJsonDocument<1024> doc;
        deserializeJson(doc, good);
        doc[field] = serialized(value);
        std::string text;
        serializeJson(doc, text);
        assert(!decodeVehicle(text.c_str(), text.size(), 200, state));
        assert(state.updatedAtMs == 100);
    };
    invalid("schema_version", "2"); invalid("schema_version", "\"1\"");
    invalid("battery_percent", "null"); invalid("battery_percent", "101");
    invalid("battery_percent", "\"78\""); invalid("battery_percent", "true");
    invalid("range_km", "-1"); invalid("speed_kmh", "351");
    invalid("charging", "true"); invalid("charging", "0");
    invalid("observed_at", "0"); invalid("data_age_ms", "-1");
    invalid("data_age_ms", "4294967296"); invalid("vehicle_state", "\"unknown\""); invalid("speed_valid", "1"); invalid("source", "\"unknown\"");
    const std::string oversized(1537, ' ');
    assert(!decodeVehicle(oversized.c_str(), oversized.size(), 0, state));
    ApiCache cache;
    assert(!cache.state().valid);
    cache.accept(state);
    assert(cache.state().revision == 1 && !strcmp(cache.status(200), "API LIVE"));
    cache.fail(ApiError::Transport);
    assert(cache.state().valid && cache.state().updatedAtMs == 100);
    assert(!strcmp(cache.status(200), "CACHED / ERROR"));
    assert(!strcmp(cache.status(15000), "STALE"));
    cache.fail(ApiError::Offline);
    assert(!strcmp(cache.status(200), "OFFLINE / CACHE"));
    state.connection = VehicleConnection::Offline;
    cache.accept(state);
    assert(!strcmp(cache.status(200), "CAR OFFLINE"));
    state.connection = VehicleConnection::Asleep;
    cache.accept(state);
    assert(!strcmp(cache.status(200), "CAR ASLEEP"));
    state.connection = VehicleConnection::Online;
    state.source = VehicleSource::Fixture;
    cache.accept(state);
    assert(!strcmp(cache.status(200), "API TEST"));
    state.updatedAtMs = UINT32_MAX - 499;
    state.dataAgeMs = 1000;
    assert(ageMs(state, 500) == 2000);
    state.dataAgeMs = UINT32_MAX;
    assert(ageMs(state, 500) == UINT32_MAX);
    int toggles = 0;
    SettingsApp settings([](void* p) { ++*static_cast<int*>(p); }, &toggles);
    settings.onEnter();
    settings.onInput({InputType::Next}); settings.onInput({InputType::Next});
    settings.onInput({InputType::Confirm});
    assert(toggles == 1 && !settings.haptics());
    settings.onInput({InputType::Tap, 233, 220});
    assert(toggles == 2);
    settings.onInput({InputType::Next});
    assert(settings.selected() == 0);
    puts("PASS: JSON schema/types/bounds, cache/error/freshness, fixture identity, age rollover, source selection");
}
