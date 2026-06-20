#include "api_client.h"

#include <glog/logging.h>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

// HTTP 오류 응답(4xx/5xx)과 연결 자체 실패를 구분한다.
// httpStatus > 0 이면 서버가 응답한 것이므로 networkError=false, JSON 바디에서 메시지를 읽는다.
// httpStatus == 0 이면 연결 실패이므로 networkError=true, Qt errorString을 그대로 반환한다.
static std::pair<QString, bool> parseHttpError(QNetworkReply* reply) {
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus > 0) {
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        QString msg = resp["message"].toString();
        if (msg.isEmpty()) msg = reply->errorString();
        return {msg, false};
    }
    return {reply->errorString(), true};
}

ApiClient::ApiClient(const QString& baseUrl, const QString& apiKey, QObject* parent)
    : QObject(parent),
    nam_(new QNetworkAccessManager(this)),
    baseUrl_(baseUrl),
    apiKey_(apiKey)
{}

// ── 시나리오 1: POST /v1/devices/register ──
void ApiClient::registerDevice(const QString& mac,
                               const QString& name,
                               const QString& phoneNum,
                               int            deviceType,
                               int            rssi,
                               const QString& requestedAt,
                               const QString& bssid)
{
    QJsonObject body;
    body["mac_address"]  = mac;
    body["owner_name"]   = name;
    body["phone_number"] = phoneNum;
    body["type"]         = deviceType;
    body["rssi"]         = rssi;
    body["requested_at"] = requestedAt;
    body["bssid"]        = bssid;

    const QString url = baseUrl_ + "/v1/devices/register";
    LOG(INFO) << "ApiClient::registerDevice POST " << url.toStdString()
              << " mac=" << mac.toStdString() << " name=" << name.toStdString()
              << " type=" << deviceType << " rssi=" << rssi;

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");//before post, after reuqest
    req.setRawHeader("x-api-key", apiKey_.toUtf8());   // 서버 인증 헤더
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            auto [msg, netErr] = parseHttpError(reply);
            LOG(ERROR) << "ApiClient::registerDevice error mac=" << mac.toStdString()
                       << " networkError=" << netErr << " msg=" << msg.toStdString();
            emit registerFailed(mac, msg, netErr);
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
    body["type"]  = deviceType;
    body["requested_at"] = requestedAt;
    // ※ rssi, vendor 는 변경 요청에 포함하지 않음 (프로토콜 명세)

    const QString url = baseUrl_ + "/v1/devices/update";
    LOG(INFO) << "ApiClient::updateDevice POST " << url.toStdString()
              << " mac=" << mac.toStdString() << " name=" << name.toStdString()
              << " type=" << deviceType;

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-api-key", apiKey_.toUtf8());   // 서버 인증 헤더
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            auto [msg, netErr] = parseHttpError(reply);
            LOG(ERROR) << "ApiClient::updateDevice error mac=" << mac.toStdString()
                       << " networkError=" << netErr << " msg=" << msg.toStdString();
            emit updateFailed(mac, msg, netErr);
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
    req.setRawHeader("x-api-key", apiKey_.toUtf8());   // 서버 인증 헤더
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->deleteResource(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            auto [msg, netErr] = parseHttpError(reply);
            LOG(ERROR) << "ApiClient::deleteDevice error mac=" << mac.toStdString()
                       << " networkError=" << netErr << " msg=" << msg.toStdString();
            emit deleteFailed(mac, msg, netErr);
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
    req.setRawHeader("x-api-key", apiKey_.toUtf8());   // 서버 인증 헤더
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            //테스트용 http status 확인
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            LOG(ERROR) << "ApiClient::fetchDeviceList network error err="
                       << reply->errorString().toStdString()
                       << " httpStatus=" << httpStatus;// http status checking code
            emit deviceListFailed(reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        // 실제 응답 형태:
        // { "data": [ { "mac":..., "name":..., "phone_num":..., "type":"1",
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
            d.deviceType   = o["type"].toInt();
            d.registeredAt = o["request_at"].toString();
            d.bssid        = o["bssid"].toString().toUpper();
            devices.push_back(d);
        }
        LOG(INFO) << "ApiClient::fetchDeviceList success count=" << devices.size();
        emit deviceListFetched(devices);
    });
}

//시나리오 5: POST ap
void ApiClient::registerAPs(const QString& bssid,
                            int            type,
                            const QString& ssid,
                            int            ch)
{
    QJsonObject body;
    body["bssid"] = bssid;
    body["type"]  = type;
    body["ssid"]  = ssid;
    body["ch"]    = ch;

    const QString url = baseUrl_ + "/api/v1/aps";
    LOG(INFO) << "ApiClient::registerAPs POST " << url.toStdString()
              << " bssid=" << bssid.toStdString();

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-api-key", apiKey_.toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, bssid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            auto [msg, netErr] = parseHttpError(reply);
            LOG(ERROR) << "ApiClient::registerAPs error bssid=" << bssid.toStdString()
                       << " networkError=" << netErr << " msg=" << msg.toStdString();
            emit registerAPsFailed();
            return;
        }
        LOG(INFO) << "ApiClient::registerAPs success bssid=" << bssid.toStdString();
        emit registerAPsSuccess();
    });
}

