#include "RenameDialog.h"

#include <cstring>

#include "Offsets/vtables/IFlashUI.h"
#include "Offsets/vtables/IUIElement.h"
#include "Offsets/vtables/IUIElementEventListener.h"
#include "crysystem/SSystemGlobalEnvironment.h"
#include "guimodule/SUIEventDesc.h"
#include "guimodule/SUITypes.h"

#include "Log.h"

using Offsets::IUIElement;

namespace RenameDialog {
namespace {

std::function<void(const std::string&)> g_onAccept;
std::function<void()> g_onCancel;
std::function<void()> g_onReset;
bool g_open = false;

/// Returns the dialog element, or null when the mod's pak is not installed.
IUIElement* Element()
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (!env || !env->pFlashUI)
        return nullptr;

    static _smart_ptr<IUIElement> el;   // holds a ref for the session
    if (el)
        return el.get();

    env->pFlashUI->GetUIElement(el, "SavegameRenamer");
    if (!el)
        SR_LOG("element not found, is Mods/savegame_renamer/Data/savegame_renamer.pak installed?");
    return el.get();
}

/// Returns the vanilla menu element, used to silence it while the dialog is up.
IUIElement* MenuElement()
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (!env || !env->pFlashUI)
        return nullptr;

    static _smart_ptr<IUIElement> el;
    if (!el)
        env->pFlashUI->GetUIElement(el, "Menu");
    return el.get();
}

/// Calls a function on `el` by its XML name, falling back to the raw AS2 name.
///
/// @param el Element to call into.
/// @param name Name from the <function name=...> attribute.
/// @param funcname Name of the ActionScript function itself.
/// @param args Arguments to pass.
/// @return True when either name resolved.
bool Call(IUIElement* el, const char* name, const char* funcname, const SUIArguments& args)
{
    if (!el)
        return false;
    if (el->CallFunction(name, args, nullptr, nullptr))
        return true;
    if (funcname && el->CallFunction(funcname, args, nullptr, nullptr))
        return true;
    SR_LOG("CallFunction('%s'/'%s') failed", name, funcname ? funcname : "-");
    return false;
}

/// Stops the pause menu reacting to input while the dialog is up.
///
/// The menu is not key-driven: the game forwards actions into Menu.gfx itself,
/// so events_exclusive on our own element does not hold it back. Menu.gfx has
/// its own switch for that feed.
///
/// @param busy Whether the menu should ignore the forwarded input.
void SetMenuBusy(bool busy)
{
    SUIArguments args;
    args.AddArgument(busy);
    Call(MenuElement(), "SetBusyProtection", "fc_setBusyProtection", args);
}

class Listener : public Offsets::IUIElementEventListener {
    void OnUIEvent(IUIElement*, const SUIEventDesc& ev, const SUIArguments& args, void*) override
    {
        const char* name = ev.sName ? ev.sName : ev.sDisplayName;
        if (!name)
            return;

        if (_stricmp(name, "onRenameAccept") == 0) {
            const std::string typed =
                args.GetArgCount() > 0 ? args.GetArg(0).AsString().c_str() : "";
            g_open = false;
            SetMenuBusy(false);
            SR_LOG("dialog accepted with '%s'", typed.c_str());
            if (g_onAccept)
                g_onAccept(typed);
        } else if (_stricmp(name, "onRenameCancel") == 0) {
            g_open = false;
            SetMenuBusy(false);
            SR_LOG("dialog cancelled");
            if (g_onCancel)
                g_onCancel();
        } else if (_stricmp(name, "onRenameReset") == 0) {
            g_open = false;
            SetMenuBusy(false);
            SR_LOG("dialog reset");
            if (g_onReset)
                g_onReset();
        }
    }

    void OnUIEventEx(IUIElement*, const char*, const SUIArguments&, void*) override {}
    void OnInit(IUIElement*, Offsets::IFlashPlayer*) override {}
    void OnUnload(IUIElement*) override {}
    void OnSetVisible(IUIElement*, bool) override {}
    void OnInstanceCreated(IUIElement*, IUIElement*) override {}
    void OnInstanceDestroyed(IUIElement*, IUIElement*) override {}
    void Dtor(char) override {}
};

Listener g_listener;

}  // namespace

bool Install()
{
    IUIElement* el = Element();
    if (!el)
        return false;
    el->AddEventListener(&g_listener, "SavegameRenamer");
    SR_LOG("dialog element ready");
    return true;
}

bool IsOpen()
{
    return g_open;
}

bool Show(const std::string& currentName, bool canReset)
{
    SUIArguments args;
    args.AddArgument(currentName.c_str());
    args.AddArgument(canReset);

    if (!Call(Element(), "Open", "fc_open", args))
        return false;

    g_open = true;
    SetMenuBusy(true);
    return true;
}

void Hide()
{
    Call(Element(), "Close", "fc_close", SUIArguments());
    g_open = false;
    SetMenuBusy(false);
}

bool SendInput(const char* action)
{
    SUIArguments args;
    args.AddArgument(action);
    return Call(Element(), "SetInput", "fc_setInput", args);
}

void SetAcceptHandler(std::function<void(const std::string&)> handler)
{
    g_onAccept = std::move(handler);
}

void SetCancelHandler(std::function<void()> handler)
{
    g_onCancel = std::move(handler);
}

void SetResetHandler(std::function<void()> handler)
{
    g_onReset = std::move(handler);
}

}  // namespace RenameDialog
