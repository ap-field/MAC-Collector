#include "capture.h"
#include "db.h"
#include <pcap.h>

#include <cstring>
#include <QDebug>
#include <QDateTime>

CaptureWorker::CaptureWorker(QObject* parent)
    : QObject(parent), parser_(-60), db_(nullptr), stop_(false) {}

CaptureWorker::~CaptureWorker() = default;

void CaptureWorker::configure(const QString& iface, int rssiThreshold, Db* db) {
    iface_ = iface;
    parser_.setRssiThreshold(rssiThreshold);
    db_ = db;
}

void CaptureWorker::requestStop() {
    stop_ = true;          // stop_ 으로 통일
    if (pcap_)
        pcap_breakloop(pcap_);  // pcap_ 으로 통일
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

    struct bpf_program fp;
    const char* filter =
        "(type mgt subtype auth) or "
        "(type mgt subtype assoc-req) or "
        "(type mgt subtype reassoc-req) or "
        "type data";
    if (pcap_compile(pcap, &fp, filter, 1, PCAP_NETMASK_UNKNOWN) < 0) {
        emit errorOccurred(QString("BPF 컴파일 실패: %1").arg(pcap_geterr(pcap)));
        pcap_close(pcap);
        emit finished();
        return;
    }
    if (pcap_setfilter(pcap, &fp) < 0) {
        emit errorOccurred(QString("BPF 필터 적용 실패: %1").arg(pcap_geterr(pcap)));
        pcap_freecode(&fp);
        pcap_close(pcap);
        emit finished();
        return;
    }
    pcap_freecode(&fp);

    pcap_ = pcap;
    qDebug() << "[CAPTURE] 캡처 루프 시작 -" << iface_;

    int pktCount = 0, parseOk = 0, parseFail = 0;

    while (!stop_.load()) {
        pcap_pkthdr*   hdr  = nullptr;
        const uint8_t* data = nullptr;

        int rc = pcap_next_ex(pcap_, &hdr, &data);
        if (rc == 0)  continue;   // timeout
        if (rc == -2) { qDebug() << "[CAPTURE] breakloop"; break; }
        if (rc < 0)   { qDebug() << "[CAPTURE] pcap 에러:" << pcap_geterr(pcap_); break; }

        pktCount++;
        if (pktCount <= 5 || pktCount % 500 == 0)
            qDebug() << "[CAPTURE] 패킷 수신 #" << pktCount << "len:" << hdr->caplen;

        Parser::Result r = parser_.parse(data, static_cast<int>(hdr->caplen));
        if (!r.ok) { parseFail++; continue; }
        parseOk++;

        if (seenInSession_.find(r.addr2) != seenInSession_.end()) continue;
        seenInSession_.insert(r.addr2);

        QString macStr = QString::fromStdString(r.addr2.toString());
        QString ts     = QDateTime::currentDateTime().toString("yyMMdd'T'HHmmss");

        qDebug() << "[CAPTURE] MAC 탐지:" << macStr << "RSSI:" << r.rssi;
        emit candidateFound(macStr, r.rssi, ts);
    }

    qDebug() << "[CAPTURE] 종료 | 패킷:" << pktCount << "파싱OK:" << parseOk << "파싱실패:" << parseFail;
    pcap_close(pcap_);
    pcap_ = nullptr;
    emit finished();
}