void ApiClient::fetchAPs()
{
    const QString url = baseUrl_ + "/api/v1/aps";
    LOG(INFO) << "ApiClient::fetchAPs GET " << url.toStdString();

    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("x-api-key", apiKey_.toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG(ERROR) << "ApiClient::fetchAPs network error err="
                       << reply->errorString().toStdString();
            emit apListFailed(reply->errorString());
            return;
        }
        QJsonObject data = QJsonDocument::fromJson(reply->readAll()).object()["data"].toObject();
        QVector<ApRecord> aps;
        for (const QString& bssid : data.keys()) {
            QJsonObject o = data[bssid].toObject();
            ApRecord ap;
            ap.bssid   = bssid.toUpper();
            ap.ssid    = o["ssid"].toString();
            ap.type    = o["type"].toInt();
            ap.channel = o["ch"].toInt();
            aps.push_back(ap);
        }
        LOG(INFO) << "ApiClient::fetchAPs success count=" << aps.size();
        emit apListFetched(aps);
    });
}

// GET /api/v1/aps/{bssid}
void ApiClient::fetchOneAP(const QString& bssid)
{
    const QString encodedBssid = QString::fromUtf8(QUrl::toPercentEncoding(bssid));
    const QString url = baseUrl_ + "/api/v1/aps/" + encodedBssid;
    LOG(INFO) << "ApiClient::fetchOneAP GET " << url.toStdString();

    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("x-api-key", apiKey_.toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, bssid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG(ERROR) << "ApiClient::fetchOneAP network error bssid=" << bssid.toStdString()
                       << " err=" << reply->errorString().toStdString();
            emit apFetchFailed(reply->errorString());
            return;
        }
        QJsonObject data = QJsonDocument::fromJson(reply->readAll()).object()["data"].toObject();
        ApRecord ap;
        ap.bssid   = data["bssid"].toString().toUpper();
        ap.type    = data["type"].toInt();
        ap.ssid    = data["ssid"].toString();
        ap.channel = data["ch"].toInt();
        LOG(INFO) << "ApiClient::fetchOneAP success bssid=" << bssid.toStdString();
        emit apFetched(ap);
    });
}

// POST /api/v1/aps/update
void ApiClient::updateAP(const QString& bssid,
                         int            type,
                         const QString& ssid,
                         int            ch)
{
    QJsonObject body;
    body["bssid"] = bssid;
    body["type"]  = type;
    body["ssid"]  = ssid;
    body["ch"]    = ch;

    const QString url = baseUrl_ + "/api/v1/aps/update";
    LOG(INFO) << "ApiClient::updateAP POST " << url.toStdString()
              << " bssid=" << bssid.toStdString();

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-api-key", apiKey_.toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, bssid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            auto [msg,netErr]=parseHttpError(reply);
            LOG(ERROR) << "ApiClient::updateAP network error bssid=" << bssid.toStdString()
                       << "network err=" << netErr << "msg = " << msg.toStdString() ;
            emit updateAPFailed(bssid, msg, netErr);
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (resp["code"].toInt() == 200) {
            LOG(INFO) << "ApiClient::updateAP success bssid=" << bssid.toStdString();
            emit updateAPSuccess(bssid);
        } else {
            LOG(WARNING) << "ApiClient::updateAP failed bssid=" << bssid.toStdString()
                         << " code=" << resp["code"].toInt()
                         << " message=" << resp["message"].toString().toStdString();
            emit updateAPFailed(bssid, resp["message"].toString(), /*networkError=*/false);
        }
    });
}

// DELETE /api/v1/aps/{bssid}
void ApiClient::deleteAP(const QString& bssid)
{
    const QString encodedBssid = QString::fromUtf8(QUrl::toPercentEncoding(bssid));
    const QString url = baseUrl_ + "/api/v1/aps/" + encodedBssid;
    LOG(INFO) << "ApiClient::deleteAP DELETE " << url.toStdString()
              << " bssid=" << bssid.toStdString();

    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("x-api-key", apiKey_.toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam_->deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, bssid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            auto [msg, netErr] = parseHttpError(reply);
            LOG(ERROR) << "ApiClient::deleteAP network error bssid=" << bssid.toStdString()
                       << " err=" << "networkError = " << "msg = " << msg.toStdString();
            emit deleteAPFailed(bssid, msg, netErr);
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (resp["code"].toInt() == 200) {
            LOG(INFO) << "ApiClient::deleteAP success bssid=" << bssid.toStdString();
            emit deleteAPSuccess(bssid);
        } else {
            LOG(WARNING) << "ApiClient::deleteAP failed bssid=" << bssid.toStdString()
                         << " code=" << resp["code"].toInt()
                         << " message=" << resp["message"].toString().toStdString();
            emit deleteAPFailed(bssid, resp["message"].toString(), /*networkError=*/false);
        }
    });
}