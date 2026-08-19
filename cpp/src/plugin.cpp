// Savegame Renamer - In-Game GUI (KCSE plugin).
//
// Renames an existing savegame from the load menu. The displayed name lives in
// the UIDescription field named by whs::UiField::Quest, inside the .whs
// description header; the original quest and objective keys are stashed in a
// root attribute of the mod's own, declared in whs/Description.cpp, so the name
// can be reset.

#include <MinHook.h>

#include "CryEngine/CryCommon/SInputEvent.h"
#include "KCSE/KCSEAPI.h"
#include "Offsets/vtables/IInput.h"
#include "Offsets/vtables/IInputEventListener.h"
#include "crysystem/SSystemGlobalEnvironment.h"

#include "Log.h"
#include "game/RenameDialog.h"
#include "game/RenameFlow.h"
#include "game/SaveCatalog.h"
#include "game/SaveLoadHook.h"

KCSE_PLUGIN_INFO("SavegameRenamer", "Lefxxx", 1);

namespace {

/// Opens the rename dialog on the highlighted save list row.
class RenameKeyListener : public Offsets::IInputEventListener {
    bool OnInputEvent(const Offsets::SInputEvent& ev) override
    {
        if (ev.keyId != Offsets::eKI_F2 || !(ev.state & Offsets::eIS_Pressed))
            return false;
        if (RenameDialog::IsOpen())
            return false;

        // Self-gating: the id is -1 unless a savegame row is highlighted, which
        // is what keeps the key inert everywhere else in the menu.
        const int saveId = SaveLoadHook::SelectedSaveId();
        if (saveId < 0)
            return false;

        RenameFlow::OpenFor(saveId);
        return true;
    }

    bool OnInputEventUI(const void*) override { return false; }
    int GetPriority() const override { return 0; }
    bool _vf3(const void*) override { return false; }
};

RenameKeyListener g_renameKey;

/// Attaches the dialog and the key that opens it.
void InstallUi()
{
    // kMessage_DataLoaded is not promised to arrive once, and the two halves are
    // tracked apart: a listener added a second time applies every rename twice,
    // while a half that failed has to be reachable by the next message. The
    // element is absent until the UI has loaded, which is also why none of this
    // can run from KCSEPlugin_Load.
    static bool dialogInstalled = false;
    static bool keyArmed = false;
    if (dialogInstalled && keyArmed)
        return;

    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (!env)
        return;

    if (!dialogInstalled && RenameDialog::Install()) {
        RenameFlow::Wire();
        dialogInstalled = true;
    }

    if (!keyArmed && env->pInput) {
        env->pInput->AddEventListener(&g_renameKey);
        keyArmed = true;
        SR_LOG("F2 armed");
    }
}

void OnKcseMessage(KCSE::Message* msg)
{
    // Leaving the menu for the world takes the prompt with it.
    if (msg && msg->type == KCSE::IMessagingInterface::kMessage_LoadGame)
        RenameDialog::ShowHint(false);

    if (msg && msg->type == KCSE::IMessagingInterface::kMessage_DataLoaded)
        InstallUi();
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
