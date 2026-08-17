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

// Measured off the frame on screen: the ornament band across the top is far
// deeper than the sides, and the bottom rail is the shallowest edge.
var IN_L = 36;
var IN_R = 36;
var IN_T = 52;
var IN_B = 16;

var ROW_TITLE = 30;
var ROW_INPUT = 30;
var ROW_META = 20;
var ROW_KEY = 18;
var GAP = 10;

var SOFT_LIMIT = 40;
var MAX_CHARS = 120;

var COLOR_TITLE = 0xE8DCC0;
var COLOR_TEXT = 0xCFC2A0;
var COLOR_HINT = 0xA2957A;
var COLOR_WARN = 0xC8842E;
var COLOR_HOVER = 0xFFF0C8;

// The field is a recess in the panel, the way inventory slots are, and it
// lightens while it holds the caret: in this menu whatever is active is always
// the lighter thing on screen.
var FIELD_BG = 0x142A3D;
var FIELD_BG_FOCUS = 0x20405C;
var FIELD_LINE = 0x6B5B3A;
var FIELD_LINE_FOCUS = 0xA8945F;

var RULE_COLOR = 0x8A7645;

// The ImportAssets2 symbol names from base.xml.
var FONT_REGULAR = "DefaultFont";
var FONT_BOLD = "DefaultFontBold";
// The face the game sets its own screen headings in.
var FONT_DISPLAY = "DisplayFont";

// Derived, so a change above moves everything together.
var IN_X = BOX_X + IN_L;
var IN_W = BOX_W - IN_L - IN_R;
var Y_TITLE = BOX_Y + IN_T;
var Y_INPUT = Y_TITLE + ROW_TITLE + GAP;
var Y_META = Y_INPUT + ROW_INPUT + 8;
var Y_KEYS = BOX_Y + BOX_H - IN_B - ROW_KEY;
var Y_RULE = Y_KEYS - 10;

// ------------------------------------------------------------- dialog box --
var box = _root.createEmptyMovieClip("box", 1);
box._visible = false;

// Nothing is drawn behind the frame: the texture is transparent around its
// ornament, so a filled rectangle the size of the window shows up as black
// corners around it.
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

function strokeRect(clip, x, y, w, h, color) {
    clip.lineStyle(1, color, 100);
    clip.moveTo(x, y);
    clip.lineTo(x + w, y);
    clip.lineTo(x + w, y + h);
    clip.lineTo(x, y + h);
    clip.lineTo(x, y);
}

// A key name in a box followed by what it does, the way the game labels its own
// prompts along the bottom of the screen. Returns the clip; its `spanW` is how
// wide the pair came out, so a row of them can be centred.
function mkKeyHint(parent, name, depth, key, label) {
    var clip = parent.createEmptyMovieClip(name, depth);

    var keyText = mkText(clip, "key", 1, 5, 1, 120, ROW_KEY, 13,
                         COLOR_TITLE, FONT_REGULAR, "left");
    setText(keyText, key);
    var keyW = keyText.textWidth + 4;
    keyText._width = keyW;

    strokeRect(clip, 0, 0, keyW + 10, ROW_KEY, RULE_COLOR);

    var labelText = mkText(clip, "label", 2, keyW + 16, 1, 200, ROW_KEY, 13,
                           COLOR_HINT, FONT_REGULAR, "left");
    setText(labelText, label);
    labelText._width = labelText.textWidth + 4;

    clip.spanW = keyW + 16 + labelText.textWidth + 4;
    return clip;
}

// ----------------------------------------------------------------- fields --

var title = mkText(box, "title", 2, IN_X, Y_TITLE, IN_W, ROW_TITLE, 25,
                   COLOR_TITLE, FONT_DISPLAY, "center");
setText(title, "Rename savegame");

var input = box.createTextField("input", 3, IN_X, Y_INPUT, IN_W, ROW_INPUT);
input.type = "input";
input.selectable = true;
input.embedFonts = true;
input.background = true;
input.backgroundColor = FIELD_BG;
input.border = true;
input.borderColor = FIELD_LINE;
input.maxChars = MAX_CHARS;

var inputFmt = new TextFormat();
inputFmt.font = FONT_REGULAR;
inputFmt.size = 18;
inputFmt.color = COLOR_TEXT;
inputFmt.leftMargin = 8;
input.setNewTextFormat(inputFmt);

// The counter sits at the right of the row under the field and the reset
// control at its left, so the one that only sometimes applies never shifts the
// one that always does.
var counter = mkText(box, "counter", 4, IN_X, Y_META, IN_W, ROW_META, 14,
                     COLOR_HINT, FONT_REGULAR, "right");

// Reset is bordered like a key prompt so it reads as something to press rather
// than as another line of text.
var resetClip = box.createEmptyMovieClip("resetClip", 5);
resetClip._x = IN_X;
resetClip._y = Y_META - 2;
resetClip._visible = false;

var resetBtn = mkText(resetClip, "label", 1, 8, 1, 180, ROW_KEY, 13,
                      COLOR_TEXT, FONT_REGULAR, "left");
setText(resetBtn, "Reset to original");
var resetW = resetBtn.textWidth + 4;
resetBtn._width = resetW;

resetClip.beginFill(0xFFFFFF, 0);
resetClip.moveTo(0, 0);
resetClip.lineTo(resetW + 16, 0);
resetClip.lineTo(resetW + 16, ROW_KEY);
resetClip.lineTo(0, ROW_KEY);
resetClip.endFill();
strokeRect(resetClip, 0, 0, resetW + 16, ROW_KEY, RULE_COLOR);

// A hairline above the prompts, separating them from the field the way the
// game separates its own bottom prompt bar.
box.lineStyle(1, RULE_COLOR, 55);
box.moveTo(IN_X, Y_RULE);
box.lineTo(IN_X + IN_W, Y_RULE);

var keyAccept = mkKeyHint(box, "keyAccept", 6, "Enter", "accept");
var keyCancel = mkKeyHint(box, "keyCancel", 7, "Esc", "cancel");

var keysW = keyAccept.spanW + 26 + keyCancel.spanW;
keyAccept._x = IN_X + (IN_W - keysW) / 2;
keyAccept._y = Y_KEYS;
keyCancel._x = keyAccept._x + keyAccept.spanW + 26;
keyCancel._y = Y_KEYS;

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

input.onSetFocus = function () {
    input.backgroundColor = FIELD_BG_FOCUS;
    input.borderColor = FIELD_LINE_FOCUS;
};

input.onKillFocus = function () {
    input.backgroundColor = FIELD_BG;
    input.borderColor = FIELD_LINE;
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
