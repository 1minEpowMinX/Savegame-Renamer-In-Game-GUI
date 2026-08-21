#pragma once

/// Reads what the player has highlighted in the menu movie.
///
/// The movie's own selection state is what the vanilla delete action follows, so
/// a row read here is the one the mouse and the arrow keys are on.
namespace MenuSelection {

/// Returns the id of the savegame row the player has highlighted.
///
/// @return The id, or -1 when the highlighted item is not a savegame row.
int SaveId();

}  // namespace MenuSelection
