// Savegame Renamer dialog -- frame 1 DoAction (AS2 / Flash 8 / Scaleform GFx).
//
// Engine API (inbound, on _root -- see UIElements/SavegameRenamer.xml):
//   fc_open(currentName, canReset)   show the dialog, prefilled
//   fc_close()                       hide it without emitting an event
//   fc_setInput(action)              "accept" | "cancel", fed by the plugin's
//                                    input listener because the engine delivers
//                                    typed characters to the movie but not
//                                    Enter or Esc
// Events (outbound via fscommand):
//   onRenameAccept(name), onRenameCancel(), onRenameReset()
//
// Written for FFDec's AS2 parser: one declaration per var, no object literals,
// no ternary, no chained assignments.

// ------------------------------------------------------- layout constants --
// base.xml draws the frame at 520x260, matching the texture's 2:1 shape, so the
// window uses exactly that size and the ornament is never stretched.
var BOX_X = 140;
var BOX_Y = 95;
var BOX_W = 520;
var BOX_H = 260;
var PAD = 54;
var TITLE_H = 30;
var INPUT_H = 34;
var SOFT_LIMIT = 40;
var MAX_CHARS = 120;

var COLOR_TEXT = 0xCFC2A0;
var COLOR_WARN = 0xC08040;
var COLOR_HOVER = 0xFFF0C8;

// ------------------------------------------------------------- dialog box --
var box = _root.createEmptyMovieClip("box", 1);
box._visible = false;

// A plain panel first, so the dialog stays readable if the frame ever fails to
// load, then the game's own modal frame over it.
box.beginFill(0x000000, 85);
box.moveTo(BOX_X, BOX_Y);
box.lineTo(BOX_X + BOX_W, BOX_Y);
box.lineTo(BOX_X + BOX_W, BOX_Y + BOX_H);
box.lineTo(BOX_X, BOX_Y + BOX_H);
box.endFill();

var frame = box.attachMovie("RenamerFrame", "frame", 1);
frame._x = BOX_X;
frame._y = BOX_Y;

// ---------------------------------------------------------------- helpers --

// embedFonts must be set BEFORE the format is applied: unset, the field falls
// back to device rendering, which draws nothing here because the movie carries
// no device font. The names are the ImportAssets2 symbols from base.xml.
function mkText(parent, name, depth, x, y, w, h, size, color, font) {
    var tf = parent.createTextField(name, depth, x, y, w, h);
    tf.selectable = false;
    tf.embedFonts = true;
    var fmt = new TextFormat();
    fmt.font = font;
    fmt.size = size;
    fmt.color = color;
    tf.setNewTextFormat(fmt);
    tf.styleFmt = fmt;
    return tf;
}

// setNewTextFormat only styles text assigned afterwards, and some GFx builds
// drop it on assignment, so the format is re-applied over the whole field.
function setText(tf, value) {
    tf.text = value;
    tf.setTextFormat(tf.styleFmt);
}

// ----------------------------------------------------------------- fields --

// The ImportAssets2 symbol names from base.xml. TextFormat.font accepts these
// directly; the typeface name baked into the font tag ("Kingdom Come Regular")
// resolves too, but the symbol is what the vanilla menu movie uses.
var FONT_REGULAR = "DefaultFont";
var FONT_BOLD = "DefaultFontBold";

var title = mkText(box, "title", 2, BOX_X + PAD, BOX_Y + PAD,
                   BOX_W - PAD * 2, TITLE_H, 22, COLOR_TEXT, FONT_BOLD);
setText(title, "Rename savegame");

var input = box.createTextField("input", 3, BOX_X + PAD, BOX_Y + PAD + TITLE_H,
                                BOX_W - PAD * 2, INPUT_H);
input.type = "input";
input.selectable = true;
input.embedFonts = true;
input.border = true;
input.borderColor = 0x6B5B3A;
input.maxChars = MAX_CHARS;

var inputFmt = new TextFormat();
inputFmt.font = FONT_REGULAR;
inputFmt.size = 20;
inputFmt.color = COLOR_TEXT;
input.setNewTextFormat(inputFmt);

var counter = mkText(box, "counter", 4, BOX_X + PAD,
                     BOX_Y + PAD + TITLE_H + INPUT_H + 6, 200, 22, 16,
                     COLOR_TEXT, FONT_REGULAR);

// A TextField has no onRelease in AS2, so the button is a clip with an
// invisible hit area and the label parented inside it.
var RESET_W = 220;
var RESET_H = 24;

var resetClip = box.createEmptyMovieClip("resetClip", 5);
resetClip._x = BOX_X + BOX_W - PAD - RESET_W;
resetClip._y = BOX_Y + PAD + TITLE_H + INPUT_H + 6;
resetClip._visible = false;

resetClip.beginFill(0xFFFFFF, 0);
resetClip.moveTo(0, 0);
resetClip.lineTo(RESET_W, 0);
resetClip.lineTo(RESET_W, RESET_H);
resetClip.lineTo(0, RESET_H);
resetClip.endFill();

var resetBtn = mkText(resetClip, "label", 1, 0, 0, RESET_W, RESET_H, 16,
                      COLOR_TEXT, FONT_REGULAR);
setText(resetBtn, "Reset to original");

var hint = mkText(box, "hint", 6, BOX_X + PAD, BOX_Y + BOX_H - PAD - 4,
                  BOX_W - PAD * 2, 22, 15, COLOR_TEXT, FONT_REGULAR);
setText(hint, "Enter - accept, Esc - cancel");

// -------------------------------------------------------------- behaviour --

function updateCounter() {
    if (input.text.length > SOFT_LIMIT) {
        counter.styleFmt.color = COLOR_WARN;
    } else {
        counter.styleFmt.color = COLOR_TEXT;
    }
    setText(counter, input.text.length + " / " + SOFT_LIMIT);
}

// Characters typed into the field arrive with the default format, which is
// bound to no font and therefore draws empty boxes. Re-applying the format
// resets the caret to the start, so it is put back where it was.
function restyleInput() {
    var caret = Selection.getCaretIndex();
    input.setTextFormat(inputFmt);
    Selection.setSelection(caret, caret);
}

input.onChanged = function () {
    restyleInput();
    updateCounter();
};

resetClip.onRollOver = function () {
    resetBtn.styleFmt.color = COLOR_HOVER;
    setText(resetBtn, "Reset to original");
};

resetClip.onRollOut = function () {
    resetBtn.styleFmt.color = COLOR_TEXT;
    setText(resetBtn, "Reset to original");
};

resetClip.onRelease = function () {
    box._visible = false;
    Selection.setFocus(null);
    fscommand("onRenameReset", "");
};

// ------------------------------------------------------------ engine calls --

function fc_open(currentName, canReset) {
    box._visible = true;
    input.text = currentName;
    input.setTextFormat(inputFmt);
    resetClip._visible = canReset;
    updateCounter();
    Selection.setFocus(input);
    Selection.setSelection(input.text.length, input.text.length);
}

function fc_close() {
    box._visible = false;
    Selection.setFocus(null);
}

function fc_setInput(action) {
    if (action == "accept") {
        var typed = input.text;
        box._visible = false;
        Selection.setFocus(null);
        fscommand("onRenameAccept", typed);
    } else if (action == "cancel") {
        box._visible = false;
        Selection.setFocus(null);
        fscommand("onRenameCancel", "");
    }
}

stop();
