#include "ui.h"
#include "db.h"
#include "mac.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
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
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* imDummy = new QLineEdit(this);
    imDummy->setFixedSize(0, 0);
    imDummy->setAttribute(Qt::WA_InputMethodEnabled, true);
    imDummy->setInputMethodHints(Qt::ImhNone);
    imDummy->hide();  // 숨기되 삭제하지 않음

    //*/ ────────── ✅ 한글 입력 테스트 패널 추가 ──────────
    auto* testPanel = new QWidget(this);
    testPanel->setStyleSheet(
        "background: #EFF6FF;"
        "border-bottom: 2px solid #BFDBFE;"
        );
    auto* testLayout = new QHBoxLayout(testPanel);
    testLayout->setContentsMargins(16, 10, 16, 10);
    testLayout->setSpacing(10);

    auto* testLabel = new QLabel("🇰🇷 한글 입력 테스트:");
    testLabel->setStyleSheet("color: #1D4ED8; font-size: 11pt; font-weight: 700; background: transparent;");
    testLayout->addWidget(testLabel);

    testEdit_ = new QLineEdit();
    testEdit_->setPlaceholderText("홍길동");
    testEdit_->setMinimumHeight(38);
    testEdit_->setStyleSheet(
        "QLineEdit {"
        "  background: white; color: #1F2937;"
        "  border: 1.5px solid #93C5FD; border-radius: 4px;"
        "  padding: 6px 12px; font-size: 12pt;"
        "}"
        "QLineEdit:focus {"
        "  border: 2px solid #2563EB;"
        "}"
        );
    testLayout->addWidget(testEdit_, 1);

    testResultLabel_ = new QLabel("입력값: -");
    testResultLabel_->setMinimumWidth(200);
    testResultLabel_->setStyleSheet(
        "color: #1F2937; font-size: 11pt; background: transparent; padding: 0 8px;"
        );
    testLayout->addWidget(testResultLabel_);

    auto* clearBtn = new QPushButton("지우기");
    clearBtn->setMinimumSize(80, 36);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 4px; padding: 6px 14px;"
        "  font-size: 10pt; font-weight: 600; }"
        "QPushButton:hover { background: #1D4ED8; }"
        );
    testLayout->addWidget(clearBtn);

    root->addWidget(testPanel);
    // ────────── 테스트 패널 끝 ──────────*/

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"MAC 주소", "제조사", "신호", ""});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setFixedHeight(36);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(54);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setStyleSheet(
        "QTableWidget { background: white; border: none; gridline-color: transparent; }"
        "QHeaderView::section { background: #F3F4F6; color: #374151;"
        "  padding: 8px 12px; border: none;"
        "  border-bottom: 1px solid #E5E7EB;"
        "  font-weight: 700; font-size: 10pt; }"
        "QTableWidget::item { padding: 8px 12px;"
        "  border-bottom: 1px solid #F3F4F6; }"
        "QTableWidget::item:hover { background: #F9FAFB; }"
        );

    root->addWidget(table_);
}

void Phase1Widget::addCandidate(const QString& macStr, int rssi) {
    for (int row = 0; row < table_->rowCount(); ++row) {
        QTableWidgetItem* it = table_->item(row, 0);
        if (it && it->text() == macStr) return;
    }
    int row = table_->rowCount();
    table_->insertRow(row);

    // MAC (monospace, bold)
    auto* macItem = new QTableWidgetItem(macStr);
    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(12);
    mono.setBold(true);
    macItem->setFont(mono);
    macItem->setForeground(QColor("#1F2937"));
    table_->setItem(row, 0, macItem);

    // Vendor (하드코딩 — 추후 OUI 매핑으로 교체)
    auto* vendorItem = new QTableWidgetItem("Samsung Electronics");
    vendorItem->setForeground(QColor("#4B5563"));
    table_->setItem(row, 1, vendorItem);

    // RSSI (색상 코딩)
    auto* rssiItem = new QTableWidgetItem(QString("%1 dBm").arg(rssi));
    QFont rssiFont = rssiItem->font();
    rssiFont.setBold(true);
    rssiItem->setFont(rssiFont);
    if (rssi >= -50)      rssiItem->setForeground(QColor("#10B981")); // 강
    else if (rssi >= -75) rssiItem->setForeground(QColor("#F59E0B")); // 중
    else                  rssiItem->setForeground(QColor("#6B7280")); // 약
    table_->setItem(row, 2, rssiItem);

    // 등록 버튼
    auto* btn = new QPushButton("등록");
    btn->setMinimumSize(80, 36);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("mac", macStr);
    btn->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 4px; padding: 6px 18px;"
        "  font-weight: 600; font-size: 11pt; }"
        "QPushButton:hover { background: #1D4ED8; }"
        "QPushButton:pressed { background: #1E40AF; }"
        );
    connect(btn, &QPushButton::clicked, this, &Phase1Widget::onRegisterButtonClicked);
    table_->setCellWidget(row, 3, btn);

    emit candidateCountChanged(table_->rowCount());
}

