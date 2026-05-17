#include "ui.h"
#include "db.h"
#include "mac.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QDateTime>
#include <QMediaPlayer>
#include <QAudioOutput>

// ════════════════════════════════════════════════
//  SettingsDialog
// ════════════════════════════════════════════════

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("캡처 설정");
    setMinimumWidth(420);
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(16);
    root->setContentsMargins(24, 24, 24, 24);

    auto* title = new QLabel("📡 MAC 수집 시스템 설정");
    title->setStyleSheet("QLabel { font-size: 14pt; font-weight: 700; color: #1E3A5F; }");
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* form = new QFormLayout();
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto mkLabel = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("QLabel { font-size: 10pt; font-weight: 600; color: #374151; }");
        return l;
    };

    // 인터페이스 목록 (시스템 NIC 자동 열거 + 직접 입력 가능)
    ifaceCombo_ = new QComboBox();
    ifaceCombo_->setEditable(true);
    ifaceCombo_->setMinimumHeight(36);
    for (const auto& ni : QNetworkInterface::allInterfaces())
        ifaceCombo_->addItem(ni.name());
    ifaceCombo_->setCurrentText("wlan0mon");
    ifaceCombo_->setStyleSheet(
        "QComboBox { border: 1.5px solid #D1D5DB; border-radius: 6px;"
        "  padding: 6px 10px; font-size: 10pt; }"
        "QComboBox:focus { border: 2px solid #2563EB; }");
    form->addRow(mkLabel("네트워크 인터페이스:"), ifaceCombo_);

    // 채널 (0 = 변경 안함)
    channelSpin_ = new QSpinBox();
    channelSpin_->setRange(0, 14);
    channelSpin_->setValue(0);
    channelSpin_->setSpecialValueText("변경 안함 (0)");
    channelSpin_->setMinimumHeight(36);
    channelSpin_->setStyleSheet(
        "QSpinBox { border: 1.5px solid #D1D5DB; border-radius: 6px;"
        "  padding: 6px 10px; font-size: 10pt; }"
        "QSpinBox:focus { border: 2px solid #2563EB; }");
    form->addRow(mkLabel("채널 번호 (1-14):"), channelSpin_);

    // RSSI 임계값
    rssiSpin_ = new QSpinBox();
    rssiSpin_->setRange(-100, -20);
    rssiSpin_->setValue(-60);
    rssiSpin_->setSuffix(" dBm");
    rssiSpin_->setMinimumHeight(36);
    rssiSpin_->setStyleSheet(
        "QSpinBox { border: 1.5px solid #D1D5DB; border-radius: 6px;"
        "  padding: 6px 10px; font-size: 10pt; }"
        "QSpinBox:focus { border: 2px solid #2563EB; }");
    form->addRow(mkLabel("RSSI 임계값:"), rssiSpin_);

    // DB 경로
    dbEdit_ = new QLineEdit("MAC_address.db");
    dbEdit_->setMinimumHeight(36);
    dbEdit_->setStyleSheet(
        "QLineEdit { border: 1.5px solid #D1D5DB; border-radius: 6px;"
        "  padding: 6px 10px; font-size: 10pt; }"
        "QLineEdit:focus { border: 2px solid #2563EB; }");
    form->addRow(mkLabel("DB 파일 경로:"), dbEdit_);

    root->addLayout(form);

    // 버튼
    auto* btnRow = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("취소");
    cancelBtn->setMinimumSize(100, 40);
    cancelBtn->setStyleSheet(
        "QPushButton { background: #F3F4F6; color: #374151;"
        "  border: 1px solid #D1D5DB; border-radius: 6px; font-size: 10pt; }"
        "QPushButton:hover { background: #E5E7EB; }");

    auto* okBtn = new QPushButton("시작");
    okBtn->setMinimumSize(120, 40);
    okBtn->setDefault(true);
    okBtn->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 6px; font-size: 10pt; font-weight: 700; }"
        "QPushButton:hover { background: #1D4ED8; }");

    btnRow->addStretch(1);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    root->addLayout(btnRow);

    connect(okBtn,     &QPushButton::clicked, this, &SettingsDialog::onOk);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::onOk() {
    if (ifaceCombo_->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "입력 오류", "네트워크 인터페이스를 입력하세요.");
        return;
    }
    accept();
}

QString SettingsDialog::iface()         const { return ifaceCombo_->currentText().trimmed(); }
int     SettingsDialog::channel()       const { return channelSpin_->value(); }
int     SettingsDialog::rssiThreshold() const { return rssiSpin_->value(); }
QString SettingsDialog::dbPath()        const { return dbEdit_->text().trimmed(); }

// ════════════════════════════════════════════════
//  AudioPlayer
// ════════════════════════════════════════════════

AudioPlayer& AudioPlayer::instance() {
    static AudioPlayer inst;
    return inst;
}

AudioPlayer::AudioPlayer(QObject* parent)
    : QObject(parent)
{
    player_   = new QMediaPlayer(this);
    audioOut_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOut_);
    audioOut_->setVolume(1.0f);

    // EndOfMedia 이벤트로 다음 파일 재생 (StoppedState보다 안정적)
    connect(player_, &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia
                    && queueIdx_ < queue_.size())
                {
                    playNext();
                }
            });
}

void AudioPlayer::play(const QStringList& files) {
    stop();
    queue_    = files;
    queueIdx_ = 0;
    playNext();
}

