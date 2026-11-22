#pragma once
#include <cstdint>
namespace sw {
class Network {
public:
    void begin();
    void update(uint32_t now);
    void reconnect();
    void requestConnect(uint32_t now);
    bool online() const { return online_; }
    const char* label() const;
    const char* address() const { return address_; }
private:
    bool configured_ = false, online_ = false, attempting_ = false;
    uint32_t lastAttempt_ = 0;
    bool scanning_ = false, scanRequested_ = false, scanned_ = false;
    char address_[20] = "No connection";
};
}
