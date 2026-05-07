#include "capture.h"
#include "db.h"
#include "ui.h"

#include <QApplication>
#include <QThread>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

struct CliOpts {
    std::string iface;
    int         rssiThreshold = -20;
    std::string dbPath        = "allowlist.db";
};

void printUsage(const char* prog) {
    std::fprintf(stderr,
                 "usage: %s <iface> [-t <rssi_dbm>] [--db <path>]\n"
                 "  iface         monitor mode interface (e.g. wlan0mon)\n"
                 "  -t <dbm>      RSSI threshold (default -20)\n"
                 "  --db <path>   SQLite DB path (default allowlist.db)\n",
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

    KioskWindow win(&db);
    win.show();

    QThread thread;
    auto* worker = new CaptureWorker();
    worker->configure(QString::fromStdString(opts.iface), opts.rssiThreshold, &db);
    worker->moveToThread(&thread);

    QObject::connect(&thread, &QThread::started,            worker, &CaptureWorker::run);
    QObject::connect(worker,  &CaptureWorker::candidateFound,
                     &win,    &KioskWindow::onCandidateFound);
    QObject::connect(worker,  &CaptureWorker::errorOccurred,
                     &win,    &KioskWindow::onCaptureError);
    QObject::connect(worker,  &CaptureWorker::finished,     &thread, &QThread::quit);
    QObject::connect(&thread, &QThread::finished,           worker,  &QObject::deleteLater);

    thread.start();

    int rc = app.exec();

    worker->requestStop();
    thread.quit();
    thread.wait();

    db.close();
    return rc;
}

} // namespace

int main(int argc, char** argv) {
    CliOpts opts;
    if (!parseCli(argc, argv, opts)) {
        printUsage(argv[0]);
        return 1;
    }
    return runApp(argc, argv, opts);
}