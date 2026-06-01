#include "ui.h"
#include "db.h"
#include "mac.h"
#include "api_client.h"

#include <glog/logging.h>
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
        LOG(INFO) << "SettingsDialog::saveSettings saved to " << path.toStdString();
    } else {
        LOG(ERROR) << "SettingsDialog::saveSettings failed to open " << path.toStdString();
    }
}

void SettingsDialog::onOk() {
    LOG(INFO) << "SettingsDialog::onOk clicked";
    if (ifaceCombo_->currentText().trimmed().isEmpty()) {
        LOG(WARNING) << "SettingsDialog::onOk validation failed: empty iface";
        QMessageBox::warning(this, "입력 오류", "네트워크 인터페이스를 입력하세요.");
        return;
    }
    int ch = channelEdit_->text().toInt();
    if (ch < 0 || ch > 14) {
        LOG(WARNING) << "SettingsDialog::onOk validation failed: channel=" << ch;
        QMessageBox::warning(this, "입력 오류", "채널 번호는 0∼14 사이여야 합니다.");
        return;
    }
    int rssi = rssiEdit_->text().toInt();
    if (rssi < -100 || rssi > -20) {
        LOG(WARNING) << "SettingsDialog::onOk validation failed: rssi=" << rssi;
        QMessageBox::warning(this, "입력 오류", "RSSI는 -100 ~ -20 사이여야 합니다.");
        return;
    }
    LOG(INFO) << "SettingsDialog::onOk accepted iface="
              << ifaceCombo_->currentText().trimmed().toStdString()
              << " channel=" << ch << " rssi=" << rssi;
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

    // 재생 실패(파일 로드/코덱 오류 등)는 조용히 묻히므로 반드시 기록한다.
    connect(player_, &QMediaPlayer::errorOccurred,
            this, [this](QMediaPlayer::Error error, const QString& errorString) {
                if (error == QMediaPlayer::NoError) return;
                LOG(ERROR) << "AudioPlayer 재생 오류 source="
                           << player_->source().toString().toStdString()
                           << " error=" << static_cast<int>(error)
                           << " msg=" << errorString.toStdString();
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
    LOG(INFO) << "AudioPlayer::playNext 재생 path=" << path.toStdString()
              << " (" << queueIdx_ << "/" << queue_.size() << ")";
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

    testResultLabel_ = new QLabel("입력값: ");
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

    // 등록 완료된(중복/서버) 행을 5분마다 자동 정리. 미등록 후보 행은 보존한다.
    autoRefreshTimer_ = new QTimer(this);
    autoRefreshTimer_->setInterval(5 * 60 * 1000);
    connect(autoRefreshTimer_, &QTimer::timeout,
            this, &Phase1Widget::clearRegistered);
    autoRefreshTimer_->start();
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

void Phase1Widget::clearRegistered() {
    // 테이블을 역방향으로 순회: 열 3의 cellWidget 이 QPushButton("등록")이면
    // 아직 미등록 후보이므로 보존하고, 그 외(badge+변경 묶음 위젯)는 등록된
    // 항목이므로 행을 제거한다. 역방향이라 removeRow 후 인덱스가 밀리지 않는다.
    for (int i = table_->rowCount() - 1; i >= 0; --i) {
        if (qobject_cast<QPushButton*>(table_->cellWidget(i, 3)))
            continue;                 // 미등록 후보 → 유지
        table_->removeRow(i);         // 등록된 항목 → 삭제
    }
    emit candidateCountChanged(table_->rowCount());
}

int Phase1Widget::candidateCount() const {
    return table_->rowCount();
}

void Phase1Widget::onRegisterButtonClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    LOG(INFO) << "Phase1Widget register button clicked mac="
              << btn->property("macStr").toString().toStdString();
    emit registerRequested(btn->property("macStr").toString(),
                           btn->property("timestamp").toString());
}

void Phase1Widget::onUpdateButtonClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    LOG(INFO) << "Phase1Widget update button clicked mac="
              << btn->property("macStr").toString().toStdString();
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
    LOG(INFO) << "Phase2Widget::onConfirm mac=" << macLabel_->text().toStdString();
    if (nameEdit_->text().trimmed().isEmpty()) {
        LOG(WARNING) << "Phase2Widget::onConfirm validation failed: empty name";
        QMessageBox::warning(this, "입력 오류", "이름을 입력해주세요.");
        nameEdit_->setFocus();
        return;
    }
    if (phoneEdit_->text().trimmed().isEmpty()) {
        LOG(WARNING) << "Phase2Widget::onConfirm validation failed: empty phone";
        QMessageBox::warning(this, "입력 오류", "전화번호를 입력해주세요.");
        phoneEdit_->setFocus();
        return;
    }

    static const QStringList typeKeys = {
        "phone", "notebook", "tablet", "iot", "other"
    };
    QString typeKey = typeKeys.value(typeCombo_->currentIndex(), "other");
    LOG(INFO) << "Phase2Widget::onConfirm confirmed mac=" << macLabel_->text().toStdString()
              << " name=" << nameEdit_->text().trimmed().toStdString()
              << " type=" << typeKey.toStdString();
    emit confirmed(macLabel_->text(),
                   nameEdit_->text().trimmed(),
                   phoneEdit_->text().trimmed(),
                   typeKey);
}

void Phase2Widget::onCancel() {
    LOG(INFO) << "Phase2Widget::onCancel";
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

AdminPage::AdminPage(Db* db, ApiClient* api, QWidget* parent)
    : QWidget(parent), db_(db), api_(api)
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
    LOG(INFO) << "AdminPage::onSearch keyword=" << searchEdit_->text().trimmed().toStdString();
    reloadTable(searchEdit_->text().trimmed());
}

// DB에 저장된 날짜 문자열을 화면 표시용 yyMMddTHHmmss 로 정규화.
static QString fmtStationDate(const std::string& raw) {
    const QString s = QString::fromStdString(raw).trimmed();
    if (s.isEmpty()) return s;

    static const char* kInputFmts[] = {
        "yyMMdd'T'HHmmss",
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-dd'T'HH:mm:ss",
    };
    for (const char* f : kInputFmts) {
        QDateTime dt = QDateTime::fromString(s, f);
        if (dt.isValid())
            return dt.toString("yyMMdd'T'HHmmss");
    }
    return s;
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
        table_->setItem(row, 4, mkItem(fmtStationDate(s.registeredAt)));
        table_->setItem(row, 5, mkItem(fmtStationDate(s.updatedAt)));
    }
    table_->ensurePolished();
    for (int i = 0; i < 5; ++i)
        table_->resizeColumnToContents(i);
}

