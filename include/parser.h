#pragma once

#include <cstdint>
#include "mac.h"

class Parser {
public:
    enum class FrameKind : uint8_t { Auth, Assoc, Data };

    struct Result {
        bool      ok;      // 파싱 성공 + RSSI threshold + 대상 프레임 통과
        Mac       staMac;  // 수집 대상 Station MAC (DS 비트로 결정)
        int       rssi;    // dBm
        FrameKind kind;    // Auth / Assoc(Re) / QoS-Data
        Mac       apBssid; // 해당 STA 가 붙은 AP 의 BSSID
    };

    struct BeaconInfo {
        bool        ok;
        Mac         bssid;
        std::string ssid;
        int         channel;
    };

    explicit Parser(int rssiThresholdDbm);

    void setRssiThreshold(int dbm);
    int  rssiThreshold() const { return rssiThreshold_; }

    BeaconInfo parseBeacon(const uint8_t* data, int len);
    Result parse(const uint8_t* data, int len) const;

private:
    int rssiThreshold_;
};