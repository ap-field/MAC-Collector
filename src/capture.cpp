#include "capture.h"
#include "db.h"
#include <pcap.h>

#include <cstdio>
#include <cstring>
#include <QDebug>
#include <QDateTime>

CaptureWorker::CaptureWorker(QObject* parent)
    : QObject(parent), parser_(-60), db_(nullptr), stop_(false) {
    LOG(INFO)<< "CaptureWorker";
}

CaptureWorker::~CaptureWorker() {
    LOG(INFO)<<"~CaptureWorker";
}

void CaptureWorker::configure(const QString& iface, int rssiThreshold, const QVector<int>& channels, Db* db) {
    iface_ = iface;
    parser_.setRssiThreshold(rssiThreshold);
    (void)channels;
    db_ = db;
}

void CaptureWorker::requestStop() {
    LOG(INFO)<<"requestStop";

    stop_ = true;          // stop_ 으로 통일// 수정
    if (pcap_)
        pcap_breakloop(pcap_);  // pcap_ 으로 통일 // 수정
}

void CaptureWorker::forgetSeen(const Mac& mac) {
    std::lock_guard<std::mutex> lk(seenMu_);
    size_t n = seenInSession_.erase(mac);
    LOG(INFO) << "CaptureWorker::forgetSeen mac=" << mac.toString() << " erased=" << n;
}

