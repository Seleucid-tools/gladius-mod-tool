#include "MainWindow.h"
#include "PipelineTab.h"
#include "EditorTab.h"
#include "SchoolBuilderTab.h"
#include "LogPanel.h"
#include "XisoRunner.h"
#include "Ps2IsoRunner.h"

#include <QTabWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QSettings>
#include <QMessageBox>
#include <QApplication>
#include <QStatusBar>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Gladius Mod Tool v0.7");
    setMinimumSize(900, 600);

    // ── Shared log panel ──────────────────────────────────────────────────────
    m_logPanel = new LogPanel(this);
    m_logPanel->setMinimumHeight(120);

    // ── Platform tabs (top) ───────────────────────────────────────────────────
    m_platformTabs = new QTabWidget(this);

    m_gcTab   = new PipelineTab(Platform::GC,   m_logPanel, this);
    m_xboxTab = new PipelineTab(Platform::Xbox, m_logPanel, this);
    m_ps2Tab  = new PipelineTab(Platform::PS2,  m_logPanel, this);
    m_editorTab  = new EditorTab(this);
    m_schoolTab  = new SchoolBuilderTab(this);

    m_platformTabs->addTab(m_gcTab,     "Gamecube");
    m_platformTabs->addTab(m_xboxTab,   "Xbox");
    m_platformTabs->addTab(m_ps2Tab,    "PS2");
    m_platformTabs->addTab(m_editorTab, "File editor");
    m_platformTabs->addTab(m_schoolTab, "School Builder");

    // School builder follows the editor's working directory (pipeline or manual browse)
    connect(m_editorTab, &EditorTab::workingDirChanged,
            m_schoolTab, &SchoolBuilderTab::setRootPath);
    connect(m_editorTab, &EditorTab::workingDirChanged,
            this, [this](const QString &moddedDir, const QString &vanillaDir) {
        m_lastModdedDir  = moddedDir;
        m_lastVanillaDir = vanillaDir;
    });

    // When either pipeline finishes an unpack, populate editor and switch to it
    auto wireUnpack = [this](PipelineTab *tab) {
        connect(tab, &PipelineTab::unpackComplete,
                m_editorTab, &EditorTab::setRootPath);
        connect(tab, &PipelineTab::unpackComplete,
                this, [this](const QString &, const QString &) {
            m_platformTabs->setCurrentWidget(m_editorTab);
        });
    };
    wireUnpack(m_gcTab);
    wireUnpack(m_xboxTab);
    wireUnpack(m_ps2Tab);

    // ── Vertical splitter: tabs above, log below ───────────────────────────────
    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->addWidget(m_platformTabs);
    m_splitter->addWidget(m_logPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    setCentralWidget(m_splitter);

    // ── Menu ──────────────────────────────────────────────────────────────────
    buildMenu();

    // ── Status bar ────────────────────────────────────────────────────────────
    QString xisoStatus = XisoRunner::binaryPath().isEmpty()
        ? "extract-xiso: not found"
        : "extract-xiso: found";
    QString ps2Status = Ps2IsoRunner::binaryPath().isEmpty()
        ? "ps2isotool: not found"
        : "ps2isotool: found";
    statusBar()->showMessage(xisoStatus + "   |   " + ps2Status);

    restoreSettings();
}

void MainWindow::buildMenu()
{
    auto *fileMenu = menuBar()->addMenu("&File");

    auto *clearLog = new QAction("Clear log", this);
    connect(clearLog, &QAction::triggered, m_logPanel, &LogPanel::clear);
    fileMenu->addAction(clearLog);

    fileMenu->addSeparator();

    auto *quitAction = new QAction("&Quit", this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAction);

    auto *editMenu = menuBar()->addMenu("&Edit");

    auto *findAction = new QAction("&Find…", this);
    findAction->setShortcut(QKeySequence("Ctrl+F"));
    findAction->setToolTip("Open the find bar in the file editor");
    connect(findAction, &QAction::triggered, m_editorTab, &EditorTab::showFindBar);
    editMenu->addAction(findAction);

    auto *helpMenu = menuBar()->addMenu("&Help");
    auto *about = new QAction("About", this);
    connect(about, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "Gladius Mod Tool",
            "<b>Gladius Mod Tool v0.7</b><br>"
            "Qt6 / C++ front-end for the Gladius modding toolkit.<br><br>"
            "Python scripts embedded from original toolkit by the Gladius modding community.<br>"
            "extract-xiso compiled from open-source.<br><br>"
            "Linux native build — no Wine required.");
    });
    helpMenu->addAction(about);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void MainWindow::restoreSettings()
{
    QSettings s("GladiusModTool", "MainWindow");
    if (s.contains("geometry"))
        restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("splitter"))
        m_splitter->restoreState(s.value("splitter").toByteArray());

    m_lastModdedDir  = s.value("editor/moddedDir").toString();
    m_lastVanillaDir = s.value("editor/vanillaDir").toString();

    if (!m_lastModdedDir.isEmpty() && QDir(m_lastModdedDir).exists()) {
        // setRootPath emits workingDirChanged, which wires school builder automatically
        m_editorTab->setRootPath(m_lastModdedDir, m_lastVanillaDir);
    }
}

void MainWindow::saveSettings()
{
    QSettings s("GladiusModTool", "MainWindow");
    s.setValue("geometry", saveGeometry());
    s.setValue("splitter", m_splitter->saveState());

    if (!m_lastModdedDir.isEmpty()) {
        s.setValue("editor/moddedDir",  m_lastModdedDir);
        s.setValue("editor/vanillaDir", m_lastVanillaDir);
    }
}
