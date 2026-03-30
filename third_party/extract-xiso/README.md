# third_party/extract-xiso

Xbox ISO extraction and creation tool by in@fishtank.com.

Source: https://github.com/XboxDev/extract-xiso (v2.7.1)
License: BSD-style (see LICENSE.TXT)

This directory contains the unmodified `extract-xiso.c` source. The
`CMakeLists.txt` has been patched to remove the top-level `project()` call
so it works cleanly as a CMake `add_subdirectory()` target. Two benign legacy C
compiler warnings are suppressed via `target_compile_options`. No other source
files were modified.
