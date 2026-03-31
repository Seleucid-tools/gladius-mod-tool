#pragma once
#include <QHash>
#include <QString>

// Loads gladiushashes.json (companion file next to the executable).
// Maps CRC32 hash -> file path string, used by BecTool during unpack.
class GladiusHashes
{
public:
    // Load from the companion file. Safe to call multiple times; only loads once.
    // Returns false if the file cannot be found or parsed.
    static bool load();

    // Look up a hash. Returns the known file path, or "unknown-<n>.bin" if not found.
    static QString lookup(quint32 hash, int unknownIndex);

    // True after a successful load().
    static bool isLoaded();

private:
    static QHash<quint32, QString> s_map;
    static bool s_loaded;
};
