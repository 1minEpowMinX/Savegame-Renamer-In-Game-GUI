#include "RenameDialog.h"

#include <cstring>

#include "CryEngine/CryCommon/SInputEvent.h"
#include "Offsets/vtables/IFlashUI.h"
#include "Offsets/vtables/IInput.h"
#include "Offsets/vtables/IInputEventListener.h"
#include "Offsets/vtables/IUIElement.h"
#include "Offsets/vtables/IUIElementEventListener.h"
#include "crysystem/SSystemGlobalEnvironment.h"
#include "guimodule/SUIEventDesc.h"
#include "guimodule/SUITypes.h"

#include "Log.h"
#include "Strings.h"

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

/// Returns the element that carries the prompt over the save list.
///
/// It is a second declaration of the same movie, with every input attribute
/// off: the dialog's element grabs the mouse across the whole screen, so
/// keeping that one visible for the prompt took clicks and the wheel away from
/// the list underneath.
IUIElement* HintElement()
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (!env || !env->pFlashUI)
        return nullptr;

    static _smart_ptr<IUIElement> el;
    if (!el)
        env->pFlashUI->GetUIElement(el, "SavegameRenamerHint");
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

/// Sends the current language's captions into `el`.
///
/// The movie cannot reach the localization tables itself: a key assigned to a
/// text field from ActionScript is not resolved on the way in. It is called
/// before every showing rather than once at startup, so a language changed
/// mid-session lands and an element instantiated late is not missed.
///
/// @param el Element to caption.
void ApplyLabels(IUIElement* el)
{
    const Strings::Labels labels = Strings::Get();

    SUIArguments args;
    args.AddArgument(labels.title.c_str());
    args.AddArgument(labels.accept.c_str());
    args.AddArgument(labels.cancel.c_str());
    args.AddArgument(labels.reset.c_str());
    args.AddArgument(labels.hint.c_str());
    Call(el, "SetLabels", "fc_setLabels", args);
}

/// Feeds Enter and Esc into the movie while the dialog is open.
///
/// The engine delivers typed characters to a Scaleform input field on its own
/// but routes these two elsewhere, so they are observed here and forwarded as
/// SetInput calls.
///
/// Unlike MCM's listener this one consumes what it handles: the load list also
/// acts on Enter, and SetBusyProtection does not hold that back, so a confirmed
/// rename would otherwise be followed by the game offering to load the save.
/// Both edges are swallowed, or the release alone still reaches the menu.
class KeyListener : public Offsets::IInputEventListener {
    static bool IsOurs(const Offsets::SInputEvent& ev)
    {
        return ev.keyId == Offsets::eKI_Enter
            || ev.keyId == Offsets::eKI_NP_Enter
            || ev.keyId == Offsets::eKI_Escape
            || ev.keyId == Offsets::eKI_Delete;
    }

    bool OnInputEvent(const Offsets::SInputEvent& ev) override
    {
        if (!g_open || !IsOurs(ev))
            return false;

        if (ev.state & Offsets::eIS_Pressed) {
            if (ev.keyId == Offsets::eKI_Escape)
                SendInput("cancel");
            else if (ev.keyId == Offsets::eKI_Delete)
                SendInput("reset");   // ignored by the movie unless resettable
            else
                SendInput("accept");
        }
        return true;
    }

    bool OnInputEventUI(const void*) override { return false; }
    int GetPriority() const override { return 0; }
    bool _vf3(const void*) override { return false; }
};

KeyListener g_keyListener;

/// Takes the dialog off screen after the movie has already hidden itself.
///
/// The movie clears its own artwork before emitting the event, but the element
/// stays visible as far as the engine is concerned until this runs.
void Dismiss()
{
    auto* env = SSystemGlobalEnvironment::GetInstance();
    if (env && env->pFlashUI) {
        _smart_ptr<IUIElement> el;
        env->pFlashUI->GetUIElement(el, "SavegameRenamer");
        if (el)
            el->SetVisible(false);
    }
    if (env && env->pInput)
        env->pInput->RemoveEventListener(&g_keyListener);

    g_open = false;
    SetMenuBusy(false);
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
            Dismiss();
            SR_LOG("dialog accepted with '%s'", typed.c_str());
            if (g_onAccept)
                g_onAccept(typed);
        } else if (_stricmp(name, "onRenameCancel") == 0) {
            Dismiss();
            SR_LOG("dialog cancelled");
            if (g_onCancel)
                g_onCancel();
        } else if (_stricmp(name, "onRenameReset") == 0) {
            Dismiss();
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
    IUIElement* el = Element();
    if (!el)
        return false;

    // The element has to be visible before anything is called into it: while it
    // is hidden the engine does not render the movie, and a call lands in a
    // frame that was never drawn.
    el->SetVisible(true);
    ApplyLabels(el);

    SUIArguments args;
    args.AddArgument(currentName.c_str());
    args.AddArgument(canReset);
    if (!Call(el, "Open", "fc_open", args)) {
        el->SetVisible(false);
        return false;
    }

    g_open = true;
    SetMenuBusy(true);
    if (auto* env = SSystemGlobalEnvironment::GetInstance(); env && env->pInput)
        env->pInput->AddEventListener(&g_keyListener);
    return true;
}

void ShowHint(bool visible)
{
    IUIElement* el = HintElement();
    if (!el)
        return;

    if (visible) {
        el->SetVisible(true);
        ApplyLabels(el);
    }

    SUIArguments args;
    args.AddArgument(visible);
    Call(el, "ShowHint", "fc_showHint", args);

    if (!visible)
        el->SetVisible(false);
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
