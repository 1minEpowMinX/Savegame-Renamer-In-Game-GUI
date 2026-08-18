"""Turn src/Localization/strings.xml into one <Language>_xml.pak per language.

The game reads these out of the mod folder alongside its own tables and merges
them into the localization manager, so the plugin can resolve the mod's keys the
same way it resolves the game's.

Each pak holds a single table in the game's format: one row per key, holding the
key, the English text and the text for that language. Rows are written for every
shipped language, falling back to English, so a player never sees a bare key.

Entry timestamps are fixed rather than taken from the source file: the paks are
committed, and a build would otherwise rewrite all sixteen on every run.

Run with no arguments; paths resolve relative to this file.
"""

import os
import sys
import xml.etree.ElementTree as ET
import zipfile

# ET.indent, which lays the generated table out the way the game's own tables
# are laid out. The guard is here rather than in build.py because this is where
# the requirement comes from, and build.py reaches every path through this
# import.
MIN_PYTHON = (3, 9)
if sys.version_info < MIN_PYTHON:
    raise SystemExit("Python %d.%d or newer is required, this is %d.%d"
                     % (MIN_PYTHON + sys.version_info[:2]))

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCALIZATION_DIR = os.path.join(PROJECT_ROOT, "src", "Localization")
SOURCE = os.path.join(LOCALIZATION_DIR, "strings.xml")
TABLE = "text_ui_savegame_renamer.xml"

# The languages the game ships, spelled as it spells its own paks.
LANGUAGES = ["Chineses", "Chineset", "Czech", "English", "French", "German",
             "Italian", "Japanese", "Korean", "Polish", "Portuguese", "Russian",
             "Spanish", "Turkish", "Ukrainian", "Vietnamese"]

EPOCH = (2026, 8, 18, 0, 0, 0)


def read_strings(path):
    """Read the source table.

    @param path Path of strings.xml.
    @return List of (key, english, {language: text}).
    """
    rows = []
    for node in ET.parse(path).getroot().findall("String"):
        texts = dict((t.get("language"), t.text or "") for t in node.findall("text"))
        unknown = sorted(set(texts) - set(LANGUAGES))
        if unknown:
            raise RuntimeError("%s: unknown language %s" % (node.get("key"), unknown))
        rows.append((node.get("key"), node.get("english"), texts))
    return rows


def table_for(rows, language):
    """Render the game-format table for one language.

    @param rows Output of read_strings.
    @param language Language to pick texts for.
    @return The XML document as text.
    """
    out = ET.Element("Table")
    for key, english, texts in rows:
        row = ET.SubElement(out, "Row")
        for value in (key, english, texts.get(language, english)):
            ET.SubElement(row, "Cell").text = value
    ET.indent(out, space="\t")
    return ET.tostring(out, encoding="unicode") + "\n"


def build():
    """Write one pak per language, returning their paths.

    @return List of paths written.
    """
    rows = read_strings(SOURCE)
    written = []
    for language in LANGUAGES:
        path = os.path.join(LOCALIZATION_DIR, language + "_xml.pak")
        info = zipfile.ZipInfo(TABLE, EPOCH)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o600 << 16
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            z.writestr(info, table_for(rows, language).encode("utf-8"))
        written.append(path)
    return written


def main():
    """Build the paks and report what was written."""
    try:
        written = build()
    except (RuntimeError, ET.ParseError) as exc:
        print(exc)
        return 1
    print("wrote %d localization paks" % len(written))
    return 0


if __name__ == "__main__":
    sys.exit(main())