void AudioPlayer::stop() {
    player_->stop();
    queue_.clear();
    queueIdx_ = 0;
}

void AudioPlayer::playNext() {
    if (queueIdx_ >= queue_.size()) return;
    const QString path = queue_.at(queueIdx_++);
    if (path.startsWith(":/"))
        player_->setSource(QUrl("qrc" + path));
    else
        player_->setSource(QUrl::fromLocalFile(path));
    player_->play();
}

// ════════════════════════════════════════════════
//  Phase1Widget
// ════════════════════════════════════════════════

Phase1Widget::Phase1Widget(Db* db, QWidget* parent)
    : QWidget(parent), db_(db)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 한글 입력 테스트 패널 ──
    auto* testPanel = new QWidget();
    testPanel->setFixedHeight(60);
    testPanel->setStyleSheet(
        "QWidget { background: #EFF6FF;"
        "          border-bottom: 2px solid #BFDBFE; }");

    auto* testLayout = new QHBoxLayout(testPanel);
    testLayout->setContentsMargins(16, 10, 16, 10);
    testLayout->setSpacing(10);

    auto* testLabel = new QLabel("🇰🇷 한글 입력 테스트:");
    testLabel->setStyleSheet(
        "QLabel { color: #1D4ED8; font-size: 11pt;"
        "         font-weight: 700; background: transparent; }");
    testLayout->addWidget(testLabel);

    testEdit_ = new QLineEdit();
    testEdit_->setPlaceholderText("홍길동");
    testEdit_->setMinimumHeight(38);
    testEdit_->setAttribute(Qt::WA_InputMethodEnabled, true);
    testEdit_->setInputMethodHints(Qt::ImhNone);
    testEdit_->setStyleSheet(
        "QLineEdit {"
        "  background: white; color: #1F2937;"
        "  border: 1.5px solid #93C5FD; border-radius: 4px;"
        "  padding: 6px 12px; font-size: 12pt; }"
        "QLineEdit:focus { border: 2px solid #2563EB; }");
    testLayout->addWidget(testEdit_, 1);

    testResultLabel_ = new QLabel("입력값: -");
    testResultLabel_->setMinimumWidth(200);
    testResultLabel_->setStyleSheet(
        "QLabel { color: #1F2937; font-size: 11pt;"
        "         background: transparent; padding: 0 8px; }");
    testLayout->addWidget(testResultLabel_);

    auto* clearBtn = new QPushButton("지우기");
    clearBtn->setMinimumSize(80, 36);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 4px; padding: 6px 14px;"
        "  font-size: 10pt; font-weight: 600; }"
        "QPushButton:hover { background: #1D4ED8; }");

    connect(testEdit_, &QLineEdit::textChanged,
            this, [this](const QString& text) {
                testResultLabel_->setText(
                    "입력값: " + (text.isEmpty() ? QString("-") : text));
            });
    connect(clearBtn, &QPushButton::clicked,
            this, [this]() {
                testEdit_->clear();
                testEdit_->setFocus();
            });

    testLayout->addWidget(clearBtn);
    root->addWidget(testPanel, 0);

    // ── 감지 테이블 ──
    table_ = new QTableWidget(0, 6, this);
    table_->setHorizontalHeaderLabels(
        {"MAC 주소", "제조사", "신호", "감지 시각", "", ""});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setFixedHeight(36);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(54);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::StrongFocus);
    table_->setStyleSheet(
        "QTableWidget { background: white; border: none;"
        "               gridline-color: transparent; }"
        "QHeaderView::section { background: #F3F4F6; color: #374151;"
        "  padding: 8px 12px; border: none;"
        "  border-bottom: 1px solid #E5E7EB;"
        "  font-weight: 700; font-size: 10pt; }"
        "QTableWidget::item { padding: 8px 12px;"
        "  border-bottom: 1px solid #F3F4F6; }"
        "QTableWidget::item:hover { background: #F9FAFB; }");
    root->addWidget(table_, 1);
}

void Phase1Widget::addCandidate(const QString& macStr, int rssi,
                                const QString& vendor,
                                const QString& timestamp)
{
    for (int i = 0; i < table_->rowCount(); ++i) {
        if (table_->item(i, 0) &&
            table_->item(i, 0)->text() == macStr) return;
    }

    int row = table_->rowCount();
    table_->insertRow(row);

    auto mkItem = [](const QString& t) {
        auto* it = new QTableWidgetItem(t);
        it->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        return it;
    };

    table_->setItem(row, 0, mkItem(macStr));
    table_->setItem(row, 1, mkItem(vendor));
    table_->setItem(row, 2, mkItem(QString("%1 dBm").arg(rssi)));
    table_->setItem(row, 3, mkItem(timestamp));

    auto* regBtn = new QPushButton("등록");
    regBtn->setProperty("macStr",    macStr);
    regBtn->setProperty("vendor",    vendor);
    regBtn->setProperty("timestamp", timestamp);
    regBtn->setCursor(Qt::PointingHandCursor);
    regBtn->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 6px; padding: 6px 16px;"
        "  font-size: 10pt; font-weight: 600; }"
        "QPushButton:hover { background: #1D4ED8; }");
    connect(regBtn, &QPushButton::clicked,
            this, &Phase1Widget::onRegisterButtonClicked);
    table_->setCellWidget(row, 4, regBtn);
    table_->setItem(row, 5, mkItem(""));

    emit candidateCountChanged(table_->rowCount());
}

