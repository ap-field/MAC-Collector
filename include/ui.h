#pragma once
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
class Db;

// Phase 1 ─ 후보 표시
class Phase1Widget : public QWidget {
    Q_OBJECT
public:
    explicit Phase1Widget(QWidget* parent = nullptr);
    void addCandidate(const QString& macStr, int rssi);
    void removeCandidate(const QString& macStr);
    void clearCandidates();
    int  candidateCount() const;

signals:
    void registerRequested(QString macStr);
    void candidateCountChanged(int count);

private slots:
    void onRegisterButtonClicked();

private:
    QTableWidget* table_;
};

// Phase 2 ─ 정보 입력
class Phase2Widget : public QWidget {
    Q_OBJECT
public:
    explicit Phase2Widget(QWidget* parent = nullptr);
    void setTargetMac(const QString& macStr);
    void focusFirstInput();

signals:
    void confirmed(QString macStr, QString name, QString phone, QString deviceType);
    void canceled();

private slots:
    void onConfirm();
    void onCancel();

private:
    QLabel*      macLabel_;
    QLineEdit*   nameEdit_;
    QLineEdit*   phoneEdit_;
    QComboBox*   typeCombo_;
    QPushButton* okBtn_;
    QPushButton* cancelBtn_;
};

// Phase 3 ─ 등록 완료
class Phase3Widget : public QWidget {
    Q_OBJECT
public:
    explicit Phase3Widget(QWidget* parent = nullptr);
    void showCompleted();

signals:
    void autoReturn();

private:
    QLabel* msgLabel_;
};

// AdminPage
class AdminPage : public QWidget {
    Q_OBJECT
public:
    explicit AdminPage(Db* db, QWidget* parent = nullptr);
    void refresh();
    void focusSearch();

signals:
    void backRequested();

private slots:
    void onSearch();
    void onDeleteSelected();
    void onExportCsv();
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
    QPushButton*  exportBtn_;
    QPushButton*  backBtn_;

    void reloadStations(const QString& keyword);
    void reloadAps(const QString& keyword);
    void reloadUsers();
};

// KioskWindow
class KioskWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit KioskWindow(Db* db, QWidget* parent = nullptr);

public slots:
    void onCandidateFound(QString macStr, int rssi);
    void onCaptureError(QString msg);

private slots:
    void goPhase1();
    void goPhase2(QString macStr);
    void goPhase3();
    void goAdmin();
    void onPhase2Confirmed(QString macStr, QString name, QString phone, QString deviceType);
    void updateElapsed();
    void updateDeviceCount(int count);

private:
    void buildHeader();
    void buildStatusBar();
    void makePhaseStep(QWidget*& wOut, QLabel*& circleOut, QLabel*& textOut,
                       const QString& numText, const QString& labelText);
    void setActivePhase(int phase);       // 1, 2, 3
    void setChromeVisible(bool visible);  // hide header/status bar for admin page
    void updatePhaseIndicator(int activeStep);

    Db* db_;

    // Header
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
    QLabel*  scanningLabel_  = nullptr;

    // Stack
    QStackedWidget* stack_ = nullptr;
    Phase1Widget*   p1_    = nullptr;
    Phase2Widget*   p2_    = nullptr;
    Phase3Widget*   p3_    = nullptr;
    AdminPage*      admin_ = nullptr;

    // Status bar
    QWidget*     statusBar_        = nullptr;
    QLabel*      scanStatusLabel_  = nullptr;
    QLabel*      deviceCountLabel_ = nullptr;
    QLabel*      elapsedLabel_     = nullptr;
    QPushButton* adminBtn_         = nullptr;

    QTimer* elapsedTimer_   = nullptr;
    int     elapsedSeconds_ = 0;
};