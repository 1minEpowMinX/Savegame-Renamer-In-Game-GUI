#pragma once

/// Hooks the load-menu controller so the visible save list can be rebuilt.
///
/// C_UISaveLoad is a value member of C_UIMenu with no accessor of its own, and
/// the page builder takes the playline as an argument that nothing else exposes.
/// Both are captured from the game's own call to BuildLoadGamePage.
namespace SaveLoadHook {

/// Hooks C_UISaveLoad::BuildLoadGamePage.
///
/// @return True when the hook was created and enabled.
bool Install();

/// Returns true once the game has built the load page at least once.
bool Ready();

/// Rebuilds the load page from the manager's current descriptions.
///
/// Rewriting a .whs updates the file; the manager still holds the old
/// description and the page still holds the old text, so a rename has to run
/// SaveCatalog::Refresh() and then this.
///
/// @return True when the page was rebuilt.
bool RebuildLoadPage();

/// Returns the id of the savegame row the player has highlighted.
///
/// Reads the menu movie's own selection state, so it follows the mouse and the
/// arrow keys exactly as the vanilla delete action does. Returns -1 when the
/// highlighted item is not a savegame row, which is what makes the rename key
/// inert everywhere except the save list.
int SelectedSaveId();

/// Writes every readable property of the highlighted menu row to the log.
///
/// Diagnostic aid for working out which of the row's fields carries the id the
/// rest of the mod uses.
void ProbeSelection();

}  // namespace SaveLoadHook
