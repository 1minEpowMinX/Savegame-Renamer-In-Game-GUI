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
// The stage is 800x450 and the element is stretched over the whole screen, so
// these are effectively fractions of it. The window keeps the 2:1 shape of the
// frame texture behind it.
var BOX_W = 440;
var BOX_H = 220;
var BOX_X = 180;
var BOX_Y = 115;

// The frame's ornament eats into the window, so content is laid out inside
// these insets rather than against the outer edge.
var IN_L = 38;
var IN_R = 38;
var IN_T = 30;
var IN_B = 22;

var ROW_TITLE = 26;
var ROW_INPUT = 30;
var ROW_META = 20;
var GAP = 10;

var SOFT_LIMIT = 40;
var MAX_CHARS = 120;

var COLOR_TITLE = 0xE8DCC0;
var COLOR_TEXT = 0xCFC2A0;
var COLOR_HINT = 0xA2957A;
var COLOR_WARN = 0xC8842E;
var COLOR_HOVER = 0xFFF0C8;

// The ImportAssets2 symbol names from base.xml.
var FONT_REGULAR = "DefaultFont";
var FONT_BOLD = "DefaultFontBold";

// Derived, so a change above moves everything together.
var IN_X = BOX_X + IN_L;
var IN_W = BOX_W - IN_L - IN_R;
var Y_TITLE = BOX_Y + IN_T;
var Y_INPUT = Y_TITLE + ROW_TITLE + GAP;
var Y_META = Y_INPUT + ROW_INPUT + 6;
var Y_HINT = BOX_Y + BOX_H - IN_B - ROW_META;

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
frame._width = BOX_W;
frame._height = BOX_H;

// ---------------------------------------------------------------- helpers --

// embedFonts must be set BEFORE the format is applied: unset, the field falls
// back to device rendering, which draws nothing here because the movie carries
// no device font.
function mkText(parent, name, depth, x, y, w, h, size, color, font, align) {
    var tf = parent.createTextField(name, depth, x, y, w, h);
    tf.selectable = false;
    tf.embedFonts = true;
    var fmt = new TextFormat();
    fmt.font = font;
    fmt.size = size;
    fmt.color = color;
    fmt.align = align;
    tf.setNewTextFormat(fmt);
    tf.styleFmt = fmt;
    return tf;
}

// setNewTextFormat only styles text assigned afterwards, and this player drops
// it on assignment, so the format is re-applied over the whole field.
function setText(tf, value) {
    tf.text = value;
    tf.setTextFormat(tf.styleFmt);
}

// ----------------------------------------------------------------- fields --

var title = mkText(box, "title", 2, IN_X, Y_TITLE, IN_W, ROW_TITLE, 21,
                   COLOR_TITLE, FONT_BOLD, "center");
setText(title, "Rename savegame");

var input = box.createTextField("input", 3, IN_X, Y_INPUT, IN_W, ROW_INPUT);
input.type = "input";
input.selectable = true;
input.embedFonts = true;
input.border = true;
input.borderColor = 0x6B5B3A;
input.maxChars = MAX_CHARS;

var inputFmt = new TextFormat();
inputFmt.font = FONT_REGULAR;
inputFmt.size = 18;
inputFmt.color = COLOR_TEXT;
inputFmt.leftMargin = 6;
input.setNewTextFormat(inputFmt);

// The counter sits at the right of the row under the field and the reset
// control at its left, so the one that only sometimes applies never shifts the
// one that always does.
var counter = mkText(box, "counter", 4, IN_X, Y_META, IN_W, ROW_META, 14,
                     COLOR_HINT, FONT_REGULAR, "right");

var RESET_W = 190;
var resetClip = box.createEmptyMovieClip("resetClip", 5);
resetClip._x = IN_X;
resetClip._y = Y_META;
resetClip._visible = false;

// A TextField has no onRelease in AS2, so the control is a clip with an
// invisible hit area and the label parented inside it.
resetClip.beginFill(0xFFFFFF, 0);
resetClip.moveTo(0, 0);
resetClip.lineTo(RESET_W, 0);
resetClip.lineTo(RESET_W, ROW_META);
resetClip.lineTo(0, ROW_META);
resetClip.endFill();

var resetBtn = mkText(resetClip, "label", 1, 0, 0, RESET_W, ROW_META, 14,
                      COLOR_TEXT, FONT_REGULAR, "left");
setText(resetBtn, "Reset to original name");

var hint = mkText(box, "hint", 6, IN_X, Y_HINT, IN_W, ROW_META, 14,
                  COLOR_HINT, FONT_REGULAR, "center");
setText(hint, "Enter accepts, Esc cancels, an empty name resets");

// -------------------------------------------------------------- behaviour --

function updateCounter() {
    if (input.text.length > SOFT_LIMIT) {
        counter.styleFmt.color = COLOR_WARN;
    } else {
        counter.styleFmt.color = COLOR_HINT;
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
    setText(resetBtn, "Reset to original name");
};

resetClip.onRollOut = function () {
    resetBtn.styleFmt.color = COLOR_TEXT;
    setText(resetBtn, "Reset to original name");
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
