#pragma once
#include "VehicleData.h"
namespace sw {
enum class ApiError : uint8_t { None, NotConfigured, Offline, Transport, Http, Payload, TlsConfig, Clock, Worker, Waiting };
inline const char* errorLabel(ApiError error) {
    switch (error) {
    case ApiError::None: return "OK";
    case ApiError::NotConfigured: return "API NOT CONFIGURED";
    case ApiError::Offline: return "OFFLINE";
    case ApiError::Transport: return "NETWORK ERROR";
    case ApiError::Http: return "HTTP ERROR";
    case ApiError::Payload: return "BAD DATA";
    case ApiError::TlsConfig: return "TLS CONFIG NEEDED";
    case ApiError::Waiting: return "WAITING FOR DATA";
    case ApiError::Clock: return "CLOCK SYNC NEEDED";
    default: return "WORKER ERROR";
    }
}
class ApiCache {
public:
    void accept(VehicleState state) {
        state.revision = state_.revision + 1;
        state_ = state;
        error_ = ApiError::None;
    }
    void fail(ApiError error) { error_ = error; } // Preserve last good sample, including its age.
    const VehicleState& state() const { return state_; }
    ApiError error() const { return error_; }
    const char* status(uint32_t now) const {
        if (error_ == ApiError::NotConfigured) return errorLabel(error_);
        if (error_ == ApiError::Offline) return state_.valid ? "OFFLINE / CACHE" : "OFFLINE";
        if (!state_.valid) return errorLabel(error_);
        if (error_ == ApiError::None && state_.connection == VehicleConnection::Offline) return "CAR OFFLINE";
        if (error_ == ApiError::None && state_.connection == VehicleConnection::Asleep) return "CAR ASLEEP";
        if (stale(state_, now)) return "STALE";
        if (error_ != ApiError::None) return "CACHED / ERROR";
        return state_.source == VehicleSource::Fixture ? "API TEST" : "API LIVE";
    }
private:
    VehicleState state_;
    ApiError error_ = ApiError::NotConfigured;
};
}
