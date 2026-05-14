#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

class QNetworkAccessManager;
class QNetworkReply;

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(const QString& baseUrl, QObject* parent = nullptr);

    // 시나리오 1 — 신규 등록
    void registerAsset(const QString& mac,
                       const QString& name,
                       const QString& phone,
                       int            rssi,
                       const QString& vendor,
                       const QString& requestedAt);

    // 시나리오 3 — 정보 변경
    void updateAsset(const QString& mac,
                     const QString& name,
                     const QString& phone,
                     int            rssi,
                     const QString& vendor,
                     const QString& requestedAt);

    // 시나리오 2 — 중복 확인 (Phase2 확인 클릭 시점)
    void checkAsset(const QString& mac);

signals:
    void registerSuccess(QString mac, QString registeredAt);
    void registerFailed(QString mac, QString reason);

    void updateSuccess(QString mac, QString updatedAt);
    void updateFailed(QString mac, QString reason);

    void checkResult(QString mac, bool exists, QJsonObject data);

private:
    QNetworkAccessManager* nam_;
    QString                baseUrl_;
};