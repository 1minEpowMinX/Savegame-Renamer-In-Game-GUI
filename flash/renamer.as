// Savegame Renamer dialog -- frame 1 DoAction (AS2 / Flash 8 / Scaleform GFx).
//
// Engine API (inbound, on _root -- see UIElements/SavegameRenamer.xml):
//   fc_open(currentName, canReset)   show the dialog, prefilled
//   fc_close()                       hide it without emitting an event
//   fc_showHint(visible)             show the F2 prompt over the save list
//   fc_setInput(action)              "accept" | "cancel" | "reset", fed by the
//                                    plugin's input listener because the engine
//                                    delivers typed characters to the movie but
//                                    not these keys
// Events (outbound via fscommand):
//   onRenameAccept(name), onRenameCancel(), onRenameReset()
//
// Every plate here is the game's own: the window frame, the field backing and
// the key caps are imported from the vanilla movies (see base.xml), so the
// dialog is built from the same parts as the screens around it.
//
// Written for FFDec's AS2 parser: one declaration per var, no object literals,
// no ternary, no chained assignments.

// ------------------------------------------------------- layout constants --
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
// The cap is drawn in perspective, with a thicker rail along its bottom edge,
// so its optical centre sits above its geometric one and the key name has to
// ride high to look centred.
var KEY_RISE = 2;
var KEY_PAD = 20;   // padding around the key name inside its cap
// DefaultFontBold maps to the same typeface as DefaultFont (see the font
// mappings the game logs at startup), so the weight is synthesised by the
// player after the text has been measured: the glyphs render wider than
// textWidth reports, and grow rightwards.
//
// That costs two separate corrections, and they are separate constants because
// they are not the same number: the cap has to be wide enough to hold the drawn
// run, while the field has to move left by however far the run's centre drifted.
// Tying both to one figure means neither can be adjusted without disturbing the
// other.
var KEY_BOLD_SPREAD = 1.12;   // how much wider to cut the cap
var KEY_BOLD_DRIFT = 0.12;    // how far the drawn centre sits right of measured

// What is left over after that correction does not scale with the word: names
// of every length lean the same way, so it is corrected by a fixed shift.
//
// Its cause is not established. The cap was the suspect and was cleared: the
// lit face of key_long.dds spans columns 6..121 of 128 and centres exactly on
// the plate, so the plate is symmetric. Letting the field auto-size onto the
// run the player laid out was tried instead of this arithmetic and came out
// visibly worse. Treat the number as measured off the screen, not derived.
var KEY_FACE_OFFSET = 2;
var KEY_BOX = 120;  // width the key name is measured and centred in
var KEY_MIN = 30;   // narrowest cap, so "Del" does not become a square
var KEY_GAP = 6;    // cap to its own label
var PAIR_GAP = 40;  // pair to the next pair

// The rows total 98 of the 152 units between the frame's rails. The surplus is
// split evenly between the three joints rather than pooled above the prompts,
// which is what left a hole in the middle of the window.
var GAP_TITLE = 16;
var GAP_COUNTER = 4;
var GAP_RULE = 16;
var GAP_KEYS = 14;

var SOFT_LIMIT = 40;
var MAX_CHARS = 120;

var COLOR_TITLE = 0xE8DCC0;
var COLOR_TEXT = 0xCFC2A0;
var COLOR_HINT = 0xC0B190;
var COLOR_WARN = 0xC8842E;
var COLOR_HOVER = 0xFFF0C8;
// The key plate is a light cap, so its letter is dark, matching the prompts the
// game prints along the bottom of the screen.
var COLOR_KEYCAP = 0x2A2118;

// The backing dims while the field is idle and comes up to full while it holds
// the caret: in this menu whatever is active is always the brighter thing.
var FIELD_ALPHA = 75;
var FIELD_ALPHA_FOCUS = 100;

// The ImportAssets2 symbol names from base.xml.
var FONT_REGULAR = "DefaultFont";
var FONT_BOLD = "DefaultFontBold";
// The face the game sets its own screen headings in.
var FONT_DISPLAY = "DisplayFont";

