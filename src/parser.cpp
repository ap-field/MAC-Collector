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

    Mac staMac, apBssid;

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

        // 관리 프레임의 BSSID = addr3. 송신자(addr2)가 BSSID 와 같으면 AP 가 보낸
        // 프레임이므로 STA 가 아니다 → 수집 제외. (auth 는 양방향이라 AP 응답도 잡힘)
        if (frameMac == Mac(hdr.addr3)) {
            DLOG(INFO) << "[PARSER] skip AP-sourced MGT (addr2==BSSID) mac="
                       << macStr.toStdString();
            return r;
        }
        staMac  = Mac(hdr.addr2);
        apBssid = Mac(hdr.addr3);

    } else if (type == Dot11::TYPE_DATA) {
        // QoS data 만 대상. Null/QoS-Null 은 페이로드가 없어 제외.
        if (!Dot11::isQoS(fc))     return r;
        if (Dot11::isNullData(fc)) return r;

        // ToDS/FromDS 조합으로 주소 의미가 바뀐다. BSSID 가 아닌 개별 주소가 STA.
        if (toDs && !fromDs) {            // 업링크 STA→AP
            staMac  = Mac(hdr.addr2);     // SA
            apBssid = Mac(hdr.addr1);     // BSSID
        } else if (!toDs && fromDs) {     // 다운링크 AP→STA
            staMac  = Mac(hdr.addr1);     // DA(STA)
            apBssid = Mac(hdr.addr2);     // BSSID
        } else {
            return r;                     // 00(IBSS) / 11(WDS) → 수집 대상 아님
        }
        r.kind = FrameKind::Data;
        DLOG(INFO) << "[PARSER] QoS-DATA sta=" << staMac.toString()
                   << " ap=" << apBssid.toString()
                   << " ToDS=" << toDs << " FromDS=" << fromDs
                   << " RSSI=" << rssi;
    } else {
        return r;
    }

    // 멀티캐스트/브로드캐스트/널 주소는 실제 단말이 아니므로 제외 (특히 다운링크 addr1).
    if (staMac.isGroup() || staMac.isNull()) {
        DLOG(INFO) << "[PARSER] skip non-station STA mac=" << staMac.toString();
        return r;
    }

    r.ok      = true;
    r.staMac  = staMac;
    r.rssi    = rssi;
    r.apBssid = apBssid;

    const char* kindName =
        (r.kind == FrameKind::Auth)  ? "auth"  :
        (r.kind == FrameKind::Assoc) ? "assoc" : "data";
    LOG(INFO) << "Parser::parse accepted mac=" << staMac.toString()
              << " kind=" << kindName << " rssi=" << rssi;
    return r;
}

Parser::BeaconInfo Parser::parseBeacon(const uint8_t* data, int len){
    BeaconInfo info{false, Mac{}, " ", 0};
    int rssi = 0;
    uint16_t rtlen = 0;
    if (!extractRadiotap(data, len, &rssi, &rtlen))
        return info;
    if (len < rtlen +24)
        return info;

    Dot11Header hdr;
    std::memcpy(&hdr, data + rtlen, sizeof(hdr));

    uint16_t fc = hdr.frameControl;
    if (Dot11::frameType(fc) != Dot11::TYPE_MGT)
        return info;
    if(Dot11::frameSubtype(fc) != Dot11::SUBTYPE_BEACON)
        return info;

    info.bssid = Mac(hdr.addr2);

    int ieOff = rtlen + 24 + 12;

    while (ieOff +2 <= len) {
        uint8_t id = data[ieOff];
        uint8_t elen = data[ieOff + 1];
        if (ieOff + 2 + elen > len) break;

        if(id == 0 && elen > 0){
            info.ssid = std::string(data + ieOff + 2, data + ieOff +2 + elen);
        } else if (id == 3 && elen == 1) {
            info.channel = data[ieOff + 2];
        }
        ieOff += 2 + elen;
    }
    info.ok =true;
    return info;

}