void Phase1Widget::removeCandidate(const QString& macStr) {
    for (int row = 0; row < table_->rowCount(); ++row) {
        QTableWidgetItem* it = table_->item(row, 0);
        if (it && it->text() == macStr) {
            table_->removeRow(row);
            emit candidateCountChanged(table_->rowCount());
            return;
        }
    }
}

void Phase1Widget::clearCandidates() {
    table_->setRowCount(0);
    emit candidateCountChanged(0);
}

int Phase1Widget::candidateCount() const {
    return table_->rowCount();
}

void Phase1Widget::onRegisterButtonClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString mac = btn->property("mac").toString();
    if (!mac.isEmpty()) emit registerRequested(mac);
}

// ────────── Phase2Widget ──────────
Phase2Widget::Phase2Widget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 30, 40, 30);

    macLabel_ = new QLabel();
    macLabel_->setAlignment(Qt::AlignCenter);
    macLabel_->setStyleSheet(
        "background: #F3F4F6; color: #1F2937;"
        "font-family: monospace; font-size: 16pt; font-weight: 600;"
        "padding: 14px; border-radius: 6px;"
        );
    root->addWidget(macLabel_);
    root->addSpacing(20);

    auto* form = new QFormLayout();
    form->setSpacing(14);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    const char* inputStyle =
        "QLineEdit { padding: 8px 12px; font-size: 14pt;"
        "  border: 1px solid #D1D5DB; border-radius: 4px;"
        "  background: white; color: #1F2937; }"
        "QLineEdit:focus { border: 2px solid #2563EB; }";

    nameEdit_  = new QLineEdit();
    phoneEdit_ = new QLineEdit();
    typeCombo_ = new QComboBox();

    // ── 명시적 IM 활성화 속성 부여 ──────────────────
    nameEdit_->setAttribute(Qt::WA_InputMethodEnabled, true);
    phoneEdit_->setAttribute(Qt::WA_InputMethodEnabled, true);

    // inputMethodHints 초기화 (한글 차단 힌트 제거)
    nameEdit_->setInputMethodHints(Qt::ImhNone);
    phoneEdit_->setInputMethodHints(Qt::ImhNone);

    nameEdit_->setMinimumHeight(44);
    phoneEdit_->setMinimumHeight(44);
    typeCombo_->setMinimumHeight(44);

    nameEdit_->setStyleSheet(inputStyle);
    phoneEdit_->setStyleSheet(inputStyle);
    typeCombo_->setStyleSheet(
        "QComboBox { padding: 8px 12px; font-size: 14pt;"
        "  border: 1px solid #000000; border-radius: 4px; background: white; }"
        );
    typeCombo_->addItems({"notebook", "phone", "tablet", "ap", "iot", "other"});

    auto* nameLbl  = new QLabel("이름:");
    auto* phoneLbl = new QLabel("전화번호:");
    auto* typeLbl  = new QLabel("장비 유형:");
    const char* lblStyle = "font-size: 12pt; font-weight: 600; color: #1F2937;";
    nameLbl->setStyleSheet(lblStyle);
    phoneLbl->setStyleSheet(lblStyle);
    typeLbl->setStyleSheet(lblStyle);

    form->addRow(nameLbl,  nameEdit_);
    form->addRow(phoneLbl, phoneEdit_);
    form->addRow(typeLbl,  typeCombo_);

    root->addLayout(form);
    root->addStretch();

    auto* btnRow = new QHBoxLayout();
    cancelBtn_ = new QPushButton("취소");
    okBtn_     = new QPushButton("확인");
    cancelBtn_->setMinimumSize(140, 50);
    okBtn_->setMinimumSize(140, 50);
    cancelBtn_->setCursor(Qt::PointingHandCursor);
    okBtn_->setCursor(Qt::PointingHandCursor);

    cancelBtn_->setStyleSheet(
        "QPushButton { background: white; color: #4B5563;"
        "  border: 1px solid #D1D5DB; border-radius: 6px;"
        "  padding: 10px 24px; font-size: 13pt; font-weight: 600; }"
        "QPushButton:hover { background: #F3F4F6; }"
        );
    okBtn_->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 6px; padding: 10px 24px;"
        "  font-size: 13pt; font-weight: 700; }"
        "QPushButton:hover { background: #1D4ED8; }"
        "QPushButton:pressed { background: #1E40AF; }"
        );
    btnRow->addWidget(cancelBtn_);
    btnRow->addWidget(okBtn_);
    root->addLayout(btnRow);

    // Tab 순서
    setTabOrder(nameEdit_, phoneEdit_);
    setTabOrder(phoneEdit_, typeCombo_);
    setTabOrder(typeCombo_, okBtn_);
    setTabOrder(okBtn_, cancelBtn_);

    connect(okBtn_,     &QPushButton::clicked, this, &Phase2Widget::onConfirm);
    connect(cancelBtn_, &QPushButton::clicked, this, &Phase2Widget::onCancel);

    // Enter 처리: 이름→전화번호 포커스, 전화번호 Enter→확인
    connect(nameEdit_,  &QLineEdit::returnPressed, phoneEdit_, qOverload<>(&QWidget::setFocus));
    connect(phoneEdit_, &QLineEdit::returnPressed, this,       &Phase2Widget::onConfirm);
}

