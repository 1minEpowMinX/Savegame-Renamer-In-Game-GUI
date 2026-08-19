"""Render the two images the Nexus page needs from the in-game screenshot.

The screenshot beside this file is the mod running: the dialog open over the save
list, six rows of it under the same quest name, and the F2 prompt in the corner
the game keeps its own prompts in. Both images are cut from it rather than drawn,
so the page shows what the player gets and nothing else.

Type is set in the dialog's own colours, read off flash/renamer.as.

Run with no arguments; the PNGs land beside this file.
"""

import os

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "Fonts")
SHOT = os.path.join(HERE, "screenshot-rename-dialog.png")

# The dialog's own colours (flash/renamer.as). GOLD is COLOR_PROMPT, the shade
# every gold caption in the vanilla menu is set in.
GOLD = (0xF6, 0xE8, 0x90)
HINT = (0xC0, 0xB1, 0x90)

DISPLAY = os.path.join(FONTS, "BOOKOSB.TTF")
BODY_ITALIC = os.path.join(FONTS, "constani.ttf")

# Regions of the 2560x1600 screenshot, measured off it.
#
# DIALOG is the frame with its finials and nothing else: the "Back" caption of
# the menu underneath sits just past its lower right corner.
#
# FRAME_16_9 is the 16:9 band that keeps the game's logo, the dialog and the F2
# prompt while dropping the frame-time counter above them.
DIALOG = (566, 385, 1972, 1207)
FRAME_16_9 = (0, 45, 2560, 1485)


def font(path, size):
    """Load a face at `size`.

    @param path Font file.
    @param size Point size.
    @return The loaded font.
    """
    return ImageFont.truetype(path, size)


def tracked_width(draw, text, face, tracking):
    """Measure `text` as tracked_text would draw it.

    @param draw Drawing context.
    @param text Text to measure.
    @param face Font to measure with.
    @param tracking Extra pixels between characters.
    @return Width in pixels.
    """
    total = 0
    for char in text:
        total += draw.textlength(char, font=face) + tracking
    return total - tracking if text else 0


def tracked_text(draw, xy, text, face, fill, tracking):
    """Draw `text` one character at a time with `tracking` between them.

    Pillow has no letter-spacing, and the game's headings are widely tracked
    caps.

    @param draw Drawing context.
    @param xy Top-left position.
    @param text Text to draw.
    @param face Font to draw with.
    @param fill Colour.
    @param tracking Extra pixels between characters.
    @return Width drawn.
    """
    x, y = xy
    for char in text:
        draw.text((x, y), char, font=face, fill=fill)
        x += draw.textlength(char, font=face) + tracking
    return tracked_width(draw, text, face, tracking)


def scrim(img, box, strength, feather):
    """Darken `box` under a feathered mask.

    @param img Image to darken, modified in place.
    @param box (x0, y0, x1, y1) of the darkened area.
    @param strength Peak opacity of the black, 0-255.
    @param feather Blur radius of the mask edge.
    @return None.
    """
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).rectangle(box, fill=strength)
    img.paste((0, 0, 0), (0, 0), mask.filter(ImageFilter.GaussianBlur(feather)))


def rule(draw, x0, y, x1, alpha, thickness=2):
    """Draw one of the menu's hairline gold rules.

    @param draw Drawing context.
    @param x0 Left end.
    @param y Vertical position.
    @param x1 Right end.
    @param alpha Opacity, 0-1, applied against black.
    @param thickness Height in pixels.
    @return None.
    """
    draw.rectangle((x0, y, x1, y + thickness - 1),
                   fill=tuple(int(c * alpha) for c in GOLD))


