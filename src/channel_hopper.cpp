/* channel_hopper.cpp — ChannelHopper 구현.
   전용 스레드가 config.channels 를 순서대로 돈다. 각 채널마다 net.cpp 의 set_channel(iw)을
   불러 채널을 맞추고(=기존 채널 변경 함수 재사용), 지금 채널을 currentChannel 에 공개한다.
   dwell(머무는 시간)만큼 자고 다음 채널로. 채널 변경이 잠깐 실패해도 다음 바퀴에 자동 재시도.
   stop()은 condition_variable 로 dwell 잠을 즉시 깨운다. 종료 신호(running())도 같이 본다. */

#include "channel_hopper.h"
#include "net.h"     // set_channel, running()

#include <sstream>
#include <unordered_set>
#include <glog/logging.h>

namespace {
// 한 채널 dwell 이 끝났는데 공격 버스트가 진행 중이면, 채널을 바로 안 바꾸고
// 이만큼까지만 기다려 준다(버스트가 채널을 가로지르지 않게). 너무 오래 막지 않으려 상한.
constexpr std::chrono::milliseconds kMaxHopDefer{100};

// CA(회사 AP) 채널을 한 바퀴에 더 자주 끼워 넣은 "방문 순서"를 만든다.
// 평범한 채널은 weight=1, CA 채널은 caWeight. 같은 채널이 연달아 나오지 않게 고르게 흩는다
// (smooth weighted round-robin — nginx 가 쓰는 방식). caWeight<=1 이거나 CA 가 없으면 원래 순서.
std::vector<int> buildSchedule(const ChannelHopConfig& cfg) {
    const int weight = cfg.caWeight > 1 ? cfg.caWeight : 1;
    if (cfg.channels.empty() || weight == 1) return cfg.channels;

    const std::unordered_set<int> ca(cfg.caChannels.begin(), cfg.caChannels.end());
    std::vector<int> w;                 // 채널별 가중치(CA=caWeight, 그 외 1)
    int total = 0;
    for (int ch : cfg.channels) {
        const int wi = ca.count(ch) ? weight : 1;
        w.push_back(wi);
        total += wi;
    }
    // smooth WRR: 매번 모든 채널 current 에 제 가중치를 더하고, 가장 큰 채널을 뽑아 total 만큼 깎는다.
    // total 번 뽑으면 가중치 비율대로 고르게 퍼진 한 바퀴가 나온다(예: 1,6,11 + CA 6 x3 → 6,1,6,11,6).
    std::vector<int> current(cfg.channels.size(), 0);
    std::vector<int> schedule;
    schedule.reserve(total);
    for (int n = 0; n < total; ++n) {
        size_t best = 0;
        for (size_t i = 0; i < cfg.channels.size(); ++i) {
            current[i] += w[i];
            if (current[i] > current[best]) best = i;
        }
        current[best] -= total;
        schedule.push_back(cfg.channels[best]);
    }
    return schedule;
}
}  // namespace

ChannelHopper::ChannelHopper(std::string        iface,
                             ChannelHopConfig   config,
                             std::atomic<int>*  currentChannel,
                             std::atomic<bool>* bursting)
    : iface_(std::move(iface)),
      config_(std::move(config)),
      currentChannel_(currentChannel),
      bursting_(bursting) {
    schedule_ = buildSchedule(config_);   // 가중 반영한 방문 순서를 시작 때 1회 계산
}

ChannelHopper::~ChannelHopper() { stop(); }

bool ChannelHopper::start() {
    if (config_.channels.empty()) {
        LOG(ERROR) << "[hopper] 채널 목록이 비어 있어 채널 호핑을 시작하지 않습니다";
        return false;
    }
    if (running_.exchange(true)) return true;   // 이미 돌고 있음 — 여러 번 불러도 OK
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this] { run(); });
    return true;
}

void ChannelHopper::stop() {
    running_.store(false);
    stopCv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

bool ChannelHopper::setChannel(int channel) {
    // 채널 변경은 기존 net.cpp 의 set_channel(fork + `iw dev <iface> set channel N`) 재사용.
    if (set_channel(iface_.c_str(), channel)) {
        if (currentChannel_) currentChannel_->store(channel);   // 지금 채널 공개(공격 스레드가 읽음)
        return true;
    }
    LOG(WARNING) << "[hopper] 채널 " << channel << " 변경 실패 — 다음 바퀴에 자동 재시도 "
                    "('iw dev " << iface_ << " set channel " << channel << "' 수동 확인)";
    return false;
}

void ChannelHopper::sleepOrUntilStop(std::chrono::milliseconds dur) {
    std::unique_lock<std::mutex> lock(stopMtx_);
    // dur 동안 자되, stop()(running_=false)나 종료 신호(running()=false)면 즉시 깨어남.
    stopCv_.wait_for(lock, dur, [this] { return !running_.load() || !running(); });
}

void ChannelHopper::deferWhileBursting() {
    if (!bursting_) return;
    const auto start = std::chrono::steady_clock::now();
    while (running_.load() && running() && bursting_->load() &&
           (std::chrono::steady_clock::now() - start) < kMaxHopDefer) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

namespace {
std::string joinCsv(const std::vector<int>& v) {
    std::ostringstream s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) s << ",";
        s << v[i];
    }
    return s.str();
}
}  // namespace

std::string ChannelHopper::summary() const {
    std::ostringstream oss;
    oss << "채널 [" << joinCsv(config_.channels) << "] 순환 — "
        << config_.dwell.count() << "ms dwell";
    if (config_.caWeight > 1 && !config_.caChannels.empty())
        oss << " · CA 채널 [" << joinCsv(config_.caChannels) << "] x" << config_.caWeight << " 자주 방문";
    return oss.str();
}

void ChannelHopper::run() {
    size_t idx = 0;
    while (running_.load() && running()) {
        setChannel(schedule_[idx]);           // 가중 순서대로 (CA 채널이 더 자주 등장). 실패해도 진행
        sleepOrUntilStop(config_.dwell);      // 이 채널에 머무는 시간
        deferWhileBursting();                 // 버스트 중이면 잠깐 양보 후 채널 변경
        idx = (idx + 1) % schedule_.size();
    }
}
