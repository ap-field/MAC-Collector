#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>

class QNetworkAccessManager;

// 서버 /api/v1/aps 응답의 단일 AP 정보
struct ApRecord {
    QString ssid;
    QString bssid;
    int     type    = 0;
    int     channel = 0;
};

// 서버 /v1/devices/lists 응답의 단일 디바이스 정보
struct DeviceRecord {
    QString mac;            // 대문자 "AA:BB:CC:DD:EE:FF"
    QString name;
    QString phoneNum;
    int     deviceType = 0;       // 0=기타 1=노트북 2=핸드폰 3=태블릿 4=IoT
    QString registeredAt;   // "yyMMddTHHmmss"
    QString updatedAt;      // 없으면 빈 문자열
    QString bssid;          // 감지 당시 연결 AP — 없으면 빈 문자열
};

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(const QString& baseUrl,
                       const QString& apiKey,
                       QObject* parent = nullptr);

    // 시나리오 1 — 신규 등록
    void registerDevice(const QString& mac,
                        const QString& name,
                        const QString& phoneNum,
                        int            deviceType,   // 1=노트북 2=핸드폰 0=기타
                        int            rssi,
                        const QString& requestedAt, // "yyMMddTHHmmss"
                        const QString& bssid = QString());

    // 시나리오 3 — 정보 변경
    void updateDevice(const QString& mac,
                      const QString& name,
                      const QString& phoneNum,
                      int            deviceType,
                      const QString& requestedAt);

    // 시나리오 2 — 전체 MAC 목록 조회 (중복 확인용)
    void fetchDeviceList();

    // AP 목록 조회
    void fetchAPs();

    // 시나리오 4 — 삭제 (관리자 페이지에서 등록 기기 제거)
    // DELETE /v1/devices/delete/{mac} — MAC 을 경로에 담아 보낸다(바디 없음).
    void deleteDevice(const QString& mac);

    void registerAPs(const QString& bssid,
                     int type,
                     const QString& ssid,
                     int            ch);

    void fetchOneAP(const QString& bssid);

    void updateAP(const QString& bssid,
                  int            type,
                  const QString& ssid,
                  int            ch);

    void deleteAP(const QString& bssid);

signals:
    void registerSuccess(QString mac);
    // networkError=true 면 서버 연결 자체 실패(서버 다운). false 면 서버가 도달했으나 거절.
    void registerFailed(QString mac, QString reason, bool networkError);

    void updateSuccess(QString mac, QString updatedAt);
    void updateFailed(QString mac, QString reason, bool networkError);

    void deleteSuccess(QString mac);
    void deleteFailed(QString mac, QString reason, bool networkError);

    void registerAPsSuccess();
    void registerAPsFailed();


    void apListFetched(QVector<ApRecord> aps);
    void apListFailed(QString reason);

    void apFetched(ApRecord ap);
    void apFetchFailed(QString reason);

    void updateAPSuccess(QString bssid);
    void updateAPFailed(QString bssid, QString reason, bool networkError);

    void deleteAPSuccess(QString bssid);
    void deleteAPFailed(QString bssid, QString reason, bool networkError);

    // 조회 성공: 등록된 디바이스 전체 정보 반환
    void deviceListFetched(QVector<DeviceRecord> devices);
    void deviceListFailed(QString reason);

private:
    QNetworkAccessManager* nam_;
    QString                baseUrl_;
    QString                apiKey_;   // 모든 요청의 x-api-key 헤더 값(서버 인증)
};