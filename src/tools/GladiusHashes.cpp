#include "GladiusHashes.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

QHash<quint32, QString> GladiusHashes::s_map;
bool GladiusHashes::s_loaded = false;

bool GladiusHashes::load()
{
    if (s_loaded)
        return true;

    // Look next to the executable
    QString path = QCoreApplication::applicationDirPath() + "/gladiushashes.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        // Try current working directory as fallback
        f.setFileName(QDir::currentPath() + "/gladiushashes.json");
        if (!f.open(QIODevice::ReadOnly))
            return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (doc.isNull() || !doc.isObject())
        return false;

    QJsonObject obj = doc.object();
    s_map.reserve(obj.size());
    for (auto it = obj.begin(); it != obj.end(); ++it)
        s_map.insert(it.key().toUInt(nullptr, 16), it.value().toString());

    s_loaded = true;
    return true;
}

QString GladiusHashes::lookup(quint32 hash, int unknownIndex)
{
    auto it = s_map.find(hash);
    if (it != s_map.end())
        return it.value();
    return QStringLiteral("unknown-%1.bin").arg(unknownIndex);
}

bool GladiusHashes::isLoaded()
{
    return s_loaded;
}
