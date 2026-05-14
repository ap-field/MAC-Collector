#include "capture.h"
#include "db.h"
#include "ui.h"
#include "api_client.h"

#include <QApplication>
#include <QThread>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

struct CliOpts {
    std::string iface;
    int         rssiThreshold = -60;
    std::string dbPath        = "MAC_address.db";
    std::string apiBaseUrl    = "http://localhost:8080";
};

void printUsage(const char* prog) {
    std::fprintf(stderr,
                 "usage: %s <iface> [-t <rssi_dbm>] [--db <path>] [--api <url>]\n"
                 "  iface         monitor mode interface (e.g. wlan0mon)\n"
                 "  -t <dbm>      RSSI threshold (default -60)\n"
                 "  --db <path>   SQLite DB path (default MAC_address.db)\n"
                 "  --api <url>   API base URL   (default http://localhost:8080)\n",
                 prog);
}

bool parseCli(int argc, char** argv, CliOpts& out) {
    if (argc < 2) return false;
    int i = 1;
    out.iface = argv[i++];
    while (i < argc) {
        if (std::strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            out.rssiThreshold = std::atoi(argv[i + 1]);
            i += 2;
        } else if (std::strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            out.dbPath = argv[i + 1];
            i += 2;
        } else if (std::strcmp(argv[i], "--api") == 0 && i + 1 < argc) {
            out.apiBaseUrl = argv[i + 1];
            i += 2;
        } else {
            return false;
        }
    }
    return true;
}

int runApp(int argc, char** argv, const CliOpts& opts) {
    QApplication app(argc, argv);

    Db db;
    if (!db.open(opts.dbPath)) {
        std::fprintf(stderr, "DB 열기 실패: %s\n", opts.dbPath.c_str());
        return 2;
    }

    ApiClient api(QString::fromStdString(opts.apiBaseUrl));

    KioskWindow win(&db, &api);
    win.show();

    QThread thread;
    auto* worker = new CaptureWorker();
    worker->configure(QString::fromStdString(opts.iface), opts.rssiThreshold, &db);
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

    thread.start();

    int rc = app.exec();

    // requestStop()이 pcap_breakloop()를 호출하고
    // finished → thread.quit() 이 자동 처리되므로 quit() 중복 제거
    worker->requestStop();
    thread.wait();

    db.close();
    return rc;
}

} // namespace

int main(int argc, char** argv) {
    // ── IME 설정 (QApplication 생성 전 필수) ──────────
    setenv("QT_IM_MODULE",   "ibus", 1);
    setenv("XMODIFIERS",     "@im=ibus", 1);
    setenv("GTK_IM_MODULE",  "ibus", 1);
    // setenv("QT_PLUGIN_PATH",
    //        "/home/kali/Qt/6.11.0/gcc_64/plugins", 1);  // ← 추가

    CliOpts opts;
    if (!parseCli(argc, argv, opts)) {
        printUsage(argv[0]);
        return 1;
    }
    return runApp(argc, argv, opts);
}