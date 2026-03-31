#pragma once
#include "ToolTypes.h"

namespace Tools {

// Unpack a GameCube ISO to a directory.
// outFileList: base name of the file list to write (e.g. "GladiusGamecubeVanilla_FileList.txt")
bool ngcIsoUnpack(const QString &isoPath, const QString &outDir,
                  const QString &outFileList,
                  const LogCb &out, const LogCb &err);

// Repack a directory back into a GameCube ISO.
// inDir    : directory containing all extracted files
// fstFile  : path for the new fst.bin
// fstMap   : the file list produced by unpack
// outIso   : path for the output ISO
bool ngcIsoPack(const QString &inDir, const QString &fstFile,
                const QString &fstMap, const QString &outIso,
                const LogCb &out, const LogCb &err);

} // namespace Tools
