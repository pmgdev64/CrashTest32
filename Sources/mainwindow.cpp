// mainwindow.cpp
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFont>
#include <QFrame>
#include <QString>
#include <winternl.h>  // Cho UNICODE_STRING

// ?? Bugcheck table ???????????????????????????????????????????????
const BugcheckMode MainWindow::MODES[] = {
    { "CRITICAL_PROCESS_DIED",
      "A critical system process terminated unexpectedly.",
      (NTSTATUS)0xC000021A },
    { "ASSERTION_FAILURE",
      "A kernel-mode assertion check failed.",
      (NTSTATUS)0xC0000420 },
    { "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED",
      "A system thread generated an exception which was not handled.",
      (NTSTATUS)0xC000026C },
    { "IRQL_NOT_LESS_OR_EQUAL",
      "Invalid IRQL — classic driver crash signature.",
      (NTSTATUS)0xC0000005 },
    { "UNEXPECTED_KERNEL_MODE_TRAP",
      "Unexpected trap occurred in kernel mode.",
      (NTSTATUS)0xC0000091 },
    { "KMODE_EXCEPTION_NOT_HANDLED",
      "A kernel mode exception was not handled by any handler.",
      (NTSTATUS)0xC0000094 },
    { "MANUALLY_INITIATED_CRASH",
      "Manual crash trigger — no actual corruption.",
      (NTSTATUS)0xDEADDEAD },
};
const int MainWindow::MODE_COUNT = sizeof(MODES) / sizeof(MODES[0]);

// ?? Constructor ??????????????????????????????????????????????????
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      RtlAdjustPrivilege(NULL),
      NtRaiseHardError(NULL),
      privilegeOk(false),
      countdown(0)
{
    setWindowTitle("CrashTest - BSOD Trigger Tool");
    setFixedSize(520, 480);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(onCountdownTick()));

    setupMenuBar();
    setupUI();
    setupNtdll();
}

MainWindow::~MainWindow() {}

// ?? Menu bar ?????????????????????????????????????????????????????
void MainWindow::setupMenuBar() {
    QMenuBar* mb = menuBar();

    QMenu* fileMenu = mb->addMenu("&File");
    QAction* exitAct = new QAction("E&xit", this);
    connect(exitAct, SIGNAL(triggered()), this, SLOT(onExit()));
    fileMenu->addAction(exitAct);

    QMenu* helpMenu = mb->addMenu("&Help");
    QAction* aboutAct = new QAction("&About", this);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(onAbout()));
    helpMenu->addAction(aboutAct);
}

// ?? UI layout ????????????????????????????????????????????????????
void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* root = new QVBoxLayout(central);
    root->setSpacing(8);
    root->setMargin(10);

    // ?? Bugcheck selector group ??
    QGroupBox* selectGroup = new QGroupBox("Bugcheck Mode", central);
    QGridLayout* grid = new QGridLayout(selectGroup);

    modeCombo = new QComboBox(selectGroup);
    for (int i = 0; i < MODE_COUNT; i++)
        modeCombo->addItem(MODES[i].name);
    connect(modeCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onModeChanged(int)));

    QLabel* codeTitle = new QLabel("Stop Code:", selectGroup);
    codeLabel = new QLabel("0xC000021A", selectGroup);
    QFont codeFont = codeLabel->font();
    codeFont.setBold(true);
    codeFont.setFamily("Courier New");
    codeLabel->setFont(codeFont);
    codeLabel->setStyleSheet("color: #cc0000;");

    QLabel* descTitle = new QLabel("Description:", selectGroup);
    descLabel = new QLabel(selectGroup);
    descLabel->setWordWrap(true);

    grid->addWidget(new QLabel("Mode:", selectGroup), 0, 0);
    grid->addWidget(modeCombo,   0, 1);
    grid->addWidget(codeTitle,   1, 0);
    grid->addWidget(codeLabel,   1, 1);
    grid->addWidget(descTitle,   2, 0, Qt::AlignTop);
    grid->addWidget(descLabel,   2, 1);
    grid->setColumnStretch(1, 1);

    // ?? Trigger group ??
    QGroupBox* triggerGroup = new QGroupBox("Trigger", central);
    QGridLayout* tGrid = new QGridLayout(triggerGroup);

    delaySpin = new QSpinBox(triggerGroup);
    delaySpin->setRange(1, 30);
    delaySpin->setValue(3);
    delaySpin->setSuffix(" sec");

    crashBtn  = new QPushButton("Crash Now!", triggerGroup);
    cancelBtn = new QPushButton("Cancel", triggerGroup);
    cancelBtn->setEnabled(false);

    crashBtn->setStyleSheet(
        "QPushButton { background-color: #cc2222; color: white; "
        "font-weight: bold; padding: 6px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #ee3333; }"
        "QPushButton:disabled { background-color: #888; }"
    );
    cancelBtn->setStyleSheet(
        "QPushButton { padding: 6px; border-radius: 3px; }"
    );

    connect(crashBtn,  SIGNAL(clicked()), this, SLOT(onCrashClicked()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCountdownTick()));

    tGrid->addWidget(new QLabel("Delay:", triggerGroup), 0, 0);
    tGrid->addWidget(delaySpin,  0, 1);
    tGrid->addWidget(crashBtn,   1, 0);
    tGrid->addWidget(cancelBtn,  1, 1);

    // ?? Log view ??
    QGroupBox* logGroup = new QGroupBox("Log", central);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    logView = new QTextEdit(logGroup);
    logView->setReadOnly(true);
    logView->setFont(QFont("Courier New", 9));
    logView->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    logView->setFixedHeight(150);
    logLayout->addWidget(logView);

    // ?? Status bar ??
    statusLabel = new QLabel("Ready");
    statusBar()->addWidget(statusLabel);

    root->addWidget(selectGroup);
    root->addWidget(triggerGroup);
    root->addWidget(logGroup);

    // Init first mode
    onModeChanged(0);
}

