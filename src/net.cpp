#include "net.h"

#include <iostream>
#include <string>
#include <atomic>
#include <csignal>
#include <stdexcept>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>

static std::atomic<bool> g_running(true);
// 종료 시 깨울 캡처 핸들들(어댑터 여러 개 가능). main 이 스레드 시작 전 등록 → 시그널/스레드가 읽음.
static pcap_t*           g_pcaps[8] = {};
static int               g_npcaps   = 0;

void request_stop() {
    g_running.store(false);
    for (int i = 0; i < g_npcaps; ++i)        // 등록된 모든 캡처 핸들을 pcap_breakloop 로 깨움
        if (g_pcaps[i]) pcap_breakloop(g_pcaps[i]);
}

static void on_sigint(int) { request_stop(); }

bool running() { return g_running.load(); }


// 명령행 인자 파싱. 잘못된 인자가 있으면 false 반환. 성공 시 out 에 결과 채워짐.
void install_signals() {
    std::signal(SIGINT,  on_sigint);
    std::signal(SIGTERM, on_sigint);
}
// 종료 시 pcap_breakloop 로 깨울 캡처 핸들을 등록(추가). 어댑터마다 한 번씩 부른다(최대 8개).
void set_capture_handle(pcap_t* pcap) {
    if (g_npcaps < 8) g_pcaps[g_npcaps++] = pcap;
}

pcap_t* open_monitor_pcap(const char* ifname, int timeout_ms,
                          bool immediate_mode, int buffer_bytes) {
    char errbuf[PCAP_ERRBUF_SIZE];
    // pcap_open_live 대신 pcap_create+activate 로 — immediate_mode/버퍼크기를 줄 수 있어서.
    pcap_t* pcap = pcap_create(ifname, errbuf);
    if (!pcap) {
        std::cerr << "pcap_create : " << errbuf << "\n"
                  << "  -> root 권한과 monitor mode 인터페이스를 확인하세요.\n";
        return nullptr;
    }

    // 아래 4줄은 pcap_open_live(ifname, 65535, 1, timeout_ms) 와 동등한 기본 설정.
    pcap_set_snaplen(pcap, 65535);                              // 프레임 전체 캡처
    pcap_set_promisc(pcap, 1);                                  // promiscuous
    pcap_set_timeout(pcap, timeout_ms);                         // 타임아웃(ms)
    if (immediate_mode) pcap_set_immediate_mode(pcap, 1);       // 프레임당 즉시 전달
    if (buffer_bytes > 0) pcap_set_buffer_size(pcap, buffer_bytes);  // 커널 캡처 버퍼(드롭 마진)

    if (int rc = pcap_activate(pcap); rc != 0) {
        std::cerr << "pcap_activate(" << ifname << ") 실패 (rc=" << rc << "): "
                  << pcap_geterr(pcap) << "\n"
                  << "  -> root 권한과 monitor mode 인터페이스를 확인하세요.\n";
        pcap_close(pcap);
        return nullptr;
    }

    int dlt = pcap_datalink(pcap);
    if (dlt != DLT_IEEE802_11_RADIO) {
        std::cerr << "interface '" << ifname
                  << "' 는 monitor mode (radiotap) 가 아닙니다. "
                  << "current DLT = " << dlt << "\n";
        pcap_close(pcap);
        return nullptr;
    }

    return pcap;
}

bool set_channel(const char* ifname, int channel) {
    const std::string ch = std::to_string(channel);
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execlp("sudo", "sudo", "-n", "iw", "dev", ifname, "set", "channel", ch.c_str(), static_cast<char*>(nullptr));
        _exit(127); //iw 미설치시에 실행됨 (127 == 명령어 못 찾음)
    }
    // iw 가 행(hang)이면 호퍼/종료가 무한히 막히지 않게 최대 대기시간을 둔다(무한 waitpid 금지).
    //   WNOHANG 으로 폴링하다 한계를 넘으면 자식을 SIGKILL 후 (역시 바운드로) 거두고 실패 처리.
    constexpr int kStepMs    = 10;
    constexpr int kTimeoutMs = 2000;   // `iw set channel` 은 보통 즉시 — 2초면 충분
    int status = 0;
    for (int waited = 0; waited < kTimeoutMs; waited += kStepMs) {
        const pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) return WIFEXITED(status) && WEXITSTATUS(status) == 0;   // 정상 수확
        if (r < 0 && errno != EINTR) break;                                   // 수확 오류(EINTR 은 재시도)
        usleep(kStepMs * 1000);                                               // 아직 안 끝남 — 잠깐 자고 다시 폴링
    }
    // 시간 초과 — iw 가 멈춤. 강제 종료 후 좀비 방지로 거두되, 거두기도 바운드(D-state 면 SIGKILL 도 즉시 안 먹힘).
    kill(pid, SIGKILL);
    for (int waited = 0; waited < 200; waited += kStepMs) {
        if (waitpid(pid, &status, WNOHANG) != 0) break;   // 거둠(>0) 또는 오류(-1)
        usleep(kStepMs * 1000);
    }
    std::cerr << "[!] set_channel: iw 가 " << kTimeoutMs << "ms 내 응답 없음 — 강제 종료(채널 " << channel << ")\n";
    return false;
}
