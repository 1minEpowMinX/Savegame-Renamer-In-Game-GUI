"""Build the mod: compile the flash, build the localization, pack and deploy.

Pass --deploy to copy the result into the game's Mods folder, which also picks up
the plugin DLL from the CMake build tree.

The archive must carry no extra field in its central directory: the engine takes
the local-header data offset from the central directory's extra-field length, so
an archiver that writes an NTFS timestamp there (7-Zip, Explorer's "Send to")
pushes the engine past the start of the deflate stream and it drops the file.
zipfile writes neither. This constraint is inherited from better_arm_of_beowulf.
"""

import argparse
import os
import shutil
import sys
import time
import xml.etree.ElementTree as ET
import zipfile

import build_flash
import build_localization

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(PROJECT_ROOT, "src")
DATA_DIR = os.path.join(SRC_DIR, "Data")
LOCALIZATION_DIR = os.path.join(SRC_DIR, "Localization")
MODID = "savegame_renamer"
PAK = os.path.join(DATA_DIR, MODID + ".pak")

GAME_ROOT = r"D:\Games\Steam\steamapps\common\KingdomComeDeliverance2"
PLUGIN_DLL = (r"D:\Games\Self-Mods\KCD2\_deps\libKCD2\.buildenv\build-release"
              r"\SavegameRenamer\SavegameRenamer.dll")

MAX_EXTRA_FIELD_LEN = 0


def pack(pak_path, src_dir):
    """Write every file under src_dir into pak_path.

    Files ending in .pak are skipped so an archive living inside its own source
    directory does not pack a copy of itself.

    @param pak_path Archive to create, overwriting any existing file.
    @param src_dir Directory whose tree becomes the archive's root.
    @return Sorted list of archive-relative names written.
    """
    entries = sorted(
        (os.path.relpath(os.path.join(root, f), src_dir).replace(os.sep, "/"),
         os.path.join(root, f))
        for root, _, files in os.walk(src_dir) for f in files
        if not f.lower().endswith(".pak"))

    with zipfile.ZipFile(pak_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for arcname, full in entries:
            info = zipfile.ZipInfo(arcname, time.localtime(os.path.getmtime(full))[:6])
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o600 << 16
            with open(full, "rb") as f:
                z.writestr(info, f.read())
    return [arcname for arcname, _ in entries]


def check_archive(pak_path, src_dir, errors):
    """Verify the archive is readable by the engine and reproduces src_dir exactly.

    @param pak_path Archive to inspect.
    @param src_dir Directory the archive was packed from.
    @param errors List that failure messages are appended to.
    @return None.
    """
    with open(pak_path, "rb") as f:
        raw = f.read()
    eocd = raw.rfind(b"PK\x05\x06")
    count = int.from_bytes(raw[eocd + 10:eocd + 12], "little")
    pos = int.from_bytes(raw[eocd + 16:eocd + 20], "little")

    with zipfile.ZipFile(pak_path) as z:
        if z.testzip() is not None:
            errors.append("%s: CRC mismatch" % pak_path)

        for _ in range(count):
            name_len = int.from_bytes(raw[pos + 28:pos + 30], "little")
            extra_len = int.from_bytes(raw[pos + 30:pos + 32], "little")
            comment_len = int.from_bytes(raw[pos + 32:pos + 34], "little")
            name = raw[pos + 46:pos + 46 + name_len].decode("utf-8")
            pos += 46 + name_len + extra_len + comment_len

            if extra_len > MAX_EXTRA_FIELD_LEN:
                errors.append("%s: %s has a %d-byte central extra field; the engine will "
                              "misplace the data offset" % (pak_path, name, extra_len))

            packed = z.read(name)
            with open(os.path.join(src_dir, name.replace("/", os.sep)), "rb") as f:
                if packed != f.read():
                    errors.append("%s: %s differs from its source" % (pak_path, name))
            if name.lower().endswith(".xml"):
                try:
                    ET.fromstring(packed.decode("utf-8"))
                except (ET.ParseError, UnicodeDecodeError) as exc:
                    errors.append("%s: %s is not well-formed: %s" % (pak_path, name, exc))


def deploy():
    """Copy the manifest, the pak and the plugin DLL into the game.

    @return Destination directory, or None when a file could not be replaced.
    """
    dest = os.path.join(GAME_ROOT, "Mods", MODID)
    os.makedirs(os.path.join(dest, "Data"), exist_ok=True)
    os.makedirs(os.path.join(dest, "KCSE", "Plugins"), exist_ok=True)

    # The game merges these into its own tables, so they go beside the manifest
    # rather than inside the data pak.
    os.makedirs(os.path.join(dest, "Localization"), exist_ok=True)

    copies = [(os.path.join(SRC_DIR, "mod.manifest"), os.path.join(dest, "mod.manifest")),
              (PAK, os.path.join(dest, "Data", os.path.basename(PAK)))]
    copies += [(os.path.join(LOCALIZATION_DIR, f),
                os.path.join(dest, "Localization", f))
               for f in sorted(os.listdir(LOCALIZATION_DIR)) if f.endswith(".pak")]
    if os.path.isfile(PLUGIN_DLL):
        copies.append((PLUGIN_DLL, os.path.join(dest, "KCSE", "Plugins",
                                                os.path.basename(PLUGIN_DLL))))
    else:
        print("warning: %s not built, DLL not deployed" % PLUGIN_DLL)

    for source, target in copies:
        try:
            shutil.copyfile(source, target)
        except PermissionError:
            # The game keeps its paks and plugin DLLs open for the whole session.
            print("cannot replace %s: close the game first" % target)
            return None
    return dest


def main():
    """Build, verify and optionally deploy."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--deploy", action="store_true",
                        help="copy the result into the game's Mods folder")
    args = parser.parse_args()

    build_flash.build()
    localization = build_localization.build()

    names = pack(PAK, DATA_DIR)
    errors = []
    check_archive(PAK, DATA_DIR, errors)
    if errors:
        for e in errors:
            print(e)
        return 1

    print("built %d localization paks" % len(localization))
    print("packed %s (%d files)" % (os.path.relpath(PAK, PROJECT_ROOT), len(names)))
    for n in names:
        print("  " + n)

    if args.deploy:
        dest = deploy()
        if dest is None:
            return 1
        print("deployed to " + dest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
