#include "capture.h"
#include "db.h"
#include "ui.h"

#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QThread>
#include <csignal>

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

    QApplication app(argc, argv);
    g_app = &app;
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    SettingsDialog settingsDlg;
    if (settingsDlg.exec() != QDialog::Accepted)
        return 0;

    const QString iface   = settingsDlg.iface();
    const int     channel = settingsDlg.channel();
    const int     rssi    = settingsDlg.rssiThreshold();
    const QString dbPath  = settingsDlg.dbPath();

    if (channel > 0) {
        QProcess proc;
        // cap_net_admin은 exec 자식 프로세스에 상속되지 않으므로 sudo -n 사용
        // (setup.sh가 sudoers NOPASSWD 규칙을 추가하므로 비밀번호 불필요)
        proc.start("sudo", {"-n", "iwconfig", iface, "channel", QString::number(channel)});
        if (!proc.waitForFinished(3000)) {
            QMessageBox::warning(nullptr, "채널 설정 실패",
                                 QString("iwconfig %1 channel %2 실패\n"
                                         "모니터 모드 및 권한을 확인하세요.")
                                     .arg(iface).arg(channel));
        }
    }

    Db db;
    if (!db.open(dbPath.toStdString())) {
        QMessageBox::critical(nullptr, "DB 오류",
                              QString("DB 열기 실패: %1").arg(dbPath));
        return 2;
    }

    KioskWindow win(&db);
    win.show();

    QThread   thread;
    auto* worker = new CaptureWorker();
    worker->configure(iface, rssi, &db);
    worker->moveToThread(&thread);

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
        worker->requestStop();
        thread.quit();
        if (!thread.wait(3000)) {
            thread.terminate();
            thread.wait();
        }
    });

    thread.start();
    int rc = app.exec();
    db.close();
    return rc;
}