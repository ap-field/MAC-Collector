#include "ui.h"
#include "db.h"
#include "mac.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QDateTime>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QIntValidator>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
// ════════════════════════════════════════════════
//  SettingsDialog
// ════════════════════════════════════════════════

static QString settingsFilePath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
    + "/mac_collector_settings.json";
}

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

    auto mkLineEdit = [](const QString& placeholder = "") {
        auto* e = new QLineEdit();
        e->setPlaceholderText(placeholder);
        e->setMinimumHeight(36);
        e->setStyleSheet(
            "QLineEdit { border: 1.5px solid #D1D5DB; border-radius: 6px;"
            "  padding: 6px 10px; font-size: 10pt; }"
            "QLineEdit:focus { border: 2px solid #2563EB; }");
        return e;
    };

    // 인터페이스
    ifaceCombo_ = new QComboBox();
    ifaceCombo_->setEditable(true);
    ifaceCombo_->setMinimumHeight(36);
    for (const auto& ni : QNetworkInterface::allInterfaces())
        ifaceCombo_->addItem(ni.name());
    ifaceCombo_->setCurrentText("mon0");
    ifaceCombo_->setStyleSheet(
        "QComboBox { border: 1.5px solid #D1D5DB; border-radius: 6px;"
        "  padding: 6px 10px; font-size: 10pt; }"
        "QComboBox:focus { border: 2px solid #2563EB; }");
    form->addRow(mkLabel("네트워크 인터페이스:"), ifaceCombo_);

    // 채널 — QLineEdit + 숫자 전용
    channelEdit_ = mkLineEdit("0 (변경 안함)");
    channelEdit_->setValidator(new QIntValidator(0, 14, this));
    channelEdit_->setText("0");
    form->addRow(mkLabel("채널 번호 (0=변경 안함, 1-14):"), channelEdit_);

    // RSSI — QLineEdit + 숫자 전용
    rssiEdit_ = mkLineEdit("-20");
    rssiEdit_->setValidator(new QIntValidator(-100, -20, this));
    rssiEdit_->setText("-20");
    form->addRow(mkLabel("RSSI 임계값 (dBm):"), rssiEdit_);

    // DB 경로
    dbEdit_ = mkLineEdit("MAC_address.db");
    dbEdit_->setText("MAC_address.db");
    form->addRow(mkLabel("DB 파일 경로:"), dbEdit_);

    root->addLayout(form);

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

    loadSettings();
}

void SettingsDialog::loadSettings() {
    QFile f(settingsFilePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    if (obj.contains("iface"))
        ifaceCombo_->setCurrentText(obj["iface"].toString());
    if (obj.contains("channel"))
        channelEdit_->setText(QString::number(obj["channel"].toInt()));
    if (obj.contains("rssi"))
        rssiEdit_->setText(QString::number(obj["rssi"].toInt()));
    if (obj.contains("dbPath"))
        dbEdit_->setText(obj["dbPath"].toString());
}

void SettingsDialog::saveSettings() {
    QJsonObject obj;
    obj["iface"]   = ifaceCombo_->currentText().trimmed();
    obj["channel"] = channelEdit_->text().toInt();
    obj["rssi"]    = rssiEdit_->text().toInt();
    obj["dbPath"]  = dbEdit_->text().trimmed();

    QString path = settingsFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson());
        f.close();
    }
}

void SettingsDialog::onOk() {
    if (ifaceCombo_->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "입력 오류", "네트워크 인터페이스를 입력하세요.");
        return;
    }
    int ch = channelEdit_->text().toInt();
    if (ch < 0 || ch > 14) {
        QMessageBox::warning(this, "입력 오류", "채널 번호는 0∼14 사이여야 합니다.");
        return;
    }
    int rssi = rssiEdit_->text().toInt();
    if (rssi < -100 || rssi > -20) {
        QMessageBox::warning(this, "입력 오류", "RSSI는 -100 ~ -20 사이여야 합니다.");
        return;
    }
    saveSettings();
    accept();
}

QString SettingsDialog::iface()         const { return ifaceCombo_->currentText().trimmed(); }
int     SettingsDialog::channel()       const { return channelEdit_->text().toInt(); }
int     SettingsDialog::rssiThreshold() const { return rssiEdit_->text().toInt(); }
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

