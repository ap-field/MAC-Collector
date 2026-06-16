#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct Dot11Header {
    uint16_t frameControl;
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
    uint16_t seqCtrl;
};
#pragma pack(pop)

typedef Dot11Header* PDot11Header;

namespace Dot11 {
constexpr uint8_t TYPE_MGT  = 0;
constexpr uint8_t TYPE_CTRL = 1;
constexpr uint8_t TYPE_DATA = 2;

constexpr uint8_t SUBTYPE_PROBE_REQ   = 0x04;
constexpr uint8_t SUBTYPE_ASSOC_REQ   = 0x00;
constexpr uint8_t SUBTYPE_REASSOC_REQ = 0x02;
constexpr uint8_t SUBTYPE_BEACON      = 0x08;
constexpr uint8_t SUBTYPE_AUTH        = 0x0B;

// LLC/SNAP EtherType for EAPOL (802.1X)
constexpr uint16_t ETHERTYPE_EAPOL = 0x888E;

inline uint8_t protocolVersion(uint16_t fc) { return static_cast<uint8_t>(fc & 0x03); }
inline uint8_t frameType(uint16_t fc)       { return static_cast<uint8_t>((fc >> 2) & 0x03); }
inline uint8_t frameSubtype(uint16_t fc)    { return static_cast<uint8_t>((fc >> 4) & 0x0F); }

// Frame Control DS/flag helpers
inline bool toDS(uint16_t fc)        { return (fc >> 8) & 0x01; }
inline bool fromDS(uint16_t fc)      { return (fc >> 9) & 0x01; }
inline bool isProtected(uint16_t fc) { return (fc >> 14) & 0x01; }
inline bool isQoS(uint16_t fc)       { return (frameSubtype(fc) & 0x08) != 0; }
// Bit 2 of data subtype marks Null/CF frames that carry no payload
inline bool isNullData(uint16_t fc)  { return frameType(fc) == TYPE_DATA && (frameSubtype(fc) & 0x04); }

inline bool isProbeReq(uint16_t fc) {
    return frameType(fc) == TYPE_MGT && frameSubtype(fc) == SUBTYPE_PROBE_REQ;
}
inline bool isAuth(uint16_t fc) {
    return frameType(fc) == TYPE_MGT && frameSubtype(fc) == SUBTYPE_AUTH;
}
inline bool isAssocReq(uint16_t fc) {
    return frameType(fc) == TYPE_MGT && frameSubtype(fc) == SUBTYPE_ASSOC_REQ;
}
inline bool isReassocReq(uint16_t fc) {
    return frameType(fc) == TYPE_MGT && frameSubtype(fc) == SUBTYPE_REASSOC_REQ;
}
}