"""Where the parts of the project live.

Every path is derived from this file's own location, so a checkout sits wherever
it likes and no script carries an absolute path. Machine paths, which do differ
per developer, are in buildenv instead.
"""

import os

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

FLASH_DIR = os.path.join(PROJECT_ROOT, "flash")
SRC_DIR = os.path.join(PROJECT_ROOT, "src")
DATA_DIR = os.path.join(SRC_DIR, "Data")
LOCALIZATION_DIR = os.path.join(SRC_DIR, "Localization")
RELEASES_DIR = os.path.join(PROJECT_ROOT, "releases")

# Scratch space for the flash toolchain, which works in files rather than in
# memory. Not tracked.
BUILD_DIR = os.path.join(PROJECT_ROOT, "build")
