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

# ── zlib (for BecTool compression) ───────────────────────────────────────────
find_package(ZLIB REQUIRED)

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
    if(WIN32)
        # dotnet/NuGet cannot operate on UNC paths (\\server\share\...).
        # Stage the project to a local directory at configure time so the build
        # step only needs to run a single dotnet publish command on a local path.
        set(PS2_TOOL_LOCAL "${CMAKE_BINARY_DIR}/ps2isotool-src")
        file(COPY "${PS2_TOOL_SRC}/" DESTINATION "${PS2_TOOL_LOCAL}")
        set(PS2_TOOL_BUILD_SRC "${PS2_TOOL_LOCAL}")
    else()
        set(PS2_TOOL_BUILD_SRC "${PS2_TOOL_SRC}")
    endif()

    if(WIN32)
        # cmd.exe (Ninja's shell) loses USERPROFILE/APPDATA that NuGet requires.
        # Write a .ps1 script at configure time and invoke it via PowerShell,
        # which inherits the full user environment and works reliably.
        find_program(POWERSHELL_EXE powershell
            HINTS "C:/Windows/System32/WindowsPowerShell/v1.0")
        if(NOT POWERSHELL_EXE)
            set(POWERSHELL_EXE powershell)
        endif()
        set(_ps1 "${CMAKE_BINARY_DIR}/build_ps2isotool.ps1")
        # Convert the binary dir to a Windows-native path for MSBuild properties
        file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}" _cmake_binary_dir_native)
        # IMPORTANT: use \n (LF-only) line endings.  PowerShell backtick line
        # continuation breaks silently when the file has CRLF endings because
        # the \r before \n is NOT treated as whitespace by the tokenizer.
        # Writing the script as a single logical line with semicolons avoids the
        # issue entirely — no line continuations needed.
        file(WRITE "${_ps1}"
"# Pin NuGet packages cache to a local dir so NuGet never resolves paths\n# through the UNC share (VBoxSvr / network).\n\$env:NUGET_PACKAGES = '${_cmake_binary_dir_native}\\nuget-packages'\n& '${DOTNET_TOOL}' publish '${PS2_TOOL_BUILD_SRC}' -c Release --nologo -r ${DOTNET_RID} --self-contained true '-p:PublishSingleFile=true' -o '${PS2_TOOL_OUT}'\nexit \$LASTEXITCODE\n")
        add_custom_command(
            OUTPUT  "${PS2_TOOL_BIN}"
            COMMAND "${POWERSHELL_EXE}" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "${_ps1}"
            COMMENT "Building ps2isotool (C#/.NET self-contained, RID=${DOTNET_RID})"
            DEPENDS "${PS2_TOOL_SRC}/Program.cs"
                    "${PS2_TOOL_SRC}/ps2isotool.csproj"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT  "${PS2_TOOL_BIN}"
            COMMAND ${DOTNET_TOOL} publish "${PS2_TOOL_BUILD_SRC}"
                    -c Release --nologo -r ${DOTNET_RID} --self-contained true
                    -p:PublishSingleFile=true -o "${PS2_TOOL_OUT}"
            COMMENT "Building ps2isotool (C#/.NET self-contained, RID=${DOTNET_RID})"
            DEPENDS "${PS2_TOOL_SRC}/Program.cs"
                    "${PS2_TOOL_SRC}/ps2isotool.csproj"
            VERBATIM
        )
    endif()
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
    src/NativeRunner.h
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
    src/NativeRunner.cpp
    src/XisoRunner.cpp
    src/Ps2IsoRunner.cpp
    src/LogPanel.cpp
    src/tools/GladiusHashes.cpp
    src/tools/TokNumUpdate.cpp
    src/tools/UpdateStringsBin.cpp
    src/tools/TokTool.cpp
    src/tools/GladiusIdxUnpack.cpp
    src/tools/GladiusIdxRepack.cpp
    src/tools/NgcIsoTool.cpp
    src/tools/BecTool.cpp
    ${MOC_SOURCES}
)

qt_add_executable(gladius-mod-tool ${SOURCES})

# On Windows, set the GUI subsystem so no console window appears at launch.
# This is a no-op on other platforms.
if(WIN32)
    set_target_properties(gladius-mod-tool PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# (Python scripts removed — all tools are now compiled C++)

target_include_directories(gladius-mod-tool PRIVATE
    src
    ${ZLIB_INCLUDE_DIRS}
)

target_link_libraries(gladius-mod-tool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ZLIB::ZLIB
)

target_compile_options(gladius-mod-tool PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter>
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

# ── Copy gladiushashes.json next to the app ───────────────────────────────────
add_custom_command(TARGET gladius-mod-tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/gladiushashes.json"
        "$<TARGET_FILE_DIR:gladius-mod-tool>/gladiushashes.json"
    COMMENT "Copying gladiushashes.json alongside app"
)

# ── Copy schoolbuilder_vanilla.json into resources/ next to the app ───────────
add_custom_command(TARGET gladius-mod-tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/schoolbuilder_vanilla.json"
        "$<TARGET_FILE_DIR:gladius-mod-tool>/resources/schoolbuilder_vanilla.json"
    COMMENT "Copying schoolbuilder_vanilla.json alongside app"
)

# ── Windows self-contained deployment ────────────────────────────────────────
if(WIN32)
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
            "windeployqt not found — Qt DLLs will not be copied automatically.")
    endif()

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
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS gladius-mod-tool DESTINATION bin)
install(FILES "${CMAKE_SOURCE_DIR}/resources/gladiushashes.json" DESTINATION bin)
install(FILES "${CMAKE_SOURCE_DIR}/resources/schoolbuilder_vanilla.json" DESTINATION bin/resources)
if(XISO_TARGET)
    install(TARGETS ${XISO_TARGET} DESTINATION bin)
endif()
