#pragma once

#include <glog/logging.h>
#include <atomic>
#include <set>

#include <QObject>
#include <QString>

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


    void configure(const QString& iface, int rssiThreshold, Db* db);
    void requestStop();

public slots:
    void run();

signals:
    void candidateFound(QString macStr, int rssi, QString timestamp);
    void errorOccurred(QString msg);
    // pcap 오픈·필터 설정까지 성공해 캡처 루프에 진입했을 때 emit.
    // 최초 시작과 재시도 성공을 UI 가 동일하게 처리(🟢 수집 중)하도록 알린다.
    void captureStarted();
    void finished();

private:
    QString           iface_;
    Parser            parser_;
    Db*               db_;
    std::atomic<bool> stop_;
    pcap_t*           pcap_ = nullptr;
    std::set<Mac>     seenInSession_;
};