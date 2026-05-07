#include "capture.h"
#include "db.h"

#include <pcap.h>

#include <cstring>

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
}

void CaptureWorker::run() {
    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    pcap_t* pcap = pcap_open_live(iface_.toUtf8().constData(),
                                  2048, 1, 1000, errbuf);
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

    const char* filter =
        "type mgt subtype auth or "
        "type mgt subtype assoc-req or "
        "type mgt subtype reassoc-req";

    bpf_program bpf;
    std::memset(&bpf, 0, sizeof(bpf));
    if (pcap_compile(pcap, &bpf, filter, 1, PCAP_NETMASK_UNKNOWN) != 0) {
        emit errorOccurred(QString("pcap_compile 실패: %1").arg(pcap_geterr(pcap)));
        pcap_close(pcap);
        emit finished();
        return;
    }
    if (pcap_setfilter(pcap, &bpf) != 0) {
        emit errorOccurred(QString("pcap_setfilter 실패: %1").arg(pcap_geterr(pcap)));
        pcap_freecode(&bpf);
        pcap_close(pcap);
        emit finished();
        return;
    }
    pcap_freecode(&bpf);

    while (!stop_.load()) {
        pcap_pkthdr*   hdr  = nullptr;
        const uint8_t* data = nullptr;
        int rc = pcap_next_ex(pcap, &hdr, &data);
        if (rc == 0) continue;     // timeout
        if (rc < 0) break;          // error / EOF

        Parser::Result r = parser_.parse(data, static_cast<int>(hdr->caplen));
        if (!r.ok) continue;

        // 세션 내 중복 emit 방지
        if (seenInSession_.find(r.addr2) != seenInSession_.end()) continue;

        // DB 기등록 필터링 (station ∪ ap)
        if (db_ != nullptr && db_->macExists(r.addr2)) {
            seenInSession_.insert(r.addr2);
            continue;
        }

        seenInSession_.insert(r.addr2);
        emit candidateFound(QString::fromStdString(r.addr2.toString()), r.rssi);
    }

    pcap_close(pcap);
    emit finished();
}