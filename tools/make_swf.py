"""Write a minimal uncompressed SWF holding one empty frame script.

The dialog is drawn entirely from ActionScript, so the container needs no
symbols, no fonts and no timeline beyond a single frame. It exists only to give
FFDec a `\\frame 1 - DoAction` tag to compile the real script into, which keeps
renamer.as the one source of truth and keeps a third-party .swf out of the tree.

Run as: make_swf.py <output.swf> [stage_width] [stage_height]
"""

import struct
import sys

SWF_VERSION = 8
FRAME_RATE = 30
TWIPS_PER_PIXEL = 20

TAG_END = 0
TAG_SHOW_FRAME = 1
TAG_SET_BACKGROUND_COLOR = 9
TAG_DO_ACTION = 12

ACTION_END = b"\x00"


def rect(width_px, height_px):
    """Return the SWF RECT describing a stage of the given pixel size.

    @param width_px Stage width in pixels.
    @param height_px Stage height in pixels.
    @return The packed RECT, byte-aligned.
    """
    xmax = width_px * TWIPS_PER_PIXEL
    ymax = height_px * TWIPS_PER_PIXEL
    nbits = max(xmax, ymax).bit_length() + 1          # one more for the sign bit
    bits = "{:05b}".format(nbits)
    for value in (0, xmax, 0, ymax):
        bits += "{:0{n}b}".format(value, n=nbits)
    bits += "0" * (-len(bits) % 8)
    return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))


def tag(code, body=b""):
    """Return one SWF tag, choosing the short or long header form.

    @param code Tag type code.
    @param body Tag payload.
    @return The encoded tag.
    """
    if len(body) < 0x3F:
        return struct.pack("<H", (code << 6) | len(body)) + body
    return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body


def build(width_px, height_px):
    """Return a complete SWF file with one frame and one empty action list.

    @param width_px Stage width in pixels.
    @param height_px Stage height in pixels.
    @return The SWF bytes.
    """
    body = rect(width_px, height_px)
    body += struct.pack("<HH", FRAME_RATE << 8, 1)     # frame rate 8.8 fixed, frame count
    body += tag(TAG_SET_BACKGROUND_COLOR, bytes((0, 0, 0)))
    body += tag(TAG_DO_ACTION, ACTION_END)
    body += tag(TAG_SHOW_FRAME)
    body += tag(TAG_END)

    header = b"FWS" + bytes((SWF_VERSION,))
    return header + struct.pack("<I", len(header) + 4 + len(body)) + body


def main(argv):
    """Write the container to argv[1]."""
    if len(argv) < 2:
        print(__doc__)
        return 1
    width = int(argv[2]) if len(argv) > 2 else 800
    height = int(argv[3]) if len(argv) > 3 else 450
    with open(argv[1], "wb") as f:
        f.write(build(width, height))
    print("wrote %s (%dx%d)" % (argv[1], width, height))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