// Derived, so a change above moves everything together.
var IN_X = BOX_X + IN_L;
var IN_W = BOX_W - IN_L - IN_R;
var Y_TITLE = BOX_Y + IN_T;
var Y_INPUT = Y_TITLE + ROW_TITLE + GAP_TITLE;
var Y_META = Y_INPUT + ROW_INPUT + GAP_COUNTER;
var Y_RULE = Y_META + ROW_META + GAP_RULE;
var Y_KEYS = Y_RULE + GAP_KEYS;

// ------------------------------------------------------------- dialog box --
var box = _root.createEmptyMovieClip("box", 1);
box._visible = false;

// Nothing is drawn behind the frame: the texture is transparent around its
// ornament, so a filled rectangle the size of the window shows up as black
// corners around it.
var SHADOW_SPREAD_X = 60;
var SHADOW_SPREAD_Y = 50;

var shadow = box.attachMovie("RenamerShadow", "shadow", 0);
shadow._x = BOX_X - SHADOW_SPREAD_X;
shadow._y = BOX_Y - SHADOW_SPREAD_Y;
shadow._width = BOX_W + SHADOW_SPREAD_X * 2;
shadow._height = BOX_H + SHADOW_SPREAD_Y * 2;
shadow._alpha = 80;

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

// A key cap with its label beside it, the pairing the game uses for every
// prompt it prints. `spanW` is how wide the pair came out, so a row of them can
// be centred; `hit` is the clickable area over the whole pair.
function mkKeyHint(parent, name, depth, key, label, action) {
    var clip = parent.createEmptyMovieClip(name, depth);

    // The cap is measured to its key name rather than fixed, so "Enter" and
    // "Del" are not forced to the same width. The game itself ships three cap
    // widths for the same reason.
    var keyText = mkText(clip, "key", 2, 0, 2 - KEY_RISE, KEY_BOX, ROW_KEY, 12,
                         COLOR_KEYCAP, FONT_BOLD, "center");
    setText(keyText, key);
    var capW = keyText.textWidth * KEY_BOLD_SPREAD + KEY_PAD;
    if (capW < KEY_MIN) {
        capW = KEY_MIN;
    }
    // The field keeps the width it was measured in and is slid so that its
    // centre meets the cap's: resizing a TextField after the fact moves the
    // text inside it.
    //
    // The synthesised weight is corrected for here as well. The player centres
    // the run by its measured advances, then thickens each glyph rightwards, so
    // the drawn run keeps its left edge and gains all its extra width on the
    // right: its centre ends up half that gain to the right of the cap's.
    // Widening the cap cannot fix that -- only moving the field can.
    var drift = keyText.textWidth * KEY_BOLD_DRIFT;
    keyText._x = capW / 2 - KEY_BOX / 2 - drift / 2 - KEY_FACE_OFFSET;

    var cap = clip.attachMovie("RenamerKey", "cap", 1);
    cap._x = 0;
    cap._y = 0;
    cap._width = capW;
    cap._height = ROW_KEY;
    cap._alpha = 88;

    var labelText = mkText(clip, "label", 3, capW + KEY_GAP, 2, 200, ROW_KEY, 13,
                           COLOR_HINT, FONT_REGULAR, "left");
    setText(labelText, label);
    labelText._width = labelText.textWidth + 4;

    clip.spanW = capW + KEY_GAP + labelText.textWidth + 4;
    clip.labelText = labelText;
    clip.labelValue = label;
    clip.action = action;

    clip.beginFill(0xFFFFFF, 0);
    clip.moveTo(0, 0);
    clip.lineTo(clip.spanW, 0);
    clip.lineTo(clip.spanW, ROW_KEY);
    clip.lineTo(0, ROW_KEY);
    clip.endFill();

    clip.onRollOver = function () {
        this.labelText.styleFmt.color = COLOR_HOVER;
        setText(this.labelText, this.labelValue);
    };

    clip.onRollOut = function () {
        this.labelText.styleFmt.color = COLOR_HINT;
        setText(this.labelText, this.labelValue);
    };

    clip.onRelease = function () {
        fc_setInput(this.action);
    };

    return clip;
}

// ----------------------------------------------------------------- fields --

var title = mkText(box, "title", 2, IN_X, Y_TITLE, IN_W, ROW_TITLE, 25,
                   COLOR_TITLE, FONT_DISPLAY, "center");
setText(title, "Rename savegame");

