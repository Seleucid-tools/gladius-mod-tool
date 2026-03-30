#pragma once
#include <QObject>
#include <QString>
#include <QProcess>

// Wraps the ps2isotool binary (expected alongside the app executable).
// Handles both extract and build operations for PS2 UDF ISO images.
class Ps2IsoRunner : public QObject
{
    Q_OBJECT
public:
    explicit Ps2IsoRunner(QObject *parent = nullptr);

    // Returns path to ps2isotool binary (searches next to app exe).
    // Empty string if not found.
    static QString binaryPath();

public slots:
    // Extract all files from a PS2 ISO into outDir.
    // Invokes: ps2isotool extract <isoPath> <outDir>
    void extract(const QString &isoPath, const QString &outDir);

    // Build a PS2 ISO from srcDir, writing to outIso.
    // Invokes: ps2isotool build <srcDir> <outIso> <volName>
    void build(const QString &srcDir, const QString &outIso, const QString &volName);

signals:
    void output(const QString &text);
    void error(const QString &text);
    void finished(bool success);

private:
    void startProcess(const QStringList &args);
    QProcess *m_process = nullptr;
};
