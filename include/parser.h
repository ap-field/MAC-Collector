#pragma once

#include <cstdint>
#include "mac.h"

class Parser {
public:
    struct Result {
        bool     ok;       // 파싱 성공 + RSSI threshold + Probe Req 통과
        Mac      addr2;    // 송신 Station MAC
        int      rssi;     // dBm
        uint8_t  subtype;  // 0x04=Probe Req
    };

    explicit Parser(int rssiThresholdDbm);

    void setRssiThreshold(int dbm);
    int  rssiThreshold() const { return rssiThreshold_; }

    Result parse(const uint8_t* data, int len) const;

private:
    int rssiThreshold_;
};