def title_block(img, x, y, name_size, sub_size, tracking, rule_len):
    """Draw the mod's name, a rule and its subtitle at (x, y).

    @param img Image to draw into.
    @param x Left edge of the block.
    @param y Top edge of the block.
    @param name_size Type size of the name.
    @param sub_size Type size of the subtitle.
    @param tracking Extra pixels between the name's characters.
    @param rule_len Length of the rule under the name.
    @return None.
    """
    draw = ImageDraw.Draw(img)
    face = font(DISPLAY, name_size)
    tracked_text(draw, (x, y), "SAVEGAME", face, GOLD, tracking)
    tracked_text(draw, (x, y + int(name_size * 1.22)), "RENAMER", face, GOLD, tracking)

    rule_y = y + int(name_size * 2.55)
    rule(draw, x + 4, rule_y, x + rule_len, 0.55)
    draw.text((x + 2, rule_y + int(sub_size * 0.55)), "In-game GUI",
              font=font(BODY_ITALIC, sub_size), fill=HINT)


def header():
    """Render the 1400x400 mod page header.

    The name on the left, the dialog itself on the right, over the screenshot
    blurred down to a ground.

    @return Path written.
    """
    w, h = 1400, 400
    shot = Image.open(SHOT).convert("RGB")

    # Scaled to the header's width, then a band taken from the middle of it. The
    # band still carries the dialog, which the blur reduces to a shape.
    ground = shot.resize((w, int(w * shot.height / shot.width)), Image.LANCZOS)
    top = (ground.height - h) // 2
    img = ground.crop((0, top, w, top + h))
    img = img.filter(ImageFilter.GaussianBlur(22))
    img = ImageEnhance.Brightness(img).enhance(0.34)

    dialog = shot.crop(DIALOG)
    dialog = dialog.resize((int(dialog.width * 330 / dialog.height), 330), Image.LANCZOS)

    shadow = Image.new("L", img.size, 0)
    dx, dy = w - dialog.width - 56, (h - dialog.height) // 2
    ImageDraw.Draw(shadow).rectangle((dx, dy, dx + dialog.width, dy + dialog.height),
                                     fill=210)
    img.paste((0, 0, 0), (0, 0), shadow.filter(ImageFilter.GaussianBlur(26)))

    # Pasted under a softened mask: the crop carries a margin of the screen
    # behind the frame, and a hard edge against the blurred ground reads as a
    # rectangle rather than as a panel lying on it.
    edge = Image.new("L", dialog.size, 0)
    ImageDraw.Draw(edge).rectangle((4, 4, dialog.width - 5, dialog.height - 5), fill=255)
    img.paste(dialog, (dx, dy), edge.filter(ImageFilter.GaussianBlur(4)))

    scrim(img, (0, 0, dx - 40, h), 120, 60)
    title_block(img, 84, 118, 58, 24, 8, 300)

    draw = ImageDraw.Draw(img)
    rule(draw, 0, 0, w, 0.32, 3)
    rule(draw, 0, h - 3, w, 0.32, 3)

    path = os.path.join(HERE, "header-1400x400.png")
    img.save(path)
    return path


def mod_image():
    """Render the 1920x1080 mod image.

    The screenshot itself, cropped to the listing's aspect, with the name in the
    empty sky opposite the game's logo.

    @return Path written.
    """
    w, h = 1920, 1080
    img = Image.open(SHOT).convert("RGB").crop(FRAME_16_9).resize((w, h), Image.LANCZOS)

    # The block sits clear of the dialog's top left corner: the scrim stops above
    # it and the rule stops short of it.
    scrim(img, (0, 0, 800, 300), 132, 70)
    title_block(img, 96, 96, 92, 38, 13, 330)

    draw = ImageDraw.Draw(img)
    rule(draw, 0, 0, w, 0.32, 4)
    rule(draw, 0, h - 4, w, 0.32, 4)

    path = os.path.join(HERE, "mod-image-1920x1080.png")
    img.save(path)
    return path


def main():
    """Render both images and report where they went."""
    if not os.path.isfile(SHOT):
        raise SystemExit("%s is missing" % os.path.basename(SHOT))
    for path in (header(), mod_image()):
        print("wrote %s (%d bytes)" % (os.path.relpath(path, HERE), os.path.getsize(path)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
