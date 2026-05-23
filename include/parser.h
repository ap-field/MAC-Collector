#pragma once

#include <cstdint>
#include "mac.h"

class Parser {
public:
    enum class FrameKind : uint8_t { Auth, Assoc, Eapol };

    struct Result {
        bool      ok;      // 파싱 성공 + RSSI threshold + 대상 프레임 통과
        Mac       addr2;   // 송신 Station MAC
        int       rssi;    // dBm
        FrameKind kind;    // Auth / Assoc(Re) / Eapol
    };

    explicit Parser(int rssiThresholdDbm);

    void setRssiThreshold(int dbm);
    int  rssiThreshold() const { return rssiThreshold_; }

    Result parse(const uint8_t* data, int len) const;

private:
    int rssiThreshold_;
};