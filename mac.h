#pragma once

#include <cstdint>
#include <string>

class Mac {
public:
    static constexpr int SIZE = 6;

    Mac();
    Mac(const Mac& r);
    explicit Mac(const uint8_t* p);   // 6바이트 raw 버퍼로부터 생성
    Mac(const char* s);               // "AA:BB:CC:DD:EE:FF"

    Mac& operator=(const Mac& r);

    bool operator==(const Mac& r) const;
    bool operator!=(const Mac& r) const;
    bool operator<(const Mac& r) const;   // std::set / std::map 키 사용 가능

    bool isNull() const;

    const uint8_t* data() const { return mac_; }

    std::string toString() const;
    operator std::string() const { return toString(); }

private:
    uint8_t mac_[SIZE];
};