#include "SaveLoadHook.h"

#include <MinHook.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Offsets/vtables/IFlashPlayer.h"
#include "Offsets/vtables/IFlashUI.h"
#include "Offsets/vtables/IUIElement.h"
#include "REL.h"
#include "crysystem/SSystemGlobalEnvironment.h"
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

int SelectedSaveId()
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (!env || !env->pFlashUI)
        return -1;

    _smart_ptr<Offsets::IUIElement> menu;
    env->pFlashUI->GetUIElement(menu, "Menu");
    if (!menu)
        return -1;

    auto player = menu->GetFlashPlayer();
    if (!player)
        return -1;

    // Menu.gfx keeps the highlighted button as MenuManagerArray[container][index];
    // selectBtn() maintains the two indices for both mouse hover and arrow keys.
    SFlashVarValue container = SFlashVarValue::CreateUndefined();
    SFlashVarValue index = SFlashVarValue::CreateUndefined();
    if (!player->GetVariable("_root.g_selectContainer", container)
        || !player->GetVariable("_root.g_selectButton", index))
        return -1;
    if (!container.IsInt() && !container.IsDouble())
        return -1;

    char path[192];
    std::snprintf(path, sizeof(path), "_root.MenuManagerArray.%d.%d.type",
                  container.GetInt(), index.GetInt());

    SFlashVarValue type = SFlashVarValue::CreateUndefined();
    if (!player->GetVariable(path, type) || !type.IsString())
        return -1;
    // Every row of the save list carries this type; the playline buttons and the
    // plain menu entries do not.
    if (std::strcmp(type.GetConstStrPtr(), "LoadButton") != 0)
        return -1;

    std::snprintf(path, sizeof(path), "_root.MenuManagerArray.%d.%d.saveId",
                  container.GetInt(), index.GetInt());

    SFlashVarValue saveId = SFlashVarValue::CreateUndefined();
    if (!player->GetVariable(path, saveId))
        return -1;
    if (saveId.IsInt() || saveId.IsDouble())
        return saveId.GetInt();
    if (saveId.IsString())
        return std::atoi(saveId.GetConstStrPtr());
    return -1;
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