Phase1Widget::Phase1Widget(QWidget* parent)
    : QWidget(parent)
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
    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"MAC 주소", "신호", "감지 시각", ""});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setColumnWidth(3, 240);

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
        "QTableWidget::item { padding: 8px 12px; color: #111827;"
        "  border-bottom: 1px solid #F3F4F6; }"
        "QTableWidget::item:hover { background: #F9FAFB; color: #111827; }");
    root->addWidget(table_, 1);
}

void Phase1Widget::addCandidate(const QString& macStr, int rssi,
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
        it->setTextAlignment(Qt::AlignCenter);
        it->setForeground(QBrush(QColor("#111827")));
        return it;
    };

    table_->setItem(row, 0, mkItem(macStr));
    table_->setItem(row, 1, mkItem(QString("%1 dBm").arg(rssi)));
    table_->setItem(row, 2, mkItem(timestamp));

    auto* regBtn = new QPushButton("등록");
    regBtn->setProperty("macStr",    macStr);
    regBtn->setProperty("timestamp", timestamp);
    regBtn->setCursor(Qt::PointingHandCursor);
    // setMinimumWidth: CSS padding은 Qt 레이아웃 sizeHint에 반영 안 되므로 명시적으로 지정
    regBtn->setMinimumSize(90, 34);
    regBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    regBtn->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none;"
        "  border-radius: 6px; padding: 6px 16px;"
        "  font-size: 10pt; font-weight: 600; }"
        "QPushButton:hover { background: #1D4ED8; }");
    connect(regBtn, &QPushButton::clicked,
            this, &Phase1Widget::onRegisterButtonClicked);
    table_->setCellWidget(row, 3, regBtn);
    table_->ensurePolished();
    for (int i = 0; i < 3; ++i)
        table_->resizeColumnToContents(i);

    emit candidateCountChanged(table_->rowCount());
}