void Phase1Widget::showDuplicateNotice(const QString& macStr,
                                       const QString& ownerName,
                                       const QString& phone,
                                       const QString& vendor,
                                       int rssi,
                                       const QString& registeredAt)
{
    for (int i = 0; i < table_->rowCount(); ++i) {
        if (table_->item(i, 0) &&
            table_->item(i, 0)->text() == macStr) return;
    }

    int row = table_->rowCount();
    table_->insertRow(row);

    auto mkItem = [](const QString& t) {
        auto* it = new QTableWidgetItem(t);
        it->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        return it;
    };

    table_->setItem(row, 0, mkItem(macStr));
    table_->setItem(row, 1, mkItem(vendor));
    table_->setItem(row, 2, mkItem(QString("%1 dBm").arg(rssi)));
    table_->setItem(row, 3, mkItem(registeredAt));

    auto* badge = new QLabel(
        QString("✅ 등록됨  %1 / %2").arg(ownerName, phone));
    badge->setStyleSheet(
        "QLabel { color: #065F46; background: #D1FAE5;"
        "  border-radius: 6px; padding: 4px 10px;"
        "  font-size: 9pt; font-weight: 600; }");
    badge->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    table_->setCellWidget(row, 4, badge);

    auto* updBtn = new QPushButton("변경");
    updBtn->setProperty("macStr", macStr);
    updBtn->setCursor(Qt::PointingHandCursor);
    updBtn->setStyleSheet(
        "QPushButton { background: #D97706; color: white; border: none;"
        "  border-radius: 6px; padding: 6px 14px;"
        "  font-size: 10pt; font-weight: 600; }"
        "QPushButton:hover { background: #B45309; }");
    connect(updBtn, &QPushButton::clicked,
            this, &Phase1Widget::onUpdateButtonClicked);
    table_->setCellWidget(row, 5, updBtn);

    emit candidateCountChanged(table_->rowCount());
}

void Phase1Widget::removeCandidate(const QString& macStr) {
    for (int i = 0; i < table_->rowCount(); ++i) {
        if (table_->item(i, 0) &&
            table_->item(i, 0)->text() == macStr) {
            table_->removeRow(i);
            break;
        }
    }
    emit candidateCountChanged(table_->rowCount());
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
    emit registerRequested(btn->property("macStr").toString(),
                           btn->property("vendor").toString(),
                           btn->property("timestamp").toString());
}

void Phase1Widget::onUpdateButtonClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    emit updateRequested(btn->property("macStr").toString());
}

// ════════════════════════════════════════════════
//  Phase2Widget
// ════════════════════════════════════════════════

Phase2Widget::Phase2Widget(QWidget* parent) : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(60, 40, 60, 40);
    root->setSpacing(20);

    titleLabel_ = new QLabel("기기 등록");
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet(
        "QLabel { font-size: 20pt; font-weight: 700; color: #111827; }");
    root->addWidget(titleLabel_);

    auto* card = new QFrame();
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet(
        "QFrame { background: white;"
        "         border: 1px solid #E5E7EB; border-radius: 12px; }");
    auto* form = new QFormLayout(card);
    form->setContentsMargins(32, 28, 32, 28);
    form->setSpacing(18);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto mkLabel = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(
            "QLabel { font-size: 11pt; color: #374151; font-weight: 600; }");
        return l;
    };
    auto mkEdit = [](const QString& placeholder = "") {
        auto* e = new QLineEdit();
        e->setPlaceholderText(placeholder);
        e->setMinimumHeight(42);
        e->setAttribute(Qt::WA_InputMethodEnabled, true);
        e->setInputMethodHints(Qt::ImhNone);
        e->setStyleSheet(
            "QLineEdit { background: white; color: #111827;"
            "  border: 1.5px solid #D1D5DB; border-radius: 8px;"
            "  padding: 8px 14px; font-size: 12pt; }"
            "QLineEdit:focus { border: 2px solid #2563EB; }"
            "QLineEdit:read-only { background: #F3F4F6; color: #6B7280; }");
        return e;
    };

    macLabel_ = new QLabel("AA:BB:CC:DD:EE:FF");
    macLabel_->setStyleSheet(
        "QLabel { font-size: 12pt; font-family: monospace; color: #374151;"
        "  background: #F3F4F6; border: 1px solid #D1D5DB;"
        "  border-radius: 6px; padding: 8px 14px; }");
    form->addRow(mkLabel("MAC 주소"), macLabel_);

    nameEdit_  = mkEdit("홍길동");
    form->addRow(mkLabel("이름 *"), nameEdit_);

    phoneEdit_ = mkEdit("010-0000-0000");
    form->addRow(mkLabel("전화번호 *"), phoneEdit_);

    typeCombo_ = new QComboBox();
    typeCombo_->addItems({"스마트폰", "노트북", "태블릿", "IoT 기기", "기타"});
    typeCombo_->setMinimumHeight(42);
    typeCombo_->setStyleSheet(
        "QComboBox { background: white; color: #111827;"
        "  border: 1.5px solid #D1D5DB; border-radius: 8px;"
        "  padding: 8px 14px; font-size: 11pt; }"
        "QComboBox:focus { border: 2px solid #2563EB; }");
    form->addRow(mkLabel("기기 종류 *"), typeCombo_);

    root->addWidget(card, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(16);

    cancelBtn_ = new QPushButton("취소");
    cancelBtn_->setMinimumSize(120, 48);
    cancelBtn_->setCursor(Qt::PointingHandCursor);
    cancelBtn_->setStyleSheet(
        "QPushButton { background: #F3F4F6; color: #374151;"
        "  border: 1px solid #D1D5DB; border-radius: 8px;"
        "  font-size: 12pt; font-weight: 600; }"
        "QPushButton:hover { background: #E5E7EB; }");

    okBtn_ = new QPushButton("확인");
    okBtn_->setMinimumSize(180, 48);
    okBtn_->setCursor(Qt::PointingHandCursor);
    okBtn_->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 8px; font-size: 12pt; font-weight: 700; }"
        "QPushButton:hover { background: #1D4ED8; }");

    btnRow->addStretch(1);
    btnRow->addWidget(cancelBtn_);
    btnRow->addWidget(okBtn_);
    root->addLayout(btnRow);

    connect(okBtn_,     &QPushButton::clicked, this, &Phase2Widget::onConfirm);
    connect(cancelBtn_, &QPushButton::clicked, this, &Phase2Widget::onCancel);
}

