#pragma once

#include <optional>
#include <string>

#include "whs/Description.h"

/// One savegame as the load list shows it.
struct SaveEntry {
    int id = 0;                ///< SaveId, the number shown before the name.
    std::string displayName;   ///< Name read back out of the file, localized.
    whs::Description header;   ///< The header the name was read from.
};

/// Reads the game's savegame list and asks it to rebuild after a file changed.
///
/// Find() and Refresh() report nothing until the hook Install() places has
/// captured the manager instance.
namespace SaveCatalog {

/// Hooks UpdateSaveGameDescriptions so the manager instance can be captured.
///
/// @return True when the hook was created and enabled.
bool Install();

/// Returns the savegame with `saveId` in `playline`, or nothing when no file of
/// that id stands there.
///
/// @param saveId Id as shown in the load list.
/// @param playline Playline the load list is showing, counted from zero.
/// @return The matching entry.
std::optional<SaveEntry> Find(int saveId, int playline);

/// Makes the game re-read every .whs and rebuild its save lists.
///
/// @return True when the call was made.
bool Refresh();

}  // namespace SaveCatalog
