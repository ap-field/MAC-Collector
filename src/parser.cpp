#include "parser.h"
#include "radiotaphdr.h"
#include "dot11hdr.h"

#include <glog/logging.h>
#include <cstring>
#include <QDebug>

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

    uint16_t fc     = hdr.frameControl;
    uint8_t  type   = Dot11::frameType(fc);
    uint8_t  subtype = Dot11::frameSubtype(fc);
    bool     toDs   = Dot11::toDS(fc);
    bool     fromDs = Dot11::fromDS(fc);

    Mac frameMac(hdr.addr2);
    QString macStr = QString::fromStdString(frameMac.toString());

    if (type == Dot11::TYPE_MGT) {
        const char* subtypeName =
            (subtype == Dot11::SUBTYPE_AUTH)        ? "auth" :
            (subtype == Dot11::SUBTYPE_ASSOC_REQ)   ? "assoc-req" :
            (subtype == Dot11::SUBTYPE_REASSOC_REQ) ? "reassoc-req" :
            (subtype == Dot11::SUBTYPE_PROBE_REQ)   ? "probe-req" : "other";
        DLOG(INFO) << "[PARSER] MGT " << subtypeName
                   << " from=" << macStr.toStdString()
                   << " ToDS=" << toDs << " FromDS=" << fromDs
                   << " RSSI=" << rssi;

        if (subtype == Dot11::SUBTYPE_AUTH) {
            r.kind = FrameKind::Auth;
        } else if (subtype == Dot11::SUBTYPE_ASSOC_REQ ||
                   subtype == Dot11::SUBTYPE_REASSOC_REQ) {
            r.kind = FrameKind::Assoc;
        } else {
            return r;
        }
    } else if (type == Dot11::TYPE_DATA) {
        if (!toDs || fromDs) return r;
        if (Dot11::isProtected(fc)) return r;

        int llcOff = rtLen + 24;
        if (Dot11::isQoS(fc)) llcOff += 2;
        if (len < llcOff + 8) return r;

        if (data[llcOff]   != 0xAA ||
            data[llcOff+1] != 0xAA ||
            data[llcOff+2] != 0x03) return r;

        uint16_t etype = (static_cast<uint16_t>(data[llcOff+6]) << 8) | data[llcOff+7];
        if (etype != Dot11::ETHERTYPE_EAPOL) return r;

        DLOG(INFO) << "[PARSER] EAPOL from=" << macStr.toStdString()
                   << " RSSI=" << rssi;
        r.kind = FrameKind::Eapol;
    } else {
        return r;
    }

    r.ok    = true;
    r.addr2 = frameMac;
    r.rssi  = rssi;

    const char* kindName =
        (r.kind == FrameKind::Auth)  ? "auth"  :
        (r.kind == FrameKind::Assoc) ? "assoc" : "eapol";
    LOG(INFO) << "Parser::parse accepted mac=" << frameMac.toString()
              << " kind=" << kindName << " rssi=" << rssi;
    return r;
}