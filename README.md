# Gladius Mod Tool

A native Linux Qt6 GUI for the Gladius modding toolkit. Supports Gamecube and
Xbox ISO unpacking/repacking with an integrated file editor. No Wine required.

## Requirements

### System packages (Arch Linux)

```bash
sudo pacman -S \
    qt6-base \
    cmake \
    ninja \
    python \
    git \
    base-devel
```

`python` pulls in the CPython embed headers and shared library that CMake will
find via `find_package(Python3 ... Development.Embed)`.

### extract-xiso (bundled)

`extract-xiso` v2.7.1 (XboxDev/extract-xiso, BSD-style licence) is bundled
under `third_party/extract-xiso/` and compiled automatically — no separate
download required. After build, CMake copies the binary next to the app.

---

## Building

```bash
# 1. Clone / place the project
cd gladius-mod-tool

# 2. Configure
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build

# 4. Run
./build/gladius-mod-tool
```

### Debug build

```bash
cmake -B build-debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
./build-debug/gladius-mod-tool
```

### Verifying Python embed is found

```bash
cmake -B build -G Ninja 2>&1 | grep -i python
# Should show:
# -- Python3 embed libs: /usr/lib/libpython3.XX.so
# -- Python3 include:    /usr/include/python3.XX
```

If CMake can't find the embed library, make sure `python` (not just `python-pip`
or a venv) is installed:

```bash
pacman -Qs python | grep "^local/python "
# Should show: local/python 3.XX.X-X
```

---

## Project layout

```
gladius-mod-tool/
├── CMakeLists.txt
├── README.md
├── resources/
│   ├── scripts.qrc              Qt resource manifest
│   └── scripts/                 Bundled Python scripts (from original toolkit)
│       ├── bec-tool-all.py
│       ├── gladiushashes.py
│       ├── Gladius_Units_IDX_Repack.py
│       ├── Gladius_Units_IDX_Unpack.py
│       ├── ngciso-tool.py
│       ├── tok-tool.py
│       ├── Tok_Num_Update.py
│       └── Update_Strings_Bin.py
├── src/
│   ├── main.cpp
│   ├── MainWindow.{h,cpp}       Top-level window, menu, splitter
│   ├── PipelineTab.{h,cpp}      Unpack/Pack pipeline (one per platform)
│   ├── EditorTab.{h,cpp}        File tree + text editor
│   ├── PythonRunner.{h,cpp}     CPython embed, stdout/stderr → Qt signals
│   ├── XisoRunner.{h,cpp}       QProcess wrapper for extract-xiso
│   ├── LogPanel.{h,cpp}         Scrolling log output widget
│   └── ScriptExtractor.{h,cpp}  Extracts .qrc scripts to /tmp at startup
└── third_party/
    └── extract-xiso/            YOU place the source here
```

---

## Usage

### Workflow (matches original .bat file logic)

**Unpack vanilla ISO:**

1. Open the **Gamecube** or **Xbox** tab
2. Click **Browse…** and select your vanilla ISO
   - GC: renamed to `GladiusGamecubeVanilla.iso`
   - Xbox: renamed to `GladiusXboxVanilla.iso`
3. Click **Unpack vanilla ISO**
4. Watch the log panel — the pipeline runs these steps in order:
   - GC: `ngciso-tool.py -unpack` → `bec-tool-all.py -unpack` → `Gladius_Units_IDX_Unpack.py`
   - Xbox: `extract-xiso` → `bec-tool-all.py -unpack` → `Gladius_Units_IDX_Unpack.py`
5. When done, your modded BEC folder is ready to edit

**Edit files:**

1. Switch to the **File editor** tab
2. Navigate to your `*Modded_BEC/` folder in the tree on the left
3. Click any `.txt` or `.tok` file to open it
4. Edit and **Save** (Ctrl+S equivalent via the Save button)
5. The tree only shows editable text-based files — binary-only files are hidden

**Pack modded ISO:**

1. Return to the platform tab
2. Ensure the ISO path is still set (same ISO as unpack)
3. Click **Pack modded ISO**
4. Pipeline runs in order:
   - `Tok_Num_Update.py` → `tok-tool.py -c` → `Update_Strings_Bin.py`
   - `Gladius_Units_IDX_Repack.py` → `bec-tool-all.py -pack`
   - GC: `ngciso-tool.py -pack` / Xbox: `extract-xiso -c`
5. Output ISO appears in the same directory as the vanilla ISO:
   - `GladiusGamecubeModded.iso` or `GladiusXboxModded.iso`

---

## Architecture notes

### CPython embed

The Python interpreter is embedded directly in the C++ binary via the CPython
stable ABI (`Python.h`, linked against `libpython3.x.so`). Scripts are bundled
as Qt resources (compiled into the binary at build time via `scripts.qrc`) and
extracted to `/tmp/gladius-mod-tool/scripts/` once at startup.

`stdout` and `stderr` from each script are captured by a thin `PyObject`
subtype (`QtStream`) that forwards each `write()` call to a Qt signal, which
the log panel receives on the main thread.

**Important:** CPython is not thread-safe without the GIL. The `PythonRunner`
is moved to a `QThread` worker and all script invocations are serialised — only
one script runs at a time. The pipeline's state machine (`Step` enum) ensures
sequential execution by chaining `finished()` → `startNextStep()`.

### extract-xiso

The `extract-xiso` binary runs via `QProcess` in its own worker thread
(`XisoRunner`). Its stdout/stderr are forwarded to the same log panel signal
as the Python runner, so the log is unified.

### Settings persistence

Window geometry and splitter position are saved via `QSettings` to
`~/.config/GladiusModTool/MainWindow.conf` on Arch (or the XDG config dir).

---

## Known limitations / TODO

- **PS2** is intentionally not implemented (ImgBurn dependency, no Linux equivalent)
- The editor saves in **UTF-8**. Most Gladius config files are ASCII/ANSI. The
  `Update_Strings_Bin.py` script expects ANSI encoding for `lookuptext_eng.txt`
  specifically — edit that file with care or use an external editor set to Latin-1
- The file editor does **not** syntax-highlight `.tok` files (they have a custom
  format). A future version could add a simple highlighter for `SKILLCREATE`,
  `ITEMCREATE` etc.
- No drag-and-drop ISO loading yet
- `extract-xiso -c` produces `<dirname>.iso` in the working directory; the tool
  currently expects this and reports the output path in the log

---

## Credits

Python scripts by the Gladius modding community (original toolkit v007).
Qt6 GUI wrapper by this project.
