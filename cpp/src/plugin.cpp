// Savegame Renamer - In-Game GUI (KCSE plugin).
//
// Renames an existing savegame from the load menu. The displayed name lives in
// field 2 of the UIDescription attribute inside the .whs description header;
// the original quest and objective keys are stashed in a RenamerOriginal
// attribute so the name can be reset.

#include "KCSE/KCSEAPI.h"

#include "Log.h"

KCSE_PLUGIN_INFO("SavegameRenamer", "Lefxxx", 1);

namespace {

void OnKcseMessage(KCSE::Message* msg)
{
    if (msg && msg->type == KCSE::IMessagingInterface::kMessage_DataLoaded)
        SR_LOG("data loaded");
}

}  // namespace

KCSE_PLUGIN_LOAD(kcse)
{
    SR_LOG("loaded, KCSE v%u, game build %u",
           kcse->GetKCSEVersion(), kcse->GetGameVersion());

    if (auto* messaging = kcse->GetMessagingInterface())
        messaging->RegisterListener(&OnKcseMessage);

    return true;
}
