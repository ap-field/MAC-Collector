#include "app.h"

#include <filesystem>

App::App(int argc, char* argv[]){
    // /tmp은 재부팅 시 삭제되므로 영구 보존되는 ./log에 로그를 저장한다.
    const std::filesystem::path log_dir = std::filesystem::current_path() / "log" ;
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    if (!ec) {
        FLAGS_log_dir = log_dir.string();
    }

    FLAGS_alsologtostderr = true;
    google::InitGoogleLogging(argv[0]);

    if (ec) {
        LOG(WARNING) << "App: 로그 디렉터리 생성 실패 path=" << log_dir.string()
                     << " err=" << ec.message() << " (기본 위치로 대체됨)";
    }

    LOG(INFO) << "mac collector started";
}

App::~App(){
    LOG(INFO) << "mac collector terminated";

}
