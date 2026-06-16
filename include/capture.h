#pragma once

#include <glog/logging.h>
#include <atomic>
#include <mutex>
#include <set>

#include <QObject>
#include <QString>
#include <QVector>

#include "mac.h"
#include "parser.h"

struct pcap;
typedef struct pcap pcap_t;

class Db;

class CaptureWorker : public QObject {
    Q_OBJECT
public:
    explicit CaptureWorker(QObject* parent = nullptr);
    ~CaptureWorker() override;


    void configure(const QString& iface, int rssiThreshold, const QVector<int>& channels, Db* db);
    void requestStop();

    // 세션 중복 집합에서 해당 MAC 을 제거해 다시 후보로 잡히게 한다.
    // (관리자 페이지에서 삭제한 기기를 재감지/재등록할 수 있도록.)
    // 캡처 스레드가 run() 블로킹 루프에 묶여 이벤트 루프를 돌리지 않으므로,
    // 큐 연결이 아니라 메인 스레드에서 직접 호출하고 seenMu_ 로 보호한다.
    void forgetSeen(const Mac& mac);

public slots:
    void run();

signals:
    void candidateFound(QString macStr, int rssi, QString timestamp);
    void errorOccurred(QString msg);
    // pcap 오픈·필터 설정까지 성공해 캡처 루프에 진입했을 때 emit.
    // 최초 시작과 재시도 성공을 UI 가 동일하게 처리(🟢 수집 중)하도록 알린다.
    void captureStarted();
    void finished();
    void beaconFound(QString bssid, QString ssid, int channel);

private:
    QString           iface_;
    Parser            parser_;
    Db*               db_;
    std::atomic<bool> stop_;
    pcap_t*           pcap_ = nullptr;
    std::set<Mac>     seenInSession_;
    std::mutex        seenMu_;   // seenInSession_ 동시 접근 보호(캡처 스레드 ↔ 메인 스레드)
};