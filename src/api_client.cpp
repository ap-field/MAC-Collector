#include "api_client.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

ApiClient::ApiClient(const QString& baseUrl, QObject* parent)
    : QObject(parent),
    nam_(new QNetworkAccessManager(this)),
    baseUrl_(baseUrl)
{}

void ApiClient::registerAsset(const QString& mac,
                              const QString& name,
                              const QString& phone,
                              int            rssi,
                              const QString& vendor,
                              const QString& requestedAt)
{
    QJsonObject body;
    body["mac_address"]  = mac;
    body["owner_name"]   = name;
    body["phone_number"] = phone;
    body["rssi"]         = rssi;
    body["vendor"]       = vendor;
    body["requested_at"] = requestedAt;

    QNetworkRequest req(QUrl(baseUrl_ + "/v1/assets/register"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nam_->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit registerFailed(mac, reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (resp["code"].toInt() == 201) {
            QString regAt = resp["data"].toObject()["registered_at"].toString();
            emit registerSuccess(mac, regAt);
        } else {
            emit registerFailed(mac, resp["message"].toString());
        }
    });
}

void ApiClient::updateAsset(const QString& mac,
                            const QString& name,
                            const QString& phone,
                            int            rssi,
                            const QString& vendor,
                            const QString& requestedAt)
{
    QJsonObject body;
    body["mac_address"]  = mac;
    body["owner_name"]   = name;
    body["phone_number"] = phone;
    body["rssi"]         = rssi;
    body["vendor"]       = vendor;
    body["requested_at"] = requestedAt;

    QString encodedMac = QString(mac).replace(":", "%3A");
    QNetworkRequest req(QUrl(baseUrl_ + "/v1/assets/" + encodedMac));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nam_->put(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit updateFailed(mac, reply->errorString());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (resp["status"].toString() == "updated") {
            QString updAt = resp["data"].toObject()["updated_at"].toString();
            emit updateSuccess(mac, updAt);
        } else {
            emit updateFailed(mac, resp["message"].toString());
        }
    });
}

void ApiClient::checkAsset(const QString& mac) {
    QString encodedMac = QString(mac).replace(":", "%3A");
    QNetworkRequest req(QUrl(baseUrl_ + "/v1/assets/" + encodedMac));

    QNetworkReply* reply = nam_->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mac]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkResult(mac, false, QJsonObject());
            return;
        }
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (resp["status"].toString() == "duplicate") {
            emit checkResult(mac, true, resp["data"].toObject());
        } else {
            emit checkResult(mac, false, QJsonObject());
        }
    });
}