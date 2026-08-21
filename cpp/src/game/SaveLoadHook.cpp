#include "SaveLoadHook.h"

#include <cstdint>
#include <utility>

#include "REL.h"
#include "guimodule/C_UISaveLoad.h"

#include "Hook.h"
#include "Log.h"

using wh::guimodule::C_UISaveLoad;

namespace SaveLoadHook {
namespace {

// C_UISaveLoad::BuildLoadGamePage: builds menu page 8, the list of saves for one
// playline. Hooked for its `this` and its playline argument: C_UISaveLoad is a
// value member of C_UIMenu with no accessor of its own, and nothing else exposes
// the playline. Also re-invoked to redraw the list after a rename.
constexpr std::uint64_t kBuildLoadGamePageId = 94;

// C_UIMenu::PreparePage, which every page builder in the menu calls before it
// lays anything out. Hooked so the save list can report its departure the moment
// the menu turns elsewhere: what is drawn over the list covers the whole screen,
// and one left up follows the player into the settings and into the game.
constexpr std::uint64_t kPreparePageId = 43;

// wh::guimodule::E_MenuPage::LoadGame, the page the save list is drawn on.
constexpr std::uint8_t kLoadGamePage = 8;

using BuildLoadGamePageFn = void(*)(C_UISaveLoad*, int);
using PreparePageFn = void(*)(void*, std::uint8_t);

C_UISaveLoad* g_saveLoad = nullptr;
int g_playline = -1;
std::function<void(bool)> g_saveListHandler;
REL::Relocation<BuildLoadGamePageFn> g_originalBuild;
REL::Relocation<PreparePageFn> g_originalPreparePage;

/// Tells the handler where the menu now is, when one is set.
///
/// @param onSaveList Whether the save list is on screen.
void ReportSaveList(bool onSaveList)
{
    if (g_saveListHandler)
        g_saveListHandler(onSaveList);
}

void HookedBuildLoadGamePage(C_UISaveLoad* self, int playline)
{
    if (self) {
        if (!g_saveLoad)
            SR_LOG("load page controller captured at %p", static_cast<void*>(self));
        g_saveLoad = self;
        g_playline = playline;
    }
    g_originalBuild(self, playline);
    ReportSaveList(true);
}

void HookedPreparePage(void* self, std::uint8_t page)
{
    if (page != kLoadGamePage)
        ReportSaveList(false);
    g_originalPreparePage(self, page);
}

/// Returns true once the game has built the load page at least once.
bool Ready()
{
    return g_saveLoad != nullptr && g_playline >= 0;
}

}  // namespace

bool Install()
{
    if (!Hook::Install(kBuildLoadGamePageId, &HookedBuildLoadGamePage, g_originalBuild))
        return false;

    // The rest of the mod stands without this one, so its failure is reported
    // rather than fatal. What goes with it is the save list reporting that the
    // menu has left it.
    if (!Hook::Install(kPreparePageId, &HookedPreparePage, g_originalPreparePage))
        SR_LOG("could not hook PreparePage; the menu leaving the save list will go "
               "unreported");
    return true;
}

void SetSaveListHandler(std::function<void(bool)> handler)
{
    g_saveListHandler = std::move(handler);
}

bool RebuildLoadPage()
{
    if (!Ready())
        return false;

    // The trampoline, not the hooked address, so the capture and the report in
    // HookedBuildLoadGamePage do not run a second time.
    g_originalBuild(g_saveLoad, g_playline);
    return true;
}

int Playline()
{
    return g_playline;
}

}  // namespace SaveLoadHook