void Phase2Widget::setTargetMac(const QString& macStr) {
    macLabel_->setText("MAC: " + macStr);
    nameEdit_->clear();
    phoneEdit_->clear();
    typeCombo_->setCurrentIndex(0);
}

void Phase2Widget::focusFirstInput() {
    nameEdit_->setFocus();
    nameEdit_->selectAll();
    nameEdit_->setCursorPosition(0);
}

void Phase2Widget::onConfirm() {
    QString name  = nameEdit_->text().trimmed();
    QString phone = phoneEdit_->text().trimmed();
    QString type  = typeCombo_->currentText();
    QString mac   = macLabel_->text().section(' ', 1).trimmed();

    if (name.isEmpty() || phone.isEmpty()) {
        QMessageBox::warning(this, "입력 오류", "이름과 전화번호를 입력하세요.");
        if (name.isEmpty())  nameEdit_->setFocus();
        else                 phoneEdit_->setFocus();
        return;
    }
    emit confirmed(mac, name, phone, type);
}

void Phase2Widget::onCancel() { emit canceled(); }

// ────────── Phase3Widget ──────────
Phase3Widget::Phase3Widget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->addStretch();

    auto* check = new QLabel("✓");
    QFont cf = check->font();
    cf.setPointSize(80);
    cf.setBold(true);
    check->setFont(cf);
    check->setAlignment(Qt::AlignCenter);
    check->setStyleSheet("color: #10B981;");
    root->addWidget(check);

    msgLabel_ = new QLabel("성공적으로 등록되었습니다");
    QFont mf = msgLabel_->font();
    mf.setPointSize(20);
    mf.setBold(true);
    msgLabel_->setFont(mf);
    msgLabel_->setAlignment(Qt::AlignCenter);
    msgLabel_->setStyleSheet("color: #1F2937; padding: 16px;");
    root->addWidget(msgLabel_);

    auto* sub = new QLabel("3초 후 자동으로 돌아갑니다");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color: #6B7280; font-size: 12pt;");
    root->addWidget(sub);

    root->addStretch();
}

void Phase3Widget::showCompleted() {
    QTimer::singleShot(3000, this, [this]() { emit autoReturn(); });
}