void CaptureWorker::run() {
    LOG(INFO)<< "run beg";
    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    pcap_t* pcap = pcap_open_live(iface_.toUtf8().constData(),
                                  2048, 1, 100, errbuf);
    if (pcap == nullptr) {
        // 권한 부족(cap_net_raw 미적용)일 때 pcap_open_live 가 여기서 실패한다.
        // run.sh 가 setcap 으로 권한을 적용하지만, 재빌드 후 누락되면 이 경로로 떨어짐.
        LOG(ERROR) << "pcap_open_live returned null iface=" << iface_.toStdString()
                   << " errbuf=" << errbuf
                   << " (cap_net_raw 권한 또는 인터페이스 존재 여부 확인 필요)";
        emit errorOccurred(QString("pcap_open_live 실패: %1").arg(errbuf));
        emit finished();
        return;
    }
    LOG(INFO) << "pcap_open_live ok iface=" << iface_.toStdString();

    if (pcap_datalink(pcap) != DLT_IEEE802_11_RADIO) {
        LOG(ERROR) << "datalink is not DLT_IEEE802_11_RADIO iface=" << iface_.toStdString()
                   << " (모니터 모드 미설정)";
        emit errorOccurred("Radiotap(DLT_IEEE802_11_RADIO) 인터페이스가 아닙니다. 모니터 모드 확인 필요.");
        pcap_close(pcap);
        emit finished();
        return;
    }

    struct bpf_program fp;
    const char* filter =
        "(type mgt subtype beacon) or "
        "(type mgt subtype auth) or "
        "(type mgt subtype assoc-req) or "
        "(type mgt subtype reassoc-req)";
    if (pcap_compile(pcap, &fp, filter, 1, PCAP_NETMASK_UNKNOWN) < 0) {
        LOG(ERROR) << "pcap_compile failed: " << pcap_geterr(pcap);
        emit errorOccurred(QString("BPF 컴파일 실패: %1").arg(pcap_geterr(pcap)));
        pcap_close(pcap);
        emit finished();
        return;
    }
    if (pcap_setfilter(pcap, &fp) < 0) {
        LOG(ERROR) << "pcap_setfilter failed: " << pcap_geterr(pcap);
        emit errorOccurred(QString("BPF 필터 적용 실패: %1").arg(pcap_geterr(pcap)));
        pcap_freecode(&fp);
        pcap_close(pcap);
        emit finished();
        return;
    }
    pcap_freecode(&fp);

    pcap_ = pcap;
    LOG(INFO) << "[CaptureWorker] 캡처 시작 - " << iface_.toStdString();
    // 최초 시작/재시도 성공을 UI 에 알림 → 🟢 수집 중 상태로 복귀, 재시도 버튼 숨김
    emit captureStarted();

    // /sys/class/net/{iface}/operstate 를 읽어 인터페이스 up 여부 확인
    auto checkIfaceUp = [&]() -> bool {
        std::string path = "/sys/class/net/" + iface_.toStdString() + "/operstate";
        char buf[16] = {};
        FILE* f = fopen(path.c_str(), "r");
        if (!f) {
            LOG(WARNING) << "checkIfaceUp fopen failed path=" << path
                         << " (인터페이스 삭제 가능성)";
            return false;
        }
        fgets(buf, sizeof(buf), f);
        fclose(f);
        return strncmp(buf, "up", 2) == 0;
    };

    QString stopReason;
    int timeoutCount = 0;

    while (!stop_.load()) {
        pcap_pkthdr*   hdr  = nullptr;
        const uint8_t* data = nullptr;
        LOG(INFO) << "bef pcap_next_ex";
        int rc = pcap_next_ex(pcap_, &hdr, &data);
        LOG(INFO) << "aft pcap_next_ex" << rc;
        if (rc == 0) {
            // 패킷 없음(timeout) — 약 1초(100ms × 10)마다 인터페이스 상태 확인
            if (++timeoutCount >= 10) {
                timeoutCount = 0;
                if (!checkIfaceUp()) {
                    stopReason = QString("인터페이스 %1 down/삭제 감지").arg(iface_);
                    LOG(ERROR) << "[CaptureWorker] 인터페이스 down/삭제 감지 iface="
                               << iface_.toStdString() << " (operstate != up)";
                    emit errorOccurred(QString("인터페이스 %1 이(가) down되었습니다.").arg(iface_));
                    break;
                }
            }
            continue;
        }
        timeoutCount = 0;
        if (rc == -2) { stopReason = "pcap_breakloop 호출됨"; break; }
        if (rc < 0)   {
            // 무선랜이 실행 중 제거되면 pcap_next_ex 가 -1(읽기 오류)로 떨어지는 경우가 많다.
            // operstate down 경로(rc==0)와 달리 여기서도 반드시 errorOccurred 를 emit하고 종료한다.
            stopReason = QString("pcap 오류: %1").arg(pcap_geterr(pcap_));
            LOG(ERROR) << "[CaptureWorker] pcap_next_ex 오류 rc=" << rc
                       << " iface=" << iface_.toStdString()
                       << " err=" << pcap_geterr(pcap_)
                       << " (인터페이스 상실 가능)";
            emit errorOccurred(
                QString("패킷 캡처 오류(인터페이스 상실 가능): %1").arg(pcap_geterr(pcap_)));
            break;
        }

        // beacon — BSSID/SSID/채널을 캐시에만 저장. station 탐지 시 조회해 emit.
        {
            Parser::BeaconInfo bi = parser_.parseBeacon(data, static_cast<int>(hdr->caplen));
            if (bi.ok) {
                beaconCache_[bi.bssid] = bi;
                continue;
            }
        }

        Parser::Result r = parser_.parse(data, static_cast<int>(hdr->caplen));
        if (!r.ok) continue;

        // 세션 내 중복 emit 방지. forgetSeen()(메인 스레드)과 동시 접근하므로 락으로 보호.
        QString macStr = QString::fromStdString(r.addr2.toString());
        {
            std::lock_guard<std::mutex> lk(seenMu_);
            if (seenInSession_.find(r.addr2) != seenInSession_.end()) {
                LOG(INFO) << "[CAPTURE] 중복 스킵: " << macStr.toStdString();
                continue;
            }
            seenInSession_.insert(r.addr2);
        }

        const char* kindName =
            (r.kind == Parser::FrameKind::Auth)  ? "auth" :
            (r.kind == Parser::FrameKind::Assoc) ? "assoc" : "eapol";//
        QString ts = QDateTime::currentDateTime().toString("yyMMdd'T'HHmmss");

        LOG(INFO) << "[CAPTURE] MAC 탐지: " << macStr.toStdString()
                  << " 경유=" << kindName
                  << " RSSI=" << r.rssi;

        auto it = beaconCache_.find(r.apBssid);
        if (it != beaconCache_.end() && db_->macExists(r.addr2)) {
            const auto& bi = it->second;
            emit beaconFound(
                QString::fromStdString(bi.bssid.toString()),
                QString::fromStdString(bi.ssid),
                bi.channel);
        }
        emit candidateFound(macStr, r.rssi, ts, QString::fromStdString(r.apBssid.toString()));
    }

    if (stopReason.isEmpty())
        stopReason = stop_.load() ? "정지 요청" : "알 수 없는 이유";

    LOG(INFO) << "[CaptureWorker] 캡처 루프 종료 - " << stopReason.toStdString();
    pcap_close(pcap_);
    pcap_ = nullptr;
    LOG(INFO) << "[CaptureWorker] 스레드 종료 완료";
    emit finished();
    LOG(INFO)<< "run end";
}