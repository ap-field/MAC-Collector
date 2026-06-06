#include "api_client.h"

#include <glog/logging.h>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

ApiClient::ApiClient(const QString& baseUrl, QObject* parent)
    : QObject(parent),
    nam_(new QNetworkAccessManager(this)),
    baseUrl_(baseUrl)
{}

// ── 시나리오 1: POST /v1/devices/register ──
void ApiClient::registerDevice(const QString& mac,
                               const QString& name,
                               const QString& phoneNum,
                               int            deviceType,
                               int            rssi,
                               const QString& requestedAt)
{
    QJsonObject body;
    body["mac_address"]  = mac;
    body["owner_name"]   = name;
    body["phone_number"] = phoneNum;
    body["device_type"]  = QString::number(deviceType);   // String: "1"=노트북, "2"=핸드폰,... "0"=기타
    body["rssi"]         = rssi;
    body["requested_at"] = requestedAt;

    const QString url = baseUrl_ + "/v1/devices/register";
    LOG(INFO) << "ApiClient::registerDevice POST " << url.toStdString()
              << " mac=" << mac.toStdString() << " name=" << name.toStdString()
              << " type=" << deviceType << " rssi=" << rssi;

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");//before post, after reuqest

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // 연결 자체 실패(서버 다운/네트워크 단절) → networkError=true 로 알림.
            LOG(ERROR) << "ApiClient::registerDevice network error mac="
                       << mac.toStdString() << " err=" << reply->errorString().toStdString();
            emit registerFailed(mac, reply->errorString(), /*networkError=*/true);
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 성공: code 200, status "success"
        if (resp["code"].toInt() == 200 &&
            resp["status"].toString() == "success")
        {
            LOG(INFO) << "ApiClient::registerDevice success mac=" << mac.toStdString();
            emit registerSuccess(mac);
        } else {
            // 서버는 도달했으나 거절(중복 등) → networkError=false.
            LOG(WARNING) << "ApiClient::registerDevice failed mac=" << mac.toStdString()
                         << " code=" << resp["code"].toInt()
                         << " message=" << resp["message"].toString().toStdString();
            emit registerFailed(mac, resp["message"].toString(), /*networkError=*/false);
        }
    });
}

// ── 시나리오 3: POST /v1/devices/update ──
void ApiClient::updateDevice(const QString& mac,
                             const QString& name,
                             const QString& phoneNum,
                             int            deviceType,
                             const QString& requestedAt)
{
    QJsonObject body;
    body["mac_address"]  = mac;
    body["owner_name"]   = name;
    body["phone_number"] = phoneNum;
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
            // 연결 자체 실패(서버 다운/네트워크 단절) → networkError=true 로 알림.
            LOG(ERROR) << "ApiClient::updateDevice network error mac="
                       << mac.toStdString() << " err=" << reply->errorString().toStdString();
            emit updateFailed(mac, reply->errorString(), /*networkError=*/true);
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
            emit updateSuccess(mac, updAt);
        } else {
            // 서버는 도달했으나 거절 → networkError=false.
            LOG(WARNING) << "ApiClient::updateDevice failed mac=" << mac.toStdString()
                         << " code=" << resp["code"].toInt()
                         << " message=" << resp["message"].toString().toStdString();
            emit updateFailed(mac, resp["message"].toString(), /*networkError=*/false);
        }
    });
}

// ── 시나리오 4: DELETE /v1/devices/delete/{mac} ──
void ApiClient::deleteDevice(const QString& mac)
{
    // MAC 은 경로 세그먼트로 전달한다. ':' 등 특수문자를 안전하게 인코딩.
    const QString encodedMac = QString::fromUtf8(
        QUrl::toPercentEncoding(mac));
    const QString url = baseUrl_ + "/v1/devices/delete/" + encodedMac;
    LOG(INFO) << "ApiClient::deleteDevice DELETE " << url.toStdString()
              << " mac=" << mac.toStdString();

    QNetworkRequest req((QUrl(url)));

    QNetworkReply* reply = nam_->deleteResource(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // 연결 자체 실패(서버 다운/네트워크 단절) → networkError=true 로 알림.
            LOG(ERROR) << "ApiClient::deleteDevice network error mac="
                       << mac.toStdString() << " err=" << reply->errorString().toStdString();
            emit deleteFailed(mac, reply->errorString(), /*networkError=*/true);
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 성공: code 200 (서버가 "...삭제되었습니다." 메시지를 함께 반환).
        // register(200/"success")·update(201/"updated") 와 달리 status 필드가 없다.
        if (resp["code"].toInt() == 200)
        {
            LOG(INFO) << "ApiClient::deleteDevice success mac=" << mac.toStdString();
            emit deleteSuccess(mac);
        } else {
            // 서버는 도달했으나 거절(존재하지 않는 MAC 등) → networkError=false.
            LOG(WARNING) << "ApiClient::deleteDevice failed mac=" << mac.toStdString()
                         << " code=" << resp["code"].toInt()
                         << " message=" << resp["message"].toString().toStdString();
            emit deleteFailed(mac, resp["message"].toString(), /*networkError=*/false);
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
        // 실제 응답 형태:
        // { "data": [ { "mac":..., "name":..., "phone_num":..., "device_type":"1",
        //               "request_at":"yyMMddTHHmmss" }, ... ] }
        QJsonArray arr = resp["data"].toArray();
        QVector<DeviceRecord> devices;
        devices.reserve(arr.size());
        for (const auto& v : arr) {
            QJsonObject o = v.toObject();
            DeviceRecord d;
            d.mac          = o["mac"].toString().toUpper();
            d.name         = o["name"].toString();
            d.phoneNum     = o["phone_num"].toString();
            d.deviceType   = o["device_type"].toString().toInt();
            d.registeredAt = o["request_at"].toString();
            devices.push_back(d);
        }
        LOG(INFO) << "ApiClient::fetchDeviceList success count=" << devices.size();
        emit deviceListFetched(devices);
    });
}