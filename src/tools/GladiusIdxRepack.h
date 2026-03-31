#pragma once
#include "ToolTypes.h"

namespace Tools {

// Repacks human-readable text files under dataDir/units/ back into binary .idx files.
// dataDir must end with '/'.
bool idxRepack(const QString &dataDir, const LogCb &out, const LogCb &err);

} // namespace Tools
