#include "CodexLan.h"
extern "C" {
#include <mbedtls/constant_time.h>
}
#include <mbedtls/md.h>
#include <cstdio>
#include <cstring>

namespace sw {
namespace {
constexpr uint16_t port = 45255;
constexpr size_t headerBytes = 4;
constexpr size_t integerBytes = 8;
constexpr size_t legacyPayload = headerBytes + integerBytes + 1;
constexpr size_t stateOffset = headerBytes + 2 * integerBytes;
constexpr size_t nameOffset = stateOffset + 1;
constexpr size_t namedPayload = nameOffset + codexDeviceNameLength;
constexpr size_t tagBytes = 16;
constexpr uint64_t namedDeviceBit = uint64_t{1} << 63;

int hexDigit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

uint64_t readUint64(const uint8_t* bytes) {
    uint64_t value = 0;
    for (size_t i = 0; i < integerBytes; ++i) value = (value << 8) | bytes[i];
    return value;
}

void receivePacket(const uint8_t* packet, bool named, IPAddress sender, uint32_t now, CodexApp& codex) {
    CodexDeviceName name{};
    if (named) {
        const auto id = readUint64(packet + headerBytes);
        if (!(id & namedDeviceBit) || packet[nameOffset] == 0) return;
        for (size_t i = 0; i < codexDeviceNameLength && packet[nameOffset + i] != 0; ++i) {
            const auto character = packet[nameOffset + i];
            if (character < 32 || character > 126) return;
            name[i] = static_cast<char>(character);
        }
        codex.receiveLan(id, name, static_cast<char>(packet[stateOffset]),
                         readUint64(packet + headerBytes + integerBytes), now);
    } else {
        snprintf(name.data(), name.size(), "%u.%u.%u.%u",
                 unsigned(sender[0]), unsigned(sender[1]), unsigned(sender[2]), unsigned(sender[3]));
        codex.receiveLan(uint32_t(sender), name, static_cast<char>(packet[legacyPayload - 1]),
                         readUint64(packet + headerBytes), now);
    }
}
} // namespace

void CodexLan::begin(const char* hexKey) {
    if (strlen(hexKey) != 2 * sizeof(key_)) return;
    for (size_t i = 0; i < sizeof(key_); ++i) {
        const int high = hexDigit(hexKey[2 * i]);
        const int low = hexDigit(hexKey[2 * i + 1]);
        if (high < 0 || low < 0) return;
        key_[i] = uint8_t((high << 4) | low);
    }
    configured_ = true;
}

void CodexLan::update(bool wifiOnline, uint32_t now, CodexApp& codex) {
    if (!configured_) return;
    if (!wifiOnline) {
        if (listening_) {
            udp_.stop();
            listening_ = false;
            codex.disconnectLan();
        }
        return;
    }

    if (uint32_t(now - lastPoll_) < (listening_ ? 100u : 1000u)) return;
    lastPoll_ = now;
    if (!listening_) {
        listening_ = udp_.begin(port) != 0;
        if (!listening_) return;
    }
    const auto* sha256 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    for (size_t i = 0; i < codexMaxLanDevices; ++i) {
        const int size = udp_.parsePacket();
        if (size <= 0) break;
        const bool named = size == static_cast<int>(namedPayload + tagBytes);
        const size_t payload = named ? namedPayload : legacyPayload;
        if (size != static_cast<int>(payload + tagBytes)) {
            udp_.flush();
            continue;
        }
        uint8_t packet[namedPayload + tagBytes];
        if (udp_.read(packet, size) != size || memcmp(packet, named ? "SWC2" : "SWC1", headerBytes) != 0)
            continue;
        uint8_t tag[32];
        if (mbedtls_md_hmac(sha256, key_, sizeof(key_), packet, payload, tag) != 0 ||
            mbedtls_ct_memcmp(packet + payload, tag, tagBytes) != 0) continue;
        receivePacket(packet, named, udp_.remoteIP(), now, codex);
    }
}
} // namespace sw
