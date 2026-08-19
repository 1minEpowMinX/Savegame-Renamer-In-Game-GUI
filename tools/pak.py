"""Write the zip archives the build produces: the data pak, the localization
paks and the release archive.

All three are written the same way, and the two conditions on how are held here:
the engine's reader, and the fact that the paks are committed.
"""

import zipfile

# The stamp every entry carries, rather than the source file's own. The paks are
# committed, and a stamp per build rewrites them whether or not anything inside
# them changed.
EPOCH = (2026, 8, 18, 0, 0, 0)


def create(path):
    """Return an archive open for writing.

    @param path Archive to create, overwriting any existing file.
    @return The open zipfile.ZipFile, to be used as a context manager.
    """
    return zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9)


def write_entry(archive, arcname, data):
    """Add one entry to an open archive.

    @param archive Archive returned by create.
    @param arcname Name the entry takes inside the archive.
    @param data Bytes to store.
    @return None.
    """
    info = zipfile.ZipInfo(arcname, EPOCH)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o600 << 16
    archive.writestr(info, data)
