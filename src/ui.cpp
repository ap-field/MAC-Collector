#include "ui.h"
#include "db.h"
#include "mac.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

// ────────── Phase1Widget ──────────
Phase1Widget::Phase1Widget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* title = new QLabel("Wi-Fi 를 껐다 켜보세요!");
    QFont f = title->font(); f.setPointSize(20); f.setBold(true);
    title->setFont(f); title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({"MAC", "RSSI", "동작"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->verticalHeader()->setDefaultSectionSize(50);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(table_, 1);

    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    adminBtn_ = new QPushButton("관리자 모드");
    adminBtn_->setMinimumSize(140, 44);
    bottom->addWidget(adminBtn_);
    root->addLayout(bottom);

    connect(adminBtn_, &QPushButton::clicked, this, &Phase1Widget::adminRequested);
}

void Phase1Widget::addCandidate(const QString& macStr, int rssi) {
    for (int row = 0; row < table_->rowCount(); ++row) {
        QTableWidgetItem* it = table_->item(row, 0);
        if (it && it->text() == macStr) return;
    }
    int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(macStr));
    table_->setItem(row, 1, new QTableWidgetItem(QString("%1 dBm").arg(rssi)));

    auto* btn = new QPushButton("등록하기");
    btn->setMinimumSize(120, 40);
    btn->setProperty("mac", macStr);
    connect(btn, &QPushButton::clicked, this, &Phase1Widget::onRegisterButtonClicked);
    table_->setCellWidget(row, 2, btn);
}

void Phase1Widget::removeCandidate(const QString& macStr) {
    for (int row = 0; row < table_->rowCount(); ++row) {
        QTableWidgetItem* it = table_->item(row, 0);
        if (it && it->text() == macStr) { table_->removeRow(row); return; }
    }
}

void Phase1Widget::clearCandidates() { table_->setRowCount(0); }

void Phase1Widget::onRegisterButtonClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString mac = btn->property("mac").toString();
    if (!mac.isEmpty()) emit registerRequested(mac);
}

// ────────── Phase2Widget ──────────
Phase2Widget::Phase2Widget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* title = new QLabel("등록 정보 입력");
    QFont f = title->font(); f.setPointSize(18); f.setBold(true);
    title->setFont(f); title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    macLabel_ = new QLabel();
    macLabel_->setStyleSheet("font-size: 16pt; padding: 8px;");
    macLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(macLabel_);

    auto* form = new QFormLayout();
    nameEdit_  = new QLineEdit(); nameEdit_->setMinimumHeight(44);
    phoneEdit_ = new QLineEdit(); phoneEdit_->setMinimumHeight(44);
    typeCombo_ = new QComboBox(); typeCombo_->setMinimumHeight(44);
    typeCombo_->addItems({"notebook", "phone", "tablet", "ap", "iot", "other"});
    form->addRow("이름:",      nameEdit_);
    form->addRow("전화번호:",  phoneEdit_);
    form->addRow("장비 유형:", typeCombo_);
    root->addLayout(form);

    root->addStretch();

    auto* btnRow = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("취소");
    auto* okBtn     = new QPushButton("확인");
    cancelBtn->setMinimumSize(140, 50);
    okBtn->setMinimumSize(140, 50);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    root->addLayout(btnRow);

    connect(okBtn,     &QPushButton::clicked, this, &Phase2Widget::onConfirm);
    connect(cancelBtn, &QPushButton::clicked, this, &Phase2Widget::onCancel);
}

void Phase2Widget::setTargetMac(const QString& macStr) {
    macLabel_->setText("MAC: " + macStr);
    nameEdit_->clear();
    phoneEdit_->clear();
    typeCombo_->setCurrentIndex(0);
}

void Phase2Widget::onConfirm() {
    QString name  = nameEdit_->text().trimmed();
    QString phone = phoneEdit_->text().trimmed();
    QString type  = typeCombo_->currentText();
    QString mac   = macLabel_->text().section(' ', 1).trimmed();

    if (name.isEmpty() || phone.isEmpty()) {
        QMessageBox::warning(this, "입력 오류", "이름과 전화번호를 입력하세요.");
        return;
    }
    emit confirmed(mac, name, phone, type);
}

void Phase2Widget::onCancel() { emit canceled(); }

// ────────── Phase3Widget ──────────
Phase3Widget::Phase3Widget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->addStretch();

    msgLabel_ = new QLabel("등록 완료!");
    QFont f = msgLabel_->font(); f.setPointSize(28); f.setBold(true);
    msgLabel_->setFont(f); msgLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(msgLabel_);

    auto* sub = new QLabel("3초 후 자동으로 돌아갑니다.");
    sub->setAlignment(Qt::AlignCenter);
    root->addWidget(sub);

    root->addStretch();
}

