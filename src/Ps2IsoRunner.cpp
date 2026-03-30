#include "Ps2IsoRunner.h"
#include <QCoreApplication>
#include <QFileInfo>

Ps2IsoRunner::Ps2IsoRunner(QObject *parent)
    : QObject(parent)
{}

QString Ps2IsoRunner::binaryPath()
{
    QString appDir   = QCoreApplication::applicationDirPath();
    QString candidate = appDir + "/ps2isotool";
    if (QFileInfo::exists(candidate))
        return candidate;
    return {};
}

void Ps2IsoRunner::startProcess(const QStringList &args)
{
    QString bin = binaryPath();
    if (bin.isEmpty()) {
        emit error("ps2isotool binary not found next to the application.\n"
                   "Build it with: cd third_party/ps2isotool && dotnet publish -c Release");
        emit finished(false);
        return;
    }

    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
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

void Ps2IsoRunner::extract(const QString &isoPath, const QString &outDir)
{
    startProcess({"extract", isoPath, outDir});
}

void Ps2IsoRunner::build(const QString &srcDir, const QString &outIso, const QString &volName)
{
    startProcess({"build", srcDir, outIso, volName});
}