// ────────── AdminPage ──────────
AdminPage::AdminPage(Db* db, QWidget* parent) : QWidget(parent), db_(db) {
    // 페이지 자체 배경 + 자식 위젯들이 안 닿을 글로벌 디폴트
    setObjectName("adminPage");
    setStyleSheet(
        "#adminPage { background: white; }"
        "QLabel     { color: #000000; }"
        "QLineEdit  { background: white; color: #000000;"
        "             border: 1px solid #D1D5DB; border-radius: 4px;"
        "             padding: 6px 10px; }"
        "QLineEdit:focus { border: 2px solid #2563EB; }"
        "QTableWidget { background: white; color: #000000; border: none;"
        "               gridline-color: #F3F4F6; }"
        "QTableWidget::item          { color: #000000; padding: 8px 10px;"
        "                              border-bottom: 1px solid #F3F4F6; }"
        "QTableWidget::item:selected { background: #DBEAFE; color: #1E3A8A; }"
        "QHeaderView::section { background: #F3F4F6; color: #000000;"
        "                       padding: 8px 10px; border: none;"
        "                       border-bottom: 1px solid #E5E7EB;"
        "                       font-weight: 700; font-size: 10pt; }"
        "QTabWidget::pane { background: white; border: 1px solid #E5E7EB;"
        "                   border-radius: 4px; top: -1px; }"
        "QTabBar::tab   { background: #F3F4F6; color: #000000;"
        "                 padding: 10px 22px;"
        "                 border: 1px solid #E5E7EB; border-bottom: none;"
        "                 border-top-left-radius: 4px;"
        "                 border-top-right-radius: 4px;"
        "                 font-size: 11pt; font-weight: 500;"
        "                 margin-right: 2px; }"
        "QTabBar::tab:selected { background: white; font-weight: 700; }"
        "QTabBar::tab:hover:!selected { background: #E5E7EB; }"
        );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // ─── 검색 영역 ───
    auto* topRow = new QHBoxLayout();
    searchEdit_ = new QLineEdit();
    searchEdit_->setPlaceholderText("MAC / 이름 / 전화번호로 검색");
    searchEdit_->setMinimumHeight(40);

    searchBtn_ = new QPushButton("검색");
    searchBtn_->setMinimumSize(100, 40);
    searchBtn_->setCursor(Qt::PointingHandCursor);
    searchBtn_->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 4px; padding: 8px 18px;"
        "  font-size: 11pt; font-weight: 600; }"
        "QPushButton:hover  { background: #1D4ED8; }"
        "QPushButton:pressed{ background: #1E40AF; }"
        );

    topRow->addWidget(searchEdit_, 1);
    topRow->addWidget(searchBtn_);
    root->addLayout(topRow);

    // ─── 탭 + 테이블 ───
    tabs_ = new QTabWidget();

    stationTable_ = new QTableWidget(0, 4);
    stationTable_->setHorizontalHeaderLabels({"MAC", "이름", "전화번호", "유형"});
    stationTable_->horizontalHeader()->setStretchLastSection(true);
    stationTable_->horizontalHeader()->setHighlightSections(false);
    stationTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    stationTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stationTable_->setShowGrid(false);
    stationTable_->verticalHeader()->setVisible(false);
    tabs_->addTab(stationTable_, "Station");

    apTable_ = new QTableWidget(0, 2);
    apTable_->setHorizontalHeaderLabels({"MAC", "Other"});
    apTable_->horizontalHeader()->setStretchLastSection(true);
    apTable_->horizontalHeader()->setHighlightSections(false);
    apTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    apTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    apTable_->setShowGrid(false);
    apTable_->verticalHeader()->setVisible(false);
    tabs_->addTab(apTable_, "AP");

    userTable_ = new QTableWidget(0, 2);
    userTable_->setHorizontalHeaderLabels({"이름", "전화번호"});
    userTable_->horizontalHeader()->setStretchLastSection(true);
    userTable_->horizontalHeader()->setHighlightSections(false);
    userTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    userTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    userTable_->setShowGrid(false);
    userTable_->verticalHeader()->setVisible(false);
    tabs_->addTab(userTable_, "User");

    root->addWidget(tabs_, 1);

    // ─── 하단 버튼 ───
    auto* btnRow = new QHBoxLayout();
    deleteBtn_ = new QPushButton("선택 삭제");
    backBtn_   = new QPushButton("← 뒤로");
    deleteBtn_->setMinimumSize(120, 44);
    //exportBtn_->setMinimumSize(120, 44);
    backBtn_->setMinimumSize(120, 44);
    deleteBtn_->setCursor(Qt::PointingHandCursor);
    //exportBtn_->setCursor(Qt::PointingHandCursor);
    backBtn_->setCursor(Qt::PointingHandCursor);

    const char* secondaryBtn =
        "QPushButton { background: white; color: #000000;"
        "  border: 1px solid #D1D5DB; border-radius: 4px;"
        "  padding: 8px 18px; font-size: 11pt; font-weight: 600; }"
        "QPushButton:hover   { background: #F3F4F6; border-color: #9CA3AF; }"
        "QPushButton:pressed { background: #E5E7EB; }";

    const char* dangerBtn =
        "QPushButton { background: white; color: #DC2626;"
        "  border: 1px solid #FCA5A5; border-radius: 4px;"
        "  padding: 8px 18px; font-size: 11pt; font-weight: 600; }"
        "QPushButton:hover   { background: #FEF2F2; border-color: #DC2626; }"
        "QPushButton:pressed { background: #FEE2E2; }";

    deleteBtn_->setStyleSheet(dangerBtn);
    //exportBtn_->setStyleSheet(secondaryBtn);
    backBtn_->setStyleSheet(secondaryBtn);

    btnRow->addWidget(deleteBtn_);
    //btnRow->addWidget(exportBtn_);
    btnRow->addStretch();
    btnRow->addWidget(backBtn_);
    root->addLayout(btnRow);

    connect(searchBtn_,  &QPushButton::clicked,     this, &AdminPage::onSearch);
    connect(searchEdit_, &QLineEdit::returnPressed, this, &AdminPage::onSearch);
    connect(deleteBtn_,  &QPushButton::clicked,     this, &AdminPage::onDeleteSelected);
    connect(backBtn_,    &QPushButton::clicked,     this, &AdminPage::onBack);
}

