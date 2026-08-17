// Savegame Renamer - In-Game GUI (KCSE plugin).
//
// Renames an existing savegame from the load menu. The displayed name lives in
// field 2 of the UIDescription attribute inside the .whs description header;
// the original quest and objective keys are stashed in a RenamerOriginal
// attribute so the name can be reset.

#include <MinHook.h>

#include "KCSE/KCSEAPI.h"
#include "Offsets/vtables/IConsole.h"
#include "crysystem/SSystemGlobalEnvironment.h"

#include "Log.h"
#include "game/SaveCatalog.h"

KCSE_PLUGIN_INFO("SavegameRenamer", "Lefxxx", 1);

namespace {

// Temporary, replaced by the F2 dialog: lists what the catalog sees so the
// game-side plumbing can be checked before any UI exists.
void CmdList(IConsoleCmdArgs*)
{
    if (!SaveCatalog::Ready()) {
        SR_LOG("save manager not captured yet, open the load menu once");
        return;
    }
    const auto saves = SaveCatalog::List();
    SR_LOG("%zu savegames", saves.size());
    for (const auto& s : saves)
        SR_LOG("  %5d  type %d  %-40s  %s",
               s.id, s.type, s.displayName.c_str(), s.file.string().c_str());
}

void RegisterCommands()
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (!env || !env->pConsole) {
        SR_LOG("console not available, commands not registered");
        return;
    }
    env->pConsole->AddCommand("renamer_list", &CmdList, VF_NULL,
                              "Lists every savegame the renamer can see.");
}

void OnKcseMessage(KCSE::Message* msg)
{
    if (msg && msg->type == KCSE::IMessagingInterface::kMessage_DataLoaded)
        RegisterCommands();
}

}  // namespace

KCSE_PLUGIN_LOAD(kcse)
{
    SR_LOG("loaded, KCSE v%u, game build %u",
           kcse->GetKCSEVersion(), kcse->GetGameVersion());

    if (MH_Initialize() != MH_OK) {
        SR_LOG("MinHook init failed");
        return false;
    }
    if (!SaveCatalog::Install()) {
        SR_LOG("could not hook the savegame manager");
        return false;
    }

    if (auto* messaging = kcse->GetMessagingInterface())
        messaging->RegisterListener(&OnKcseMessage);

    return true;
}
