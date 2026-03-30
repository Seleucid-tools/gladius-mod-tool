cmake_minimum_required(VERSION 3.22)
project(gladius-mod-tool VERSION 0.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Disable AUTOMOC/AUTOUIC globally; enable per-target only ─────────────────
# Global AUTO* combined with Qt6's autogen timestamp tracking causes an infinite
# CMake re-run loop when the build directory is inside the source tree.
# We set them OFF globally and enable on the target explicitly below.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

# ── Qt6 ──────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ── Python3 (embed) ───────────────────────────────────────────────────────────
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Embed)
message(STATUS "Python3 embed libs: ${Python3_LIBRARIES}")
message(STATUS "Python3 include:    ${Python3_INCLUDE_DIRS}")
if(Python3_RUNTIME_LIBRARY)
    message(STATUS "Python3 runtime DLL: ${Python3_RUNTIME_LIBRARY}")
endif()

# ── extract-xiso (bundled source) ────────────────────────────────────────────
set(XISO_SRC_DIR "${CMAKE_SOURCE_DIR}/third_party/extract-xiso")

if(EXISTS "${XISO_SRC_DIR}/extract-xiso.c")
    add_subdirectory("${XISO_SRC_DIR}" extract-xiso-build EXCLUDE_FROM_ALL)
    set(XISO_TARGET extract-xiso)
    message(STATUS "extract-xiso: building from ${XISO_SRC_DIR}")
else()
    message(FATAL_ERROR
        "extract-xiso source not found at ${XISO_SRC_DIR}/extract-xiso.c")
    set(XISO_TARGET "")
endif()

# ── ps2isotool (.NET CLI shim) — self-contained single-file build ─────────────
find_program(DOTNET_TOOL dotnet)
if(DOTNET_TOOL)
    # Determine the .NET Runtime Identifier for the current host
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(DOTNET_RID "linux-arm64")
        else()
            set(DOTNET_RID "linux-x64")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(DOTNET_RID "osx-arm64")
        else()
            set(DOTNET_RID "osx-x64")
        endif()
    else()
        set(DOTNET_RID "win-x64")
    endif()
    message(STATUS "ps2isotool: building for RID ${DOTNET_RID}")

    set(PS2_TOOL_SRC "${CMAKE_SOURCE_DIR}/third_party/ps2isotool")
    set(PS2_TOOL_OUT "${CMAKE_BINARY_DIR}/ps2isotool-publish")
    # On Windows the published .NET single-file binary gets an .exe extension
    if(WIN32)
        set(PS2_TOOL_BIN "${PS2_TOOL_OUT}/ps2isotool.exe")
    else()
        set(PS2_TOOL_BIN "${PS2_TOOL_OUT}/ps2isotool")
    endif()

    # --self-contained + PublishSingleFile produces one standalone executable
    # with no separate .dll or runtime dependency.
    add_custom_command(
        OUTPUT  "${PS2_TOOL_BIN}"
        COMMAND ${DOTNET_TOOL} publish "${PS2_TOOL_SRC}"
                -c Release
                --nologo
                -r ${DOTNET_RID}
                --self-contained true
                -p:PublishSingleFile=true
                -o "${PS2_TOOL_OUT}"
        COMMENT "Building ps2isotool (C#/.NET self-contained, RID=${DOTNET_RID})"
        DEPENDS "${PS2_TOOL_SRC}/Program.cs"
                "${PS2_TOOL_SRC}/ps2isotool.csproj"
        VERBATIM
    )
    add_custom_target(ps2isotool-build DEPENDS "${PS2_TOOL_BIN}")
else()
    message(WARNING
        "dotnet SDK not found — ps2isotool will not be built automatically. "
        "PS2 ISO operations will be unavailable until ps2isotool is placed next to the app. "
        "Build manually: cd third_party/ps2isotool && "
        "dotnet publish -c Release -r <RID> --self-contained true "
        "-p:PublishSingleFile=true -o <builddir>/ps2isotool-publish")
