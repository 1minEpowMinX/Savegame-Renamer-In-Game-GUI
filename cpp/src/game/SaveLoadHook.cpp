#include "SaveLoadHook.h"

#include <MinHook.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "Offsets/vtables/IFlashPlayer.h"
#include "Offsets/vtables/IFlashUI.h"
#include "Offsets/vtables/IUIElement.h"
#include "REL.h"
#include "crysystem/SSystemGlobalEnvironment.h"
#include "guimodule/C_UISaveLoad.h"

#include "Log.h"
#include "RenameDialog.h"

using wh::guimodule::C_UISaveLoad;

namespace SaveLoadHook {
namespace {

// C_UISaveLoad::BuildLoadGamePage: builds menu page 8, the list of saves for one
// playline. Hooked for its `this` and its playline argument, neither of which is
// reachable any other way; also re-invoked to redraw the list after a rename.
constexpr std::uint64_t kBuildLoadGamePageId = 94;

// The pages either side of the save list. Building one of them means the list is
// gone, which is when the rename prompt has to go with it: our element draws
// over the whole screen and would otherwise stay up into the game.
constexpr std::uint64_t kBuildPlaylineLoadPageId = 96;
constexpr std::uint64_t kBuildPlaylineNewPageId = 97;

using BuildLoadGamePageFn = void(*)(C_UISaveLoad*, int);
using BuildPlaylinePageFn = void(*)(C_UISaveLoad*, char);
using BuildPlaylineNewPageFn = void(*)(C_UISaveLoad*);

C_UISaveLoad* g_saveLoad = nullptr;
int g_playline = -1;
bool g_inRebuild = false;
REL::Relocation<BuildLoadGamePageFn> g_originalBuild;
REL::Relocation<BuildPlaylinePageFn> g_originalPlaylineLoad;
REL::Relocation<BuildPlaylineNewPageFn> g_originalPlaylineNew;

void HookedBuildLoadGamePage(C_UISaveLoad* self, int playline)
{
    if (self) {
        if (!g_saveLoad)
            SR_LOG("load page controller captured at %p", static_cast<void*>(self));
        g_saveLoad = self;
        g_playline = playline;
    }
    g_originalBuild(self, playline);
    RenameDialog::ShowHint(true);
}

void HookedBuildPlaylineLoadPage(C_UISaveLoad* self, char a2)
{
    RenameDialog::ShowHint(false);
    g_originalPlaylineLoad(self, a2);
}

void HookedBuildPlaylineNewPage(C_UISaveLoad* self)
{
    RenameDialog::ShowHint(false);
    g_originalPlaylineNew(self);
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

    void* playlineLoad = reinterpret_cast<void*>(REL::ID(kBuildPlaylineLoadPageId).address());
    if (MH_CreateHook(playlineLoad, reinterpret_cast<void*>(&HookedBuildPlaylineLoadPage), &original) == MH_OK
        && MH_EnableHook(playlineLoad) == MH_OK) {
        g_originalPlaylineLoad =
            REL::Relocation<BuildPlaylinePageFn>(reinterpret_cast<std::uintptr_t>(original));
    }

    void* playlineNew = reinterpret_cast<void*>(REL::ID(kBuildPlaylineNewPageId).address());
    if (MH_CreateHook(playlineNew, reinterpret_cast<void*>(&HookedBuildPlaylineNewPage), &original) == MH_OK
        && MH_EnableHook(playlineNew) == MH_OK) {
        g_originalPlaylineNew =
            REL::Relocation<BuildPlaylineNewPageFn>(reinterpret_cast<std::uintptr_t>(original));
    }
    return true;
}

bool Ready()
{
    return g_saveLoad != nullptr && g_playline >= 0;
}

namespace {

/// Returns the menu movie's Scaleform player, or null.
std::shared_ptr<Offsets::IFlashPlayer> MenuPlayer()
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (!env || !env->pFlashUI)
        return nullptr;

