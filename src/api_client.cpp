#include "api_client.h"

#include <glog/logging.h>
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
                               const QString& requestedAt)
{
    QJsonObject body;
    body["mac_address"]  = mac;
    body["owner_name"]   = name;
    body["phone_number"] = phone;
    body["device_type"]  = QString::number(deviceType);   // String: "1"=노트북, "2"=핸드폰,... "0"=기타
    body["rssi"]         = rssi;
    body["requested_at"] = requestedAt;

    const QString url = baseUrl_ + "/v1/devices/register";
    LOG(INFO) << "ApiClient::registerDevice POST " << url.toStdString()
              << " mac=" << mac.toStdString() << " name=" << name.toStdString()
              << " type=" << deviceType << " rssi=" << rssi;

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG(ERROR) << "ApiClient::registerDevice network error mac="
                       << mac.toStdString() << " err=" << reply->errorString().toStdString();
            emit registerFailed(mac, reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 성공: code 200, status "success"
        if (resp["code"].toInt() == 200 &&
            resp["status"].toString() == "success")
        {
            LOG(INFO) << "ApiClient::registerDevice success mac=" << mac.toStdString();
            qDebug() << "[LOG][CREATE] mac=" << mac;   // Logging: CREATE
            emit registerSuccess(mac);
        } else {
            LOG(WARNING) << "ApiClient::registerDevice failed mac=" << mac.toStdString()
                         << " code=" << resp["code"].toInt()
                         << " message=" << resp["message"].toString().toStdString();
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
    body["device_type"]  = QString::number(deviceType);
    body["requested_at"] = requestedAt;
    // ※ rssi, vendor 는 변경 요청에 포함하지 않음 (프로토콜 명세)

    const QString url = baseUrl_ + "/v1/devices/update";
    LOG(INFO) << "ApiClient::updateDevice POST " << url.toStdString()
              << " mac=" << mac.toStdString() << " name=" << name.toStdString()
              << " type=" << deviceType;

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG(ERROR) << "ApiClient::updateDevice network error mac="
                       << mac.toStdString() << " err=" << reply->errorString().toStdString();
            emit updateFailed(mac, reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 성공: code 201, status "updated"
        if (resp["code"].toInt() == 201 &&
            resp["status"].toString() == "updated")
        {
            QString updAt = resp["data"].toObject()["updated_at"].toString();
            LOG(INFO) << "ApiClient::updateDevice success mac=" << mac.toStdString()
                      << " updated_at=" << updAt.toStdString();
            qDebug() << "[LOG][UPDATE] mac=" << mac << "updated_at=" << updAt; // Logging: UPDATE
            emit updateSuccess(mac, updAt);
        } else {
            LOG(WARNING) << "ApiClient::updateDevice failed mac=" << mac.toStdString()
                         << " code=" << resp["code"].toInt()
                         << " message=" << resp["message"].toString().toStdString();
            emit updateFailed(mac, resp["message"].toString());
        }
    });
}

// ── 시나리오 2: GET /v1/devices/lists ──
void ApiClient::fetchDeviceList()
{
    const QString url = baseUrl_ + "/v1/devices/lists";
    LOG(INFO) << "ApiClient::fetchDeviceList GET " << url.toStdString();

    QNetworkRequest req((QUrl(url)));

    QNetworkReply* reply = nam_->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG(ERROR) << "ApiClient::fetchDeviceList network error err="
                       << reply->errorString().toStdString();
            emit deviceListFailed(reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 응답: { "mac": ["AA:AA:...", "BB:BB:..."] }
        QJsonArray arr = resp["mac"].toArray();
        QStringList macs;
        for (const auto& v : arr)
            macs << v.toString().toUpper();
        LOG(INFO) << "ApiClient::fetchDeviceList success count=" << macs.size();
        emit deviceListFetched(macs);
    });
}