void Phase1Widget::showDuplicateNotice(const QString& macStr,
                                       const QString& ownerName,
                                       const QString& phone,
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
        it->setTextAlignment(Qt::AlignCenter);
        it->setForeground(QBrush(QColor("#111827")));
        return it;
    };

    table_->setItem(row, 0, mkItem(macStr));
    table_->setItem(row, 1, mkItem(QString("%1 dBm").arg(rssi)));
    table_->setItem(row, 2, mkItem(registeredAt));

    // badge + 변경 버튼을 하나의 위젯으로 묶어서 버튼 글자가 잘리지 않도록
    auto* cell = new QWidget();
    auto* hlay = new QHBoxLayout(cell);
    hlay->setContentsMargins(4, 2, 4, 2);
    hlay->setSpacing(6);

    const QString badgeText = QString("✅ %1  %2").arg(ownerName, phone);
    auto* badge = new QLabel(badgeText);
    badge->setToolTip(badgeText);
    badge->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    badge->setStyleSheet(
        "QLabel { color: #065F46; background: #D1FAE5;"
        "  border-radius: 6px; padding: 4px 10px;"
        "  font-size: 9pt; font-weight: 600; }");
    badge->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto* updBtn = new QPushButton("변경");
    updBtn->setProperty("macStr", macStr);
    updBtn->setCursor(Qt::PointingHandCursor);
    updBtn->setMinimumSize(80, 32);
    updBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    updBtn->setStyleSheet(
        "QPushButton { background: #D97706; color: white; border: none;"
        "  border-radius: 6px; padding: 4px 14px;"
        "  font-size: 10pt; font-weight: 600; }"
        "QPushButton:hover { background: #B45309; }");
    connect(updBtn, &QPushButton::clicked,
            this, &Phase1Widget::onUpdateButtonClicked);

    hlay->addWidget(badge, 1);
    hlay->addWidget(updBtn, 0);
    table_->setCellWidget(row, 3, cell);

    table_->ensurePolished();
    for (int i = 0; i < 3; ++i)
        table_->resizeColumnToContents(i);
    // 열 3: stylesheet(9pt DemiBold)와 동일한 폰트로 직접 측정
    // cell margins(4+4) + badge(text + horizontal padding 10*2) + spacing(6) + button(80)
    QFont badgeFont = QApplication::font();
    badgeFont.setPointSizeF(9.0);
    badgeFont.setWeight(QFont::DemiBold);
    int needed = 8 + QFontMetrics(badgeFont).horizontalAdvance(badgeText) + 20 + 6 + 80;
    if (table_->columnWidth(3) < needed)
        table_->setColumnWidth(3, needed);

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
    macLabel_->setAlignment(Qt::AlignCenter);
    macLabel_->setStyleSheet(
        "QLabel { font-size: 12pt; font-family: monospace; color: #374151;"
        "  background: #F3F4F6; border: 1px solid #D1D5DB;"
        "  border-radius: 6px; padding: 8px 14px; qproperty-alignment: AlignCenter; }");
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

    table_ = new QTableWidget(0, 6);
    table_->setHorizontalHeaderLabels(
        {"MAC", "이름", "전화번호", "기기", "등록일", "수정일"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    table_->setStyleSheet(
        "QTableWidget { background: white; border: none; }"
        "QHeaderView::section { background: #F3F4F6; color: #374151;"
        "  padding: 6px 12px; border: none;"
        "  border-bottom: 1px solid #E5E7EB; font-weight: 700; }"
        "QTableWidget::item { padding: 6px 20px; color: #111827;"
        "  border-bottom: 1px solid #F3F4F6; }"
        "QTableWidget::item:selected { background: #DBEAFE;"
        "  color: #1D4ED8; }");
    root->addWidget(table_, 1);

    connect(searchBtn_,  &QPushButton::clicked,      this, &AdminPage::onSearch);
    connect(searchEdit_, &QLineEdit::returnPressed,   this, &AdminPage::onSearch);
    connect(deleteBtn_,  &QPushButton::clicked,       this, &AdminPage::onDeleteSelected);
    connect(editBtn_,    &QPushButton::clicked,       this, &AdminPage::onEditSelected);
    connect(backBtn_,    &QPushButton::clicked,       this, &AdminPage::onBack);
}

void AdminPage::refresh() {
    reloadTable("");
}

void AdminPage::focusSearch() {
    searchEdit_->setFocus();
}

void AdminPage::onSearch() {
    reloadTable(searchEdit_->text().trimmed());
}

void AdminPage::reloadTable(const QString& keyword) {
    auto list = keyword.isEmpty()
    ? db_->listStations()
    : db_->searchStations(keyword.toStdString());
    auto mkItem = [](const QString& t) {
        auto* it = new QTableWidgetItem(t);
        it->setForeground(QBrush(QColor("#111827")));
        return it;
    };
    table_->setRowCount(0);
    for (const auto& s : list) {
        int row = table_->rowCount();
        table_->insertRow(row);
        table_->setItem(row, 0, mkItem(QString::fromStdString(s.mac.toString())));
        table_->setItem(row, 1, mkItem(QString::fromStdString(s.name)));
        table_->setItem(row, 2, mkItem(QString::fromStdString(s.phoneNum)));
        table_->setItem(row, 3, mkItem(QString::fromStdString(Db::typeCodeToString(s.type))));
        table_->setItem(row, 4, mkItem(QString::fromStdString(s.registeredAt)));
        table_->setItem(row, 5, mkItem(QString::fromStdString(s.updatedAt)));
    }
    table_->ensurePolished();
    for (int i = 0; i < 5; ++i)
        table_->resizeColumnToContents(i);
}

void AdminPage::onDeleteSelected() {
    int row = table_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "안내", "삭제할 항목을 선택하세요.");
        return;
    }
    QString mac  = table_->item(row, 0)->text();
    QString name = table_->item(row, 1)->text();
    if (QMessageBox::question(this, "삭제 확인",
                              QString("%1 (%2) 을 삭제하시겠습니까?").arg(mac, name))
        == QMessageBox::Yes)
    {
        db_->removeStation(Mac(mac.toUtf8().constData()));
        reloadTable("");
    }
}

void AdminPage::onEditSelected() {
    int row = table_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "안내", "수정할 항목을 선택하세요.");
        return;
    }

    QString mac     = table_->item(row, 0)->text();
    QString name    = table_->item(row, 1)->text();
    QString phone   = table_->item(row, 2)->text();
    QString typeStr = table_->item(row, 3)->text();

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
        reloadTable("");
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
    Q_ASSERT(db != nullptr);// assert 활용/자동으로 함수 빠짐
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
    p1_    = new Phase1Widget(this);
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
    AudioPlayer::instance().play({":/audio/scan_guide.wav"});  // 경로 버그 수정
}

