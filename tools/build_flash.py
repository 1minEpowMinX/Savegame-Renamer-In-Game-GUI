"""Compile flash/base.xml and flash/renamer.as into the .gfx the mod ships.

Both inputs are text, so the only binary in the source tree is the build product.

base.xml is the SWF skeleton: stage size and, critically, the ImportAssets2 tags
that pull the menu fonts out of the game's shared gfxfontlib.gfx. A text field
in a movie without those imports renders nothing at all.

renamer.as is the whole dialog. FFDec has no "compile one file" mode, so the
skeleton's placeholder script is exported to a scratch folder, overwritten, and
imported back.

Run with no arguments; paths resolve relative to this file.
"""

import os
import shutil
import subprocess
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLASH_DIR = os.path.join(PROJECT_ROOT, "flash")
OUTPUT = os.path.join(PROJECT_ROOT, "src", "Data", "Libs", "UI", "renamer.gfx")
SCRATCH = os.path.join(PROJECT_ROOT, "build", "flash")

# JPEXS Free Flash Decompiler, which compiles the skeleton and imports the
# script into it. Overridable: it has no standard install location.
FFDEC = os.environ.get("FFDEC", r"D:\Computer tech. programs\FFDec\ffdec-cli.exe")


def run(*args):
    """Run FFDec and raise on a non-zero exit.

    @param args Command line following the executable.
    @return None.
    """
    result = subprocess.run([FFDEC] + list(args), capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("ffdec %s failed:\n%s\n%s"
                           % (args[0], result.stdout, result.stderr))


def build():
    """Write the compiled .gfx, returning its path.

    @return Path of the file written.
    """
    if not os.path.isfile(FFDEC):
        raise RuntimeError("FFDec not found at %s" % FFDEC)

    shutil.rmtree(SCRATCH, ignore_errors=True)
    os.makedirs(SCRATCH)
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)

    container = os.path.join(SCRATCH, "container.swf")
    run("-xml2swf", os.path.join(FLASH_DIR, "base.xml"), container)

    exported = os.path.join(SCRATCH, "exported")
    run("-export", "script", exported, container)

    target = os.path.join(exported, "scripts", "frame_1", "DoAction.as")
    if not os.path.isfile(target):
        raise RuntimeError("FFDec exported no frame script; base.xml has no DoActionTag")
    shutil.copyfile(os.path.join(FLASH_DIR, "renamer.as"), target)

    run("-importScript", container, OUTPUT, exported)

    # The importer is quiet about a script it could not compile, so read the
    # result back and insist our source actually landed in it.
    verify = os.path.join(SCRATCH, "verify")
    run("-export", "script", verify, OUTPUT)
    with open(os.path.join(verify, "scripts", "frame_1", "DoAction.as"), encoding="utf-8") as f:
        compiled = f.read()
    if "fc_open" not in compiled:
        raise RuntimeError("renamer.as did not compile into the container")

    return OUTPUT


def main():
    """Build the .gfx and report where it went."""
    try:
        path = build()
    except RuntimeError as exc:
        print(exc)
        return 1
    print("wrote %s (%d bytes)" % (path, os.path.getsize(path)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
