# Savegame Renamer — In-Game GUI

Renames an existing savegame from Kingdom Come: Deliverance II's own load menu.
Highlight a save, press F2, type a name. No console, no external tools.

A [KCSE](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3332) plugin plus
a Scaleform element built from the game's own artwork. Vanilla files are not
replaced, so it does not conflict with other interface mods.

## Layout

| Path | What it holds |
|---|---|
| `cpp/include`, `cpp/src/whs` | the savegame header model, with no dependency on the game |
| `cpp/src/game` | everything that touches the running game: hooks, the dialog, the catalog |
| `cpp/tests` | Catch2 tests for the model, run offline on copies of real saves |
| `flash/` | the dialog: `base.xml` is the SWF skeleton, `renamer.as` is all of the behaviour |
| `src/` | what ships: the manifest, the UI element declaration, the localization source |
| `tools/` | the build |
| `docs/` | design and implementation notes (Russian) |

`src/Data/savegame_renamer.pak` and `src/Localization/*.pak` are build products
that are committed, so a checkout can be installed without a build. Both are
written with fixed timestamps and are reproducible.

## Building

The plugin builds as a subproject of [libKCD2](https://github.com/JerryYOJ/libKCD2),
whose build environment globs `Projects/*/.buildenv/CMakeLists.txt`; this project
is reached through a directory junction named `Projects\SavegameRenamer`. Visual
Studio with the C++ workload and vcpkg are what `tools/build_cpp.bat` expects.

```
tools\build_cpp.bat              plugin and tests
python tools\build.py            compile the flash, build the paks
python tools\build.py --deploy   install into the game
python tools\build.py --release  write releases/<modid>-<version>.zip
```

The flash is compiled by [JPEXS FFDec](https://github.com/jindrapetrik/jpexs-decompiler).
Paths that belong to one machine are read from the environment, each falling back
to the author's own: `FFDEC`, `KCD2_ROOT`, `KCD2_PLUGIN_DLL`.

### In Visual Studio

Open `SavegameRenamer.sln`. It is a Makefile project: the real build stays with
CMake and Ninja inside libKCD2's tree, and Build hands off to the same commands a
shell would run. What the project adds is what a batch file cannot tell the
editor — the file tree, the include paths and defines IntelliSense needs, and F5
set to launch the game so breakpoints bind once KCSE has loaded the plugin.

Two configurations, both x64:

| Configuration | Build runs |
|---|---|
| `Release` | `tools\build_cpp.bat` — the plugin alone, the loop while editing C++ |
| `Deploy` | that, then `tools\build.py --deploy` — flash, localization, pak, install |

The two are chained rather than independent: the pipeline copies the plugin it
has just built, so it does not run when the compile failed. `Deploy` needs FFDec
on the machine and the game closed, which is why the C++ loop is a configuration
of its own.

Configuring it as a CMake folder does not work: `cpp/.buildenv/CMakeLists.txt` is
a fragment that expects `RE_ROOT`, `RE_BUILDENV` and the `kcd_re` target from the
parent project, and libKCD2's build environment is the only place that supplies
them.

Clean is deliberately inert. The build directory belongs to libKCD2 and holds
every other plugin built from that tree.

Machine paths come from the environment first, the same names the Python scripts
use, each falling back to the author's layout: `KCD2Root` (or `KCD2_ROOT`) for the
game, `LibKCD2Root` for the headers, defaulting to `..\_deps\libKCD2` beside this
project.

`.clang-format` and `.editorconfig` describe the style already in the tree and are
applied by the editor without further setup.

## Translating

`src/Localization/strings.xml` holds the two strings the mod owns; everything
else it displays is taken from the game's own string tables and needs no
translation. Add a `<text language="…">` line and rebuild.

## Licence

GPLv3, see [LICENSE](LICENSE).