void Phase3Widget::showCompleted() {
    QTimer::singleShot(3000, this, [this]() { emit autoReturn(); });
}

// ────────── AdminPage ──────────
AdminPage::AdminPage(Db* db, QWidget* parent) : QWidget(parent), db_(db) {
    auto* root = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout();
    searchEdit_ = new QLineEdit();
    searchEdit_->setPlaceholderText("MAC / 이름 / 전화번호로 검색");
    searchEdit_->setMinimumHeight(40);
    searchBtn_ = new QPushButton("검색");
    searchBtn_->setMinimumSize(100, 40);
    topRow->addWidget(searchEdit_, 1);
    topRow->addWidget(searchBtn_);
    root->addLayout(topRow);

    tabs_ = new QTabWidget();

    stationTable_ = new QTableWidget(0, 4);
    stationTable_->setHorizontalHeaderLabels({"MAC", "이름", "전화번호", "유형"});
    stationTable_->horizontalHeader()->setStretchLastSection(true);
    stationTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    stationTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabs_->addTab(stationTable_, "Station");

    apTable_ = new QTableWidget(0, 2);
    apTable_->setHorizontalHeaderLabels({"MAC", "Other"});
    apTable_->horizontalHeader()->setStretchLastSection(true);
    apTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    apTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabs_->addTab(apTable_, "AP");

    userTable_ = new QTableWidget(0, 2);
    userTable_->setHorizontalHeaderLabels({"이름", "전화번호"});
    userTable_->horizontalHeader()->setStretchLastSection(true);
    userTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    userTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabs_->addTab(userTable_, "User");

    root->addWidget(tabs_, 1);

    auto* btnRow = new QHBoxLayout();
    deleteBtn_ = new QPushButton("선택 삭제");
    exportBtn_ = new QPushButton("CSV Export");
    backBtn_   = new QPushButton("뒤로");
    deleteBtn_->setMinimumSize(120, 44);
    exportBtn_->setMinimumSize(120, 44);
    backBtn_->setMinimumSize(120, 44);
    btnRow->addWidget(deleteBtn_);
    btnRow->addWidget(exportBtn_);
    btnRow->addStretch();
    btnRow->addWidget(backBtn_);
    root->addLayout(btnRow);

    connect(searchBtn_, &QPushButton::clicked, this, &AdminPage::onSearch);
    connect(deleteBtn_, &QPushButton::clicked, this, &AdminPage::onDeleteSelected);
    connect(exportBtn_, &QPushButton::clicked, this, &AdminPage::onExportCsv);
    connect(backBtn_,   &QPushButton::clicked, this, &AdminPage::onBack);
}

void AdminPage::refresh() {
    reloadStations("");
    reloadAps("");
    reloadUsers();
}

void AdminPage::reloadStations(const QString& keyword) {
    if (!db_) return;
    auto rows = keyword.isEmpty() ? db_->listStations()
                                  : db_->searchStations(keyword.toStdString());
    stationTable_->setRowCount(0);
    for (const auto& s : rows) {
        int r = stationTable_->rowCount();
        stationTable_->insertRow(r);
        stationTable_->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(s.mac.toString())));
        stationTable_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(s.name)));
        stationTable_->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(s.phoneNum)));
        stationTable_->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(Db::typeCodeToString(s.type))));
    }
}

void AdminPage::reloadAps(const QString& keyword) {
    if (!db_) return;
    auto rows = keyword.isEmpty() ? db_->listAps()
                                  : db_->searchAps(keyword.toStdString());
    apTable_->setRowCount(0);
    for (const auto& a : rows) {
        int r = apTable_->rowCount();
        apTable_->insertRow(r);
        apTable_->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(a.mac.toString())));
        apTable_->setItem(r, 1, new QTableWidgetItem(QString::number(a.other)));
    }
}

void AdminPage::reloadUsers() {
    if (!db_) return;
    auto rows = db_->listUsers();
    userTable_->setRowCount(0);
    for (const auto& u : rows) {
        int r = userTable_->rowCount();
        userTable_->insertRow(r);
        userTable_->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(u.name)));
        userTable_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(u.phoneNum)));
    }
}

void AdminPage::onSearch() {
    QString kw = searchEdit_->text().trimmed();
    reloadStations(kw);
    reloadAps(kw);
    reloadUsers();
}

