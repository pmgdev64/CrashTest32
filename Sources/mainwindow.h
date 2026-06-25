// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QTextEdit>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QGroupBox>
#include <QStatusBar>
#include <Windows.h>

typedef NTSTATUS(NTAPI* pRtlAdjustPrivilege)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
typedef NTSTATUS(NTAPI* pNtRaiseHardError)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);

struct BugcheckMode {
    const char* name;
    const char* description;
    NTSTATUS    code;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = 0);
    ~MainWindow();

private slots:
    void onCrashClicked();
    void onCountdownTick();
    void onModeChanged(int index);
    void onAbout();
    void onExit();

private:
    void setupUI();
    void setupMenuBar();
    void setupNtdll();
    void appendLog(const QString& msg);
    void triggerBSOD();

    // Widgets
    QComboBox*   modeCombo;
    QLabel*      codeLabel;
    QLabel*      descLabel;
    QSpinBox*    delaySpin;
    QPushButton* crashBtn;
    QPushButton* cancelBtn;
    QTextEdit*   logView;
    QLabel*      statusLabel;

    // Countdown
    QTimer* timer;
    int     countdown;

    // Ntdll
    pRtlAdjustPrivilege RtlAdjustPrivilege;
    pNtRaiseHardError   NtRaiseHardError;
    bool privilegeOk;

    static const BugcheckMode MODES[];
    static const int MODE_COUNT;
};

#endif
