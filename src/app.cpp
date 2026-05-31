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

    LOG(INFO) << "mac collector started";
}

App::~App(){
    LOG(INFO) << "mac collector terminated";

}
