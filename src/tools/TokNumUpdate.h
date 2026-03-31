#pragma once
#include "ToolTypes.h"

namespace Tools {

// Updates NUMENTRIES headers in skills.tok and items.tok.
// configDir must end with '/' and contain the .tok files.
bool tokNumUpdate(const QString &configDir, const LogCb &out, const LogCb &err);

} // namespace Tools