void Phase2Widget::setTargetMac(const QString& macStr) {
    macLabel_->setText(macStr);
}

void Phase2Widget::prefill(const QString& name, const QString& phone,
                           const QString& deviceType)
{
    nameEdit_->setText(name);
    phoneEdit_->setText(phone);

    static const QHash<QString, int> idx{
        {"phone",    0}, {"스마트폰", 0},
        {"notebook", 1}, {"노트북",   1},
        {"tablet",   2}, {"태블릿",   2},
        {"iot",      3}, {"IoT 기기", 3},
        {"other",    4}, {"기타",     4}
    };
    typeCombo_->setCurrentIndex(idx.value(deviceType, 0));
}

void Phase2Widget::focusFirstInput() {
    nameEdit_->setFocus();
}

void Phase2Widget::setUpdateMode(bool isUpdate) {
    titleLabel_->setText(isUpdate ? "기기 정보 변경" : "기기 등록");
    okBtn_->setText(isUpdate ? "변경" : "확인");
}

void Phase2Widget::onConfirm() {
    if (nameEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "입력 오류", "이름을 입력해주세요.");
        nameEdit_->setFocus();
        return;
    }
    if (phoneEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "입력 오류", "전화번호를 입력해주세요.");
        phoneEdit_->setFocus();
        return;
    }

    static const QStringList typeKeys = {
        "phone", "notebook", "tablet", "iot", "other"
    };
    emit confirmed(macLabel_->text(),
                   nameEdit_->text().trimmed(),
                   phoneEdit_->text().trimmed(),
                   typeKeys.value(typeCombo_->currentIndex(), "other"));
}

void Phase2Widget::onCancel() {
    emit canceled();
}

// ════════════════════════════════════════════════
//  Phase3Widget
// ════════════════════════════════════════════════

Phase3Widget::Phase3Widget(QWidget* parent) : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setAlignment(Qt::AlignCenter);
    root->setSpacing(16);

    auto* icon = new QLabel("✅");
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("QLabel { font-size: 64pt; }");
    root->addWidget(icon);

    msgLabel_ = new QLabel("등록이 완료되었습니다.");
    msgLabel_->setAlignment(Qt::AlignCenter);
    msgLabel_->setStyleSheet(
        "QLabel { font-size: 22pt; font-weight: 700; color: #111827; }");
    root->addWidget(msgLabel_);

    subLabel_ = new QLabel("잠시 후 처음 화면으로 돌아갑니다.");
    subLabel_->setAlignment(Qt::AlignCenter);
    subLabel_->setStyleSheet(
        "QLabel { font-size: 13pt; color: #6B7280; }");
    root->addWidget(subLabel_);
}

void Phase3Widget::showCompleted(bool isUpdate) {
    msgLabel_->setText(isUpdate
                           ? "정보 변경이 완료되었습니다."
                           : "등록이 완료되었습니다.");
    subLabel_->setText("잠시 후 처음 화면으로 돌아갑니다.");

    QTimer::singleShot(3000, this, [this]() {
        emit autoReturn();
    });
}

// ════════════════════════════════════════════════
//  AdminPage
// ════════════════════════════════════════════════