void AdminPage::onDeleteSelected() {
    int row = table_->currentRow();
    if (row < 0) {
        LOG(WARNING) << "AdminPage::onDeleteSelected no row selected";
        QMessageBox::information(this, "안내", "삭제할 항목을 선택하세요.");
        return;
    }
    QString mac  = table_->item(row, 0)->text();
    QString name = table_->item(row, 1)->text();
    LOG(INFO) << "AdminPage::onDeleteSelected request mac=" << mac.toStdString()
              << " name=" << name.toStdString();
    if (QMessageBox::question(this, "삭제 확인",
                              QString("%1 (%2) 을 삭제하시겠습니까?").arg(mac, name))
        == QMessageBox::Yes)
    {
        LOG(INFO) << "AdminPage::onDeleteSelected confirmed mac=" << mac.toStdString();
        db_->removeStation(Mac(mac.toUtf8().constData()));
        reloadTable("");
    } else {
        LOG(INFO) << "AdminPage::onDeleteSelected canceled mac=" << mac.toStdString();
    }
}

void AdminPage::onEditSelected() {
    int row = table_->currentRow();
    if (row < 0) {
        LOG(WARNING) << "AdminPage::onEditSelected no row selected";
        QMessageBox::information(this, "안내", "수정할 항목을 선택하세요.");
        return;
    }

    QString mac     = table_->item(row, 0)->text();
    LOG(INFO) << "AdminPage::onEditSelected mac=" << mac.toStdString();
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
    form->addRow("이름:",           nameEdit);
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
        const QString newName  = nameEdit->text();
        const QString newPhone = phoneEdt->text();
        const QString typeKey  = typeKeys.value(typeCmb->currentIndex(), "other");
        const int     typeCode = Db::typeStringToCode(typeKey.toStdString());

        LOG(INFO) << "AdminPage::onEditSelected accepted mac=" << mac.toStdString()
                  << " name=" << newName.toStdString()
                  << " phone=" << newPhone.toStdString();

        // 로컬 DB 반영
        db_->updateStation(
            Mac(mac.toUtf8().constData()),
            newName.toStdString(),
            newPhone.toStdString(),
            typeCode);

        // 서버 반영 (api_ 가 주입된 경우에만; 시나리오 3 update)
        if (api_) {
            const QString now = QDateTime::currentDateTime().toString("yyMMdd'T'HHmmss");
            LOG(INFO) << "AdminPage::onEditSelected -> ApiClient::updateDevice mac="
                      << mac.toStdString();
            api_->updateDevice(mac, newName, newPhone, typeCode, now);
        } else {
            LOG(WARNING) << "AdminPage::onEditSelected: no ApiClient, local DB only mac="
                         << mac.toStdString();
        }

        reloadTable("");

        // 수정 완료 음성 안내 (Phase2 흐름의 goPhase3 와 동일한 사운드)
        LOG(INFO) << "AdminPage::onEditSelected play update_complete audio mac="
                  << mac.toStdString();
        AudioPlayer::instance().play({":/audio/update_complete.wav"});
    } else {
        LOG(INFO) << "AdminPage::onEditSelected canceled mac=" << mac.toStdString();
    }
}

