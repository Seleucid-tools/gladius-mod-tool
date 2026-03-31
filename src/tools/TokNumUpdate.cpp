#include "TokNumUpdate.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace Tools {

static bool updateTok(const QString &path, const QString &createKeyword, int numEntriesLine,
                      const LogCb &out, const LogCb &err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err(QStringLiteral("Cannot open: ") + path);
        return false;
    }
    QStringList lines;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd())
        lines.append(in.readLine());
    f.close();

    int count = 0;
    for (const QString &line : lines) {
        if (line.startsWith(createKeyword))
            ++count;
    }

    if (numEntriesLine >= lines.size()) {
        err(QStringLiteral("NUMENTRIES line index out of range in: ") + path);
        return false;
    }
    lines[numEntriesLine] = QStringLiteral("NUMENTRIES: ") + QString::number(count);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        err(QStringLiteral("Cannot write: ") + path);
        return false;
    }
    QTextStream os(&f);
    os.setEncoding(QStringConverter::Utf8);
    for (int i = 0; i < lines.size(); ++i) {
        os << lines[i];
        if (i < lines.size() - 1)
            os << '\n';
    }
    f.close();

    out(QStringLiteral("Updated NUMENTRIES=%1 in %2").arg(count).arg(QFileInfo(path).fileName()));
    return true;
}

bool tokNumUpdate(const QString &configDir, const LogCb &out, const LogCb &err)
{
    bool ok = true;
    ok &= updateTok(configDir + "skills.tok", QStringLiteral("SKILLCREATE"), 6, out, err);
    ok &= updateTok(configDir + "items.tok",  QStringLiteral("ITEMCREATE"),  3, out, err);
    return ok;
}

} // namespace Tools