void AdminPage::refresh() {
    reloadStations("");
    reloadAps("");
    reloadUsers();
}

void AdminPage::focusSearch() {
    searchEdit_->setFocus();
    searchEdit_->selectAll();
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

void AdminPage::onBack() { emit backRequested(); }

// ────────── KioskWindow ──────────
KioskWindow::KioskWindow(Db* db, QWidget* parent)
    : QMainWindow(parent), db_(db) {
    setWindowTitle("MAC Address Collector");
    resize(960, 640);

    // 메인 컨테이너
    auto* central = new QWidget(this);
    central->setStyleSheet("background: white;");
    setCentralWidget(central);

    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 헤더
    buildHeader();
    mainLayout->addWidget(header_);

    // 스택
    stack_ = new QStackedWidget(central);
    p1_    = new Phase1Widget();
    p2_    = new Phase2Widget();
    p3_    = new Phase3Widget();
    admin_ = new AdminPage(db_);
    stack_->addWidget(p1_);
    stack_->addWidget(p2_);
    stack_->addWidget(p3_);
    stack_->addWidget(admin_);
    mainLayout->addWidget(stack_, 1);

    // 상태바
    buildStatusBar();
    mainLayout->addWidget(statusBar_);

    // 시그널 연결
    connect(p1_, &Phase1Widget::registerRequested,     this, &KioskWindow::goPhase2);
    connect(p1_, &Phase1Widget::candidateCountChanged, this, &KioskWindow::updateDeviceCount);

    connect(p2_, &Phase2Widget::confirmed, this, &KioskWindow::onPhase2Confirmed);
    connect(p2_, &Phase2Widget::canceled,  this, &KioskWindow::goPhase1);

    connect(p3_, &Phase3Widget::autoReturn, this, &KioskWindow::goPhase1);

    connect(admin_, &AdminPage::backRequested, this, &KioskWindow::goPhase1);

    // 타이머
    elapsedTimer_ = new QTimer(this);
    elapsedTimer_->setInterval(1000);
    connect(elapsedTimer_, &QTimer::timeout, this, &KioskWindow::updateElapsed);
    elapsedTimer_->start();

    goPhase1();
}

void KioskWindow::makePhaseStep(QWidget*& wOut, QLabel*& circleOut, QLabel*& textOut,
                                const QString& numText, const QString& labelText) {
    wOut = new QWidget();
    auto* l = new QHBoxLayout(wOut);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);

    circleOut = new QLabel(numText);
    circleOut->setFixedSize(28, 28);
    circleOut->setAlignment(Qt::AlignCenter);

    textOut = new QLabel(labelText);

    l->addWidget(circleOut);
    l->addWidget(textOut);
}