void KioskWindow::goPhase2Register(QString macStr, QString timestamp)
{
    isUpdateMode_     = false;
    pendingMac_       = macStr;
    pendingTimestamp_ = timestamp;

    p2_->setUpdateMode(false);
    p2_->setTargetMac(macStr);
    p2_->prefill("", "", "phone");
    p2_->focusFirstInput();

    AudioPlayer::instance().play({":/audio/input_guide.wav"});

    stack_->setCurrentIndex(1);
    updatePhaseIndicator(2);
}

void KioskWindow::goPhase2Update(QString macStr) {
    isUpdateMode_ = true;
    pendingMac_   = macStr;

    auto stations = db_->searchStations(macStr.toStdString());
    QString name, phone, deviceType;
    if (!stations.empty()) {
        name       = QString::fromStdString(stations[0].name);
        phone      = QString::fromStdString(stations[0].phoneNum);
        deviceType = QString::fromStdString(
            Db::typeCodeToString(stations[0].type));
    }

    p2_->setUpdateMode(true);
    p2_->setTargetMac(macStr);
    p2_->prefill(name, phone, deviceType);
    p2_->focusFirstInput();

    AudioPlayer::instance().play({":/audio/update_guide.wav"});

    stack_->setCurrentIndex(1);
    updatePhaseIndicator(2);
}

void KioskWindow::goPhase3() {
    stack_->setCurrentIndex(2);
    updatePhaseIndicator(3);
    p3_->showCompleted(isUpdateMode_);

    if (isUpdateMode_)
        AudioPlayer::instance().play({":/audio/update_complete.wav"});
    else
        AudioPlayer::instance().play({":/audio/register_complete.wav"});
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
    QString now = QDateTime::currentDateTime().toString("yyMMdd'T'HHmmss");
    int typeCode = Db::typeStringToCode(deviceType.toStdString());

    if (isUpdateMode_) {
        db_->updateStation(Mac(macStr.toUtf8().constData()),
                           name.toStdString(),
                           phone.toStdString(),
                           typeCode);
    } else {
        StationEntry se;
        se.mac          = Mac(macStr.toUtf8().constData());
        se.name         = name.toStdString();
        se.phoneNum     = phone.toStdString();
        se.type         = typeCode;
        se.registeredAt = now.toStdString();
        se.updatedAt    = now.toStdString();
        db_->addStation(se);
    }

    // DB 저장 직후 Phase1 테이블 갱신:
    // seenInSession_ 때문에 candidateFound가 재발생하지 않으므로
    // 직접 행을 교체해야 "등록" 버튼 → 이름/전화번호 + "변경" 버튼으로 반영된다.
    p1_->removeCandidate(macStr);
    auto updated = db_->searchStations(macStr.toStdString());
    if (!updated.empty()) {
        const auto& s = updated[0];
        p1_->showDuplicateNotice(
            macStr,
            QString::fromStdString(s.name),
            QString::fromStdString(s.phoneNum),
            pendingRssi_,
            QString::fromStdString(s.registeredAt));
    }

    goPhase3();
}

// ── 신규 MAC 감지: 로컬 DB 중복 확인 ──
void KioskWindow::onCandidateFound(QString macStr, int rssi, QString timestamp)
{
    pendingRssi_ = rssi;

    bool exists = db_->macExists(Mac(macStr.toUtf8().constData()));

    if (exists) {
        auto stations = db_->searchStations(macStr.toStdString());
        if (!stations.empty()) {
            const auto& s = stations[0];
            p1_->showDuplicateNotice(
                macStr,
                QString::fromStdString(s.name),
                QString::fromStdString(s.phoneNum),
                rssi,
                QString::fromStdString(s.registeredAt));
            AudioPlayer::instance().play({":/audio/duplicate_notice.wav"});
        }
    } else {
        p1_->addCandidate(macStr, rssi, timestamp);
        AudioPlayer::instance().play({":/audio/mac_detected.wav"});
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