void AdminPage::onBack() {
    LOG(INFO) << "AdminPage::onBack";
    emit backRequested();
}

// ════════════════════════════════════════════════
//  KioskWindow
// ════════════════════════════════════════════════

KioskWindow::KioskWindow(Db* db, ApiClient* api, QWidget* parent)
    : QMainWindow(parent), db_(db), api_(api)
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
    admin_ = new AdminPage(db_, api_, this);

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

    // ── ApiClient 응답 시그널 연결 (api_ 가 주입된 경우에만) ──
    if (api_) {
        connect(api_, &ApiClient::registerSuccess,
                this, &KioskWindow::onRegisterSuccess);
        connect(api_, &ApiClient::registerFailed,
                this, &KioskWindow::onRegisterFailed);
        connect(api_, &ApiClient::updateSuccess,
                this, &KioskWindow::onUpdateSuccess);
        connect(api_, &ApiClient::updateFailed,
                this, &KioskWindow::onUpdateFailed);
        connect(api_, &ApiClient::deviceListFetched,
                this, &KioskWindow::onDeviceListFetched);
        connect(api_, &ApiClient::deviceListFailed,
                this, &KioskWindow::onDeviceListFailed);
        LOG(INFO) << "KioskWindow: ApiClient signals connected";
    } else {
        LOG(WARNING) << "KioskWindow: no ApiClient injected, running local-DB only";
    }

    elapsedTimer_ = new QTimer(this);
    connect(elapsedTimer_, &QTimer::timeout,
            this, &KioskWindow::updateElapsed);
    elapsedTimer_->start(1000);

    goPhase1();
}

// ── X 버튼 클릭 시 앱 완전 종료 ──
void KioskWindow::closeEvent(QCloseEvent* event) {
    LOG(INFO) << "KioskWindow::closeEvent - quitting application";
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

    // 캡처 오류 시에만 나타나는 재시도 버튼 (평소엔 숨김)
    retryBtn_ = new QPushButton("🔄 재시도");
    retryBtn_->setMinimumSize(90, 28);
    retryBtn_->setCursor(Qt::PointingHandCursor);
    retryBtn_->setStyleSheet(
        "QPushButton { background: #DC2626; color: white; border: none;"
        "  border-radius: 4px; font-size: 9pt; }"
        "QPushButton:hover { background: #B91C1C; }");
    retryBtn_->hide();

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
    sbl->addWidget(retryBtn_);
    sbl->addWidget(adminBtn_);

    connect(adminBtn_, &QPushButton::clicked,
            this, &KioskWindow::goAdmin);

    // 재시도: 무선랜 복구 후 운영자가 눌러 캡처를 재개한다. 캡처 워커 스레드는 죽지
    // 않고 idle 로 살아 있으므로, captureRetryRequested 시그널이 run() 을 다시 호출한다.
    connect(retryBtn_, &QPushButton::clicked, this, [this]() {
        LOG(INFO) << "KioskWindow retry button clicked, requesting capture restart";
        retryBtn_->hide();
        scanStatusLabel_->setText("🟡 재연결 시도 중...");
        scanStatusLabel_->setStyleSheet(
            "QLabel { color: #92400E; font-size: 10pt; }");
        // captureFatal_ 은 성공(onCaptureStarted) 시까지 유지 → 재시도 실패 시 경고창 중복 방지
        emit captureRetryRequested();
    });
}