// The field is bounded by a rule above and below rather than filled. A fill
// would either hide the panel's damask (TextField backgrounds have no alpha) or
// stretch out of shape: the item plate is a clipped bitmap, so widening it past
// its own size leaves only its edges behind.
var ruleTop = box.attachMovie("RenamerRule", "ruleTop", 3);
ruleTop._x = IN_X;
ruleTop._y = Y_INPUT - 2;
ruleTop._width = IN_W;
ruleTop._alpha = FIELD_ALPHA;

var ruleBottom = box.attachMovie("RenamerRule", "ruleBottom", 10);
ruleBottom._x = IN_X;
ruleBottom._y = Y_INPUT + ROW_INPUT;
ruleBottom._width = IN_W;
ruleBottom._alpha = FIELD_ALPHA;

var input = box.createTextField("input", 4, IN_X, Y_INPUT + 4, IN_W, ROW_INPUT);
input.type = "input";
input.selectable = true;
input.embedFonts = true;
input.maxChars = MAX_CHARS;

var inputFmt = new TextFormat();
inputFmt.font = FONT_REGULAR;
inputFmt.size = 18;
inputFmt.color = COLOR_TEXT;
inputFmt.leftMargin = 10;
input.setNewTextFormat(inputFmt);

var counter = mkText(box, "counter", 5, IN_X, Y_META, IN_W, ROW_META, 14,
                     COLOR_HINT, FONT_REGULAR, "right");

// The rule the game draws between sections of a tooltip, dimmed, separating the
// prompts from the field. It has to be an attached clip: a clip's own drawing
// sits below every child it holds, so a line drawn onto `box` would be hidden
// by the frame.
var footRule = box.attachMovie("RenamerRule", "footRule", 6);
footRule._width = IN_W;
footRule._x = IN_X;
footRule._y = Y_RULE;
footRule._alpha = 35;

// Three prompts of the same shape. Reset is one of them rather than a button:
// in this game an action is a key and a word, and making it anything else is
// what put three different languages in one row.
var keyAccept = mkKeyHint(box, "keyAccept", 7, "Enter", "accept", "accept");
var keyCancel = mkKeyHint(box, "keyCancel", 8, "Esc", "cancel", "cancel");
var keyReset = mkKeyHint(box, "keyReset", 9, "Del", "reset", "reset");

function layoutKeys(withReset) {
    var total = keyAccept.spanW + PAIR_GAP + keyCancel.spanW;
    if (withReset) {
        total = total + PAIR_GAP + keyReset.spanW;
    }
    var x = IN_X + (IN_W - total) / 2;

    keyAccept._x = x;
    keyAccept._y = Y_KEYS;
    x = x + keyAccept.spanW + PAIR_GAP;

    keyCancel._x = x;
    keyCancel._y = Y_KEYS;
    x = x + keyCancel.spanW + PAIR_GAP;

    keyReset._x = x;
    keyReset._y = Y_KEYS;
    keyReset._visible = withReset;
}

layoutKeys(false);

// The prompt that tells the player the key exists at all, sitting over the save
// list rather than inside the dialog. It is placed against the same bottom-right
// corner the game puts its own load and delete prompts in.
var HINT_X = 470;
var HINT_Y = 408;

var hint = mkKeyHint(_root, "hint", 2, "F2", "rename", "");
hint._x = HINT_X;
hint._y = HINT_Y;
hint._visible = false;
hint.onRollOver = null;
hint.onRollOut = null;
hint.onRelease = null;

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
    ruleTop._alpha = FIELD_ALPHA_FOCUS;
    ruleBottom._alpha = FIELD_ALPHA_FOCUS;
};

input.onKillFocus = function () {
    ruleTop._alpha = FIELD_ALPHA;
    ruleBottom._alpha = FIELD_ALPHA;
};

// ------------------------------------------------------------ engine calls --

function fc_open(currentName, canReset) {
    box._visible = true;
    input.text = currentName;
    input.setTextFormat(inputFmt);
    layoutKeys(canReset);
    updateCounter();
    Selection.setFocus(input);
    Selection.setSelection(input.text.length, input.text.length);
}

function fc_showHint(visible) {
    hint._visible = visible;
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
    } else if (action == "reset") {
        if (keyReset._visible) {
            box._visible = false;
            Selection.setFocus(null);
            fscommand("onRenameReset", "");
        }
    }
}

stop();
