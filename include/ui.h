#pragma once
#include <QDialog>
#include <QMainWindow>
#include <QString>
#include <QWidget>

class QStackedWidget;
class QTableWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QTabWidget;
class QTimer;
class QMediaPlayer;
class QAudioOutput;
class QSpinBox;
class QCloseEvent;
class Db;

// ────────── SettingsDialog ──────────
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    QString iface()         const;
    int     channel()       const;   // 0 = 변경 안함
    int     rssiThreshold() const;
    QString dbPath()        const;

private slots:
    void onOk();

private:
    QComboBox* ifaceCombo_;
    QSpinBox*  channelSpin_;
    QSpinBox*  rssiSpin_;
    QLineEdit* dbEdit_;
};

// ────────── AudioPlayer ──────────
class AudioPlayer : public QObject {
    Q_OBJECT
public:
    static AudioPlayer& instance();
    void play(const QStringList& files);
    void stop();

private:
    explicit AudioPlayer(QObject* parent = nullptr);
    void playNext();

    QMediaPlayer*  player_;
    QAudioOutput*  audioOut_;
    QStringList    queue_;
    int            queueIdx_ = 0;
};

// ────────── Phase1Widget ──────────
class Phase1Widget : public QWidget {
    Q_OBJECT
public:
    explicit Phase1Widget(Db* db, QWidget* parent = nullptr);

    void addCandidate(const QString& macStr, int rssi,
                      const QString& vendor, const QString& timestamp);
    void showDuplicateNotice(const QString& macStr,
                             const QString& ownerName,
                             const QString& phone,
                             const QString& vendor,
                             int rssi,
                             const QString& registeredAt);
    void removeCandidate(const QString& macStr);
    void clearCandidates();
    int  candidateCount() const;

signals:
    void registerRequested(QString macStr, QString vendor, QString timestamp);
    void updateRequested(QString macStr);
    void candidateCountChanged(int count);

private slots:
    void onRegisterButtonClicked();
    void onUpdateButtonClicked();

private:
    QTableWidget* table_;
    Db*           db_;
    QLineEdit*    testEdit_        = nullptr;
    QLabel*       testResultLabel_ = nullptr;
};

// ────────── Phase2Widget ──────────
class Phase2Widget : public QWidget {
    Q_OBJECT
public:
    explicit Phase2Widget(QWidget* parent = nullptr);
    void setTargetMac(const QString& macStr);
    void prefill(const QString& name, const QString& phone,
                 const QString& deviceType);
    void focusFirstInput();
    void setUpdateMode(bool isUpdate);

signals:
    void confirmed(QString macStr, QString name,
                   QString phone, QString deviceType);
    void canceled();

private slots:
    void onConfirm();
    void onCancel();

private:
    QLabel*      titleLabel_;
    QLabel*      macLabel_;
    QLineEdit*   nameEdit_;
    QLineEdit*   phoneEdit_;
    QComboBox*   typeCombo_;
    QPushButton* okBtn_;
    QPushButton* cancelBtn_;
};

// ────────── Phase3Widget ──────────
class Phase3Widget : public QWidget {
    Q_OBJECT
public:
    explicit Phase3Widget(QWidget* parent = nullptr);
    void showCompleted(bool isUpdate = false);

signals:
    void autoReturn();

private:
    QLabel* msgLabel_;
    QLabel* subLabel_;
};

// ────────── AdminPage ──────────
class AdminPage : public QWidget {
    Q_OBJECT
public:
    explicit AdminPage(Db* db, QWidget* parent = nullptr);  // ApiClient 제거
    void refresh();
    void focusSearch();

signals:
    void backRequested();

private slots:
    void onSearch();
    void onDeleteSelected();
    void onEditSelected();
    void onBack();

private:
    Db*           db_;
    QTabWidget*   tabs_;
    QTableWidget* stationTable_;
    QTableWidget* apTable_;
    QTableWidget* userTable_;
    QLineEdit*    searchEdit_;
    QPushButton*  searchBtn_;
    QPushButton*  deleteBtn_;
    QPushButton*  editBtn_;
    QPushButton*  backBtn_;

    void reloadStations(const QString& keyword);
    void reloadAps(const QString& keyword);
    void reloadUsers();
};

// ────────── KioskWindow ──────────
class KioskWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit KioskWindow(Db* db, QWidget* parent = nullptr);  // ApiClient 제거

public slots:
    void onCandidateFound(QString macStr, int rssi,
                          QString vendor, QString timestamp);
    void onCaptureError(QString msg);

protected:
    void closeEvent(QCloseEvent* event) override;  // X 버튼 종료 처리

private slots:
    void goPhase1();
    void goPhase2Register(QString macStr, QString vendor, QString timestamp);
    void goPhase2Update(QString macStr);
    void goPhase3();
    void goAdmin();
    void onPhase2Confirmed(QString macStr, QString name,
                           QString phone, QString deviceType);
    void updateElapsed();
    void updateDeviceCount(int count);

private:
    void buildHeader();
    void buildStatusBar();
    void makePhaseStep(QWidget*& wOut, QLabel*& circleOut, QLabel*& textOut,
                       const QString& numText, const QString& labelText);
    void setActivePhase(int phase);
    void setChromeVisible(bool visible);
    void updatePhaseIndicator(int activeStep);

    Db*     db_;

    bool    isUpdateMode_     = false;
    bool    phase1Entered_    = false;
    QString pendingMac_;
    QString pendingVendor_;
    QString pendingTimestamp_;
    int     pendingRssi_      = 0;

    QWidget* header_         = nullptr;
    QLabel*  titleLabel_     = nullptr;
    QWidget* phaseStep1_     = nullptr;
    QLabel*  phaseStep1Num_  = nullptr;
    QLabel*  phaseStep1Text_ = nullptr;
    QWidget* phaseStep2_     = nullptr;
    QLabel*  phaseStep2Num_  = nullptr;
    QLabel*  phaseStep2Text_ = nullptr;
    QWidget* phaseStep3_     = nullptr;
    QLabel*  phaseStep3Num_  = nullptr;
    QLabel*  phaseStep3Text_ = nullptr;

    QStackedWidget* stack_ = nullptr;
    Phase1Widget*   p1_    = nullptr;
    Phase2Widget*   p2_    = nullptr;
    Phase3Widget*   p3_    = nullptr;
    AdminPage*      admin_ = nullptr;

    QWidget*     statusBar_        = nullptr;
    QLabel*      scanStatusLabel_  = nullptr;
    QLabel*      deviceCountLabel_ = nullptr;
    QLabel*      elapsedLabel_     = nullptr;
    QPushButton* adminBtn_         = nullptr;

    QTimer* elapsedTimer_   = nullptr;
    int     elapsedSeconds_ = 0;
};