#include "capture.h"
#include "db.h"
#include "ui.h"
#include "app.h"
#include "api_client.h"

#include <glog/logging.h>
#include <QApplication>
#include <QMessageBox>
#include <QProcess>
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
    const int     channel = settingsDlg.channel();
    const int     rssi    = settingsDlg.rssiThreshold();
    const QString dbPath  = settingsDlg.dbPath();
    LOG(INFO) << "settings accepted iface=" << iface.toStdString()
              << " channel=" << channel << " rssi=" << rssi
              << " dbPath=" << dbPath.toStdString();

    if (channel > 0) {
        LOG(INFO) << "setting channel via iwconfig iface=" << iface.toStdString()
                  << " channel=" << channel;
        QProcess proc;
        // cap_net_admin은 exec 자식 프로세스에 상속되지 않으므로 sudo -n 사용
        // (setup.sh가 sudoers NOPASSWD 규칙을 추가하므로 비밀번호 불필요)
        proc.start("sudo", {"-n", "iwconfig", iface, "channel", QString::number(channel)});
        if (!proc.waitForFinished(3000)) {
            LOG(ERROR) << "iwconfig channel set failed iface=" << iface.toStdString()
                       << " channel=" << channel;
            QMessageBox::warning(nullptr, "채널 설정 실패",
                                 QString("iwconfig %1 channel %2 실패\n"
                                         "모니터 모드 및 권한을 확인하세요.")
                                     .arg(iface).arg(channel));
        }
    }

    Db db;
    qDebug() << "[DEBUG] DB absolute path:" << QFileInfo(dbPath).absoluteFilePath();
    LOG(INFO) << "opening DB path=" << QFileInfo(dbPath).absoluteFilePath().toStdString();
    if (!db.open(dbPath.toStdString())) {
        LOG(ERROR) << "DB open failed path=" << dbPath.toStdString();
        QMessageBox::critical(nullptr, "DB 오류",
                              QString("DB 열기 실패: %1").arg(dbPath));
        return 2;
    }

    // ── REST 백엔드 ──
    // TODO: 여기에 .com 서버 주소를 입력하세요 (예: "https://api.example.com")
    //       비워두면 ApiClient 없이 로컬 DB 전용으로 동작합니다.
    const QString kApiBaseUrl = "https://ap-field.com";
    // 서버 연동은 필수. 주소가 비어 있으면 로컬 전용으로 계속하지 않고 즉시 종료한다.
    if (kApiBaseUrl.isEmpty()) {
        LOG(ERROR) << "kApiBaseUrl is empty: REST backend required, aborting";
        QMessageBox::critical(nullptr, "설정 오류",
                              "서버 주소(kApiBaseUrl)가 비어 있습니다.\n"
                              "REST 백엔드 연동이 필요하여 프로그램을 종료합니다.");
        return 3;
    }

    ApiClient* api = new ApiClient(kApiBaseUrl);
    LOG(INFO) << "ApiClient created baseUrl=" << kApiBaseUrl.toStdString();
    // 서버 우선: 시작 시 등록된 MAC 목록을 1회 동기화
    api->fetchDeviceList();

    KioskWindow win(&db, api);
    win.show();
    LOG(INFO) << "KioskWindow shown";

    QThread   thread;
    auto* worker = new CaptureWorker();
    worker->configure(iface, rssi, &db);
    worker->moveToThread(&thread);
    LOG(INFO) << "CaptureWorker configured and moved to thread";

    QObject::connect(&thread, &QThread::started,
                     worker,  &CaptureWorker::run);
    QObject::connect(worker,  &CaptureWorker::candidateFound,
                     &win,    &KioskWindow::onCandidateFound);
    QObject::connect(worker,  &CaptureWorker::errorOccurred,
                     &win,    &KioskWindow::onCaptureError);
    QObject::connect(worker,  &CaptureWorker::finished,
                     &thread, &QThread::quit);
    QObject::connect(&thread, &QThread::finished,
                     worker,  &QObject::deleteLater);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        LOG(INFO) << "aboutToQuit: stopping capture worker";
        worker->requestStop();
        thread.quit();
        if (!thread.wait(3000)) {
            LOG(WARNING) << "capture thread did not finish in time, terminating";
            thread.terminate();
            thread.wait();
        }
    });

    thread.start();
    LOG(INFO) << "capture thread started, entering event loop";
    int rc = app.exec();
    LOG(INFO) << "event loop exited rc=" << rc << ", closing DB";
    db.close();
    return rc;
}