// ?? Init ntdll ???????????????????????????????????????????????????
void MainWindow::setupNtdll() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        appendLog("[ERROR] Failed to get ntdll.dll handle.");
        return;
    }

    RtlAdjustPrivilege = (pRtlAdjustPrivilege)
        GetProcAddress(ntdll, "RtlAdjustPrivilege");
    NtRaiseHardError = (pNtRaiseHardError)
        GetProcAddress(ntdll, "NtRaiseHardError");

    if (!RtlAdjustPrivilege || !NtRaiseHardError) {
        appendLog("[ERROR] Failed to resolve ntdll functions.");
        return;
    }

    BOOLEAN was = FALSE;
    NTSTATUS st = RtlAdjustPrivilege(19, TRUE, FALSE, &was);
    if (st == 0) {
        privilegeOk = true;
        appendLog("[OK] SeShutdownPrivilege acquired.");
        appendLog("[OK] Running as Administrator.");
        statusLabel->setText("Ready — Administrator");
    } else {
        appendLog("[WARN] SeShutdownPrivilege denied.");
        appendLog("[WARN] Please run as Administrator!");
        statusLabel->setText("WARNING: Not Administrator");
        statusLabel->setStyleSheet("color: red;");
        crashBtn->setEnabled(false);
    }
}

// ?? Slots ????????????????????????????????????????????????????????
void MainWindow::onModeChanged(int index) {
    if (index < 0 || index >= MODE_COUNT) return;
    codeLabel->setText(QString("0x%1")
        .arg((uint)MODES[index].code, 8, 16, QChar('0')).toUpper());
    descLabel->setText(MODES[index].description);
}

void MainWindow::onCrashClicked() {
    if (timer->isActive()) return;

    countdown = delaySpin->value();
    crashBtn->setEnabled(false);
    cancelBtn->setEnabled(true);
    modeCombo->setEnabled(false);
    delaySpin->setEnabled(false);

    int idx = modeCombo->currentIndex();
    appendLog(QString("[>>] Triggering: %1").arg(MODES[idx].name));
    appendLog(QString("[>>] Code: 0x%1")
        .arg((uint)MODES[idx].code, 8, 16, QChar('0')).toUpper());
    appendLog(QString("[>>] Countdown: %1 sec").arg(countdown));

    timer->start(1000);
    statusLabel->setText(QString("Triggering in %1...").arg(countdown));
}

void MainWindow::onCountdownTick() {
    // Cancel ðý?c g?i t? cancelBtn
    if (!timer->isActive() && countdown > 0) return;

    countdown--;

    if (countdown <= 0 && timer->isActive()) {
        timer->stop();
        triggerBSOD();
        return;
    }

    if (!timer->isActive()) {
        // Cancel
        appendLog("[--] Cancelled by user.");
        crashBtn->setEnabled(true);
        cancelBtn->setEnabled(false);
        modeCombo->setEnabled(true);
        delaySpin->setEnabled(true);
        statusLabel->setText("Ready — Administrator");
        return;
    }

    statusLabel->setText(QString("Triggering in %1...").arg(countdown));
    appendLog(QString("[..] %1").arg(countdown));
}

void MainWindow::triggerBSOD() {
    int idx = modeCombo->currentIndex();
    appendLog("[!!] BOOM — calling NtRaiseHardError...");
    
    // T?o thông báo Bugcheck
    WCHAR message[512];
    wsprintfW(message, L"CrashTest32: %S (0x%08X)", 
              MODES[idx].name, MODES[idx].code);
    
    UNICODE_STRING bugcheckString;
    bugcheckString.Length = (USHORT)(wcslen(message) * sizeof(WCHAR));
    bugcheckString.MaximumLength = sizeof(message);
    bugcheckString.Buffer = message;
    
    // Tham s?
    ULONG_PTR arguments[4] = {
        (ULONG_PTR)&bugcheckString,
        (ULONG_PTR)MODES[idx].code,
        0,
        0
    };
    
    ULONG response = 0;
    
    // G?i NtRaiseHardError v?i OptionShutdownSystem (6)
    // S? gây BSOD ngay l?p t?c v?i thông báo
    NtRaiseHardError(MODES[idx].code, 1, 0, arguments, 6, &response);
    
    // N?u ð?n ðây là có l?i
    appendLog("[ERR] Failed to trigger BSOD - insufficient privileges?");
    crashBtn->setEnabled(true);
    cancelBtn->setEnabled(false);
    modeCombo->setEnabled(true);
    delaySpin->setEnabled(true);
    statusLabel->setText("Error — check log");
}

void MainWindow::appendLog(const QString& msg) {
    logView->append(msg);
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About CrashTest",
        "<b>CrashTest v1.0</b><br>"
        "Safe BSOD Trigger Tool<br><br>"
        "Uses NtRaiseHardError from ntdll.dll<br>"
        "Requires Administrator privileges.<br><br>"
        "FragmentGames Studio");
}

void MainWindow::onExit() {
    close();
}
