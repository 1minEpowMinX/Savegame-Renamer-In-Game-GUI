#include "RenameFlow.h"

#include <string>

#include "Log.h"
#include "RenameDialog.h"
#include "SaveCatalog.h"
#include "SaveLoadHook.h"
#include "Strings.h"
#include "whs/Description.h"

namespace RenameFlow {
namespace {

/// The save the open dialog belongs to.
struct Pending {
    /// Id of the save, or -1 while no dialog is open.
    int saveId = -1;

    /// Playline the dialog was opened on.
    ///
    /// Held rather than read again on accept: the id alone names one save per
    /// playline, and the menu may have moved on by then.
    int playline = -1;

    /// The line the dialog was opened on, localized, as the list renders it.
    ///
    /// Held to tell an edit from a confirmation of the line as it was offered.
    std::string name;

    /// Forgets the save, leaving no dialog open.
    void Clear()
    {
        saveId = -1;
        playline = -1;
        name.clear();
    }
};

Pending g_pending;

/// Applies `name` to the pending save and redraws the list.
///
/// @param name Text as typed, or empty to reset to the original quest name.
void ApplyRename(const std::string& name)
{
    auto entry = SaveCatalog::Find(g_pending.saveId, g_pending.playline);
    g_pending.Clear();
    if (!entry.has_value()) {
        SR_LOG("the save went away while the dialog was open");
        return;
    }

    entry->header.SetDisplayName(name);
    if (!entry->header.Write()) {
        SR_LOG("could not write %s", entry->header.Path().string().c_str());
        return;
    }

    // In this order. The file has changed, but the manager still holds the old
    // description and the page still holds the old text.
    SaveCatalog::Refresh();
    SaveLoadHook::RebuildLoadPage();
    SR_LOG("%d is now '%s'", entry->id,
           Strings::Localize(entry->header.DisplayName()).c_str());
}

}  // namespace

void Wire()
{
    // The hooks report where the menu is; what to draw there is this layer's to
    // decide, so nothing below it knows the dialog exists.
    SaveLoadHook::SetSaveListHandler([](bool onSaveList) {
        RenameDialog::ShowHint(onSaveList);
    });

    RenameDialog::SetAcceptHandler([](const std::string& typed) {
        // Confirming the line as it was offered is not a rename. A save that
        // has never been renamed carries a localization key rather than text,
        // which the list resolves afresh in whatever language the game is set
        // to; writing the resolved line back fixes one language's rendering of
        // it into the save everywhere.
        if (typed == g_pending.name) {
            SR_LOG("%d confirmed unchanged, left alone", g_pending.saveId);
            g_pending.Clear();
            return;
        }
        ApplyRename(typed);
    });
    RenameDialog::SetResetHandler([] { ApplyRename(""); });
    RenameDialog::SetCancelHandler([] { g_pending.Clear(); });
}

void MenuClosed()
{
    RenameDialog::ShowHint(false);
}

bool OpenFor(int saveId)
{
    const int playline = SaveLoadHook::Playline();
    const auto entry = SaveCatalog::Find(saveId, playline);
    if (!entry.has_value()) {
        SR_LOG("no savegame with id %d in playline %d", saveId, playline);
        return false;
    }

    const bool canReset = entry->header.HasCustomName();

    // The field opens on the line the list currently shows, localized, so a
    // player who only wants to mark the original can type around it instead of
    // retyping it. Clearing the field resets, same as the button.
    g_pending.saveId = saveId;
    g_pending.playline = playline;
    g_pending.name = entry->displayName;
    if (RenameDialog::Show(entry->displayName, canReset))
        return true;

    g_pending.Clear();
    SR_LOG("could not show the dialog");
    return false;
}

}  // namespace RenameFlow