// ════════════════════════════════════════════════
//  화면 전환 + 음성 재생
// ════════════════════════════════════════════════

void KioskWindow::goPhase1() {
    LOG(INFO) << "KioskWindow::goPhase1";
    stack_->setCurrentIndex(0);
    setChromeVisible(true);
    updatePhaseIndicator(1);
    isUpdateMode_  = false;
    phase1Entered_ = true;
    AudioPlayer::instance().play({":/audio/scan_guide.wav"});
}

void KioskWindow::goPhase2Register(QString macStr, QString timestamp)
{
    LOG(INFO) << "KioskWindow::goPhase2Register mac=" << macStr.toStdString();
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
    LOG(INFO) << "KioskWindow::goPhase2Update mac=" << macStr.toStdString();
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
    LOG(INFO) << "KioskWindow::goPhase3 isUpdateMode=" << isUpdateMode_;
    stack_->setCurrentIndex(2);
    updatePhaseIndicator(3);
    p3_->showCompleted(isUpdateMode_);

    if (isUpdateMode_)
        AudioPlayer::instance().play({":/audio/update_complete.wav"});
    else
        AudioPlayer::instance().play({":/audio/register_complete.wav"});
}

void KioskWindow::goAdmin() {
    LOG(INFO) << "KioskWindow::goAdmin";
    admin_->refresh();
    admin_->focusSearch();
    stack_->setCurrentIndex(3);
    setChromeVisible(false);
}

// ── Phase2 확인 처리 ──
// 서버 우선: ApiClient 로 REST 전송 → 성공 응답 콜백에서 로컬 캐시 저장 + 화면 전환.
// api_ 가 없으면(오프라인) 기존처럼 로컬 DB 에만 즉시 반영.
void KioskWindow::onPhase2Confirmed(QString macStr, QString name,
                                    QString phone, QString deviceType)
{
    LOG(INFO) << "KioskWindow::onPhase2Confirmed mac=" << macStr.toStdString()
              << " name=" << name.toStdString() << " phone=" << phone.toStdString()
              << " type=" << deviceType.toStdString()
              << " isUpdateMode=" << isUpdateMode_;

    // 비동기 응답 콜백에서 로컬 캐시에 반영하기 위해 입력값 보관
    pendingMac_   = macStr;
    pendingName_  = name;
    pendingPhone_ = phone;
    pendingType_  = deviceType;

    int typeCode = Db::typeStringToCode(deviceType.toStdString());
    QString now  = QDateTime::currentDateTime().toString("yyMMdd'T'HHmmss");

    if (api_) {
        // 서버 전송 (응답은 onRegisterSuccess/Failed, onUpdateSuccess/Failed 에서 처리)
        // 이 흐름의 응답만 commitConfirmed 로 이어지도록 플래그를 세운다.
        awaitingApiCommit_ = true;
        scanStatusLabel_->setText("⏳ 서버 전송 중...");
        if (isUpdateMode_) {
            LOG(INFO) << "onPhase2Confirmed -> ApiClient::updateDevice mac=" << macStr.toStdString();
            api_->updateDevice(macStr, name, phone, typeCode, now);
        } else {
            LOG(INFO) << "onPhase2Confirmed -> ApiClient::registerDevice mac=" << macStr.toStdString();
            api_->registerDevice(macStr, name, phone, typeCode, pendingRssi_, now);
        }
        return;  // 응답 대기
    }

    // ── api_ 미설정(오프라인) 폴백: 로컬 DB 에만 저장 ──
    LOG(WARNING) << "onPhase2Confirmed: no ApiClient, writing to local DB only";
    commitConfirmed();
}

