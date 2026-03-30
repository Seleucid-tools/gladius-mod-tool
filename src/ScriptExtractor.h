#pragma once
#include <QString>

// Extracts all Qt-embedded Python scripts from :/scripts/ to a temp directory
// once per process lifetime.  Returns the directory path so other components
// can build "python3 <dir>/<script>" command lines.
class ScriptExtractor
{
public:
    // Call once at startup.  Safe to call multiple times (no-op after first).
    static bool extractAll();

    // Directory where scripts have been extracted (empty until extractAll succeeds).
    static QString scriptsDir();

    // Full path to a specific script by base name (e.g. "bec-tool-all.py").
    static QString scriptPath(const QString &name);

private:
    static QString s_dir;
};
