"""Build the mod: compile the flash, build the localization, pack and verify.

Pass --deploy to copy the result into the game's Mods folder, or --release to
write the archive that goes on Nexus. Both take the plugin DLL from the CMake
build tree.
"""

import argparse
import os
import shutil
import sys
import xml.etree.ElementTree as ET
import zipfile

import build_flash
import build_localization
import buildenv
import pak
import project

# What the game and the Nexus page both read, so nothing else in the build keeps
# a copy of what it declares.
MANIFEST = os.path.join(project.SRC_DIR, "mod.manifest")

# Where the CMake build leaves the plugin, relative to libKCD2's tree. Derived
# rather than configured: the build writes it there and nowhere else.
PLUGIN_IN_BUILD = os.path.join(".buildenv", "build-release", "SavegameRenamer",
                               "SavegameRenamer.dll")

# The engine reads a local-header data offset past the central directory's
# extra-field length, so an archiver that writes an NTFS timestamp there (7-Zip,
# Explorer's "Send to") lands it past the start of the deflate stream and the
# file is dropped. zipfile writes neither field. Inherited from
# better_arm_of_beowulf.
MAX_EXTRA_FIELD_LEN = 0


def pack(pak_path, src_dir):
    """Write every file under src_dir into pak_path.

    Files ending in .pak are skipped.

    @param pak_path Archive to create, overwriting any existing file.
    @param src_dir Directory whose tree becomes the archive's root.
    @return Sorted list of archive-relative names written.
    """
    # .pak skipped: the archive lives inside its own source directory and would
    # pack a copy of itself.
    entries = sorted(
        (os.path.relpath(os.path.join(root, f), src_dir).replace(os.sep, "/"),
         os.path.join(root, f))
        for root, _, files in os.walk(src_dir) for f in files
        if not f.lower().endswith(".pak"))

    with pak.create(pak_path) as z:
        for arcname, full in entries:
            with open(full, "rb") as f:
                pak.write_entry(z, arcname, f.read())
    return [arcname for arcname, _ in entries]


def central_entries(raw):
    """Return the archive's central directory, entry by entry.

    @param raw The whole archive.
    @return List of (name, extra field length), or None when the archive carries
        no end-of-central-directory record.
    """
    # Read out of the bytes rather than through zipfile, which reports neither
    # where an entry stands nor how long its extra field is.
    eocd = raw.rfind(b"PK\x05\x06")
    if eocd < 0:
        return None
    count = int.from_bytes(raw[eocd + 10:eocd + 12], "little")
    pos = int.from_bytes(raw[eocd + 16:eocd + 20], "little")

    entries = []
    for _ in range(count):
        name_len = int.from_bytes(raw[pos + 28:pos + 30], "little")
        extra_len = int.from_bytes(raw[pos + 30:pos + 32], "little")
        comment_len = int.from_bytes(raw[pos + 32:pos + 34], "little")
        entries.append((raw[pos + 46:pos + 46 + name_len].decode("utf-8"), extra_len))
        pos += 46 + name_len + extra_len + comment_len
    return entries


def check_archive(pak_path, src_dir, errors):
    """Verify the archive is readable by the engine and reproduces src_dir exactly.

    @param pak_path Archive to inspect.
    @param src_dir Directory the archive was packed from.
    @param errors List that failure messages are appended to.
    @return None.
    """
    with open(pak_path, "rb") as f:
        raw = f.read()
    entries = central_entries(raw)
    if entries is None:
        errors.append("%s: no end-of-central-directory record" % pak_path)
        return

    with zipfile.ZipFile(pak_path) as z:
        if z.testzip() is not None:
            errors.append("%s: CRC mismatch" % pak_path)

        for name, extra_len in entries:
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


def declared(field):
    """Return one <info> field of the mod manifest.

    @param field Name of the element under <info>.
    @return The field's text.
    """
    return ET.parse(MANIFEST).findtext("info/" + field)


def data_pak():
    """Return the data pak the mod ships.

    @return Path of the archive.
    """
    return os.path.join(project.DATA_DIR, declared("modid") + ".pak")


