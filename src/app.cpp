#include "app.h"

App::App(int argc, char* argv[]){
    FLAGS_alsologtostderr = true;
    google::InitGoogleLogging(argv[0]);

    LOG(INFO) << "mac collector started";
}

App::~App(){
    LOG(INFO) << "mac collector terminated";

}
