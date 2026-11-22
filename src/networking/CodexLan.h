#pragma once
#include <WiFiUdp.h>
#include "../apps/CodexApp.h"

namespace sw {
class CodexLan {
public:
    void begin(const char* hexKey);
    void update(bool wifiOnline, uint32_t now, CodexApp&);

private:
    WiFiUDP udp_;
    uint8_t key_[32]{};
    uint32_t lastPoll_ = 0;
    bool configured_ = false;
    bool listening_ = false;
};
} // namespace sw