AdminPage::AdminPage(Db* db, QWidget* parent)
    : QWidget(parent), db_(db)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* topRow = new QHBoxLayout();

    searchEdit_ = new QLineEdit();
    searchEdit_->setPlaceholderText("MAC / 이름 / 전화번호 검색...");
    searchEdit_->setMinimumHeight(40);
    searchEdit_->setAttribute(Qt::WA_InputMethodEnabled, true);
    searchEdit_->setStyleSheet(
        "QLineEdit { border: 1.5px solid #D1D5DB; border-radius: 8px;"
        "  padding: 6px 14px; font-size: 11pt; }"
        "QLineEdit:focus { border: 2px solid #2563EB; }");
    topRow->addWidget(searchEdit_, 1);

    auto mkBtn = [](const QString& label, const QString& bg,
                    const QString& hov) {
        auto* b = new QPushButton(label);
        b->setMinimumSize(80, 40);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            QString("QPushButton { background: %1; color: white; border: none;"
                    "  border-radius: 8px; font-size: 10pt; font-weight: 600; }"
                    "QPushButton:hover { background: %2; }").arg(bg, hov));
        return b;
    };

    searchBtn_ = mkBtn("검색",  "#2563EB", "#1D4ED8");
    deleteBtn_ = mkBtn("삭제",  "#DC2626", "#B91C1C");
    editBtn_   = mkBtn("수정",  "#D97706", "#B45309");

    backBtn_ = new QPushButton("← 뒤로");
    backBtn_->setMinimumSize(100, 40);
    backBtn_->setCursor(Qt::PointingHandCursor);
    backBtn_->setStyleSheet(
        "QPushButton { background: #F3F4F6; color: #374151;"
        "  border: 1px solid #D1D5DB; border-radius: 8px;"
        "  font-size: 10pt; font-weight: 600; }"
        "QPushButton:hover { background: #E5E7EB; }");

    topRow->addWidget(searchBtn_);
    topRow->addWidget(deleteBtn_);
    topRow->addWidget(editBtn_);
    topRow->addWidget(backBtn_);
    root->addLayout(topRow);

    tabs_ = new QTabWidget();
    tabs_->setStyleSheet(
        "QTabBar::tab { padding: 8px 20px; font-size: 10pt; }"
        "QTabBar::tab:selected { font-weight: 700; color: #2563EB; }");

    auto mkTable = [](const QStringList& headers) {
        auto* t = new QTableWidget(0, headers.size());
        t->setHorizontalHeaderLabels(headers);
        t->horizontalHeader()->setStretchLastSection(true);
        t->horizontalHeader()->setHighlightSections(false);
        t->verticalHeader()->setVisible(false);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setShowGrid(false);
        t->setStyleSheet(
            "QTableWidget { background: white; border: none; }"
            "QHeaderView::section { background: #F3F4F6; color: #374151;"
            "  padding: 6px 12px; border: none;"
            "  border-bottom: 1px solid #E5E7EB; font-weight: 700; }"
            "QTableWidget::item { padding: 6px 12px;"
            "  border-bottom: 1px solid #F3F4F6; }"
            "QTableWidget::item:selected { background: #DBEAFE;"
            "  color: #1D4ED8; }");
        return t;
    };

    stationTable_ = mkTable(
        {"MAC", "이름", "전화번호", "기기", "제조사", "등록일", "수정일"});
    apTable_   = mkTable({"MAC", "기타"});
    userTable_ = mkTable({"이름", "전화번호"});

    tabs_->addTab(stationTable_, "스테이션");
    tabs_->addTab(apTable_,      "AP");
    tabs_->addTab(userTable_,    "사용자");
    root->addWidget(tabs_, 1);

    connect(searchBtn_, &QPushButton::clicked,  this, &AdminPage::onSearch);
    connect(searchEdit_,&QLineEdit::returnPressed, this, &AdminPage::onSearch);
    connect(deleteBtn_, &QPushButton::clicked,  this, &AdminPage::onDeleteSelected);
    connect(editBtn_,   &QPushButton::clicked,  this, &AdminPage::onEditSelected);
    connect(backBtn_,   &QPushButton::clicked,  this, &AdminPage::onBack);
}

void AdminPage::refresh() {
    reloadStations("");
    reloadAps("");
    reloadUsers();
}

void AdminPage::focusSearch() {
    searchEdit_->setFocus();
}

void AdminPage::onSearch() {
    QString kw = searchEdit_->text().trimmed();
    reloadStations(kw);
    reloadAps(kw);
}

void AdminPage::reloadStations(const QString& keyword) {
    auto list = keyword.isEmpty()
    ? db_->listStations()
    : db_->searchStations(keyword.toStdString());
    stationTable_->setRowCount(0);
    for (const auto& s : list) {
        int row = stationTable_->rowCount();
        stationTable_->insertRow(row);
        stationTable_->setItem(row, 0, new QTableWidgetItem(
                                           QString::fromStdString(s.mac.toString())));
        stationTable_->setItem(row, 1, new QTableWidgetItem(
                                           QString::fromStdString(s.name)));
        stationTable_->setItem(row, 2, new QTableWidgetItem(
                                           QString::fromStdString(s.phoneNum)));
        stationTable_->setItem(row, 3, new QTableWidgetItem(
                                           QString::fromStdString(Db::typeCodeToString(s.type))));
        stationTable_->setItem(row, 4, new QTableWidgetItem(
                                           QString::fromStdString(s.vendor)));
        stationTable_->setItem(row, 5, new QTableWidgetItem(
                                           QString::fromStdString(s.registeredAt)));
        stationTable_->setItem(row, 6, new QTableWidgetItem(
                                           QString::fromStdString(s.updatedAt)));
    }
}

void AdminPage::reloadAps(const QString& keyword) {
    auto list = keyword.isEmpty()
    ? db_->listAps()
    : db_->searchAps(keyword.toStdString());
    apTable_->setRowCount(0);
    for (const auto& a : list) {
        int row = apTable_->rowCount();
        apTable_->insertRow(row);
        apTable_->setItem(row, 0, new QTableWidgetItem(
                                      QString::fromStdString(a.mac.toString())));
        apTable_->setItem(row, 1, new QTableWidgetItem(
                                      QString::number(a.other)));
    }
}

