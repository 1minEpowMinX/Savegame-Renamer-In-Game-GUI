#pragma once

/// Turns what the dialog reports back into a rename of the save it was opened
/// on, and decides where the prompt that advertises the rename key belongs.
///
/// Owns the save the open dialog belongs to.
namespace RenameFlow {

/// Points the dialog's three outcomes at what each of them means for a save,
/// and the prompt at the page the menu is on.
///
/// Called once, after RenameDialog::Install has succeeded.
void Wire();

/// Opens the dialog for `saveId`.
///
/// @param saveId Id as shown in the load list.
/// @return True when the dialog was shown.
bool OpenFor(int saveId);

/// Reports that the menu has given way to the game world.
///
/// The prompt is drawn over the whole screen, so it does not come down with the
/// menu on its own.
void MenuClosed();

}  // namespace RenameFlow
