#include "MenuSelection.h"

#include <cstdlib>
#include <cstring>
#include <memory>

#include "Offsets/vtables/IFlashPlayer.h"
#include "Offsets/vtables/IFlashUI.h"
#include "Offsets/vtables/IUIElement.h"
#include "crysystem/SSystemGlobalEnvironment.h"

namespace MenuSelection {

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
/// @param v Value to read.
/// @param out Receives the number.
/// @return True when the value held a number.
bool ToInt(const SFlashVarValue& v, int& out)
{
    // The getters on SFlashVarValue assert the exact type and then read that
    // member of the union outright. With asserts compiled out, GetInt() on a
    // double returns the low half of its bit pattern, zero for every small whole
    // number, and ActionScript hands out doubles for plain numbers.
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
/// Every getter that hands one back allocates.
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

/// Places the menu movie's highlighted button in `out`.
///
/// @param out Receives the button, left empty when there is none.
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

    // Walked with GetElement rather than reached by a dotted path:
    // "MenuManagerArray.0.3" resolves the container, silently ignores the second
    // index and yields the first row.
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

int SaveId()
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

    // The row's saveId member is the menu's own index into the list, not the
    // number the player sees: the row heading "4317." carries saveId 101. The id
    // in the file is UIDescription field 1, which Menu.gfx prints unconditionally
    // as the heading's "<id>. " prefix, and that is the id the rest of the mod
    // keys on.
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

}  // namespace MenuSelection
