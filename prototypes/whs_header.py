"""Read and rewrite the description header of a KCD2 .whs savegame.

Layout: 4 bytes 0xFFFFFFFF, int32 little-endian length, then `length` bytes of
NUL-terminated XML (wh::framework::C_SaveGameDescription), then the save payload.
The length covers the terminating NUL. The payload is copied verbatim, so the
description may grow or shrink freely.
"""

import re
import struct

MAGIC = 0xFFFFFFFF
HEADER_SIZE = 8


class Header:
    """The description header of one save file, kept as text plus its payload."""

    def __init__(self, path):
        with open(path, "rb") as f:
            self.raw = f.read()
        magic, length = struct.unpack_from("<Ii", self.raw, 0)
        if magic != MAGIC:
            raise ValueError(f"{path}: magic is {magic:#x}, not {MAGIC:#x}")
        self.xml = self.raw[HEADER_SIZE:HEADER_SIZE + length].rstrip(b"\x00").decode("utf-8")
        self.payload = self.raw[HEADER_SIZE + length:]

    def attr(self, name):
        """Return the value of XML attribute `name` on the root element."""
        m = re.search(rf'{name}="([^"]*)"', self.xml)
        return m.group(1) if m else None

    def set_attr(self, name, value):
        """Replace the value of attribute `name`, or raise if it is absent."""
        pattern = rf'({name}=")[^"]*(")'
        self.xml, n = re.subn(pattern, lambda m: m.group(1) + value + m.group(2), self.xml, count=1)
        if n == 0:
            raise KeyError(f"attribute {name} not present")

    def ui_fields(self):
        """Return UIDescription split on '|', trailing empty element included."""
        return self.attr("UIDescription").split("|")

    def set_ui_fields(self, fields):
        """Write back the pipe-joined UIDescription fields."""
        self.set_attr("UIDescription", "|".join(fields))

    def write(self, path):
        """Write the file with a length field recomputed from the current XML."""
        body = self.xml.encode("utf-8") + b"\x00"
        with open(path, "wb") as f:
            f.write(struct.pack("<Ii", MAGIC, len(body)))
            f.write(body)
            f.write(self.payload)


# UIDescription field indices, from the format string in WHGame.dll and the
# eight values observed in real saves.
F_TYPE, F_ID, F_QUEST, F_OBJECTIVE, F_LOCATION, F_TIMESTAMP, F_DATE, F_PLAYTIME = range(8)
