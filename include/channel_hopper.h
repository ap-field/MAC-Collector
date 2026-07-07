#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// 채널 호핑 설정 — 어떤 방(채널)들을, 한 방에 얼마나(dwell) 머물지.
// 채널 목록과 dwell 은 CLI(--channels/--dwell-ms)로 바꿀 수 있게 값만 보관(하드코딩 최소화).
struct ChannelHopConfig {
    // 2.4GHz 에서 서로 안 겹치는 대표 채널 3개 — 가장 흔한 기본 순환.
    inline static const std::vector<int> TWO_FOUR_CHANNELS = {1, 6, 11};
    // 2.4GHz 전체(1~13) — 더 촘촘히 보고 싶을 때.
    inline static const std::vector<int> TWO_FOUR_ALL = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    };
    // 5GHz 비-DFS 채널(UNII-1/3) — DFS 는 CAC 대기로 호핑 부적합이라 기본 제외.
    inline static const std::vector<int> FIVE_NON_DFS = {
        36, 40, 44, 48, 149, 153, 157, 161,
    };
    // 2.4GHz 대표 + 5GHz 비-DFS — `--band all` 기본.
    inline static const std::vector<int> ALL_NON_DFS = {
        1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161,
    };

    std::vector<int>          channels = TWO_FOUR_CHANNELS;
    std::chrono::milliseconds dwell    = std::chrono::milliseconds(300);

    // CA(회사 AP) 채널 — 한 바퀴에 caWeight 번씩 방문(나머지 채널은 1번). caWeight<=1 이면 가중 없음.
    std::vector<int>          caChannels;
    int                       caWeight = 1;
};

// 채널 호핑 일꾼 — 전용 스레드 하나가 채널 목록을 순서대로 돈다.
// 채널 전환은 기존 net.cpp 의 set_channel(iw)을 그대로 재사용한다(새 래퍼 안 만듦).
// 지금 맞춘 채널은 currentChannel(atomic)로 공개해, 공격 스레드가 "지금 이 채널에 있는
// STA 만 친다"고 판단하는 데 쓴다(라디오 1개의 물리적 한계 때문).
class ChannelHopper {
public:
    // currentChannel : 지금 맞춘 채널을 여기에 적어 둠(공격 스레드가 읽음). 없으면 nullptr.
    // bursting       : 공격 버스트 진행 중 표시. true 면 채널 바꾸기를 잠깐 미룸
    //                  (버스트 도중 채널이 점프해 엉뚱한 방으로 쏘는 걸 줄임). 없으면 nullptr.
    ChannelHopper(std::string        iface,
                  ChannelHopConfig   config,
                  std::atomic<int>*  currentChannel = nullptr,
                  std::atomic<bool>* bursting        = nullptr);
    ~ChannelHopper();

    ChannelHopper(const ChannelHopper&)            = delete;
    ChannelHopper& operator=(const ChannelHopper&) = delete;

    // 시작 성공 시 true. channels 가 비어 있으면 false(조용한 실패 방지).
    // 이미 돌고 있으면 true(여러 번 불러도 안전).
    bool start();
    void stop();

    std::string summary() const;   // 배너용 한 줄 요약

private:
    void run();
    bool setChannel(int channel);
    void sleepOrUntilStop(std::chrono::milliseconds dur);
    void deferWhileBursting();   // 버스트 끝날 때까지 잠깐(상한 있음) 채널 변경 양보

    std::string               iface_;
    ChannelHopConfig          config_;
    std::vector<int>          schedule_;    // 실제 방문 순서(가중 반영). config_.channels 로부터 1회 생성.
    int                       lastChannel_{-1};  // 직전에 실제로 맞춘 채널 — 값이 바뀔 때만 INFO 로그(호퍼 스레드 전용).
    std::atomic<int>*         currentChannel_;
    std::atomic<bool>*        bursting_;
    std::thread               worker_;
    std::atomic<bool>         running_{false};

    std::mutex                stopMtx_;     // 아래 CV 전용(내부 한정) — Shared::mtx 와 절대 겹쳐 안 잡음
    std::condition_variable   stopCv_;      // dwell 잠을 stop() 때 즉시 깨우는 용도
};
