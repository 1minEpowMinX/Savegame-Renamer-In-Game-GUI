#include "SaveLoadHook.h"

#include <MinHook.h>

#include <cstdint>

#include "REL.h"
#include "guimodule/C_UISaveLoad.h"

#include "Log.h"

using wh::guimodule::C_UISaveLoad;

namespace SaveLoadHook {
namespace {

// C_UISaveLoad::BuildLoadGamePage: builds menu page 8, the list of saves for one
// playline. Hooked for its `this` and its playline argument, neither of which is
// reachable any other way; also re-invoked to redraw the list after a rename.
constexpr std::uint64_t kBuildLoadGamePageId = 94;

using BuildLoadGamePageFn = void(*)(C_UISaveLoad*, int);

C_UISaveLoad* g_saveLoad = nullptr;
int g_playline = -1;
bool g_inRebuild = false;
REL::Relocation<BuildLoadGamePageFn> g_originalBuild;

void HookedBuildLoadGamePage(C_UISaveLoad* self, int playline)
{
    if (self) {
        if (!g_saveLoad)
            SR_LOG("load page controller captured at %p", static_cast<void*>(self));
        g_saveLoad = self;
        g_playline = playline;
    }
    g_originalBuild(self, playline);
}

}  // namespace

bool Install()
{
    void* target = reinterpret_cast<void*>(REL::ID(kBuildLoadGamePageId).address());
    void* original = nullptr;
    if (MH_CreateHook(target, reinterpret_cast<void*>(&HookedBuildLoadGamePage), &original) != MH_OK)
        return false;
    if (MH_EnableHook(target) != MH_OK)
        return false;
    g_originalBuild = REL::Relocation<BuildLoadGamePageFn>(reinterpret_cast<std::uintptr_t>(original));
    return true;
}

bool Ready()
{
    return g_saveLoad != nullptr && g_playline >= 0;
}

bool RebuildLoadPage()
{
    if (!Ready() || g_inRebuild)
        return false;

    g_inRebuild = true;
    g_originalBuild(g_saveLoad, g_playline);
    g_inRebuild = false;
    return true;
}

}  // namespace SaveLoadHook
