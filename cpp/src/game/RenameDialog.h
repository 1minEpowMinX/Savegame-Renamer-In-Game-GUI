#pragma once

#include <functional>
#include <string>

/// Owns the SavegameRenamer flash element: shows the dialog and reports what the
/// player did with it.
///
/// Knows nothing about savegames. It is handed a name to display and gives back
/// the name that was typed, so the element can be exercised on its own.
namespace RenameDialog {

/// Registers the element event listener.
///
/// @return True when the element was found and the listener attached.
bool Install();

/// Returns true while the dialog is on screen.
bool IsOpen();

/// Shows the dialog with `currentName` in its input field.
///
/// @param currentName Name the load list shows for the save being renamed.
/// @param canReset Whether the save carries a stashed original name.
/// @return True when the element accepted the call.
bool Show(const std::string& currentName, bool canReset);

/// Shows or hides the prompt that advertises the rename key.
///
/// It is drawn over the save list, not inside the dialog: a player who has
/// never opened the dialog is exactly the one who needs telling.
///
/// @param visible Whether the prompt should be on screen.
void ShowHint(bool visible);

/// Forwards a key the engine does not deliver to the movie.
///
/// @param action One of "accept", "cancel" or "reset".
/// @return True when the element accepted the call.
bool SendInput(const char* action);

/// Sets the handler called when the player confirms a name.
///
/// @param handler Called with the text as typed.
void SetAcceptHandler(std::function<void(const std::string&)> handler);

/// Sets the handler called when the player dismisses the dialog.
///
/// @param handler Called with no arguments.
void SetCancelHandler(std::function<void()> handler);

/// Sets the handler called when the player asks for the original name back.
///
/// @param handler Called with no arguments.
void SetResetHandler(std::function<void()> handler);

}  // namespace RenameDialog
