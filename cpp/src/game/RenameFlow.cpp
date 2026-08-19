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

    /// The line the dialog was opened on, as the list renders it.
    ///
    /// A save that has never been renamed carries a localization key rather than
    /// text, and the list resolves it afresh in whatever language the game is
    /// set to. The dialog offers the resolved line for editing, so confirming it
    /// untouched would replace the key with one language's rendering of it and
    /// the save would keep that wording everywhere. Held to tell an edit from an
    /// accidental confirmation.
    std::string name;

    /// Forgets the save, leaving no dialog open.
    void Clear()
    {
        saveId = -1;
        name.clear();
    }
};

Pending g_pending;

/// Applies `name` to the pending save and redraws the list.
///
/// @param name Text as typed, or empty to reset to the original quest name.
void ApplyRename(const std::string& name)
{
    const auto entry = SaveCatalog::Find(g_pending.saveId);
    g_pending.Clear();
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

    // In this order. The file has changed, but the manager still holds the old
    // description and the page still holds the old text.
    SaveCatalog::Refresh();
    SaveLoadHook::RebuildLoadPage();
    SR_LOG("%d is now '%s'", entry->id, Strings::Localize(header->DisplayName()).c_str());
}

}  // namespace

void Wire()
{
    RenameDialog::SetAcceptHandler([](const std::string& typed) {
        // Confirming the line as it was offered is not a rename, and writing it
        // would cost the save its localized name for nothing.
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
    g_pending.saveId = saveId;
    g_pending.name = entry->displayName;
    if (RenameDialog::Show(entry->displayName, canReset))
        return true;

    g_pending.Clear();
    SR_LOG("could not show the dialog");
    return false;
}

}  // namespace RenameFlow
