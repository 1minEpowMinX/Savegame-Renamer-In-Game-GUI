#pragma once

#include <MinHook.h>

#include <cstdint>

#include "REL.h"

/// Places MinHook hooks on functions named by their address-library id.
namespace Hook {

/// Hooks the function at `id`, routing it through `detour`.
///
/// @param id Address-library id of the function to hook.
/// @param detour Function the calls are routed to.
/// @param original Receives the trampoline, which reaches the hooked function.
/// @return True when the hook was created and enabled.
template <typename Fn>
bool Install(std::uint64_t id, Fn detour, REL::Relocation<Fn>& original)
{
    void* target = reinterpret_cast<void*>(REL::ID(id).address());
    void* trampoline = nullptr;
    if (MH_CreateHook(target, reinterpret_cast<void*>(detour), &trampoline) != MH_OK)
        return false;
    if (MH_EnableHook(target) != MH_OK) {
        // A hook left in place stays registered against a live function while
        // doing nothing.
        MH_RemoveHook(target);
        return false;
    }
    original = REL::Relocation<Fn>(reinterpret_cast<std::uintptr_t>(trampoline));
    return true;
}

}  // namespace Hook
