#include "capture.h"
#include "db.h"
#include "ui.h"
#include "app.h"
#include "api_client.h"
#include "channel_hopper.h"

#include <glog/logging.h>
#include <QApplication>
#include <QMessageBox>
#include <QThread>
#include <csignal>
#include <qfileinfo.h>

namespace {
QApplication* g_app = nullptr;
void signalHandler(int) {
    if (g_app) g_app->quit();
}
} // namespace

int main(int argc, char** argv) {
    setenv("QT_IM_MODULE",  "ibus", 1);
    setenv("XMODIFIERS",    "@im=ibus", 1);
    setenv("GTK_IM_MODULE", "ibus", 1);

    App a(argc, argv);
    QApplication app(argc, argv);
    g_app = &app;
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    SettingsDialog settingsDlg;
    if (settingsDlg.exec() != QDialog::Accepted) {
        LOG(INFO) << "settings dialog canceled by user, exiting rc=0";
        return 0;
    }

    const QString iface   = settingsDlg.iface();
    const QVector<int> channels = settingsDlg.channel();
    const int     rssi    = settingsDlg.rssiThreshold();
    const QString dbPath  = settingsDlg.dbPath();
    LOG(INFO) << "settings accepted iface=" << iface.toStdString()
              << " channels=" << channels.size() << " rssi=" << rssi
              << " dbPath=" << dbPath.toStdString();

    ChannelHopConfig hopCfg;
    hopCfg.channels = std::vector<int>(channels.begin(), channels.end());

    ChannelHopper hopper(iface.toStdString(), hopCfg);
    if(!hopper.start()) {
        QMessageBox::warning(nullptr, "채널 호핑 오류","채널 목록이 비어 있어 채널 호핑이 불가능합니다");
        return 1;
    }
    LOG(INFO) << "channel hopper started: " << hopper.summary();

    Db db;
    LOG(INFO) << "opening DB path=" << QFileInfo(dbPath).absoluteFilePath().toStdString();
    if (!db.open(dbPath.toStdString())) {
        LOG(ERROR) << "DB open failed path=" << dbPath.toStdString();
        QMessageBox::critical(nullptr, "DB 오류",
                              QString("DB 열기 실패: %1").arg(dbPath));
        return 2;
    }

    // ── REST 백엔드 ──
    const QString kApiBaseUrl = qEnvironmentVariable("MACCOLLECTOR_API_URL");
    if (kApiBaseUrl.isEmpty()) {
        LOG(ERROR) << "MACCOLLECTOR_API_URL is empty: REST backend required, aborting";
        QMessageBox::critical(nullptr, "설정 오류",
                              "API URL (MACCOLLECTOR_API_KEY)가 비어 있습니다.\n"
                              "서버 연동이 필요하여 프로그램을 종료합니다.");
        return 3;
    }

    const QString kApiKey = qEnvironmentVariable("MACCOLLECTOR_API_KEY");
    if (kApiKey.isEmpty()) {
        LOG(ERROR) << "MACCOLLECTOR_API_KEY is empty: x-api-key required, aborting";
        QMessageBox::critical(nullptr, "설정 오류",
                              "API 키(환경변수 MACCOLLECTOR_API_KEY)가 비어 있습니다.\n"
                              "서버 인증에 필요하여 프로그램을 종료합니다.");
        return 4;
    }

    // 스택에 생성해 main 종료 시 자동 소멸 → 수동 new/delete 불필요(메모리 누수 방지).
    // db 와 동일한 방식. win 이 api 보다 먼저 소멸하므로 dangling 위험 없음.
    ApiClient api(kApiBaseUrl, kApiKey);
    LOG(INFO) << "ApiClient created baseUrl=" << kApiBaseUrl.toStdString();
    // 서버 우선: 시작 시 등록된 MAC/AP 목록을 1회 동기화
    api.fetchDeviceList();
    api.fetchAPs();

    KioskWindow win(&db, &api);
    win.show();
    LOG(INFO) << "KioskWindow shown";

    QThread   thread;
    auto* worker = new CaptureWorker();
    worker->configure(iface, rssi, channels, &db);
    worker->moveToThread(&thread);
    // 삭제 확정 시 KioskWindow 가 워커의 세션 중복 집합을 비울 수 있도록 주입.
    win.setCaptureWorker(worker);
    LOG(INFO) << "CaptureWorker configured and moved to thread";

    QObject::connect(&thread, &QThread::started,
                     worker,  &CaptureWorker::run);
    QObject::connect(worker,  &CaptureWorker::candidateFound,
                     &win,    &KioskWindow::onCandidateFound);
    QObject::connect(worker,  &CaptureWorker::errorOccurred,
                     &win,    &KioskWindow::onCaptureError);
    QObject::connect(worker,  &CaptureWorker::captureStarted,
                     &win,    &KioskWindow::onCaptureStarted);
    // 오류로 run() 이 반환돼도 스레드는 idle 이벤트 루프로 살아 있다. 재시도 버튼을 누르면
    // (큐 연결로) 워커 스레드에서 run() 이 다시 호출돼 캡처를 재개한다. finished→quit 를
    // 연결하지 않으므로 단발 오류로 스레드가 죽지 않는다.
    QObject::connect(&win,    &KioskWindow::captureRetryRequested,
                     worker,  &CaptureWorker::run);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        LOG(INFO) << "aboutToQuit: stopping capture worker";
        hopper.stop();
        worker->requestStop();
        thread.quit();
        if (!thread.wait(3000)) {
            LOG(WARNING) << "capture thread did not finish in time, terminating";
            thread.terminate();
            thread.wait();
        }
        delete worker;   // 스레드 정지 후 안전하게 해제 (수동 new/delete 누수 방지)
        worker = nullptr;
    });

    thread.start();
    LOG(INFO) << "capture thread started, entering event loop";
    int rc = app.exec();
    LOG(INFO) << "event loop exited rc=" << rc << ", closing DB";
    db.close();
    return rc;
}
