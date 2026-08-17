#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/// One savegame as the load list shows it.
struct SaveEntry {
    int id = 0;                    ///< SaveId, the number shown before the name.
    int type = 0;                  ///< wh::framework::E_SaveGameType::Type.
    std::string displayName;       ///< Name read back out of the file.
    std::filesystem::path file;    ///< Full path to the .whs.
};

/// Reads the game's savegame list and asks it to rebuild after a file changed.
///
/// The game's C_SaveGameManager is owned by C_PlayerProfileWHManager and has no
/// resolvable global, so the instance is captured from the `this` of the first
/// UpdateSaveGameDescriptions call. Install() must run before anything else.
namespace SaveCatalog {

/// Hooks UpdateSaveGameDescriptions so the manager instance can be captured.
///
/// @return True when the hook was created and enabled.
bool Install();

/// Returns true once the game has called UpdateSaveGameDescriptions at least once.
bool Ready();

/// Returns every savegame the manager currently knows about.
std::vector<SaveEntry> List();

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
