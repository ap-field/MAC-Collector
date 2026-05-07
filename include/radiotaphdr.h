#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct RadiotapHeader {
    uint8_t  it_version;
    uint8_t  it_pad;
    uint16_t it_len;
    uint32_t it_present;
};
#pragma pack(pop)

typedef RadiotapHeader* PRadiotapHeader;

// it_present bit 마스크 (radiotap.org)
namespace RadiotapPresent {
constexpr uint32_t TSFT              = 1u << 0;
constexpr uint32_t FLAGS             = 1u << 1;
constexpr uint32_t RATE              = 1u << 2;
constexpr uint32_t CHANNEL           = 1u << 3;
constexpr uint32_t FHSS              = 1u << 4;
constexpr uint32_t DBM_ANTSIGNAL     = 1u << 5;   // RSSI
constexpr uint32_t DBM_ANTNOISE      = 1u << 6;
constexpr uint32_t LOCK_QUALITY      = 1u << 7;
constexpr uint32_t TX_ATTENUATION    = 1u << 8;
constexpr uint32_t DB_TX_ATTENUATION = 1u << 9;
constexpr uint32_t DBM_TX_POWER      = 1u << 10;
constexpr uint32_t ANTENNA           = 1u << 11;
constexpr uint32_t DB_ANTSIGNAL      = 1u << 12;
constexpr uint32_t DB_ANTNOISE       = 1u << 13;
constexpr uint32_t RX_FLAGS          = 1u << 14;
constexpr uint32_t EXT               = 1u << 31;
}