#include "app.h"

#include <filesystem>

App::App(int argc, char* argv[]){
    // /tmp은 재부팅 시 삭제되므로 영구 보존되는 ./var/tmp에 로그를 저장한다.
    const std::filesystem::path log_dir = std::filesystem::current_path() / "var" / "tmp" /"mac-collector-log";
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    if (!ec) {
        FLAGS_log_dir = log_dir.string();
    }

    FLAGS_alsologtostderr = true;
    google::InitGoogleLogging(argv[0]);

    // create_directories 는 InitGoogleLogging 이전이라 그 시점엔 LOG 를 쓸 수 없다.
    // init 직후에 실패 여부를 기록한다 — 실패 시 로그는 기본 위치로 떨어진다.
    if (ec) {
        LOG(WARNING) << "App: 로그 디렉터리 생성 실패 path=" << log_dir.string()
                     << " err=" << ec.message() << " (기본 위치로 대체됨)";
    }

    LOG(INFO) << "mac collector started";
}

App::~App(){
    LOG(INFO) << "mac collector terminated";

}
