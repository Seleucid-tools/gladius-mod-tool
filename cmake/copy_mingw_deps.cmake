# copy_mingw_deps.cmake
# Uses ldd to find every MinGW DLL in the full transitive dependency tree of
# the app executable and copies them from MINGW_BIN to the output directory.
# windeployqt handles Qt itself but not the MinGW compiler runtime or Qt's own
# MinGW dependencies (ICU, zlib, Brotli, HarfBuzz, libgcc, libstdc++, …).
#
# Required variables (pass with -D):
#   APP_EXE     — full path to gladius-mod-tool.exe
#   MINGW_BIN   — MinGW bin directory (e.g. C:/msys64/mingw64/bin)
#   OUTPUT_DIR  — destination directory (same as APP_EXE's directory)

find_program(LDD_EXE ldd HINTS "${MINGW_BIN}")
if(NOT LDD_EXE)
    message(WARNING "ldd not found — MinGW runtime DLLs will not be copied.")
    return()
endif()

execute_process(
    COMMAND "${LDD_EXE}" "${APP_EXE}"
    OUTPUT_VARIABLE ldd_out
    ERROR_QUIET
)

# ldd on MSYS2 outputs MSYS-style paths (/mingw64/bin/foo.dll) rather than
# Windows paths.  Rather than translating paths, we extract the DLL name and
# look it up directly in MINGW_BIN — that's the authoritative copy anyway.
string(REPLACE "\n" ";" _lines "${ldd_out}")
foreach(_line IN LISTS _lines)
    # Match:  foo.dll => /some/path/foo.dll (0xaddr)
    if(_line MATCHES "=> [^ \t\r\n]+[/\\\\]([^ /\\\\\t\r\n]+\\.dll)")
        set(_name "${CMAKE_MATCH_1}")
        if(EXISTS "${MINGW_BIN}/${_name}" AND NOT EXISTS "${OUTPUT_DIR}/${_name}")
            file(COPY "${MINGW_BIN}/${_name}" DESTINATION "${OUTPUT_DIR}")
            message(STATUS "MinGW dep: ${_name}")
        endif()
    endif()
endforeach()
