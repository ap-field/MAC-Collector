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

class Db;

// Phase 1 ─ 후보 표시
class Phase1Widget : public QWidget {
    Q_OBJECT
public:
    explicit Phase1Widget(QWidget* parent = nullptr);
    void addCandidate(const QString& macStr, int rssi);
    void removeCandidate(const QString& macStr);
    void clearCandidates();

signals:
    void registerRequested(QString macStr);
    void adminRequested();

private slots:
    void onRegisterButtonClicked();

private:
    QTableWidget* table_;
    QPushButton*  adminBtn_;
};

// Phase 2 ─ 정보 입력
class Phase2Widget : public QWidget {
    Q_OBJECT
public:
    explicit Phase2Widget(QWidget* parent = nullptr);
    void setTargetMac(const QString& macStr);

signals:
    void confirmed(QString macStr, QString name, QString phone, QString deviceType);
    void canceled();

private slots:
    void onConfirm();
    void onCancel();

private:
    QLabel*    macLabel_;
    QLineEdit* nameEdit_;
    QLineEdit* phoneEdit_;
    QComboBox* typeCombo_;
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

private:
    Db*             db_;
    QStackedWidget* stack_;
    Phase1Widget*   p1_;
    Phase2Widget*   p2_;
    Phase3Widget*   p3_;
    AdminPage*      admin_;
};