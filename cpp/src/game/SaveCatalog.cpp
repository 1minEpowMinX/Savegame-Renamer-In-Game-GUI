#include "SaveCatalog.h"

#include <ShlObj.h>

#include <utility>

#include "REL.h"
#include "Strings.h"
#include "framework/C_SaveGameDescription.h"
#include "framework/C_SaveGameManager.h"

#include "Hook.h"
#include "Log.h"
#include "whs/Saves.h"

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

}  // namespace

bool Install()
{
    return Hook::Install(kUpdateDescriptionsId, &HookedUpdateDescriptions, g_originalUpdate);
}

std::optional<SaveEntry> Find(int saveId, int playline)
{
    if (!g_manager)
        return std::nullopt;

    const auto root = SavesRoot();
    int matchedById = 0;

    // Matched on the manager's own descriptions before anything touches the disk,
    // so the common press of the rename key costs one header read rather than the
    // whole list.
    for (const auto& slot : g_manager->m_slotsByType) {
        for (const C_SaveGameDescription* desc : slot.m_saves) {
            if (!desc || desc->m_saveIndex != saveId)
                continue;
            ++matchedById;

            // Ids are handed out per playline, so the same id and the same file
            // name occur once in each of them. The description does not say which
            // playline it came from; the file the page is showing is the one that
            // exists in the playline the page was built for, and a description
            // belonging to another one simply resolves to nothing here.
            auto header = whs::FindSave(root, playline, desc->m_fileName.c_str(), saveId);
            if (!header.has_value())
                continue;

            SaveEntry entry;
            entry.id = desc->m_saveIndex;

            // The line as the load list renders it, so the dialog can offer it
            // for editing and a custom name can be built on top of the original.
            entry.displayName = Strings::Localize(header->DisplayName());
            const std::string objective = Strings::Localize(header->ObjectiveName());
            if (!objective.empty())
                entry.displayName += " - " + objective;
            entry.header = std::move(*header);
            return entry;
        }
    }

    if (matchedById > 0)
        SR_LOG("id %d is listed but no file of it stands in playline %d", saveId, playline);
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
