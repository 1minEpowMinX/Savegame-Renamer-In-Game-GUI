"""Compile flash/base.xml and flash/renamer.as into the .gfx the mod ships.

Both inputs are text; the compiled .gfx is the only binary the source tree
carries.

base.xml is the SWF skeleton: stage size and the ImportAssets2 tags that pull the
menu fonts out of the game's shared gfxfontlib.gfx.

renamer.as is the whole dialog. It reaches the container by way of a scratch
folder: the skeleton's placeholder script is exported there, overwritten, and
imported back.

Run with no arguments; paths resolve relative to this file.
"""

import os
import shutil
import subprocess
import sys

import buildenv
import project

OUTPUT = os.path.join(project.DATA_DIR, "Libs", "UI", "renamer.gfx")
SCRATCH = os.path.join(project.BUILD_DIR, "flash")

# JPEXS Free Flash Decompiler, which compiles the skeleton and imports the
# script into it. Resolved late so that importing this module costs nothing on a
# machine that only wants the other half of the build.
def ffdec():
    """Return the path of the FFDec command line.

    @return Path as configured.
    """
    return buildenv.require("FFDEC", "the JPEXS FFDec command line")


def run(*args):
    """Run FFDec and raise on a non-zero exit.

    @param args Command line following the executable.
    @return None.
    """
    result = subprocess.run([ffdec()] + list(args), capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("ffdec %s failed:\n%s\n%s"
                           % (args[0], result.stdout, result.stderr))


def build():
    """Write the compiled .gfx, returning its path.

    @return Path of the file written.
    """
    if not os.path.isfile(ffdec()):
        raise RuntimeError("FFDec not found at %s" % ffdec())

    shutil.rmtree(SCRATCH, ignore_errors=True)
    os.makedirs(SCRATCH)
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)

    # The skeleton's ImportAssets2 tags are what make the fonts reachable: a text
    # field in a movie without them renders nothing at all.
    container = os.path.join(SCRATCH, "container.swf")
    run("-xml2swf", os.path.join(project.FLASH_DIR, "base.xml"), container)

    # FFDec has no "compile one file" mode, so the skeleton's placeholder script
    # is what the real source replaces.
    exported = os.path.join(SCRATCH, "exported")
    run("-export", "script", exported, container)

    target = os.path.join(exported, "scripts", "frame_1", "DoAction.as")
    if not os.path.isfile(target):
        raise RuntimeError("FFDec exported no frame script; base.xml has no DoActionTag")
    shutil.copyfile(os.path.join(project.FLASH_DIR, "renamer.as"), target)

    run("-importScript", container, OUTPUT, exported)

    # The importer is quiet about a script it could not compile, so read the
    # result back and insist the source actually landed in it.
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
