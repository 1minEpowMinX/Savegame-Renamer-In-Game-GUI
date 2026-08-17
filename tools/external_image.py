"""Add a Scaleform external-image reference to a built SWF.

The dialog frame is the game's own texture. A movie refers to one the way every
vanilla screen does: a DefineExternalImage2 tag naming the file, plus an
ExportAssets tag giving it a linkage name for attachMovie. The engine then loads
Libs/UI/<path> itself, so the texture is not copied into the mod.

FFDec cannot express these tags: its XML export drops every GFX-specific tag, so
they are written here, after the ActionScript has been compiled in.

Layout of DefineExternalImage2 (tag 1009), read out of the game's own files:
    u16  character id
    u32  flags, always 0x000E0000 in this game's UI
    u16  width in pixels
    u16  height in pixels
    u8   length of the linkage name, then that many bytes, no terminator
    u8   length of the file path, then that many bytes, no terminator
    u8   zero
"""

import struct

TAG_EXPORT_ASSETS = 56
TAG_EXTERNAL_IMAGE2 = 1009

IMAGE_FLAGS = 0x000E0000


def _tag(code, body):
    """Return one SWF tag, choosing the short or long header form."""
    if len(body) < 0x3F:
        return struct.pack("<H", (code << 6) | len(body)) + body
    return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body


def _pascal(text):
    """Return `text` as a length byte followed by its bytes."""
    raw = text.encode("ascii")
    if len(raw) > 255:
        raise ValueError("%s is too long for a length byte" % text)
    return bytes((len(raw),)) + raw


def external_image_tags(character_id, name, path, width, height):
    """Return the pair of tags declaring one external image.

    @param character_id Character id to assign; must not clash with base.xml.
    @param name Linkage name AS attaches by.
    @param path File path relative to Libs/UI, including the extension.
    @param width Texture width in pixels.
    @param height Texture height in pixels.
    @return The encoded tags.
    """
    body = struct.pack("<HIHH", character_id, IMAGE_FLAGS, width, height)
    body += _pascal(name) + _pascal(path) + b"\x00"

    export = struct.pack("<HH", 1, character_id) + name.encode("ascii") + b"\x00"
    return _tag(TAG_EXTERNAL_IMAGE2, body) + _tag(TAG_EXPORT_ASSETS, export)


def inject(swf_path, images):
    """Insert external-image declarations into an uncompressed SWF.

    The tags go directly after the header, before anything that uses them.

    @param swf_path File to rewrite in place; must be uncompressed ("FWS").
    @param images Sequence of (character_id, name, path, width, height).
    @return None.
    """
    with open(swf_path, "rb") as f:
        raw = f.read()
    if raw[:3] != b"FWS":
        raise RuntimeError("%s is %s, expected an uncompressed FWS" % (swf_path, raw[:3]))

    nbits = raw[8] >> 3
    rect_len = (5 + nbits * 4 + 7) // 8
    split = 8 + rect_len + 4          # header, stage rect, frame rate and count

    tags = b"".join(external_image_tags(*image) for image in images)
    body = raw[8:split] + tags + raw[split:]

    with open(swf_path, "wb") as f:
        f.write(raw[:4])
        f.write(struct.pack("<I", 8 + len(body)))
        f.write(body)
