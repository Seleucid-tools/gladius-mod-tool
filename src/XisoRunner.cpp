#include "XisoRunner.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

XisoRunner::XisoRunner(QObject *parent)
    : QObject(parent)
{}

QString XisoRunner::binaryPath()
{
    // Look next to the application executable first.
    QString appDir = QCoreApplication::applicationDirPath();
#ifdef _WIN32
    QString candidate = appDir + "/extract-xiso.exe";
#else
    QString candidate = appDir + "/extract-xiso";
#endif
    if (QFileInfo::exists(candidate))
        return candidate;

    // Fallback: check PATH via QProcess::execute probe (not done here for
    // simplicity — if the user needs it they can symlink it into appdir).
    return {};
}

void XisoRunner::startProcess(const QStringList &args, const QString &workingDir)
{
    QString bin = binaryPath();
    if (bin.isEmpty()) {
        emit error("extract-xiso binary not found next to the application.\n"
                   "Build it from source and place it alongside gladius-mod-tool.");
        emit finished(false);
        return;
    }

    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(workingDir);
    m_process->setProgram(bin);
    m_process->setArguments(args);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        emit output(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        emit error(QString::fromLocal8Bit(m_process->readAllStandardError()));
    });
    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
                emit finished(code == 0);
                m_process->deleteLater();
                m_process = nullptr;
            });

    m_process->start();
}

void XisoRunner::unpack(const QString &isoPath, const QString &workingDir)
{
    // extract-xiso <iso>  → creates <isobasename>/ directory
    startProcess({isoPath}, workingDir);
}

void XisoRunner::repack(const QString &dirPath, const QString &workingDir)
{
    // extract-xiso -c <dir>  → creates <dir>.iso
    startProcess({"-c", dirPath}, workingDir);
}
