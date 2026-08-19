#pragma once

/// Turns what the dialog reports back into a rename of the save it was opened
/// on.
///
/// Owns the save the open dialog belongs to. Nothing else knows which save that
/// is: the dialog is handed a name and gives one back.
namespace RenameFlow {

/// Points the dialog's three outcomes at what each of them means for a save.
///
/// Called once, after RenameDialog::Install has succeeded.
void Wire();

/// Opens the dialog for `saveId`.
///
/// @param saveId Id as shown in the load list.
/// @return True when the dialog was shown.
bool OpenFor(int saveId);

}  // namespace RenameFlow
