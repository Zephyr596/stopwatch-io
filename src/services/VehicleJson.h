#pragma once
#include "VehicleData.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstring>
namespace sw {
// Fixed schema, bounded body and nesting. Never mutate the caller's cache on failure.
inline bool decodeVehicle(const char* json, size_t length, uint32_t receivedAt, VehicleState& result) {
    if (!length || length > 1536) return false;
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, json, length, DeserializationOption::NestingLimit(2))) return false;
    if (!doc.is<JsonObject>() || !doc["schema_version"].is<int>() || doc["schema_version"].as<int>() != 1 ||
        !doc["battery_percent"].is<float>() || !doc["range_km"].is<float>() || !doc["speed_kmh"].is<float>() ||
        !doc["charging"].is<bool>() || !doc["data_age_ms"].is<uint32_t>() || !doc["observed_at"].is<uint64_t>() ||
        !doc["source"].is<const char*>()) return false;
    VehicleState state;
    state.batteryPercent = doc["battery_percent"];
    state.rangeKm = doc["range_km"];
    state.speedKmh = doc["speed_kmh"];
    state.charging = doc["charging"];
    state.dataAgeMs = doc["data_age_ms"];
    state.observedAt = doc["observed_at"];
    const char* source = doc["source"];
    if (strcmp(source, "teslamate") && strcmp(source, "fixture")) return false;
    if (!std::isfinite(state.batteryPercent) || state.batteryPercent < 0 || state.batteryPercent > 100 ||
        !std::isfinite(state.rangeKm) || state.rangeKm < 0 || state.rangeKm > 1500 ||
        !std::isfinite(state.speedKmh) || state.speedKmh < 0 || state.speedKmh > 350 ||
        (state.charging && state.speedKmh != 0) || state.observedAt < 1700000000ULL ||
        state.observedAt > 4102444800ULL) return false;
    state.source = !strcmp(source, "fixture") ? VehicleSource::Fixture : VehicleSource::Api;
    if (state.source == VehicleSource::Api) {
        if (!doc["vehicle_state"].is<const char*>() || !doc["range_kind"].is<const char*>() || !doc["speed_valid"].is<bool>()) return false;
        const char* connection = doc["vehicle_state"];
        const char* range = doc["range_kind"];
        if (!strcmp(connection, "online")) state.connection = VehicleConnection::Online;
        else if (!strcmp(connection, "offline")) state.connection = VehicleConnection::Offline;
        else if (!strcmp(connection, "asleep")) state.connection = VehicleConnection::Asleep;
        else return false;
        if (!strcmp(range, "estimated")) state.rangeKind = RangeKind::Estimated;
        else if (!strcmp(range, "rated")) state.rangeKind = RangeKind::Rated;
        else if (!strcmp(range, "ideal")) state.rangeKind = RangeKind::Ideal;
        else return false;
        state.speedValid = doc["speed_valid"];
    }
    state.updatedAtMs = receivedAt;
    state.valid = true;
    result = state;
    return true;
}
}
