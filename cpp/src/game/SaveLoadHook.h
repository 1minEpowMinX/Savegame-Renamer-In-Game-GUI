#pragma once

#include <functional>

/// Hooks the load-menu controller so the visible save list can be rebuilt, and
/// reports when the menu arrives at that list and when it leaves.
///
/// The controller and the playline it draws are captured from the game's own
/// call to BuildLoadGamePage.
namespace SaveLoadHook {

/// Hooks C_UISaveLoad::BuildLoadGamePage, and C_UIMenu::PreparePage alongside
/// it. A PreparePage hook that could not be placed is reported and skipped.
///
/// @return True when the load-page hook was created and enabled.
bool Install();

/// Sets the handler called when the menu arrives at the save list or leaves it.
///
/// Arrival is reported once the page has been built, so a handler that draws
/// over the list runs after it is laid out; departure is reported as the menu
/// turns to the page it is leaving for.
///
/// @param handler Called with whether the save list is now on screen.
void SetSaveListHandler(std::function<void(bool)> handler);

/// Rebuilds the load page from the manager's current descriptions.
///
/// @return True when the page was rebuilt.
bool RebuildLoadPage();

/// Returns the playline the load page was last built for.
///
/// @return The index counted from zero, or -1 before the page has been built.
int Playline();

}  // namespace SaveLoadHook