    _smart_ptr<Offsets::IUIElement> menu;
    env->pFlashUI->GetUIElement(menu, "Menu");
    if (!menu)
        return nullptr;
    return menu->GetFlashPlayer();
}

/// Reads a flash value as an integer whatever numeric type it carries.
///
/// The getters on SFlashVarValue assert the exact type and then read that
/// member of the union outright. With asserts compiled out, GetInt() on a
/// double returns the low half of its bit pattern, which is zero for every
/// small whole number. ActionScript hands out doubles for plain numbers, so
/// this conversion is not optional.
///
/// @param v Value to read.
/// @param out Receives the number.
/// @return True when the value held a number.
bool ToInt(const SFlashVarValue& v, int& out)
{
    if (v.IsInt())
        out = v.GetInt();
    else if (v.IsUInt())
        out = static_cast<int>(v.GetUInt());
    else if (v.IsDouble())
        out = static_cast<int>(v.GetDouble());
    else if (v.IsFloat())
        out = static_cast<int>(v.GetFloat());
    else if (v.IsString())
        out = std::atoi(v.GetConstStrPtr());
    else
        return false;
    return true;
}

/// Owns an IFlashVariableObject and releases it.
///
/// Every getter that hands one back allocates, so the traversal below would leak
/// one object per key press without this.
class VarObj {
public:
    VarObj() = default;
    explicit VarObj(Offsets::IFlashVariableObject* obj) : m_obj(obj) {}
    VarObj(const VarObj&) = delete;
    VarObj& operator=(const VarObj&) = delete;
    ~VarObj() { Reset(); }

    void Reset()
    {
        if (m_obj) {
            m_obj->Release();
            m_obj = nullptr;
        }
    }

    Offsets::IFlashVariableObject* Get() const { return m_obj; }
    Offsets::IFlashVariableObject** Receive() { Reset(); return &m_obj; }
    explicit operator bool() const { return m_obj != nullptr; }

private:
    Offsets::IFlashVariableObject* m_obj = nullptr;
};

/// Returns the highlighted button of the menu movie, or an empty holder.
///
/// The two selection indices address MenuManagerArray, which has to be walked
/// with GetElement: a dotted path such as "MenuManagerArray.0.3" resolves the
/// container but silently ignores the second index and yields the first row.
void SelectedButton(VarObj& out)
{
    out.Reset();

    auto player = MenuPlayer();
    if (!player)
        return;

    SFlashVarValue containerValue = SFlashVarValue::CreateUndefined();
    SFlashVarValue indexValue = SFlashVarValue::CreateUndefined();
    if (!player->GetVariable("_root.g_selectContainer", containerValue)
        || !player->GetVariable("_root.g_selectButton", indexValue))
        return;

    int container = 0;
    int index = 0;
    if (!ToInt(containerValue, container) || !ToInt(indexValue, index))
        return;
    if (container < 0 || index < 0)
        return;

    VarObj array;
    if (!player->GetVariable("_root.MenuManagerArray", *array.Receive()) || !array)
        return;

    VarObj row;
    if (!array.Get()->GetElement(static_cast<unsigned>(container), *row.Receive()) || !row)
        return;

    if (!row.Get()->GetElement(static_cast<unsigned>(index), *out.Receive()))
        out.Reset();
}

}  // namespace

int SelectedSaveId()
{
    VarObj button;
    SelectedButton(button);
    if (!button)
        return -1;

    // Every row of the save list carries this type; playline buttons and plain
    // menu entries do not, which is what keeps the rename key inert elsewhere.
    SFlashVarValue type = SFlashVarValue::CreateUndefined();
    if (!button.Get()->GetMember("type", type) || !type.IsString())
        return -1;
    if (std::strcmp(type.GetConstStrPtr(), "LoadButton") != 0)
        return -1;

    // The row's own saveId member is the menu's internal slot index, not the
    // number the player sees: row "4317." carries saveId 101. The visible number
    // is UIDescription field 1, which the row prints as the "<id>. " prefix of
    // its heading, and that is the id the rest of the mod keys on.
    VarObj head;
    if (!button.Get()->GetMember("tfHead", *head.Receive()) || !head)
        return -1;

    SFlashVarValue text = SFlashVarValue::CreateUndefined();
    if (!head.Get()->GetMember("text", text) || !text.IsString())
        return -1;

    const char* s = text.GetConstStrPtr();
    int id = 0;
    std::size_t digits = 0;
    while (s[digits] >= '0' && s[digits] <= '9') {
        id = id * 10 + (s[digits] - '0');
        ++digits;
    }
    if (digits == 0 || s[digits] != '.')
        return -1;
    return id;
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