void AdminPage::reloadUsers() {
    auto list = db_->listUsers();
    userTable_->setRowCount(0);
    for (const auto& u : list) {
        int row = userTable_->rowCount();
        userTable_->insertRow(row);
        userTable_->setItem(row, 0, new QTableWidgetItem(
                                        QString::fromStdString(u.name)));
        userTable_->setItem(row, 1, new QTableWidgetItem(
                                        QString::fromStdString(u.phoneNum)));
    }
}

void AdminPage::onDeleteSelected() {
    int tab = tabs_->currentIndex();
    if (tab == 0) {
        int row = stationTable_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "안내", "삭제할 항목을 선택하세요.");
            return;
        }
        QString mac   = stationTable_->item(row, 0)->text();
        QString name  = stationTable_->item(row, 1)->text();
        QString phone = stationTable_->item(row, 2)->text();
        if (QMessageBox::question(this, "삭제 확인",
                                  QString("%1 (%2) 을 삭제하시겠습니까?").arg(mac, name))
            == QMessageBox::Yes)
        {
            db_->removeStation(Mac(mac.toUtf8().constData()),
                               name.toStdString(), phone.toStdString());
            reloadStations("");
        }
    } else if (tab == 1) {
        int row = apTable_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "안내", "삭제할 항목을 선택하세요.");
            return;
        }
        QString mac = apTable_->item(row, 0)->text();
        if (QMessageBox::question(this, "삭제 확인",
                                  QString("AP %1 을 삭제하시겠습니까?").arg(mac))
            == QMessageBox::Yes)
        {
            db_->removeAp(Mac(mac.toUtf8().constData()));
            reloadAps("");
        }
    }
}

void AdminPage::onEditSelected() {
    if (tabs_->currentIndex() != 0) {
        QMessageBox::information(this, "안내", "스테이션 탭에서만 수정 가능합니다.");
        return;
    }
    int row = stationTable_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "안내", "수정할 항목을 선택하세요.");
        return;
    }

    QString mac     = stationTable_->item(row, 0)->text();
    QString name    = stationTable_->item(row, 1)->text();
    QString phone   = stationTable_->item(row, 2)->text();
    QString typeStr = stationTable_->item(row, 3)->text();

    QDialog dlg(this);
    dlg.setWindowTitle("정보 수정");
    auto* form = new QFormLayout(&dlg);

    auto* macLbl   = new QLabel(mac);
    auto* nameEdit = new QLineEdit(name);
    auto* phoneEdt = new QLineEdit(phone);
    auto* typeCmb  = new QComboBox();
    typeCmb->addItems({"스마트폰", "노트북", "태블릿", "IoT 기기", "기타"});

    static const QHash<QString, int> idx{
                                         {"phone",0},{"notebook",1},{"tablet",2},{"iot",3},{"other",4}};
    typeCmb->setCurrentIndex(idx.value(typeStr, 4));

    nameEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
    phoneEdt->setAttribute(Qt::WA_InputMethodEnabled, true);

    form->addRow("MAC (수정 불가):", macLbl);
    form->addRow("이름:",            nameEdit);
    form->addRow("전화번호:",        phoneEdt);
    form->addRow("기기 종류:",       typeCmb);

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        static const QStringList typeKeys = {
                                             "phone","notebook","tablet","iot","other"};
        db_->updateStation(
            Mac(mac.toUtf8().constData()),
            nameEdit->text().toStdString(),
            phoneEdt->text().toStdString(),
            Db::typeStringToCode(
                typeKeys.value(typeCmb->currentIndex(), "other").toStdString()));
        reloadStations("");
    }
}

void AdminPage::onBack() {
    emit backRequested();
}

// ════════════════════════════════════════════════
//  KioskWindow
// ════════════════════════════════════════════════

KioskWindow::KioskWindow(Db* db, QWidget* parent)
    : QMainWindow(parent), db_(db)
{
    setWindowTitle("MAC 수집 키오스크");
    resize(1024, 768);

    auto* central    = new QWidget(this);
    setCentralWidget(central);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    buildHeader();
    rootLayout->addWidget(header_);

    stack_ = new QStackedWidget();
    p1_    = new Phase1Widget(db_, this);
    p2_    = new Phase2Widget(this);
    p3_    = new Phase3Widget(this);
    admin_ = new AdminPage(db_, this);

    stack_->addWidget(p1_);     // 0
    stack_->addWidget(p2_);     // 1
    stack_->addWidget(p3_);     // 2
    stack_->addWidget(admin_);  // 3
    rootLayout->addWidget(stack_, 1);

    buildStatusBar();
    rootLayout->addWidget(statusBar_);

    connect(p1_, &Phase1Widget::registerRequested,
            this, &KioskWindow::goPhase2Register);
    connect(p1_, &Phase1Widget::updateRequested,
            this, &KioskWindow::goPhase2Update);
    connect(p1_, &Phase1Widget::candidateCountChanged,
            this, &KioskWindow::updateDeviceCount);

    connect(p2_, &Phase2Widget::confirmed,   // 버그 수정: 주석 해제
            this, &KioskWindow::onPhase2Confirmed);
    connect(p2_, &Phase2Widget::canceled,
            this, &KioskWindow::goPhase1);

    connect(p3_, &Phase3Widget::autoReturn,
            this, &KioskWindow::goPhase1);

    connect(admin_, &AdminPage::backRequested,
            this, &KioskWindow::goPhase1);

    elapsedTimer_ = new QTimer(this);
    connect(elapsedTimer_, &QTimer::timeout,
            this, &KioskWindow::updateElapsed);
    elapsedTimer_->start(1000);

    goPhase1();
}