// pending* 멤버를 로컬 캐시(DB)에 반영하고 Phase1 행을 갱신한 뒤 Phase3 로 전환.
// 서버 성공 응답 또는 오프라인 폴백에서 호출된다.
void KioskWindow::commitConfirmed() {
    QString now  = QDateTime::currentDateTime().toString("yyMMdd'T'HHmmss");
    int typeCode = Db::typeStringToCode(pendingType_.toStdString());
    LOG(INFO) << "KioskWindow::commitConfirmed mac=" << pendingMac_.toStdString()
              << " isUpdateMode=" << isUpdateMode_;

    if (isUpdateMode_) {
        db_->updateStation(Mac(pendingMac_.toUtf8().constData()),
                           pendingName_.toStdString(),
                           pendingPhone_.toStdString(),
                           typeCode);
    } else {
        StationEntry se;
        se.mac          = Mac(pendingMac_.toUtf8().constData());
        se.name         = pendingName_.toStdString();
        se.phoneNum     = pendingPhone_.toStdString();
        se.type         = typeCode;
        se.registeredAt = now.toStdString();
        se.updatedAt    = now.toStdString();
        db_->addStation(se);
    }

    // DB 저장 직후 Phase1 테이블 갱신:
    // seenInSession_ 때문에 candidateFound가 재발생하지 않으므로
    // 직접 행을 교체해야 "등록" 버튼 → 이름/전화번호 + "변경" 버튼으로 반영된다.
    p1_->removeCandidate(pendingMac_);
    auto updated = db_->searchStations(pendingMac_.toStdString());
    if (!updated.empty()) {
        const auto& s = updated[0];
        p1_->showDuplicateNotice(
            pendingMac_,
            QString::fromStdString(s.name),
            QString::fromStdString(s.phoneNum),
            pendingRssi_,
            QString::fromStdString(s.registeredAt));
    }

    scanStatusLabel_->setText("🟢 수집 중...");
    goPhase3();
}

// ── ApiClient 응답 슬롯 ──
void KioskWindow::onRegisterSuccess(QString mac) {
    LOG(INFO) << "KioskWindow::onRegisterSuccess mac=" << mac.toStdString();
    if (!awaitingApiCommit_) {
        // AdminPage 등 Phase2 외 경로의 응답: 해당 경로가 이미 DB 를 처리했으므로 무시.
        LOG(INFO) << "onRegisterSuccess: not a Phase2 flow, skip commit";
        return;
    }
    awaitingApiCommit_ = false;
    commitConfirmed();  // 서버 성공 → 로컬 캐시에 저장
}

void KioskWindow::onRegisterFailed(QString mac, QString reason) {
    LOG(ERROR) << "KioskWindow::onRegisterFailed mac=" << mac.toStdString()
               << " reason=" << reason.toStdString();
    if (!awaitingApiCommit_) {
        LOG(INFO) << "onRegisterFailed: not a Phase2 flow, skip UI handling";
        return;
    }
    awaitingApiCommit_ = false;
    scanStatusLabel_->setText("🟢 수집 중...");
    QMessageBox::warning(this, "등록 실패",
                         QString("서버 등록에 실패했습니다.\n%1").arg(reason));
    // Phase2 유지 (화면 전환하지 않음)
}

void KioskWindow::onUpdateSuccess(QString mac, QString updatedAt) {
    LOG(INFO) << "KioskWindow::onUpdateSuccess mac=" << mac.toStdString()
              << " updatedAt=" << updatedAt.toStdString();
    if (!awaitingApiCommit_) {
        // AdminPage 수정 응답: onEditSelected 가 이미 db_->updateStation 으로 반영했으므로
        // 여기서 commitConfirmed 를 타면 빈 pending* 으로 잘못된 행이 생긴다. 무시한다.
        LOG(INFO) << "onUpdateSuccess: not a Phase2 flow (admin edit?), skip commit";
        return;
    }
    awaitingApiCommit_ = false;
    commitConfirmed();  // 서버 성공 → 로컬 캐시에 반영
}

