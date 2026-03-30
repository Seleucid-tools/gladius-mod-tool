# extract-xiso — subdirectory build (patched from upstream)
# cmake_minimum_required intentionally omitted: this file is only ever used
# via add_subdirectory() from the parent; a nested cmake_minimum_required()
# forces CMake to re-evaluate the policy stack on every configure run, which
# combined with Qt's resource generation causes an infinite reconfigure loop.
#
# Original source: https://github.com/XboxDev/extract-xiso (v2.7.1)
# License: BSD-style (see LICENSE.TXT)

add_executable(extract-xiso extract-xiso.c)

# __LINUX__, __APPLE__, or __WINDOWS__ depending on host
string(TOUPPER "__${CMAKE_SYSTEM_NAME}__" _XISO_PLATFORM_DEFINE)
target_compile_definitions(extract-xiso PRIVATE ${_XISO_PLATFORM_DEFINE})

# Suppress two benign warnings in this legacy C code:
#   -Wint-to-pointer-cast  (avl_traverse callback cast, line 1729)
#   -Wunused-result        (chdir return value ignored, line 1179)
target_compile_options(extract-xiso PRIVATE
    -Wno-int-to-pointer-cast
    -Wno-unused-result
)
