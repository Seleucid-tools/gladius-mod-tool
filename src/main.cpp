#include "MainWindow.h"
#include "PythonRunner.h"
#include "ScriptExtractor.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GladiusModTool");
    app.setOrganizationName("GladiusModTool");
    app.setApplicationVersion("0.7.0");

    // ── Extract bundled Python scripts to temp dir ─────────────────────────────
    if (!ScriptExtractor::extractAll()) {
        QMessageBox::critical(nullptr, "Startup error",
            "Failed to extract bundled Python scripts.\n"
            "Check that the system temporary directory is writable.");
        return 1;
    }

    // ── Initialise embedded Python interpreter ────────────────────────────────
    if (!PythonRunner::init()) {
        QMessageBox::critical(nullptr, "Startup error",
            "Failed to initialise embedded Python interpreter.\n"
            "Ensure Python 3 development libraries are installed.");
        return 1;
    }

    MainWindow w;
    w.show();

    int ret = app.exec();

    // Shutdown Python cleanly after the event loop exits
    PythonRunner::shutdown();

    return ret;
}
