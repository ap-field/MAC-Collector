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

    // 랜덤 MAC = locally-administered 비트(첫 옥텟 bit1) → 첫 옥텟 하위니블 2/6/A/E.
    bool isRandom() const;

    // group(멀티캐스트/브로드캐스트) 주소 = 첫 옥텟 I/G 비트(bit0). 실제 단말이 아님.
    bool isGroup() const;

    const uint8_t* data() const { return mac_; }

    std::string toString() const;
    operator std::string() const { return toString(); }

private:
    uint8_t mac_[SIZE];
};