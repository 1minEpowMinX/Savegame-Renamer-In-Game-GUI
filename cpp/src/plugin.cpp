// Savegame Renamer - In-Game GUI (KCSE plugin).
//
// Renames an existing savegame from the load menu. The displayed name lives in
// field 2 of the UIDescription attribute inside the .whs description header;
// the original quest and objective keys are stashed in a RenamerOriginal
// attribute so the name can be reset.

#include <MinHook.h>

#include "CryEngine/CryCommon/SInputEvent.h"
#include "KCSE/KCSEAPI.h"
#include "Offsets/vtables/IConsole.h"
#include "Offsets/vtables/IInput.h"
#include "Offsets/vtables/IInputEventListener.h"
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

// The save the open dialog belongs to. Task 10 replaces the console argument
// with the row highlighted in the load menu; everything downstream stays.
int g_pendingSaveId = -1;

/// Applies `name` to the pending save and redraws the list.
///
/// @param name Text as typed, or empty to reset to the original quest name.
void ApplyRename(const std::string& name)
{
    const auto entry = SaveCatalog::Find(g_pendingSaveId);
    g_pendingSaveId = -1;
    if (!entry.has_value()) {
        SR_LOG("the save went away while the dialog was open");
        return;
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

    SaveCatalog::Refresh();
    SaveLoadHook::RebuildLoadPage();
    SR_LOG("%d is now '%s'", entry->id, header->DisplayName().c_str());
}

/// Opens the dialog for `saveId`.
///
/// @param saveId Id as shown in the load list.
/// @return True when the dialog was shown.
bool OpenFor(int saveId)
{
    const auto entry = SaveCatalog::Find(saveId);
    if (!entry.has_value()) {
        SR_LOG("no savegame with id %d", saveId);
        return false;
    }

    const auto header = whs::Description::Read(entry->file);
    const bool canReset = header.has_value() && header->HasCustomName();

    // The field opens on the line the list currently shows, localized, so a
    // player who only wants to mark the original can type around it instead of
    // retyping it. Clearing the field resets, same as the button.
    g_pendingSaveId = saveId;
    if (RenameDialog::Show(entry->displayName, canReset)) {
        return true;
    }
    g_pendingSaveId = -1;
    SR_LOG("could not show the dialog");
    return false;
}

/// Opens the rename dialog on the highlighted save list row.
///
/// Self-gating: SelectedSaveId returns -1 unless a savegame row is highlighted,
/// so the key does nothing anywhere else in the menu.
class RenameKeyListener : public Offsets::IInputEventListener {
    bool OnInputEvent(const Offsets::SInputEvent& ev) override
    {
        if (ev.keyId != Offsets::eKI_F2 || !(ev.state & Offsets::eIS_Pressed))
            return false;
        if (RenameDialog::IsOpen())
            return false;

        const int saveId = SaveLoadHook::SelectedSaveId();
        if (saveId < 0)
            return false;

        OpenFor(saveId);
        return true;
    }

    bool OnInputEventUI(const void*) override { return false; }
    int GetPriority() const override { return 0; }
    bool _vf3(const void*) override { return false; }
};

RenameKeyListener g_renameKey;

// Temporary, replaced by the F2 hook: opens the dialog for a save chosen by id.
void CmdDialog(IConsoleCmdArgs* args)
{
    if (RenameDialog::IsOpen()) {
        RenameDialog::Hide();
        return;
    }
    if (args->GetArgCount() < 2) {
        SR_LOG("usage: renamer_dialog <id>");
        return;
    }
    OpenFor(std::atoi(args->GetArg(1)));
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
    env->pConsole->AddCommand("renamer_probe", [](IConsoleCmdArgs*) { SaveLoadHook::ProbeSelection(); },
                              VF_NULL,
                              "renamer_probe -- logs the properties of the highlighted menu row.");

    // The flash element only exists once the UI has loaded, so this cannot run
    // from KCSEPlugin_Load.
    RenameDialog::Install();
    RenameDialog::SetAcceptHandler(&ApplyRename);
    RenameDialog::SetResetHandler([] { ApplyRename(""); });
    RenameDialog::SetCancelHandler([] { g_pendingSaveId = -1; });

    if (auto* env = SSystemGlobalEnvironment::GetInstance(); env && env->pInput) {
        env->pInput->AddEventListener(&g_renameKey);
        SR_LOG("F2 armed");
    }
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
