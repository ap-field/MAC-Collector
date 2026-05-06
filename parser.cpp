#include "parser.h"
#include "Radiotaphdr.h"
#include "Dot11hdr.h"

#include <cstring>

namespace {

// Radiotap 에서 Antenna Signal(RSSI dBm) 추출 + 헤더 길이.
bool extractRadiotap(const uint8_t* data, int len,
                     int* outRssi, uint16_t* outRtLen)
{
    if (len < 8) return false;

    RadiotapHdr rt;
    std::memcpy(&rt, data, sizeof(rt));
    if (rt.it_version != 0) return false;

    uint16_t rtLen;
    std::memcpy(&rtLen, &rt.it_len, sizeof(rtLen));
    if (rtLen < 8 || rtLen > len) return false;
    *outRtLen = rtLen;

    // present 확장 워드 스킵
    uint32_t present0 = rt.it_present;
    size_t   fieldOff = 8;
    {
        uint32_t cur = present0;
        while (cur & (1u << 31)) {
            if (fieldOff + 4 > rtLen) return false;
            uint32_t next;
            std::memcpy(&next, data + fieldOff, 4);
            fieldOff += 4;
            cur = next;
        }
    }

    auto align = [&](size_t a) {
        size_t mod = fieldOff & (a - 1);
        if (mod) fieldOff += (a - mod);
    };

    struct F { uint8_t bit, align, size; };
    static constexpr F defs[] = {
        {0, 8, 8},   // TSFT
        {1, 1, 1},   // Flags
        {2, 1, 1},   // Rate
        {3, 2, 4},   // Channel
        {4, 2, 2},   // FHSS
        {5, 1, 1},   // Antenna signal (RSSI)
    };

    for (const auto& f : defs) {
        if (!(present0 & (1u << f.bit))) continue;
        align(f.align);
        if (f.bit == 5) {
            if (fieldOff >= rtLen) return false;
            int8_t sig;
            std::memcpy(&sig, data + fieldOff, 1);
            *outRssi = sig;
            return true;
        }
        fieldOff += f.size;
        if (fieldOff > rtLen) return false;
    }
    return false;
}

} // anonymous


Parser::Parser(int rssiThresholdDbm)
    : rssiThreshold_(rssiThresholdDbm) {}

void Parser::setRssiThreshold(int dbm) {
    rssiThreshold_ = dbm;
}

Parser::Result Parser::parse(const uint8_t* data, int len) const
{
    Result r{ false, Mac{}, 0, 0xFF };

    if (!data || len < 16) return r;

    int      rssi  = 0;
    uint16_t rtLen = 0;
    if (!extractRadiotap(data, len, &rssi, &rtLen)) return r;
    if (rssi < rssiThreshold_)                      return r;
    if (len < rtLen + 16)                           return r;

    Dot11Hdr hdr;
    std::memcpy(&hdr, data + rtLen, sizeof(hdr));

    uint16_t fc      = hdr.frame_control;
    uint8_t  type    = (fc >> 2) & 0x03;   // 0=mgmt
    uint8_t  subtype = (fc >> 4) & 0x0F;

    if (type != 0) return r;
    // 0x0=Assoc Req, 0x2=Reassoc Req, 0xB=Auth
    if (subtype != 0x0 && subtype != 0x2 && subtype != 0xB) return r;

    r.ok      = true;
    r.addr2   = Mac(hdr.addr2);
    r.rssi    = rssi;
    r.subtype = subtype;
    return r;
}