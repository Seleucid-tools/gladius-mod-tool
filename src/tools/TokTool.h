#pragma once
#include "ToolTypes.h"

namespace Tools {

// Compress a human-readable .tok file into three binary components.
// inputTok   → skills.tok (human-readable)
// outStrings → skills_strings.bin
// outLines   → skills_lines.bin
// outMain    → skills.tok.brf
bool tokCompress(const QString &inputTok,
                 const QString &outStrings,
                 const QString &outLines,
                 const QString &outMain,
                 const LogCb &out, const LogCb &err);

// Decompress the three binary components back into a human-readable .tok file.
bool tokDecompress(const QString &inStrings,
                   const QString &inLines,
                   const QString &inMain,
                   const QString &outTok,
                   const LogCb &out, const LogCb &err);

} // namespace Tools
