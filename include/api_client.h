#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>

class QNetworkAccessManager;

// 서버 /v1/devices/lists 응답의 단일 디바이스 정보
struct DeviceRecord {
    QString mac;            // 대문자 "AA:BB:CC:DD:EE:FF"
    QString name;
    QString phoneNum;
    int     deviceType = 0;       // 0=기타 1=노트북 2=핸드폰 3=태블릿 4=IoT
    QString registeredAt;   // "yyMMddTHHmmss"
    QString updatedAt;      // 없으면 빈 문자열
};

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(const QString& baseUrl, QObject* parent = nullptr);

    // 시나리오 1 — 신규 등록
    void registerDevice(const QString& mac,
                        const QString& name,
                        const QString& phoneNum,
                        int            deviceType,   // 1=노트북 2=핸드폰 0=기타
                        int            rssi,
                        const QString& requestedAt); // "yyMMddTHHmmss"

    // 시나리오 3 — 정보 변경
    void updateDevice(const QString& mac,
                      const QString& name,
                      const QString& phoneNum,
                      int            deviceType,
                      const QString& requestedAt);

    // 시나리오 2 — 전체 MAC 목록 조회 (중복 확인용)
    void fetchDeviceList();

signals:
    void registerSuccess(QString mac);
    void registerFailed(QString mac, QString reason);

    void updateSuccess(QString mac, QString updatedAt);
    void updateFailed(QString mac, QString reason);

    // 조회 성공: 등록된 디바이스 전체 정보 반환
    void deviceListFetched(QVector<DeviceRecord> devices);
    void deviceListFailed(QString reason);

private:
    QNetworkAccessManager* nam_;
    QString                baseUrl_;
};