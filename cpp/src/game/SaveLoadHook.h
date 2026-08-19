#pragma once

/// Hooks the load-menu controller so the visible save list can be rebuilt.
///
/// The controller and the playline it draws are captured from the game's own
/// call to BuildLoadGamePage.
namespace SaveLoadHook {

/// Hooks C_UISaveLoad::BuildLoadGamePage, and C_UIMenu::PreparePage alongside
/// it. A PreparePage hook that could not be placed is reported and skipped.
///
/// @return True when the load-page hook was created and enabled.
bool Install();

/// Returns true once the game has built the load page at least once.
bool Ready();

/// Rebuilds the load page from the manager's current descriptions.
///
/// @return True when the page was rebuilt.
bool RebuildLoadPage();

/// Returns the id of the savegame row the player has highlighted.
///
/// Reads the menu movie's own selection state, which follows the mouse and the
/// arrow keys as the vanilla delete action does.
///
/// @return The id, or -1 when the highlighted item is not a savegame row.
int SelectedSaveId();

}  // namespace SaveLoadHook
