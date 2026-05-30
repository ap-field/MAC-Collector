#pragma once

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>

class QNetworkAccessManager;

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(const QString& baseUrl, QObject* parent = nullptr);

    // 시나리오 1 — 신규 등록
    void registerDevice(const QString& mac,
                        const QString& name,
                        const QString& phone,
                        int            deviceType,   // 1=노트북 2=핸드폰 0=기타
                        int            rssi,
                        const QString& requestedAt); // "yyMMddTHHmmss"

    // 시나리오 3 — 정보 변경
    void updateDevice(const QString& mac,
                      const QString& name,
                      const QString& phone,
                      int            deviceType,
                      const QString& requestedAt);

    // 시나리오 2 — 전체 MAC 목록 조회 (중복 확인용)
    void fetchDeviceList();

signals:
    void registerSuccess(QString mac);
    void registerFailed(QString mac, QString reason);

    void updateSuccess(QString mac, QString updatedAt);
    void updateFailed(QString mac, QString reason);

    // 조회 성공: 등록된 MAC 목록 반환
    void deviceListFetched(QStringList macs);
    void deviceListFailed(QString reason);

private:
    QNetworkAccessManager* nam_;
    QString                baseUrl_;
};