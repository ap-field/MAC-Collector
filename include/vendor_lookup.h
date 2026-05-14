#pragma once

#include <QString>
#include <QHash>

class VendorLookup {
public:
    static VendorLookup& instance();

    // "AA:BB:CC:DD:EE:FF" 형식 MAC → 앞 3바이트로 제조사 조회
    // 미등록 시 "Unknown" 반환
    QString lookup(const QString& mac) const;

private:
    VendorLookup();
    QHash<QString, QString> table_;  // "AA:BB:CC" → "제조사명"
};