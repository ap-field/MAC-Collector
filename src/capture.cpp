#include "capture.h"
#include "db.h"
#include <pcap.h>

#include <cstdio>
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
    qDebug() << "[CaptureWorker] 캡처 시작 -" << iface_;

    // /sys/class/net/{iface}/operstate 를 읽어 인터페이스 up 여부 확인
    auto checkIfaceUp = [&]() -> bool {
        std::string path = "/sys/class/net/" + iface_.toStdString() + "/operstate";
        char buf[16] = {};
        FILE* f = fopen(path.c_str(), "r");
        if (!f) return false;
        fgets(buf, sizeof(buf), f);
        fclose(f);
        return strncmp(buf, "up", 2) == 0;
    };

    QString stopReason;
    int timeoutCount = 0;

    while (!stop_.load()) {
        pcap_pkthdr*   hdr  = nullptr;
        const uint8_t* data = nullptr;

        int rc = pcap_next_ex(pcap_, &hdr, &data);
        if (rc == 0) {
            // 패킷 없음(timeout) — 약 1초(100ms × 10)마다 인터페이스 상태 확인
            if (++timeoutCount >= 10) {
                timeoutCount = 0;
                if (!checkIfaceUp()) {
                    stopReason = QString("인터페이스 %1 down/삭제 감지").arg(iface_);
                    emit errorOccurred(QString("인터페이스 %1 이(가) down되었습니다.").arg(iface_));
                    break;
                }
            }
            continue;
        }
        timeoutCount = 0;
        if (rc == -2) { stopReason = "pcap_breakloop 호출됨"; break; }
        if (rc < 0)   { stopReason = QString("pcap 오류: %1").arg(pcap_geterr(pcap_)); break; }

        Parser::Result r = parser_.parse(data, static_cast<int>(hdr->caplen));
        if (!r.ok) continue;

        // 세션 내 중복 emit 방지
        QString macStr = QString::fromStdString(r.addr2.toString());
        if (seenInSession_.find(r.addr2) != seenInSession_.end()) {
            qDebug() << "[CAPTURE] 중복 스킵:" << macStr;
            continue;
        }
        seenInSession_.insert(r.addr2);

        const char* kindName =
            (r.kind == Parser::FrameKind::Auth)  ? "auth" :
            (r.kind == Parser::FrameKind::Assoc) ? "assoc" : "eapol";
        QString ts = QDateTime::currentDateTime().toString("yyMMdd'T'HHmmss");

        qDebug() << "[CAPTURE] MAC 탐지:" << macStr
                 << "경유:" << kindName
                 << "RSSI:" << r.rssi;
        emit candidateFound(macStr, r.rssi, ts);
    }

    if (stopReason.isEmpty())
        stopReason = stop_.load() ? "정지 요청" : "알 수 없는 이유";

    qDebug() << "[CaptureWorker] 캡처 루프 종료 -" << stopReason;
    pcap_close(pcap_);
    pcap_ = nullptr;
    qDebug() << "[CaptureWorker] 스레드 종료 완료";
    emit finished();
}