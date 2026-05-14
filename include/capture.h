#pragma once

#include <atomic>
#include <set>

#include <QObject>
#include <QString>

#include "mac.h"
#include "parser.h"

struct pcap;
typedef struct pcap pcap_t;   // <pcap.h> 는 .cpp 에서만 include

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
    void candidateFound(QString macStr, int rssi, QString vendor, QString timestamp);
    void errorOccurred(QString msg);
    void finished();

private:
    QString           iface_;
    Parser            parser_;
    Db*               db_;
    std::atomic<bool> stop_;
    pcap_t*           pcap_ = nullptr;
    std::set<Mac>     seenInSession_;
};