endif()

# ── Run MOC manually on headers that use Q_OBJECT ────────────────────────────
# Instead of global AUTOMOC (which writes autogen stamps that dirty the
# manifest), we list the headers explicitly and let qt_wrap_cpp do one
# code-generation pass at configure time, writing output into the build dir
# with no ongoing file-watch dependency on the source tree.
qt_wrap_cpp(MOC_SOURCES
    src/MainWindow.h
    src/PipelineTab.h
    src/EditorTab.h
    src/SchoolBuilderTab.h
    src/PythonRunner.h
    src/XisoRunner.h
    src/Ps2IsoRunner.h
    src/LogPanel.h
)

# ── Main executable ───────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/PipelineTab.cpp
    src/EditorTab.cpp
    src/SchoolBuilderTab.cpp
    src/PythonRunner.cpp
    src/XisoRunner.cpp
    src/Ps2IsoRunner.cpp
    src/LogPanel.cpp
    src/ScriptExtractor.cpp
    ${MOC_SOURCES}
)

qt_add_executable(gladius-mod-tool ${SOURCES})

# On Windows, set the GUI subsystem so no console window appears at launch.
# This is a no-op on other platforms.
if(WIN32)
    set_target_properties(gladius-mod-tool PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── Qt resources ──────────────────────────────────────────────────────────────
qt_add_resources(gladius-mod-tool "scripts"
    PREFIX "/scripts"
    BASE   "resources/scripts"
    FILES
        resources/scripts/bec-tool-all.py
        resources/scripts/gladiushashes.py
        resources/scripts/Gladius_Units_IDX_Repack.py
        resources/scripts/Gladius_Units_IDX_Unpack.py
        resources/scripts/ngciso-tool.py
        resources/scripts/tok-tool.py
        resources/scripts/Tok_Num_Update.py
        resources/scripts/Update_Strings_Bin.py
)

target_include_directories(gladius-mod-tool PRIVATE
    src
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(gladius-mod-tool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${Python3_LIBRARIES}
)

# ── Compiler options — suppress warnings from Python/Qt header ordering ───────
# -Wno-builtin-macro-redefined: Python 3.14+ object.h 'slots' struct member
# collides with Qt's 'slots' empty macro.  GCC/Clang only; MSVC does not
# recognise this flag.
target_compile_options(gladius-mod-tool PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-builtin-macro-redefined>
)

# ── Copy extract-xiso next to the app after build ────────────────────────────
# Use $<TARGET_FILE_NAME:...> so the destination preserves the correct filename
# on every platform (extract-xiso on Linux/macOS, extract-xiso.exe on Windows).
if(XISO_TARGET)
    add_dependencies(gladius-mod-tool ${XISO_TARGET})
    add_custom_command(TARGET gladius-mod-tool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${XISO_TARGET}>
            $<TARGET_FILE_DIR:gladius-mod-tool>/$<TARGET_FILE_NAME:${XISO_TARGET}>
        COMMENT "Copying extract-xiso binary alongside app"
    )
endif()

# ── Copy ps2isotool next to the app ──────────────────────────────────────────
# Use an ALL custom target (not POST_BUILD) so the copy runs whenever
# ps2isotool-build updates the binary, independent of the C++ link step.
if(DOTNET_TOOL)
    get_filename_component(PS2_TOOL_BIN_NAME "${PS2_TOOL_BIN}" NAME)
    add_custom_target(copy-ps2isotool ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PS2_TOOL_BIN}"
            "$<TARGET_FILE_DIR:gladius-mod-tool>/${PS2_TOOL_BIN_NAME}"
        DEPENDS ps2isotool-build
        COMMENT "Copying ps2isotool to build directory"
    )
    add_dependencies(gladius-mod-tool copy-ps2isotool)
endif()

# ── Windows self-contained deployment ────────────────────────────────────────
if(WIN32)
    # windeployqt scans the exe and copies all required Qt DLLs and plugins
    # into the output directory, producing a portable deployment folder.
    find_program(WINDEPLOYQT_TOOL windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
    )
    if(WINDEPLOYQT_TOOL)
        message(STATUS "windeployqt: ${WINDEPLOYQT_TOOL}")
        add_custom_command(TARGET gladius-mod-tool POST_BUILD
            COMMAND "${WINDEPLOYQT_TOOL}"
                --no-translations
                --no-system-d3d-compiler
                "$<TARGET_FILE:gladius-mod-tool>"
            COMMENT "Running windeployqt to bundle Qt DLLs"
        )
    else()
        message(WARNING
            "windeployqt not found — Qt DLLs will not be copied automatically.\n"
            "Add <Qt install>/bin to PATH and reconfigure, or run windeployqt manually:\n"
            "  windeployqt --no-translations gladius-mod-tool.exe")
    endif()

    # Copy all MinGW runtime DLLs the exe depends on.  windeployqt handles Qt
    # but not the compiler runtime (libgcc, libstdc++, libwinpthread, ICU, …).
    # The helper script uses file(GET_RUNTIME_DEPENDENCIES) to resolve the full
    # dependency tree and copies every DLL that lives inside the MinGW tree.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET gladius-mod-tool POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DAPP_EXE=$<TARGET_FILE:gladius-mod-tool>"
                "-DMINGW_BIN=${MINGW_BIN}"
                "-DOUTPUT_DIR=$<TARGET_FILE_DIR:gladius-mod-tool>"
                -P "${CMAKE_SOURCE_DIR}/cmake/copy_mingw_deps.cmake"
            COMMENT "Copying MinGW runtime DLLs"
        )
    endif()

    # Copy the Python runtime DLL (e.g. libpython3.14.dll) next to the exe so
    # the app runs without a system-wide Python installation on the target machine.
    if(Python3_RUNTIME_LIBRARY)
        add_custom_command(TARGET gladius-mod-tool POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${Python3_RUNTIME_LIBRARY}"
                "$<TARGET_FILE_DIR:gladius-mod-tool>"
            COMMENT "Copying Python runtime DLL alongside app"
        )
    else()
        message(WARNING
            "Python3_RUNTIME_LIBRARY not set — Python DLL will not be copied automatically.\n"
            "Copy python3XX.dll manually next to the exe for self-contained deployment.")
    endif()

    # Bundle the Python standard library for fully self-contained deployment.
    # PythonRunner sets config.home = appDir/python at startup when that
    # directory exists; Python then resolves stdlib at python/lib/pythonX.Y/.
    # The helper script strips large unused subtrees (test suite, pycache, idle,
    # tkinter, ensurepip) shrinking the stdlib from ~220 MB down to ~35 MB.
    if(Python3_STDLIB AND EXISTS "${Python3_STDLIB}")
        set(PY_LIB_DEST
            "$<TARGET_FILE_DIR:gladius-mod-tool>/python/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")
        message(STATUS "Python stdlib to bundle: ${Python3_STDLIB} → python/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/")
        add_custom_command(TARGET gladius-mod-tool POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DPY_STDLIB_SRC=${Python3_STDLIB}"
                "-DPY_STDLIB_DST=${PY_LIB_DEST}"
                -P "${CMAKE_SOURCE_DIR}/cmake/bundle_python_stdlib.cmake"
            COMMENT "Bundling Python ${Python3_VERSION} stdlib (stripped) → python/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/"
        )
    else()
        message(WARNING
            "Python3_STDLIB not found — stdlib will not be bundled automatically.\n"
            "Copy the Python ${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR} stdlib to "
            "python/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/ "
            "next to the exe for self-contained deployment.")
    endif()
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS gladius-mod-tool DESTINATION bin)
if(XISO_TARGET)
    install(TARGETS ${XISO_TARGET} DESTINATION bin)
endif()
