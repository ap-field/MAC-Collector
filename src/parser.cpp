#include "parser.h"
#include "radiotaphdr.h"
#include "dot11hdr.h"

#include <cstring>

namespace {

bool extractRadiotap(const uint8_t* data, int len,
                     int* outRssi, uint16_t* outRtLen)
{
    if (len < 8) return false;

    RadiotapHeader rt;
    std::memcpy(&rt, data, sizeof(rt));
    if (rt.it_version != 0) return false;

    uint16_t rtLen;
    std::memcpy(&rtLen, &rt.it_len, sizeof(rtLen));
    if (rtLen < 8 || rtLen > len) return false;
    *outRtLen = rtLen;

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
    Result r{ false, Mac{}, 0, FrameKind::Auth };

    if (!data || len < 16) return r;

    int      rssi  = 0;
    uint16_t rtLen = 0;
    if (!extractRadiotap(data, len, &rssi, &rtLen)) return r;
    if (rssi < rssiThreshold_)                      return r;
    if (len < rtLen + 16)                           return r;

    Dot11Header hdr;
    std::memcpy(&hdr, data + rtLen, sizeof(hdr));

    uint16_t fc      = hdr.frameControl;
    uint8_t  type    = Dot11::frameType(fc);
    uint8_t  subtype = Dot11::frameSubtype(fc);

    // STA→AP 방향(ToDS=1, FromDS=0)만 처리 — 클라이언트 MAC을 addr2로 확보
    if (!Dot11::toDS(fc) || Dot11::fromDS(fc)) return r;

    if (type == Dot11::TYPE_MGT) {
        if (subtype == Dot11::SUBTYPE_AUTH) {
            r.kind = Parser::FrameKind::Auth;
        } else if (subtype == Dot11::SUBTYPE_ASSOC_REQ ||
                   subtype == Dot11::SUBTYPE_REASSOC_REQ) {
            r.kind = Parser::FrameKind::Assoc;
        } else {
            return r;
        }
    } else if (type == Dot11::TYPE_DATA) {
        // 암호화된 프레임은 LLC 헤더를 읽을 수 없음
        if (Dot11::isProtected(fc))  return r;
        // 페이로드 없는 Null/CF 프레임 제외
        if (Dot11::isNullData(fc))   return r;

        // LLC/SNAP 오프셋 계산 (802.11 기본 헤더 24B + QoS 2B)
        int llcOff = rtLen + 24;
        if (Dot11::isQoS(fc)) llcOff += 2;
        if (len < llcOff + 8) return r;

        // LLC/SNAP: AA AA 03 <OUI 3B> <EtherType 2B>
        if (data[llcOff]   != 0xAA ||
            data[llcOff+1] != 0xAA ||
            data[llcOff+2] != 0x03) return r;

        uint16_t etype = (static_cast<uint16_t>(data[llcOff+6]) << 8) | data[llcOff+7];
        if (etype != Dot11::ETHERTYPE_EAPOL) return r;

        r.kind = Parser::FrameKind::Eapol;
    } else {
        return r;
    }

    r.ok    = true;
    r.addr2 = Mac(hdr.addr2);
    r.rssi  = rssi;
    return r;
}