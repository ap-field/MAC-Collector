#include "api_client.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDebug>

ApiClient::ApiClient(const QString& baseUrl, QObject* parent)
    : QObject(parent),
    nam_(new QNetworkAccessManager(this)),
    baseUrl_(baseUrl)
{}

// ── 시나리오 1: POST /v1/devices/register ──
void ApiClient::registerDevice(const QString& mac,
                               const QString& name,
                               const QString& phone,
                               int            deviceType,
                               int            rssi,
                               const QString& vendor,
                               const QString& requestedAt)
{
    QJsonObject body;
    body["mac_address"]  = mac;
    body["owner_name"]   = name;
    body["phone_number"] = phone;
    body["device_type"]  = deviceType;   // Integer: 1=노트북, 2=핸드폰, 0=기타
    body["rssi"]         = rssi;
    body["vendor"]       = vendor;
    body["requested_at"] = requestedAt;  // "yyMMddTHHmmss"

    QNetworkRequest req(QUrl(baseUrl_ + "/v1/devices/register"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit registerFailed(mac, reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 성공: code 200, status "success"
        if (resp["code"].toInt() == 200 &&
            resp["status"].toString() == "success")
        {
            qDebug() << "[LOG][CREATE] mac=" << mac;   // Logging: CREATE
            emit registerSuccess(mac);
        } else {
            emit registerFailed(mac, resp["message"].toString());
        }
    });
}

// ── 시나리오 3: POST /v1/devices/update ──
void ApiClient::updateDevice(const QString& mac,
                             const QString& name,
                             const QString& phone,
                             int            deviceType,
                             const QString& requestedAt)
{
    QJsonObject body;
    body["mac_address"]  = mac;
    body["owner_name"]   = name;
    body["phone_number"] = phone;
    body["device_type"]  = deviceType;
    body["requested_at"] = requestedAt;
    // ※ rssi, vendor 는 변경 요청에 포함하지 않음 (프로토콜 명세)

    QNetworkRequest req(QUrl(baseUrl_ + "/v1/devices/update"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit updateFailed(mac, reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 성공: code 201, status "updated"
        if (resp["code"].toInt() == 201 &&
            resp["status"].toString() == "updated")
        {
            QString updAt = resp["data"].toObject()["updated_at"].toString();
            qDebug() << "[LOG][UPDATE] mac=" << mac << "updated_at=" << updAt; // Logging: UPDATE
            emit updateSuccess(mac, updAt);
        } else {
            emit updateFailed(mac, resp["message"].toString());
        }
    });
}

// ── 시나리오 2: GET /v1/devices/lists ──
void ApiClient::fetchDeviceList()
{
    QNetworkRequest req(QUrl(baseUrl_ + "/v1/devices/lists"));

    QNetworkReply* reply = nam_->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit deviceListFailed(reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 응답: { "mac": ["AA:AA:...", "BB:BB:..."] }
        QJsonArray arr = resp["mac"].toArray();
        QStringList macs;
        for (const auto& v : arr)
            macs << v.toString().toUpper();
        emit deviceListFetched(macs);
    });
}