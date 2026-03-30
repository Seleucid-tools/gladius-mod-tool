#include "ScriptExtractor.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDirIterator>
#include <QDebug>

QString ScriptExtractor::s_dir;

bool ScriptExtractor::extractAll()
{
    if (!s_dir.isEmpty())
        return true;  // already done

    // Use a persistent temp location so re-launches are fast.
    QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    s_dir = base + "/gladius-mod-tool/scripts";

    QDir dir;
    if (!dir.mkpath(s_dir)) {
        qWarning() << "ScriptExtractor: cannot create" << s_dir;
        s_dir.clear();
        return false;
    }

    QDirIterator it(":/scripts", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString resPath = it.next();
        QFileInfo fi(resPath);
        if (fi.isDir()) continue;

        QString dest = s_dir + "/" + fi.fileName();
        // Overwrite if the embedded copy is different (dev workflow)
        if (QFile::exists(dest))
            QFile::remove(dest);

        if (!QFile::copy(resPath, dest)) {
            qWarning() << "ScriptExtractor: failed to copy" << resPath << "→" << dest;
            s_dir.clear();
            return false;
        }
        // Ensure the file is readable/executable
        QFile::setPermissions(dest,
            QFile::ReadOwner | QFile::WriteOwner |
            QFile::ReadGroup | QFile::ReadOther);
    }

    qDebug() << "ScriptExtractor: scripts extracted to" << s_dir;
    return true;
}

QString ScriptExtractor::scriptsDir()
{
    return s_dir;
}

QString ScriptExtractor::scriptPath(const QString &name)
{
    if (s_dir.isEmpty()) return {};
    return s_dir + "/" + name;
}