void KioskWindow::buildHeader() {
    header_ = new QWidget();
    auto* headerLayout = new QVBoxLayout(header_);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    // 1) 타이틀 밴드
    titleLabel_ = new QLabel("Wi-Fi 를 껐다 켜보세요!");
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet(
        "background: white; color: #1F2937;"
        "font-size: 20pt; font-weight: 700;"
        "padding: 18px 14px; border-bottom: 1px solid #F3F4F6;"
        );
    headerLayout->addWidget(titleLabel_);

    // 2) 페이즈 인디케이터 + Scanning 밴드
    auto* phaseBand = new QWidget();
    phaseBand->setStyleSheet("background: #F9FAFB; border-bottom: 1px solid #E5E7EB;");
    auto* phaseLayout = new QHBoxLayout(phaseBand);
    phaseLayout->setContentsMargins(20, 10, 20, 10);
    phaseLayout->setSpacing(0);

    makePhaseStep(phaseStep1_, phaseStep1Num_, phaseStep1Text_, "1", "검색");
    makePhaseStep(phaseStep2_, phaseStep2Num_, phaseStep2Text_, "2", "등록");
    makePhaseStep(phaseStep3_, phaseStep3Num_, phaseStep3Text_, "3", "완료");

    auto makeConnector = []() {
        auto* line = new QFrame();
        line->setFixedSize(32, 1);
        line->setStyleSheet("background: #D1D5DB;");
        return line;
    };

    phaseLayout->addWidget(phaseStep1_);
    phaseLayout->addSpacing(8);
    phaseLayout->addWidget(makeConnector(), 0, Qt::AlignVCenter);
    phaseLayout->addSpacing(8);
    phaseLayout->addWidget(phaseStep2_);
    phaseLayout->addSpacing(8);
    phaseLayout->addWidget(makeConnector(), 0, Qt::AlignVCenter);
    phaseLayout->addSpacing(8);
    phaseLayout->addWidget(phaseStep3_);
    phaseLayout->addStretch();

    // scanningLabel_ = new QLabel("● Scanning");
    // scanningLabel_->setStyleSheet("color: #10B981; font-weight: 600; font-size: 11pt;");
    //phaseLayout->addWidget(scanningLabel_);

    headerLayout->addWidget(phaseBand);
}

void KioskWindow::buildStatusBar() {
    statusBar_ = new QWidget();
    statusBar_->setStyleSheet("background: #F3F4F6; border-top: 1px solid #E5E7EB;");
    auto* sb = new QHBoxLayout(statusBar_);
    sb->setContentsMargins(16, 8, 16, 8);

    const char* sbStyle = "color: #6B7280; font-size: 10pt;";

    scanStatusLabel_ = new QLabel("📡 스캔 중...");
    scanStatusLabel_->setStyleSheet(sbStyle);
    sb->addWidget(scanStatusLabel_);

    sb->addSpacing(20);

    deviceCountLabel_ = new QLabel("0 devices");
    deviceCountLabel_->setStyleSheet(sbStyle);
    sb->addWidget(deviceCountLabel_);

    sb->addSpacing(20);

    elapsedLabel_ = new QLabel("00:00");
    elapsedLabel_->setStyleSheet("color: #6B7280; font-size: 10pt; font-family: monospace;");
    sb->addWidget(elapsedLabel_);

    sb->addStretch();

    adminBtn_ = new QPushButton("⚙ 관리자");
    adminBtn_->setMinimumSize(110, 32);
    adminBtn_->setCursor(Qt::PointingHandCursor);
    adminBtn_->setStyleSheet(
        "QPushButton { background: white; color: #4B5563;"
        "  border: 1px solid #000000; border-radius: 4px;"
        "  padding: 6px 14px; font-size: 10pt; font-weight: 500; }"
        "QPushButton:hover { background: #EFF6FF; border-color: #93C5FD; color: #2563EB; }"
        );
    connect(adminBtn_, &QPushButton::clicked, this, &KioskWindow::goAdmin);
    sb->addWidget(adminBtn_);
}

