#include "mac.h"

#include <cstdio>
#include <cstring>

Mac::Mac() {
    std::memset(mac_, 0, SIZE);
}

Mac::Mac(const Mac& r) {
    std::memcpy(mac_, r.mac_, SIZE);
}

Mac::Mac(const uint8_t* p) {
    if (p == nullptr) {
        std::memset(mac_, 0, SIZE);
        return;
    }
    std::memcpy(mac_, p, SIZE);
}

Mac::Mac(const char* s) {
    std::memset(mac_, 0, SIZE);
    if (s == nullptr) return;

    // "AA:BB:CC:DD:EE:FF" 또는 "aa:bb:cc:dd:ee:ff"
    unsigned int v[SIZE] = {0};
    int n = std::sscanf(s,
                        "%02x:%02x:%02x:%02x:%02x:%02x",
                        &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
    if (n != SIZE) return;

    for (int i = 0; i < SIZE; ++i) {
        mac_[i] = static_cast<uint8_t>(v[i] & 0xFF);
    }
}

Mac& Mac::operator=(const Mac& r) {
    if (this != &r) {
        std::memcpy(mac_, r.mac_, SIZE);
    }
    return *this;
}

bool Mac::operator==(const Mac& r) const {
    return std::memcmp(mac_, r.mac_, SIZE) == 0;
}

bool Mac::operator!=(const Mac& r) const {
    return !(*this == r);
}

bool Mac::operator<(const Mac& r) const {
    return std::memcmp(mac_, r.mac_, SIZE) < 0;
}

bool Mac::isNull() const {
    for (int i = 0; i < SIZE; ++i) {
        if (mac_[i] != 0) return false;
    }
    return true;
}

std::string Mac::toString() const {
    char buf[18];
    std::snprintf(buf, sizeof(buf),
                  "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
    return std::string(buf);
}