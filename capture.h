#pragma once

#include <atomic>
#include <set>

#include <QObject>
#include <QString>

#include "mac.h"
#include "parser.h"

class Db;  // forward decl. <pcap.h> 는 .cpp 에서만

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
    void candidateFound(QString macStr, int rssi);
    void errorOccurred(QString msg);
    void finished();

private:
    QString           iface_;
    Parser            parser_;
    Db*               db_;
    std::atomic<bool> stop_;
    std::set<Mac>     seenInSession_;  // 동일 MAC 중복 emit 방지
};