void KioskWindow::updatePhaseIndicator(int activeStep) {
    auto apply = [](QLabel* circle, QLabel* text,
                    const QString& circleText, const QString& state) {
        circle->setText(circleText);
        if (state == "active") {
            circle->setStyleSheet(
                "background: #2563EB; color: white;"
                "border-radius: 14px; font-weight: 700; font-size: 11pt;"
                );
            text->setStyleSheet("color: #1F2937; font-weight: 600; font-size: 11pt;");
        } else if (state == "done") {
            circle->setStyleSheet(
                "background: #10B981; color: white;"
                "border-radius: 14px; font-weight: 700; font-size: 11pt;"
                );
            text->setStyleSheet("color: #1F2937; font-weight: 600; font-size: 11pt;");
        } else { // inactive
            circle->setStyleSheet(
                "background: white; color: #000000;"
                "border: 1.5px solid #000000; border-radius: 14px;"
                "font-weight: 600; font-size: 11pt;"
                );
            text->setStyleSheet("color: #000000; font-size: 11pt;");
        }
    };

    if (activeStep == 1) {
        apply(phaseStep1Num_, phaseStep1Text_, "1", "active");
        apply(phaseStep2Num_, phaseStep2Text_, "2", "inactive");
        apply(phaseStep3Num_, phaseStep3Text_, "3", "inactive");
    } else if (activeStep == 2) {
        apply(phaseStep1Num_, phaseStep1Text_, "✓", "done");
        apply(phaseStep2Num_, phaseStep2Text_, "2", "active");
        apply(phaseStep3Num_, phaseStep3Text_, "3", "inactive");
    } else if (activeStep == 3) {
        apply(phaseStep1Num_, phaseStep1Text_, "✓", "done");
        apply(phaseStep2Num_, phaseStep2Text_, "✓", "done");
        apply(phaseStep3Num_, phaseStep3Text_, "3", "active");
    }
}

void KioskWindow::setActivePhase(int phase) {
    updatePhaseIndicator(phase);
    switch (phase) {
    case 1: titleLabel_->setText("Wi-Fi 를 껐다 켜보세요!"); break;
    case 2: titleLabel_->setText("등록 정보 입력");          break;
    case 3: titleLabel_->setText("등록 완료!");              break;
    }
}

void KioskWindow::setChromeVisible(bool visible) {
    if (header_)    header_->setVisible(visible);
    if (statusBar_) statusBar_->setVisible(visible);
}

void KioskWindow::onCandidateFound(QString macStr, int rssi) {
    p1_->addCandidate(macStr, rssi);
}

void KioskWindow::onCaptureError(QString msg) {
    QMessageBox::critical(this, "캡처 오류", msg);
}

void KioskWindow::updateElapsed() {
    ++elapsedSeconds_;
    int mm = (elapsedSeconds_ / 60) % 60;
    int ss = elapsedSeconds_ % 60;
    elapsedLabel_->setText(QString("%1:%2")
                               .arg(mm, 2, 10, QChar('0'))
                               .arg(ss, 2, 10, QChar('0')));
}

void KioskWindow::updateDeviceCount(int count) {
    deviceCountLabel_->setText(QString("%1 devices").arg(count));
}

void KioskWindow::goPhase1() {
    setChromeVisible(true);
    setActivePhase(1);
    stack_->setCurrentWidget(p1_);
}

void KioskWindow::goPhase2(QString macStr) {
    setChromeVisible(true);
    setActivePhase(2);
    p2_->setTargetMac(macStr);
    stack_->setCurrentWidget(p2_);
    QTimer::singleShot(0, p2_, &Phase2Widget::focusFirstInput);
}

void KioskWindow::goPhase3() {
    setChromeVisible(true);
    setActivePhase(3);
    stack_->setCurrentWidget(p3_);
    p3_->showCompleted();
}

void KioskWindow::goAdmin() {
    setChromeVisible(false);
    admin_->refresh();
    stack_->setCurrentWidget(admin_);
    admin_->focusSearch();
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