def layout(require_plugin):
    """Return the mod folder's contents as (source, relative destination) pairs.

    @param require_plugin Whether a missing plugin raises rather than warns.
    @return List of pairs, the destinations relative to the mod folder.
    """
    # Shared by deploy() and release(), so an installed mod and a released one
    # carry the same arrangement.
    data = data_pak()
    # The notices go beside the licence: the plugin links libKCD2 and MinHook
    # statically, and MinHook's terms ask for its notice wherever the binary goes.
    pairs = [(MANIFEST, "mod.manifest"),
             (os.path.join(project.PROJECT_ROOT, "LICENSE"), "LICENSE"),
             (os.path.join(project.PROJECT_ROOT, "THIRD-PARTY-NOTICES.txt"),
              "THIRD-PARTY-NOTICES.txt"),
             (data, "Data/" + os.path.basename(data))]
    # The game merges these into its own string tables, so they sit beside the
    # manifest rather than inside the data pak.
    pairs += [(os.path.join(project.LOCALIZATION_DIR, f), "Localization/" + f)
              for f in sorted(os.listdir(project.LOCALIZATION_DIR)) if f.endswith(".pak")]
    plugin = os.path.join(buildenv.require("LIBKCD2_ROOT", "the libKCD2 checkout"),
                          PLUGIN_IN_BUILD)
    if os.path.isfile(plugin):
        pairs.append((plugin, "KCSE/Plugins/" + os.path.basename(plugin)))
    elif require_plugin:
        # A deploy without it leaves whatever was installed last in place; a
        # release without it is an archive that installs a mod doing nothing.
        raise RuntimeError(
            "%s is not built, and an archive without it installs a mod that does "
            "nothing.\nRun tools%sbuild_cpp.bat first." % (plugin, os.sep))
    else:
        print("warning: %s not built" % plugin)
    return pairs


def deploy():
    """Copy the mod folder's contents into the game.

    @return Destination directory, or None when a file could not be replaced.
    """
    dest = os.path.join(buildenv.require("KCD2_ROOT", "the game installation"),
                        "Mods", declared("modid"))
    for source, relative in layout(require_plugin=False):
        target = os.path.join(dest, relative.replace("/", os.sep))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        try:
            shutil.copyfile(source, target)
        except PermissionError:
            # The game keeps its paks and plugin DLLs open for the whole session.
            print("cannot replace %s: close the game first" % target)
            return None
    return dest


def release():
    """Write the archive that goes on Nexus, and return its path.

    The archive holds one folder named after the modid.

    @return Path of the archive written.
    """
    # The folder inside the archive is what unpacks into Mods/, and what this
    # author's other mods ship, so both install the same way.
    modid = declared("modid")
    os.makedirs(project.RELEASES_DIR, exist_ok=True)
    path = os.path.join(project.RELEASES_DIR, "%s-%s.zip" % (modid, declared("version")))

    with pak.create(path) as z:
        for source, relative in layout(require_plugin=True):
            with open(source, "rb") as f:
                pak.write_entry(z, modid + "/" + relative, f.read())
    return path


def main():
    """Build, verify and optionally deploy."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--deploy", action="store_true",
                        help="copy the result into the game's Mods folder")
    parser.add_argument("--release", action="store_true",
                        help="write releases/<modid>-<version>.zip")
    args = parser.parse_args()

    try:
        return run(args)
    except RuntimeError as exc:
        # An unconfigured path is a setup fault rather than a crash.
        print(exc)
        return 1


def run(args):
    """Build, verify and act on `args`.

    @param args Parsed command line.
    @return Process exit status.
    """
    build_flash.build()
    localization = build_localization.build()

    data = data_pak()
    names = pack(data, project.DATA_DIR)
    errors = []
    check_archive(data, project.DATA_DIR, errors)
    if errors:
        for e in errors:
            print(e)
        return 1

    print("built %d localization paks" % len(localization))
    print("packed %s (%d files)"
          % (os.path.relpath(data, project.PROJECT_ROOT), len(names)))
    for n in names:
        print("  " + n)

    if args.deploy:
        dest = deploy()
        if dest is None:
            return 1
        print("deployed to " + dest)

    if args.release:
        path = release()
        print("wrote %s (%d bytes)" % (os.path.relpath(path, project.PROJECT_ROOT),
                                       os.path.getsize(path)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