void AdminPage::onDeleteSelected() {
    if (!db_) return;
    QWidget* current = tabs_->currentWidget();
    if (current == stationTable_) {
        auto items = stationTable_->selectedItems();
        if (items.isEmpty()) {
            QMessageBox::information(this, "삭제", "선택된 항목이 없습니다.");
            return;
        }
        int row = items.first()->row();
        QString mac   = stationTable_->item(row, 0)->text();
        QString name  = stationTable_->item(row, 1)->text();
        QString phone = stationTable_->item(row, 2)->text();
        if (QMessageBox::question(this, "삭제 확인",
                                  QString("Station %1 / %2 을(를) 삭제할까요?").arg(mac, name)) != QMessageBox::Yes) return;
        db_->removeStation(Mac(mac.toUtf8().constData()), name.toStdString(), phone.toStdString());
        reloadStations(searchEdit_->text().trimmed());
    } else if (current == apTable_) {
        auto items = apTable_->selectedItems();
        if (items.isEmpty()) {
            QMessageBox::information(this, "삭제", "선택된 항목이 없습니다.");
            return;
        }
        int row = items.first()->row();
        QString mac = apTable_->item(row, 0)->text();
        if (QMessageBox::question(this, "삭제 확인",
                                  QString("AP %1 을(를) 삭제할까요?").arg(mac)) != QMessageBox::Yes) return;
        db_->removeAp(Mac(mac.toUtf8().constData()));
        reloadAps(searchEdit_->text().trimmed());
    } else {
        QMessageBox::information(this, "삭제", "User 직접 삭제는 지원하지 않습니다.");
    }
}

void AdminPage::onExportCsv() {
    if (!db_) return;
    QString path = QFileDialog::getSaveFileName(this, "CSV 저장", "allowlist_export.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;
    bool ok = db_->exportCsv(path.toStdString());
    if (ok) QMessageBox::information(this, "Export", "내보내기 완료: " + path);
    else    QMessageBox::warning(this, "Export", "내보내기 실패");
}

void AdminPage::onBack() { emit backRequested(); }

// ────────── KioskWindow ──────────
KioskWindow::KioskWindow(Db* db, QWidget* parent)
    : QMainWindow(parent), db_(db) {
    setWindowTitle("WIPS Allowlist Collector");
    resize(900, 600);

    stack_ = new QStackedWidget(this);
    setCentralWidget(stack_);

    p1_    = new Phase1Widget();
    p2_    = new Phase2Widget();
    p3_    = new Phase3Widget();
    admin_ = new AdminPage(db_);

    stack_->addWidget(p1_);
    stack_->addWidget(p2_);
    stack_->addWidget(p3_);
    stack_->addWidget(admin_);

    connect(p1_, &Phase1Widget::registerRequested, this, &KioskWindow::goPhase2);
    connect(p1_, &Phase1Widget::adminRequested,    this, &KioskWindow::goAdmin);

    connect(p2_, &Phase2Widget::confirmed, this, &KioskWindow::onPhase2Confirmed);
    connect(p2_, &Phase2Widget::canceled,  this, &KioskWindow::goPhase1);

    connect(p3_, &Phase3Widget::autoReturn, this, &KioskWindow::goPhase1);

    connect(admin_, &AdminPage::backRequested, this, &KioskWindow::goPhase1);

    goPhase1();
}

void KioskWindow::onCandidateFound(QString macStr, int rssi) { p1_->addCandidate(macStr, rssi); }
void KioskWindow::onCaptureError(QString msg) { QMessageBox::critical(this, "캡처 오류", msg); }

void KioskWindow::goPhase1() { stack_->setCurrentWidget(p1_); }
void KioskWindow::goPhase2(QString macStr) {
    p2_->setTargetMac(macStr);
    stack_->setCurrentWidget(p2_);
}
void KioskWindow::goPhase3() {
    stack_->setCurrentWidget(p3_);
    p3_->showCompleted();
}
void KioskWindow::goAdmin() {
    admin_->refresh();
    stack_->setCurrentWidget(admin_);
}

void KioskWindow::onPhase2Confirmed(QString macStr, QString name, QString phone, QString deviceType) {
    if (!db_) {
        QMessageBox::warning(this, "DB 오류", "DB 가 열려있지 않습니다.");
        return;
    }
    Mac mac(macStr.toUtf8().constData());

    bool ok = false;
    if (deviceType == "ap") {
        ApEntry a{mac, 0};
        ok = db_->addAp(a);
    } else {
        StationEntry s{mac, name.toStdString(), phone.toStdString(),
                       Db::typeStringToCode(deviceType.toStdString())};
        ok = db_->addStation(s);
    }

    if (!ok) {
        QMessageBox::warning(this, "등록 실패", "DB INSERT 실패");
        goPhase1();
        return;
    }

    p1_->removeCandidate(macStr);
    goPhase3();
}