// ── X 버튼 클릭 시 앱 완전 종료 ──
void KioskWindow::closeEvent(QCloseEvent* event) {
    QApplication::quit();
    event->accept();
}

// ── Header ──
void KioskWindow::buildHeader() {
    header_ = new QWidget();
    header_->setFixedHeight(72);
    header_->setStyleSheet("QWidget { background: #1E3A5F; }");

    auto* hLayout = new QHBoxLayout(header_);
    hLayout->setContentsMargins(24, 0, 24, 0);
    hLayout->setSpacing(0);

    titleLabel_ = new QLabel("📡 Wi-Fi MAC 수집 시스템");
    titleLabel_->setStyleSheet(
        "QLabel { color: white; font-size: 16pt; font-weight: 700; }");
    hLayout->addWidget(titleLabel_);
    hLayout->addStretch(1);

    makePhaseStep(phaseStep1_, phaseStep1Num_, phaseStep1Text_, "1", "감지");
    makePhaseStep(phaseStep2_, phaseStep2Num_, phaseStep2Text_, "2", "입력");
    makePhaseStep(phaseStep3_, phaseStep3Num_, phaseStep3Text_, "3", "완료");

    hLayout->addWidget(phaseStep1_);
    hLayout->addWidget(phaseStep2_);
    hLayout->addWidget(phaseStep3_);
}

void KioskWindow::makePhaseStep(QWidget*& wOut, QLabel*& circleOut,
                                QLabel*& textOut,
                                const QString& numText,
                                const QString& labelText)
{
    wOut = new QWidget();
    wOut->setFixedWidth(80);
    auto* vl = new QVBoxLayout(wOut);
    vl->setContentsMargins(0, 8, 0, 8);
    vl->setSpacing(4);
    vl->setAlignment(Qt::AlignCenter);

    circleOut = new QLabel(numText);
    circleOut->setFixedSize(32, 32);
    circleOut->setAlignment(Qt::AlignCenter);
    circleOut->setStyleSheet(
        "QLabel { background: #374151; color: #9CA3AF;"
        "  border-radius: 16px; font-size: 12pt; font-weight: 700; }");

    textOut = new QLabel(labelText);
    textOut->setAlignment(Qt::AlignCenter);
    textOut->setStyleSheet("QLabel { color: #9CA3AF; font-size: 9pt; }");

    vl->addWidget(circleOut, 0, Qt::AlignCenter);
    vl->addWidget(textOut,   0, Qt::AlignCenter);
}

void KioskWindow::setActivePhase(int phase) {
    auto activate = [](QLabel* circle, QLabel* text, bool active) {
        if (active) {
            circle->setStyleSheet(
                "QLabel { background: #3B82F6; color: white;"
                "  border-radius: 16px; font-size: 12pt; font-weight: 700; }");
            text->setStyleSheet(
                "QLabel { color: white; font-size: 9pt; font-weight: 700; }");
        } else {
            circle->setStyleSheet(
                "QLabel { background: #374151; color: #9CA3AF;"
                "  border-radius: 16px; font-size: 12pt; font-weight: 700; }");
            text->setStyleSheet(
                "QLabel { color: #9CA3AF; font-size: 9pt; }");
        }
    };
    activate(phaseStep1Num_, phaseStep1Text_, phase == 1);
    activate(phaseStep2Num_, phaseStep2Text_, phase == 2);
    activate(phaseStep3Num_, phaseStep3Text_, phase == 3);
}

void KioskWindow::setChromeVisible(bool visible) {
    header_->setVisible(visible);
    statusBar_->setVisible(visible);
}

void KioskWindow::updatePhaseIndicator(int activeStep) {
    setActivePhase(activeStep);
}

// ── Status bar ──
void KioskWindow::buildStatusBar() {
    statusBar_ = new QWidget();
    statusBar_->setFixedHeight(40);
    statusBar_->setStyleSheet(
        "QWidget { background: #F9FAFB;"
        "          border-top: 1px solid #E5E7EB; }");

    auto* sbl = new QHBoxLayout(statusBar_);
    sbl->setContentsMargins(16, 0, 16, 0);
    sbl->setSpacing(16);

    scanStatusLabel_ = new QLabel("🟢 수집 중...");
    scanStatusLabel_->setStyleSheet(
        "QLabel { color: #065F46; font-size: 10pt; }");

    deviceCountLabel_ = new QLabel("감지: 0 개");
    deviceCountLabel_->setStyleSheet(
        "QLabel { color: #374151; font-size: 10pt; }");

    elapsedLabel_ = new QLabel("경과: 00:00");
    elapsedLabel_->setStyleSheet(
        "QLabel { color: #374151; font-size: 10pt; }");

    adminBtn_ = new QPushButton("⚙ 관리자");
    adminBtn_->setMinimumSize(90, 28);
    adminBtn_->setCursor(Qt::PointingHandCursor);
    adminBtn_->setStyleSheet(
        "QPushButton { background: #374151; color: white; border: none;"
        "  border-radius: 4px; font-size: 9pt; }"
        "QPushButton:hover { background: #1F2937; }");

    sbl->addWidget(scanStatusLabel_);
    sbl->addStretch(1);
    sbl->addWidget(deviceCountLabel_);
    sbl->addWidget(elapsedLabel_);
    sbl->addWidget(adminBtn_);

    connect(adminBtn_, &QPushButton::clicked,
            this, &KioskWindow::goAdmin);
}

