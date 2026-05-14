#include "capture.h"
#include "db.h"
#include "vendor_lookup.h"

#include <pcap.h>

#include <cstring>
#include <QDebug>
#include <QDateTime>

CaptureWorker::CaptureWorker(QObject* parent)
    : QObject(parent), parser_(-20), db_(nullptr), stop_(false) {}

CaptureWorker::~CaptureWorker() = default;

void CaptureWorker::configure(const QString& iface, int rssiThreshold, Db* db) {
    iface_ = iface;
    parser_.setRssiThreshold(rssiThreshold);
    db_ = db;
}

void CaptureWorker::requestStop() {
    stop_.store(true);
    if (pcap_) pcap_breakloop(pcap_);
}

void CaptureWorker::run() {
    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    pcap_t* pcap = pcap_open_live(iface_.toUtf8().constData(),
                                  2048, 1, 100, errbuf);
    if (pcap == nullptr) {
        emit errorOccurred(QString("pcap_open_live 실패: %1").arg(errbuf));
        emit finished();
        return;
    }

    if (pcap_datalink(pcap) != DLT_IEEE802_11_RADIO) {
        emit errorOccurred("Radiotap(DLT_IEEE802_11_RADIO) 인터페이스가 아닙니다. 모니터 모드 확인 필요.");
        pcap_close(pcap);
        emit finished();
        return;
    }

    pcap_ = pcap;

    while (!stop_.load()) {
        pcap_pkthdr*   hdr  = nullptr;
        const uint8_t* data = nullptr;

        int rc = pcap_next_ex(pcap_, &hdr, &data);
        if (rc == 0)  continue;   // timeout
        if (rc == -2) break;      // pcap_breakloop 호출됨
        if (rc < 0)   break;      // 그 외 에러 / EOF

        Parser::Result r = parser_.parse(data, static_cast<int>(hdr->caplen));
        if (!r.ok) continue;

        // 세션 내 중복 emit 방지
        if (seenInSession_.find(r.addr2) != seenInSession_.end()) continue;
        seenInSession_.insert(r.addr2);

        // DB 기등록 필터링은 UI 레이어(Phase1Widget)에서 수행
        // capture 레이어는 emit만 담당

        QString macStr = QString::fromStdString(r.addr2.toString());
        QString vendor = VendorLookup::instance().lookup(macStr);
        QString ts     = QDateTime::currentDateTime().toString(Qt::ISODate);

        emit candidateFound(macStr, r.rssi, vendor, ts);
    }

    pcap_close(pcap_);
    pcap_ = nullptr;
    emit finished();
}