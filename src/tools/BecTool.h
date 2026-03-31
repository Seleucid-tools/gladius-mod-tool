#pragma once
#include "ToolTypes.h"

namespace Tools {

enum class BecPlatform { GC, Xbox, PS2 };

// Unpack a .bec archive to a directory.
bool becUnpack(const QString &becFile, const QString &outDir,
               BecPlatform platform, bool demoBec,
               const LogCb &out, const LogCb &err);

// Pack a directory into a .bec archive.
// fileList: path to the filelist.txt produced during unpack
bool becPack(const QString &inDir, const QString &outFile,
             const QString &fileList, BecPlatform platform,
             const LogCb &out, const LogCb &err);

} // namespace Tools
