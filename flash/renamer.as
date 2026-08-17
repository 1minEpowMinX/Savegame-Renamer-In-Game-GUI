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
var BOX_X = 150;
var BOX_Y = 150;
var BOX_W = 500;
var BOX_H = 150;
var PAD = 20;
var TITLE_H = 30;
var INPUT_H = 34;
var SOFT_LIMIT = 40;
var MAX_CHARS = 120;

var COLOR_TEXT = 0xCFC2A0;
var COLOR_WARN = 0xC08040;

// ------------------------------------------------------------- dialog box --
var box = _root.createEmptyMovieClip("box", 1);
box._visible = false;

box.beginFill(0x000000, 85);
box.moveTo(BOX_X, BOX_Y);
box.lineTo(BOX_X + BOX_W, BOX_Y);
box.lineTo(BOX_X + BOX_W, BOX_Y + BOX_H);
box.lineTo(BOX_X, BOX_Y + BOX_H);
box.endFill();

// ---------------------------------------------------------------- helpers --

function mkText(parent, name, depth, x, y, w, h, size, color) {
    var tf = parent.createTextField(name, depth, x, y, w, h);
    tf.selectable = false;
    tf.embedFonts = false;
    var fmt = new TextFormat();
    fmt.size = size;
    fmt.color = color;
    tf.setNewTextFormat(fmt);
    return tf;
}

// ----------------------------------------------------------------- fields --

var title = mkText(box, "title", 2, BOX_X + PAD, BOX_Y + PAD - 6,
                   BOX_W - PAD * 2, TITLE_H, 22, COLOR_TEXT);
title.text = "Rename savegame";

var input = box.createTextField("input", 3, BOX_X + PAD, BOX_Y + PAD + TITLE_H,
                                BOX_W - PAD * 2, INPUT_H);
input.type = "input";
input.selectable = true;
input.embedFonts = false;
input.border = true;
input.borderColor = 0x6B5B3A;
input.maxChars = MAX_CHARS;

var inputFmt = new TextFormat();
inputFmt.size = 20;
inputFmt.color = COLOR_TEXT;
input.setNewTextFormat(inputFmt);

var counter = mkText(box, "counter", 4, BOX_X + PAD,
                     BOX_Y + PAD + TITLE_H + INPUT_H + 6, 120, 22, 16, COLOR_TEXT);

var resetBtn = mkText(box, "resetBtn", 5, BOX_X + BOX_W - PAD - 220,
                      BOX_Y + PAD + TITLE_H + INPUT_H + 6, 220, 22, 16, COLOR_TEXT);
resetBtn.selectable = true;
resetBtn.text = "Reset to original";
resetBtn._visible = false;

var hint = mkText(box, "hint", 6, BOX_X + PAD, BOX_Y + BOX_H - 26,
                  BOX_W - PAD * 2, 22, 15, COLOR_TEXT);
hint.text = "Enter - accept, Esc - cancel";

// -------------------------------------------------------------- behaviour --

function updateCounter() {
    counter.text = input.text.length + " / " + SOFT_LIMIT;
    var fmt = new TextFormat();
    if (input.text.length > SOFT_LIMIT) {
        fmt.color = COLOR_WARN;
    } else {
        fmt.color = COLOR_TEXT;
    }
    counter.setTextFormat(fmt);
}

input.onChanged = function () {
    updateCounter();
};

resetBtn.onRelease = function () {
    box._visible = false;
    Selection.setFocus(null);
    fscommand("onRenameReset", "");
};

// ------------------------------------------------------------ engine calls --

function fc_open(currentName, canReset) {
    box._visible = true;
    input.text = currentName;
    resetBtn._visible = canReset;
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