void KioskWindow::onUpdateFailed(QString mac, QString reason) {
    LOG(ERROR) << "KioskWindow::onUpdateFailed mac=" << mac.toStdString()
               << " reason=" << reason.toStdString();
    if (!awaitingApiCommit_) {
        LOG(INFO) << "onUpdateFailed: not a Phase2 flow, skip UI handling";
        return;
    }
    awaitingApiCommit_ = false;
    scanStatusLabel_->setText("🟢 수집 중...");
    QMessageBox::warning(this, "변경 실패",
                         QString("서버 변경에 실패했습니다.\n%1").arg(reason));
    // Phase2 유지
}

void KioskWindow::onDeviceListFetched(QStringList macs) {
    // 서버 우선 동기화: 서버가 보유한 MAC 목록.
    // /lists 는 MAC 만 반환하므로 이름/전화번호가 없는 항목은 로컬 캐시에 채울 수 없음.
    // 대신 MAC 집합을 보관해 두고, 로컬 DB 에 없더라도 서버에 있으면 중복으로 판정한다.
    serverMacs_.clear();
    for (const QString& m : macs)
        serverMacs_.insert(m.toUpper());
    LOG(INFO) << "KioskWindow::onDeviceListFetched serverCount=" << macs.size();
}

void KioskWindow::onDeviceListFailed(QString reason) {
    LOG(WARNING) << "KioskWindow::onDeviceListFailed reason=" << reason.toStdString();
}

// ── 신규 MAC 감지: 로컬 DB 중복 확인 ──
void KioskWindow::onCandidateFound(QString macStr, int rssi, QString timestamp)
{
    LOG(INFO) << "KioskWindow::onCandidateFound mac=" << macStr.toStdString()
              << " rssi=" << rssi << " ts=" << timestamp.toStdString();
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
            LOG(INFO) << "KioskWindow::onCandidateFound duplicate(local) mac=" << macStr.toStdString();
            AudioPlayer::instance().play({":/audio/duplicate_notice.wav"});
        }
    } else if (serverMacs_.contains(macStr.toUpper())) {
        // 로컬 DB 에는 없지만 서버에는 등록된 MAC: 신규가 아니라 "이미 등록됨"으로 처리.
        // /lists 는 이름/전화를 주지 않으므로 해당 칸은 비워서 안내한다.
        p1_->showDuplicateNotice(macStr, QString(), QString(), rssi, QString());
        LOG(INFO) << "KioskWindow::onCandidateFound duplicate(server) mac=" << macStr.toStdString();
        AudioPlayer::instance().play({":/audio/duplicate_notice.wav"});
    } else {
        LOG(INFO) << "KioskWindow::onCandidateFound new mac=" << macStr.toStdString();
        p1_->addCandidate(macStr, rssi, timestamp);
        AudioPlayer::instance().play({":/audio/mac_detected.wav"});
    }
}

void KioskWindow::onCaptureError(QString msg) {
    LOG(ERROR) << "KioskWindow::onCaptureError msg=" << msg.toStdString();
    // 캡처 오류가 나도 프로그램을 종료하지 않는다. 상태표시줄에 에러 사유를 남기고
    // '재시도' 버튼을 띄워, 무선랜 복구 후 운영자가 직접 캡처를 재개할 수 있게 한다.
    scanStatusLabel_->setText("🔴 캡처 오류: " + msg);
    scanStatusLabel_->setStyleSheet(
        "QLabel { color: #B91C1C; font-size: 10pt; }");
    retryBtn_->show();

    if (captureFatal_) return;  // 같은 오류 에피소드에서 경고창 중복 표시 방지
    captureFatal_ = true;

    LOG(ERROR) << "KioskWindow::onCaptureError capture stopped (program keeps running)";
    QMessageBox::warning(this, "캡처 중단",
        QString("무선랜 캡처가 중단되었습니다. 프로그램은 계속 실행됩니다.\n\n"
                "무선랜을 복구한 뒤 하단의 '재시도' 버튼을 눌러 주세요.\n\n사유: %1").arg(msg));
}

void KioskWindow::onCaptureStarted() {
    // 최초 시작 또는 재시도 성공 → 정상 수집 상태로 복귀
    LOG(INFO) << "KioskWindow::onCaptureStarted capture running";
    captureFatal_ = false;
    retryBtn_->hide();
    scanStatusLabel_->setText("🟢 수집 중...");
    scanStatusLabel_->setStyleSheet(
        "QLabel { color: #065F46; font-size: 10pt; }");
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