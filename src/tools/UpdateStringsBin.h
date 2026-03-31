#pragma once
#include "ToolTypes.h"

namespace Tools {

// Converts lookuptext_eng.txt → lookuptext_eng.bin.
// configDir must end with '/' and contain lookuptext_eng.txt.
bool updateStringsBin(const QString &configDir, const LogCb &out, const LogCb &err);

} // namespace Tools
