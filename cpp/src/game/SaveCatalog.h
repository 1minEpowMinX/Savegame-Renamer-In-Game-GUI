#pragma once

#include <filesystem>
#include <optional>
#include <string>

/// One savegame as the load list shows it.
struct SaveEntry {
    int id = 0;                    ///< SaveId, the number shown before the name.
    std::string displayName;       ///< Name read back out of the file.
    std::filesystem::path file;    ///< Full path to the .whs.
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

/// Returns the savegame with `saveId`, or nothing when it is not listed.
///
/// @param saveId Id as shown in the load list.
/// @return The matching entry.
std::optional<SaveEntry> Find(int saveId);

/// Makes the game re-read every .whs and rebuild its per-type lists.
///
/// @return True when the call was made.
bool Refresh();

}  // namespace SaveCatalog
