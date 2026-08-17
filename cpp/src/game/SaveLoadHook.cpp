#include "SaveLoadHook.h"

#include <MinHook.h>

#include <cstdint>
#include <cstdio>
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

    SFlashVarValue container = SFlashVarValue::CreateUndefined();
    SFlashVarValue index = SFlashVarValue::CreateUndefined();
    if (!player->GetVariable("_root.g_selectContainer", container)
        || !player->GetVariable("_root.g_selectButton", index))
        return;
    if (container.IsUndefined() || index.IsUndefined())
        return;

    VarObj array;
    if (!player->GetVariable("_root.MenuManagerArray", *array.Receive()) || !array)
        return;

    VarObj row;
    if (!array.Get()->GetElement(static_cast<unsigned>(container.GetInt()), *row.Receive()) || !row)
        return;

    if (!row.Get()->GetElement(static_cast<unsigned>(index.GetInt()), *out.Receive()))
        out.Reset();
}

/// Renders a flash value as text for the log.
std::string Describe(const SFlashVarValue& v)
{
    char buf[128];
    if (v.IsInt() || v.IsUInt())
        std::snprintf(buf, sizeof(buf), "int %d", v.GetInt());
    else if (v.IsDouble())
        std::snprintf(buf, sizeof(buf), "double %.3f", v.GetDouble());
    else if (v.IsString())
        std::snprintf(buf, sizeof(buf), "string '%s'", v.GetConstStrPtr());
    else if (v.IsBool())
        std::snprintf(buf, sizeof(buf), "bool %d", v.GetInt());
    else if (v.IsNull())
        std::snprintf(buf, sizeof(buf), "null");
    else
        std::snprintf(buf, sizeof(buf), "undefined");
    return buf;
}

}  // namespace

void ProbeSelection()
{
    auto player = MenuPlayer();
    if (!player) {
        SR_LOG("probe: no menu player");
        return;
    }

    SFlashVarValue container = SFlashVarValue::CreateUndefined();
    SFlashVarValue index = SFlashVarValue::CreateUndefined();
    const bool haveC = player->GetVariable("_root.g_selectContainer", container);
    const bool haveI = player->GetVariable("_root.g_selectButton", index);
    SR_LOG("probe: g_selectContainer read=%d %s, g_selectButton read=%d %s",
           haveC, Describe(container).c_str(), haveI, Describe(index).c_str());
    if (!haveC || !haveI)
        return;

    VarObj button;
    SelectedButton(button);
    if (!button) {
        SR_LOG("probe: no highlighted button");
        return;
    }

    static const char* kMembers[] = {"type", "saveId", "playlineId", "_name", "timestamp"};
    for (const char* member : kMembers) {
        SFlashVarValue value = SFlashVarValue::CreateUndefined();
        const bool ok = button.Get()->GetMember(member, value);
        SR_LOG("probe: %-12s read=%d %s", member, ok, Describe(value).c_str());
    }

    VarObj head;
    if (button.Get()->GetMember("tfHead", *head.Receive()) && head) {
        SFlashVarValue text = SFlashVarValue::CreateUndefined();
        const bool ok = head.Get()->GetMember("text", text);
        SR_LOG("probe: %-12s read=%d %s", "tfHead.text", ok, Describe(text).c_str());
    }

    SR_LOG("probe: SelectedSaveId() -> %d", SelectedSaveId());
}

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
