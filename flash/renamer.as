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

// The key plates come out of the game's own button library: buttons.gfx holds a
// sprite named mc_button whose frames are the three plate widths, each already
// carrying a centred, black, bold text field for the key name. Loading it is
// what the game does for every prompt it prints, so a plate built here matches
// the ones on the same screen down to the pixel.
// The game asks for it as buttons.swf, the name it was authored under. The
// shipped name is what is asked for here instead: the log shows the file
// opener is handed the string as written, so the authoring name reaches it
// unrewritten and finds nothing.
var CAP_LIBRARY = "buttons.gfx";
var CAP_NATIVE_H = 64;             // height the sprite is authored at

// Frame names, which describe the plate rather than the key: ControlsEnum puts
// Esc on the medium plate and Delete on the wide one alongside Enter, while F2
// and every other function key get the small square.
var CAP_SMALL = "key";
var CAP_MEDIUM = "escape";
var CAP_WIDE = "enter";

// What each plate advances by at native height, from ControlsEnum's E_WIDTH_n.
// It is narrower than the plate is drawn: the artwork carries transparent margin
// that the next word is allowed to sit over.
var CAP_SPAN_SMALL = 52;
var CAP_SPAN_MEDIUM = 83;
var CAP_SPAN_WIDE = 106;

// TextExtension.ButtonsScale: the game sizes an inline plate at one and a half
// times the type it interrupts, and centres it on that line. The measure is the
// character box, not the line box, so it is taken from the size the face is set
// at: a line carries leading on top of the type, and sizing the plate off the
// line makes it half again as tall as the game's own.
var CAP_SCALE = 1.5;

var KEY_GAP = 6;    // cap to its own label
var PAIR_GAP = 40;  // pair to the next pair

// The rows total 100 of the 152 units between the frame's rails: the prompt row
// is as tall as its plates, which are one and a half times the type. The surplus
// is split between the joints rather than pooled above the prompts, which is
// what left a hole in the middle of the window.
var GAP_TITLE = 16;
var GAP_COUNTER = 4;
var GAP_RULE = 16;
var GAP_KEYS = 16;

var SOFT_LIMIT = 40;
var MAX_CHARS = 120;

var COLOR_TITLE = 0xE8DCC0;
var COLOR_TEXT = 0xCFC2A0;
var COLOR_HINT = 0xC0B190;
var COLOR_WARN = 0xC8842E;
var COLOR_HOVER = 0xFFF0C8;

// The backing dims while the field is idle and comes up to full while it holds
// the caret: in this menu whatever is active is always the brighter thing.
var FIELD_ALPHA = 75;
var FIELD_ALPHA_FOCUS = 100;

