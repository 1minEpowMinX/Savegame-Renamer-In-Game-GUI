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

#include <cstdlib>
#include <string>

#include "Log.h"
#include "game/RenameDialog.h"
#include "game/SaveCatalog.h"
#include "game/SaveLoadHook.h"
#include "whs/Description.h"

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

// Temporary, replaced by the F2 dialog: renames one savegame by id. Everything
// after the id becomes the name, spaces included; passing no name resets the
// save to the quest name it had before the first rename.
void CmdSet(IConsoleCmdArgs* args)
{
    if (args->GetArgCount() < 2) {
        SR_LOG("usage: renamer_set <id> [name]");
        return;
    }

    const int id = std::atoi(args->GetArg(1));
    const auto entry = SaveCatalog::Find(id);
    if (!entry.has_value()) {
        SR_LOG("no savegame with id %d", id);
        return;
    }

    std::string name;
    for (int i = 2; i < args->GetArgCount(); ++i) {
        if (!name.empty())
            name += ' ';
        name += args->GetArg(i);
    }

    auto header = whs::Description::Read(entry->file);
    if (!header.has_value()) {
        SR_LOG("could not read %s", entry->file.string().c_str());
        return;
    }

    header->SetDisplayName(name);
    if (!header->Write()) {
        SR_LOG("could not write %s", entry->file.string().c_str());
        return;
    }

    // The file is current, the manager's descriptions are not, and the page
    // that is already on screen is built from those descriptions.
    SaveCatalog::Refresh();
    const bool redrawn = SaveLoadHook::RebuildLoadPage();

    SR_LOG("%d is now '%s'%s", id, header->DisplayName().c_str(),
           redrawn ? "" : " (load page not open, list will refresh on next open)");
}

// Temporary, replaced by the F2 hook: toggles the dialog so the element can be
// checked before it is wired to the save list.
void CmdDialog(IConsoleCmdArgs* args)
{
    if (RenameDialog::IsOpen()) {
        RenameDialog::Hide();
        return;
    }
    const char* name = args->GetArgCount() > 1 ? args->GetArg(1) : "test name";
    if (!RenameDialog::Show(name, true))
        SR_LOG("could not show the dialog");
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
    env->pConsole->AddCommand("renamer_set", &CmdSet, VF_NULL,
                              "renamer_set <id> [name] -- renames a savegame, or resets it "
                              "to its quest name when no name is given.");
    env->pConsole->AddCommand("renamer_dialog", &CmdDialog, VF_NULL,
                              "renamer_dialog [name] -- shows or hides the rename dialog.");

    // The flash element only exists once the UI has loaded, so this cannot run
    // from KCSEPlugin_Load.
    RenameDialog::Install();
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
    if (!SaveLoadHook::Install()) {
        SR_LOG("could not hook the load menu");
        return false;
    }

    if (auto* messaging = kcse->GetMessagingInterface())
        messaging->RegisterListener(&OnKcseMessage);

    return true;
}
