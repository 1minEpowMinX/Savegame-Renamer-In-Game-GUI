<h1 align="center">Savegame Renamer &mdash; In-Game GUI</h1>

<p align="center">
  <img src="https://img.shields.io/badge/C++%2017-%2300599C.svg?logo=c%2B%2B&logoColor=white" alt="C++17">
  <img src="https://img.shields.io/badge/CMake-064F8C?logo=cmake&logoColor=fff" alt="CMake">
  <img src="https://img.shields.io/badge/Catch2-f5f5f5?logo=c%2B%2B&logoColor=555" alt="Catch2">
  <img src="https://img.shields.io/badge/Python%203.9+-3776AB?logo=python&logoColor=fff" alt="Python 3.9+">
  <img src="https://img.shields.io/badge/ActionScript%202-B4232C?logo=adobe&logoColor=fff" alt="ActionScript 2">
  <img src="https://img.shields.io/badge/Licence-GPL--3.0-blue.svg" alt="GPL-3.0">
</p>

> Renames an existing savegame from Kingdom Come: Deliverance II's own load menu.
> Highlight a save, press F2, type a name. No console, no external tools.

## ✨ What it is

A [KCSE](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3332) plugin plus
a Scaleform element built from the game's own artwork. Vanilla files are not
replaced, so it does not conflict with other interface mods.

## 🗂️ Layout

| Path | What it holds |
|---|---|
| `cpp/include`, `cpp/src/whs` | the savegame header model, with no dependency on the game |
| `cpp/src/game` | everything that touches the running game: the hooks, the dialog, the catalog, and the rename they drive |
| `cpp/tests` | Catch2 tests for the model, run offline on copies of real saves |
| `flash/` | the dialog: `base.xml` is the SWF skeleton, `renamer.as` is all of the behaviour |
| `src/` | what ships: the manifest, the UI element declaration, the localization source |
| `tools/` | the build |
| `prototypes/` | working code that settled a question and is not part of the build |
| `docs/` | design and implementation notes (Russian) |
| `docs/nexus/` | the mod page: its text, its images, and the screenshot and script they are made from |

`src/Data/savegame_renamer.pak` and `src/Localization/*.pak` are build products
that are committed, so a checkout can be installed without a build. Both are
written with fixed timestamps and are reproducible.

## ⚙️ Building

### Prerequisites

| What | Why |
|---|---|
| Visual Studio with the C++ workload | the compiler, and `vcvars64.bat` the build calls |
| vcpkg | libKCD2's CMake uses it in manifest mode |
| A JVM | [JPEXS FFDec](https://github.com/jindrapetrik/jpexs-decompiler) compiles the dialog |
| Python 3.9 or newer | the pak and localization build |

The scripts use the standard library only, so there is nothing to install past
the interpreter itself; 3.9 is where `ET.indent` arrives, which is what lays out
the generated string tables.

### 1. Fill in the machine paths

Copy `build.env.example` to `build.env` and fill it in. It is the one place the
paths are written: the Python scripts, `tools/build_cpp.bat` and the Visual
Studio project all read that file, and it is not tracked, so no checkout carries
another developer's layout. A variable already set in the environment wins over
the file, which is enough for a one-off override.

### 2. Link the project into libKCD2

The plugin builds as a subproject of
[libKCD2](https://github.com/JerryYOJ/libKCD2), whose build environment globs
`Projects/*/.buildenv/CMakeLists.txt`, so this project has to be linked in there
once. From an elevated prompt:

```sh
mklink /J "<libKCD2>\Projects\SavegameRenamer" "<this checkout>\cpp"
```

`tools/build_cpp.bat` checks the link before building. Without it CMake simply
does not see the project, the other plugins in that tree build, and the run
reports success having produced nothing.

### 3. Build

```sh
tools\build_cpp.bat              # plugin and tests
python tools\build.py            # compile the flash, build the paks
python tools\build.py --deploy   # install into the game
python tools\build.py --release  # write releases/<modid>-<version>.zip
```

> ℹ️ `cpp/tests/RealSave.cpp` runs against a **copy** of a real savegame named by
> `KCD2_TEST_SAVE`, and skips itself when the variable is unset, so the suite
> still runs on a machine without the game.

## 💻 In Visual Studio

Open `SavegameRenamer.sln`. It is a Makefile project: the real build stays with
CMake and Ninja inside libKCD2's tree, and Build hands off to the same commands a
shell would run. What the project adds is what a batch file cannot tell the
editor: the file tree, the include paths and defines IntelliSense needs, and F5
set to launch the game so breakpoints bind once KCSE has loaded the plugin.

Three configurations, all x64:

| Configuration | Build runs | F5 starts |
|---|---|---|
| `Build` | `tools\build_cpp.bat` &mdash; the C++ alone, the loop while editing it | the test executable |
| `Deploy` | that, then `tools\build.py --deploy` &mdash; flash, localization, pak, install | the game |
| `Release` | that, then `--deploy --release` as well &mdash; also writes the release archive | the game |

The commands are chained rather than independent: the pipeline copies the
plugin it has just built, so it does not run when the compile failed. `Deploy`
and `Release` need FFDec on the machine and the game closed, which is why the
C++ loop is a configuration of its own.

Each configuration starts what it has just built. `Build` installs nothing, so a
game started from it would be running whichever plugin was deployed last rather
than the one just compiled.

### ⚠️ Notes

* Configuring it as a CMake folder does not work: `cpp/.buildenv/CMakeLists.txt`
  is a fragment that expects `RE_ROOT`, `RE_BUILDENV` and the `kcd_re` target
  from the parent project, and libKCD2's build environment is the only place
  that supplies them.
* Clean is deliberately inert. The build directory belongs to libKCD2 and holds
  every other plugin built from that tree.
* The project reads `build.env` for the same two paths the rest of the build
  takes from it, `KCD2_ROOT` and `LIBKCD2_ROOT`, so the solution carries no
  absolute path of its own.
* `.clang-format` and `.editorconfig` describe the style already in the tree and
  are applied by the editor without further setup.

## 🌍 Translating

`src/Localization/strings.xml` holds the two strings the mod owns; everything
else it displays is taken from the game's own string tables and needs no
translation. Add a `<text language="…">` line and rebuild.

## 📝 Licence

Copyright © 2026 [1minEpowMinX](https://github.com/1minEpowMinX).  
This project is [GPL-3.0](LICENSE) licensed.
