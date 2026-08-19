#include "SaveCatalog.h"

#include <ShlObj.h>

#include "REL.h"
#include "Strings.h"
#include "framework/C_SaveGameDescription.h"
#include "framework/C_SaveGameManager.h"

#include "Hook.h"
#include "Log.h"
#include "whs/Description.h"

using wh::framework::C_SaveGameDescription;
using wh::framework::C_SaveGameManager;

namespace SaveCatalog {
namespace {

// C_SaveGameManager::UpdateSaveGameDescriptions: enumerates "*.whs" and rebuilds
// m_slotsByType. Hooked for its `this`: the manager is owned by
// C_PlayerProfileWHManager and has no resolvable global, so this call is the only
// handle on it. Also re-invoked as the list refresh.
constexpr std::uint64_t kUpdateDescriptionsId = 38334;

using UpdateDescriptionsFn = void(*)(C_SaveGameManager*);

C_SaveGameManager* g_manager = nullptr;
REL::Relocation<UpdateDescriptionsFn> g_originalUpdate;

void HookedUpdateDescriptions(C_SaveGameManager* self)
{
    if (self && !g_manager) {
        g_manager = self;
        SR_LOG("save manager captured at %p", static_cast<void*>(self));
    }
    g_originalUpdate(self);
}

/// Returns <Saved Games>\kingdomcome2\saves, or an empty path.
std::filesystem::path SavesRoot()
{
    PWSTR raw = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_SavedGames, 0, nullptr, &raw)))
        return {};
    std::filesystem::path root{raw};
    ::CoTaskMemFree(raw);
    return root / L"kingdomcome2" / L"saves";
}

/// Returns the full path of `fileName` under the playline holding save `saveId`.
///
/// Candidates are matched on the SaveId inside the header as well as on the
/// name. An ambiguous match resolves to an empty path.
///
/// @param fileName Bare name such as "permanent3754.whs".
/// @param saveId Id the header must declare.
/// @return The resolved path, or an empty path.
std::filesystem::path ResolvePath(const std::string& fileName, int saveId)
{
    const auto root = SavesRoot();
    if (root.empty())
        return {};

    // The description carries the bare file name and the manager does not expose
    // the playline index, so every "<root>/playline*/<name>" is a candidate and
    // the header is what tells them apart.
    std::error_code ec;
    std::filesystem::path found;
    for (const auto& dir : std::filesystem::directory_iterator(root, ec)) {
        if (!dir.is_directory(ec))
            continue;
        const auto candidate = dir.path() / fileName;
        if (!std::filesystem::exists(candidate, ec))
            continue;

        const auto header = whs::Description::Read(candidate);
        if (!header.has_value() || header->SaveId() != saveId)
            continue;
        if (!found.empty()) {
            SR_LOG("'%s' with id %d exists in more than one playline, skipping",
                   fileName.c_str(), saveId);
            return {};
        }
        found = candidate;
    }
    return found;
}

}  // namespace

bool Install()
{
    return Hook::Install(kUpdateDescriptionsId, &HookedUpdateDescriptions, g_originalUpdate);
}

std::optional<SaveEntry> Find(int saveId)
{
    if (!g_manager)
        return std::nullopt;

    // Matched on the manager's own description before anything touches the disk:
    // resolving a path reads the header of every file of that name across the
    // playlines, and doing it for the whole list would run on each press of the
    // rename key.
    for (const auto& slot : g_manager->m_slotsByType) {
        for (const C_SaveGameDescription* desc : slot.m_saves) {
            if (!desc || desc->m_saveIndex != saveId)
                continue;

            SaveEntry entry;
            entry.id = desc->m_saveIndex;
            entry.file = ResolvePath(desc->m_fileName.c_str(), entry.id);
            if (entry.file.empty())
                return std::nullopt;

            const auto header = whs::Description::Read(entry.file);
            if (!header.has_value())
                return std::nullopt;

            // The line as the load list renders it, so the dialog can offer it
            // for editing and a custom name can be built on top of the original.
            entry.displayName = Strings::Localize(header->DisplayName());
            const std::string objective = Strings::Localize(header->ObjectiveName());
            if (!objective.empty())
                entry.displayName += " - " + objective;
            return entry;
        }
    }
    return std::nullopt;
}

bool Refresh()
{
    if (!g_manager)
        return false;

    // The trampoline, not the hooked address, so the capture in
    // HookedUpdateDescriptions does not run a second time.
    g_originalUpdate(g_manager);
    return true;
}

}  // namespace SaveCatalog
