#pragma once
#include <QObject>
#include <QString>
#include <QProcess>

// Wraps the extract-xiso binary (expected alongside the app executable).
// Handles both unpack (-x) and repack (-c) operations.
class XisoRunner : public QObject
{
    Q_OBJECT
public:
    explicit XisoRunner(QObject *parent = nullptr);

    // Returns path to extract-xiso binary (searches next to app exe).
    // Empty string if not found.
    static QString binaryPath();

public slots:
    // Unpack: extract-xiso <isoPath>   (creates a dir next to the iso)
    void unpack(const QString &isoPath, const QString &workingDir);

    // Repack: extract-xiso -c <dirPath>  (creates <dirPath>.iso)
    void repack(const QString &dirPath, const QString &workingDir);

signals:
    void output(const QString &text);
    void error(const QString &text);
    void finished(bool success);

private:
    void startProcess(const QStringList &args, const QString &workingDir);
    QProcess *m_process = nullptr;
};
