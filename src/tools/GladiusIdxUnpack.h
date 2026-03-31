#pragma once
#include "ToolTypes.h"

namespace Tools {

// Unpacks binary .idx files under dataDir/units/ into human-readable text files.
// dataDir must end with '/'.
// Reads skills/items/classes from dataDir/config/*.tok.
bool idxUnpack(const QString &dataDir, const LogCb &out, const LogCb &err);

} // namespace Tools