// The ImportAssets2 symbol names from base.xml.
var FONT_REGULAR = "DefaultFont";
// The face the game sets its own screen headings in.
var FONT_DISPLAY = "DisplayFont";
// The face of the line the game prints under the save list, which the rename
// prompt sits directly above and has to match.
var FONT_ITALIC = "DefaultFontItalic";
// Sampled out of Menu.gfx: every gold caption in the menu is this colour, and
// the line the prompt sits above is set at size 15.
var COLOR_PROMPT = 0xF6E890;
// A step under the line it sits above: the prompt is an aside about the screen,
// and set level with that line it read as the louder of the two.
var PROMPT_SIZE = 13;

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
// be centred, and `rowH` how tall.
function mkKeyHint(parent, name, depth, key, capFrame, capSpan, label, action,
                   labelSize, labelFont, labelColor) {
    var clip = parent.createEmptyMovieClip(name, depth);

    // The label is laid out first because the plate is sized from it: a plate
    // is one and a half lines tall and centred on the line, so the line has to
    // be measured before either can be placed.
    var labelText = mkText(clip, "label", 3, 0, 0, 400, labelSize * 3, labelSize,
                           labelColor, labelFont, "left");
    setText(labelText, label);

    var metrics = labelText.getLineMetrics(0);
    var capH = labelSize * CAP_SCALE;
    var capW = capSpan * capH / CAP_NATIVE_H;
    var rowH = capH;

    // A text field starts its first line two units below its own top edge, so
    // the field is hung high enough that the line's middle meets the plate's.
    labelText._x = capW + KEY_GAP;
    labelText._y = rowH / 2 - 2 - metrics.height / 2;
    labelText._width = labelText.textWidth + 4;

    // The plate is drawn from its left edge and centred on its own origin
    // vertically, which is why the holder sits on the row's centre line and not
    // at its top.
    var holder = clip.createEmptyMovieClip("cap", 1);
    holder._x = 0;
    holder._y = rowH / 2;

    // Loading is asynchronous, and what the listener needs is held in this
    // scope rather than on the holder: a clip loaded into keeps its name and
    // its place but is rebuilt, so anything hung on it beforehand is gone by
    // the time the load reports back.
    var capScale = capH / CAP_NATIVE_H * 100;
    var loader = new MovieClipLoader();
    var watcher = new Object();
    loader.addListener(watcher);
    watcher.onLoadInit = function () {
        holder.mc_button.gotoAndStop(capFrame);
        holder.mc_button.tField.textAutoSize = "shrink";
        holder.mc_button.tField.text = key;
        holder.mc_button._xscale = capScale;
        holder.mc_button._yscale = capScale;
        // Both belong to the hold prompts, which none of these keys are, and
        // the frames carry them regardless.
        holder.mc_button.mc_holdIcon._visible = false;
        holder.mc_button.mc_holdIndicator._visible = false;
    };
    loader.loadClip(CAP_LIBRARY, holder);

    // The loader is collected once nothing refers to it, taking the pending
    // load with it.
    clip.capLoader = loader;
    clip.capWatcher = watcher;

    clip.rowH = rowH;
    clip.spanW = capW + KEY_GAP + labelText.textWidth + 4;
    clip.labelText = labelText;
    clip.labelValue = label;
    clip.action = action;
    clip.restColor = labelColor;

    clip.beginFill(0xFFFFFF, 0);
    clip.moveTo(0, 0);
    clip.lineTo(clip.spanW, 0);
    clip.lineTo(clip.spanW, rowH);
    clip.lineTo(0, rowH);
    clip.endFill();

    clip.onRollOver = function () {
        this.labelText.styleFmt.color = COLOR_HOVER;
        setText(this.labelText, this.labelValue);
    };

    clip.onRollOut = function () {
        this.labelText.styleFmt.color = this.restColor;
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
var keyAccept = mkKeyHint(box, "keyAccept", 7, "Enter", CAP_WIDE, CAP_SPAN_WIDE,
                          "accept", "accept", 13, FONT_REGULAR, COLOR_HINT);
var keyCancel = mkKeyHint(box, "keyCancel", 8, "Esc", CAP_MEDIUM, CAP_SPAN_MEDIUM,
                          "cancel", "cancel", 13, FONT_REGULAR, COLOR_HINT);
var keyReset = mkKeyHint(box, "keyReset", 9, "Del", CAP_WIDE, CAP_SPAN_WIDE,
                         "reset", "reset", 13, FONT_REGULAR, COLOR_HINT);

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
// The game keeps its own prompts in a column on the right: a gold rule with the
// "you can load this with E" line under it. The rename prompt goes directly
// above that rule and on its centre line, so it joins that block instead of
// floating beside it. Positioned by its centre, so the prompt stays put when
// its wording changes length.
var HINT_CENTRE_X = 612;
var HINT_CENTRE_Y = 389;

// The prompt belongs to the game's own line below it, not to the dialog, so it
// is built at that line's size, face and colour rather than the dialog's. F2 is
// a function key, which the game puts on the small square plate.
var hint = mkKeyHint(_root, "hint", 2, "F2", CAP_SMALL, CAP_SPAN_SMALL,
                     "rename save", "", PROMPT_SIZE, FONT_ITALIC, COLOR_PROMPT);
hint._x = HINT_CENTRE_X - hint.spanW / 2;
hint._y = HINT_CENTRE_Y - hint.rowH / 2;
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
