#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "whs/Description.h"

namespace whs {

/// Returns the directory of playline `index` under `root`.
///
/// Directories are matched on the number their name ends in rather than on its
/// text, so "playline01" answers for playline 1 as "playline1" does. A number
/// claimed by more than one directory resolves to an empty path.
///
/// @param root Saves folder holding the playlines.
/// @param index Playline index, counted from zero as the load menu counts them.
/// @return The directory, or an empty path.
std::filesystem::path PlaylineDir(const std::filesystem::path& root, int index);

/// Returns the description of `fileName` inside playline `index`.
///
/// The file answers only when its header declares `saveId`, which is what keeps
/// a name reused across playlines from resolving to the wrong playthrough.
///
/// @param root Saves folder holding the playlines.
/// @param index Playline index, counted from zero as the load menu counts them.
/// @param fileName Bare name such as "permanent3754.whs".
/// @param saveId Id the header must declare.
/// @return The description, naming the file it was read from, or nothing.
std::optional<Description> FindSave(const std::filesystem::path& root, int index,
                                    const std::string& fileName, int saveId);

}  // namespace whs
