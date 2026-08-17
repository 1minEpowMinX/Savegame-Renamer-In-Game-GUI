#include "SaveCatalog.h"

#include <MinHook.h>
#include <ShlObj.h>

#include "REL.h"
#include "framework/C_LocalizedString.h"
#include "framework/C_SaveGameDescription.h"
#include "framework/C_SaveGameManager.h"

#include "Log.h"
#include "whs/Description.h"

using wh::framework::C_SaveGameDescription;
using wh::framework::C_SaveGameManager;

namespace SaveCatalog {
namespace {

// C_SaveGameManager::UpdateSaveGameDescriptions: enumerates "*.whs" and rebuilds
// m_slotsByType. Hooked for its `this`, which is the only handle on the manager
// this plugin can get; also re-invoked as the list refresh.
constexpr std::uint64_t kUpdateDescriptionsId = 38334;

using UpdateDescriptionsFn = void(*)(C_SaveGameManager*);

C_SaveGameManager* g_manager = nullptr;
REL::Relocation<UpdateDescriptionsFn> g_originalUpdate;
bool g_inRefresh = false;

void HookedUpdateDescriptions(C_SaveGameManager* self)
{
    if (self && !g_manager) {
        g_manager = self;
        SR_LOG("save manager captured at %p", static_cast<void*>(self));
    }
    g_originalUpdate(self);
}

/// Returns `text` with localization markup resolved.
///
/// A save the mod has not renamed carries "@qname_..." keys; a save it has
/// renamed carries the player's own words. Localize resolves the first and,
/// per its contract, returns the second untouched.
///
/// @param text Authored string from the header.
/// @return The readable text.
std::string Localize(const std::string& text)
{
    if (text.empty())
        return text;

    CryStringT<char> output;
    if (!wh::framework::C_LocalizedString::Localize(CryStringT<char>(text.c_str()), output))
        return text;
    return output.c_str();
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
/// The description carries the bare file name; the directory is "%s/playline%d/%s"
/// and the manager does not expose the playline index. Candidates are therefore
/// matched on the SaveId inside the header as well as on the name, and an
/// ambiguous match is refused rather than guessed.
///
/// @param fileName Bare name such as "permanent3754.whs".
/// @param saveId Id the header must declare.
/// @return The resolved path, or an empty path.
std::filesystem::path ResolvePath(const std::string& fileName, int saveId)
{
    const auto root = SavesRoot();
    if (root.empty())
        return {};

    std::error_code ec;
    std::filesystem::path found;
    for (const auto& dir : std::filesystem::directory_iterator(root, ec)) {
        if (!dir.is_directory())
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
    void* target = reinterpret_cast<void*>(REL::ID(kUpdateDescriptionsId).address());
    void* original = nullptr;
    if (MH_CreateHook(target, reinterpret_cast<void*>(&HookedUpdateDescriptions), &original) != MH_OK)
        return false;
    if (MH_EnableHook(target) != MH_OK)
        return false;
    g_originalUpdate = REL::Relocation<UpdateDescriptionsFn>(reinterpret_cast<std::uintptr_t>(original));
    return true;
}

bool Ready()
{
    return g_manager != nullptr;
}

std::vector<SaveEntry> List()
{
    std::vector<SaveEntry> out;
    if (!g_manager)
        return out;

    for (const auto& slot : g_manager->m_slotsByType) {
        for (const C_SaveGameDescription* desc : slot.m_saves) {
            if (!desc)
                continue;

            SaveEntry entry;
            entry.id = desc->m_saveIndex;
            // E_SaveGameType is an empty enum-wrapper struct, so the member is a
            // single byte holding E_SaveGameType::Type rather than a readable field.
            entry.type = *reinterpret_cast<const std::uint8_t*>(&desc->m_saveType);
            entry.file = ResolvePath(desc->m_fileName.c_str(), entry.id);
            if (entry.file.empty())
                continue;

            const auto header = whs::Description::Read(entry.file);
            if (!header.has_value())
                continue;

            // The line as the load list renders it, so the dialog can offer it
            // for editing and a custom name can be built on top of the original.
            entry.displayName = Localize(header->DisplayName());
            const std::string objective = Localize(header->ObjectiveName());
            if (!objective.empty())
                entry.displayName += " - " + objective;

            out.push_back(std::move(entry));
        }
    }
    return out;
}

std::optional<SaveEntry> Find(int saveId)
{
    for (auto& entry : List()) {
        if (entry.id == saveId)
            return entry;
    }
    return std::nullopt;
}

bool Refresh()
{
    if (!g_manager || g_inRefresh)
        return false;

    // The hook is still armed, so guard against re-entering our own thunk.
    g_inRefresh = true;
    g_originalUpdate(g_manager);
    g_inRefresh = false;
    return true;
}

}  // namespace SaveCatalog
