// Savegame Renamer dialog -- frame 1 DoAction (AS2 / Flash 8 / Scaleform GFx).
//
// Engine API (inbound, on _root -- see UIElements/SavegameRenamer.xml):
//   fc_open(currentName, canReset)   show the dialog, prefilled
//   fc_close()                       hide it without emitting an event
//   fc_setInput(action)              "accept" | "cancel", fed by the plugin's
//                                    input listener because the engine does not
//                                    deliver those keys to the movie
// Events (outbound via fscommand):
//   onRenameAccept(name), onRenameCancel(), onRenameReset()
//
// Written for FFDec's AS2 parser: one declaration per var, no object literals,
// no ternary, no chained assignments.

// ------------------------------------------------------- layout constants --
var STAGE_W = 800;
var STAGE_H = 450;
var BOX_W = 500;
var BOX_H = 150;
var BOX_X = 150;
var BOX_Y = 150;

// ------------------------------------------------------------- dialog box --
var box = _root.createEmptyMovieClip("box", 1);
box._visible = false;

box.beginFill(0x000000, 80);
box.moveTo(BOX_X, BOX_Y);
box.lineTo(BOX_X + BOX_W, BOX_Y);
box.lineTo(BOX_X + BOX_W, BOX_Y + BOX_H);
box.lineTo(BOX_X, BOX_Y + BOX_H);
box.endFill();

// ----------------------------------------------------------- engine calls --

function fc_open(currentName, canReset) {
    box._visible = true;
}

function fc_close() {
    box._visible = false;
}

function fc_setInput(action) {
    if (action == "accept") {
        box._visible = false;
        fscommand("onRenameAccept", "");
    } else if (action == "cancel") {
        box._visible = false;
        fscommand("onRenameCancel", "");
    }
}

stop();
