# Gladius Mod Tool

A Qt6 GUI for the Gladius modding toolkit. Supports Gamecube, Xbox, and PS2 ISO
unpacking/repacking with an integrated file editor. Runs natively on Linux and
Windows — no Wine required.

## Requirements

All modding logic is implemented natively in C++. There is no Python dependency.

### extract-xiso (bundled)

`extract-xiso` v2.7.1 by **in, XboxDev** ([@XboxDev/extract-xiso](https://github.com/xboxdev/extract-xiso),
BSD-style licence) is bundled under `third_party/extract-xiso/` and compiled
automatically — no separate download required. After build, CMake copies the
binary next to the app.

### ps2isotool (bundled)

`ps2isotool` by **Finzenku** ([@Finzenku/Ps2IsoTools](https://github.com/Finzenku/Ps2IsoTools),
MIT licence) is used for PS2 ISO extraction and building. CMake compiles it
automatically via `dotnet publish` if the .NET SDK is present, then copies the
self-contained binary next to the app.

### gladiushashes.json (bundled)

`resources/gladiushashes.json` maps BEC path hashes to filenames. CMake copies
it next to the executable at build time. It must remain alongside the binary at
runtime — the app will still work without it but unknown files will be named
`unknown-N.bin`.

---

## Building on Linux

### System packages (Arch Linux)

```bash
sudo pacman -S \
    qt6-base \
    zlib \
    cmake \
    ninja \
    git \
    base-devel
```

### Configure and build

```bash
cd gladius-mod-tool

cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build

./build/gladius-mod-tool
```

### Debug build

```bash
cmake -B build-debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
./build-debug/gladius-mod-tool
```

---

## Building on Windows

The recommended toolchain is **MSYS2 with MinGW-w64**.

### 1. Install MSYS2

Download and run the installer from <https://www.msys2.org/>, then open the
**MSYS2 MinGW x64** shell.

### 2. Install dependencies

```bash
pacman -S \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-gcc \
    git
```

### 3. Configure and build

> **Important:** Always build from the **MSYS2 MinGW x64** shell.

```bash
cd gladius-mod-tool

cmake -B C:/build/gladius -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

cmake --build C:/build/gladius
```

### 4. Run

```bash
./build/gladius-mod-tool.exe
```

Or double-click `gladius-mod-tool.exe` in Explorer.

### What the build copies automatically

After a successful build the output directory is fully self-contained:

| Path | Contents |
|------|----------|
| `gladius-mod-tool.exe` | C++ application |
| `extract-xiso.exe` | Bundled, compiled from source |
| `ps2isotool.exe` | .NET self-contained (if `dotnet` is found) |
| `gladiushashes.json` | BEC path-hash lookup table |
| `Qt6*.dll`, `platforms/`, `styles/`, … | Qt runtime (via `windeployqt`) |

The resulting folder can be zipped and distributed to machines that have no
Python or Qt installed.

---

## Project layout

```
gladius-mod-tool/
├── CMakeLists.txt
├── README.md
├── resources/
│   └── gladiushashes.json       BEC hash → filename lookup table
├── src/
│   ├── main.cpp
│   ├── MainWindow.{h,cpp}       Top-level window, menu, splitter
│   ├── PipelineTab.{h,cpp}      Unpack/Pack pipeline (one per platform)
│   ├── EditorTab.{h,cpp}        File tree + text editor
│   ├── NativeRunner.{h,cpp}     Dispatches pipeline steps to C++ tools
│   ├── XisoRunner.{h,cpp}       QProcess wrapper for extract-xiso
│   ├── LogPanel.{h,cpp}         Scrolling log output widget
│   └── tools/
│       ├── ToolTypes.h          Shared LogCb type alias
│       ├── BecTool.{h,cpp}      BEC archive unpack/pack
│       ├── NgcIsoTool.{h,cpp}   GameCube ISO unpack/pack
│       ├── GladiusIdxUnpack.{h,cpp}  Unit IDX binary → text
│       ├── GladiusIdxRepack.{h,cpp}  Unit IDX text → binary
│       ├── TokTool.{h,cpp}      .tok dictionary compress/decompress
│       ├── TokNumUpdate.{h,cpp} Update NUMENTRIES in skills/items.tok
│       ├── UpdateStringsBin.{h,cpp}  lookuptext_eng.txt → .bin
│       └── GladiusHashes.{h,cpp}    Runtime hash lookup from JSON
└── third_party/
    └── extract-xiso/
```

---

## Usage

### Workflow

**Unpack vanilla ISO:**

1. Open the **Gamecube**, **Xbox**, or **PS2** tab
2. Click **Browse…** and select your vanilla ISO
3. Click **Unpack vanilla ISO**
4. Watch the log panel — the pipeline runs these steps in order:
   - GC: `ngciso-tool -unpack` → `bec-tool -unpack` → `idx-unpack`
   - Xbox: `extract-xiso` → `bec-tool -unpack` → `idx-unpack`
5. When done, your modded BEC folder is ready to edit

**Edit files:**

1. Switch to the **File editor** tab
2. Navigate to your `*_working_BEC/` folder in the tree on the left
3. Click any `.txt` or `.tok` file to open it
4. Edit and **Save**
5. The tree only shows editable text-based files — binary files are hidden

**Pack modded ISO:**

1. Return to the platform tab
2. Ensure the ISO path is still set
3. Click **Pack modded ISO**
4. Pipeline runs in order:
   - `tok-num-update` → `tok-tool -c` → `update-strings-bin`
   - `idx-repack` → `bec-tool -pack`
   - GC: `ngciso-tool -pack` / Xbox: `extract-xiso -c`
5. Output ISO appears next to the vanilla ISO

---

## Architecture notes

### Native tool pipeline

All modding operations are implemented directly in C++ under `src/tools/`.
`NativeRunner` receives a tool name and argument list from `PipelineTab` and
dispatches synchronously to the appropriate C++ function on a worker thread,
emitting `output`, `error`, and `finished` signals back to the UI — the same
interface previously used by `PythonRunner`.

The BEC unpack extracts files in parallel using `std::async`, batched to
`hardware_concurrency()` workers at a time to avoid exhausting OS thread and
file-descriptor limits.

### extract-xiso

The `extract-xiso` binary runs via `QProcess` in its own worker thread
(`XisoRunner`). Its stdout/stderr are forwarded to the same log panel signals
as the native tools, so the log is unified.

### Settings persistence

Window geometry and splitter position are saved via `QSettings` to
`~/.config/GladiusModTool/MainWindow.conf` on Linux (or the platform equivalent
on Windows).

---

## Known limitations / TODO

- The editor saves in **UTF-8**. `lookuptext_eng.txt` expects Windows-1252
  encoding — the `update-strings-bin` tool handles the conversion automatically,
  but edit that file with care in external editors
- The file editor does not syntax-highlight `.tok` files
- No drag-and-drop ISO loading yet
- `extract-xiso -c` produces `<dirname>.iso` in the working directory; the tool
  currently expects this and reports the output path in the log

---

## Credits

Modding logic based on the original Gladius toolkit Python scripts (v007) by
**JimB16** ([@JimB16/Gladius](https://github.com/JimB16/Gladius)).

IDX unpack/repack tools and `lookuptext` strings bin updater by
**Swift016** ([@Swift016](https://github.com/Swift016)) — unpublished.

`extract-xiso` by **in, XboxDev** ([@XboxDev/extract-xiso](https://github.com/xboxdev/extract-xiso)).

`ps2isotool` by **Finzenku** ([@Finzenku/Ps2IsoTools](https://github.com/Finzenku/Ps2IsoTools)).

Qt6 GUI and C++ reimplementation by this project.