// ════════════════════════════════════════════════
//  화면 전환 + 음성 재생
// ════════════════════════════════════════════════

void KioskWindow::goPhase1() {
    stack_->setCurrentIndex(0);
    setChromeVisible(true);
    updatePhaseIndicator(1);
    isUpdateMode_  = false;
    phase1Entered_ = true;
    AudioPlayer::instance().play({":/audio/scan_guide.mp3"});  // 경로 버그 수정
}

void KioskWindow::goPhase2Register(QString macStr, QString vendor,
                                   QString timestamp)
{
    isUpdateMode_     = false;
    pendingMac_       = macStr;
    pendingVendor_    = vendor;
    pendingTimestamp_ = timestamp;

    p2_->setUpdateMode(false);
    p2_->setTargetMac(macStr);
    p2_->prefill("", "", "phone");
    p2_->focusFirstInput();

    AudioPlayer::instance().play({":/audio/input_guide.mp3"});

    stack_->setCurrentIndex(1);
    updatePhaseIndicator(2);
}

void KioskWindow::goPhase2Update(QString macStr) {
    isUpdateMode_ = true;
    pendingMac_   = macStr;

    auto stations = db_->searchStations(macStr.toStdString());
    QString name, phone, deviceType, vendor;
    if (!stations.empty()) {
        name       = QString::fromStdString(stations[0].name);
        phone      = QString::fromStdString(stations[0].phoneNum);
        deviceType = QString::fromStdString(
            Db::typeCodeToString(stations[0].type));
        vendor     = QString::fromStdString(stations[0].vendor);
    }
    pendingVendor_ = vendor;

    p2_->setUpdateMode(true);
    p2_->setTargetMac(macStr);
    p2_->prefill(name, phone, deviceType);
    p2_->focusFirstInput();

    AudioPlayer::instance().play({":/audio/update_guide.mp3"});

    stack_->setCurrentIndex(1);
    updatePhaseIndicator(2);
}

void KioskWindow::goPhase3() {
    stack_->setCurrentIndex(2);
    updatePhaseIndicator(3);
    p3_->showCompleted(isUpdateMode_);

    if (isUpdateMode_)
        AudioPlayer::instance().play({":/audio/update_complete.mp3"});
    else
        AudioPlayer::instance().play({":/audio/register_complete.mp3"});
}

void KioskWindow::goAdmin() {
    admin_->refresh();
    admin_->focusSearch();
    stack_->setCurrentIndex(3);
    setChromeVisible(false);
}

// ── Phase2 확인 처리 (로컬 DB 저장, timestamp: yyMMddHHmm) ──
void KioskWindow::onPhase2Confirmed(QString macStr, QString name,
                                    QString phone, QString deviceType)
{
    // timestamp 형식: yyMMddHHmm (예: 2506171430)
    QString now = QDateTime::currentDateTime().toString("yyMMddHHmm");

    if (isUpdateMode_) {
        db_->updateStation(Mac(macStr.toUtf8().constData()),
                           name.toStdString(),
                           phone.toStdString(),
                           Db::typeStringToCode(deviceType.toStdString()));
    } else {
        StationEntry se;
        se.mac          = Mac(macStr.toUtf8().constData());
        se.name         = name.toStdString();
        se.phoneNum     = phone.toStdString();
        se.type         = Db::typeStringToCode(deviceType.toStdString());
        se.vendor       = pendingVendor_.toStdString();
        se.registeredAt = now.toStdString();
        db_->addStation(se);
    }

    goPhase3();
}

// ── 신규 MAC 감지: 로컬 DB 중복 확인 ──
void KioskWindow::onCandidateFound(QString macStr, int rssi,
                                   QString vendor, QString timestamp)
{
    pendingRssi_ = rssi;

    bool exists = db_->macExists(Mac(macStr.toUtf8().constData()));

    if (exists) {
        // 로컬 DB에서 기존 정보 조회 후 중복 행 표시
        auto stations = db_->searchStations(macStr.toStdString());
        if (!stations.empty()) {
            const auto& s = stations[0];
            p1_->showDuplicateNotice(
                macStr,
                QString::fromStdString(s.name),
                QString::fromStdString(s.phoneNum),
                QString::fromStdString(s.vendor),
                rssi,
                QString::fromStdString(s.registeredAt));
            AudioPlayer::instance().play({":/audio/duplicate_notice.mp3"});
        }
    } else {
        p1_->addCandidate(macStr, rssi, vendor, timestamp);
        AudioPlayer::instance().play({":/audio/mac_detected.mp3"});
    }
}

void KioskWindow::onCaptureError(QString msg) {
    scanStatusLabel_->setText("🔴 오류: " + msg);
}

void KioskWindow::updateElapsed() {
    ++elapsedSeconds_;
    int m = elapsedSeconds_ / 60;
    int s = elapsedSeconds_ % 60;
    elapsedLabel_->setText(
        QString("경과: %1:%2")
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0')));
}

void KioskWindow::updateDeviceCount(int count) {
    deviceCountLabel_->setText(QString("감지: %1 개").arg(count));
}