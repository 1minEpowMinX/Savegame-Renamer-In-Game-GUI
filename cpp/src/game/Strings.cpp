#include "Strings.h"

#include "framework/C_LocalizedString.h"

namespace Strings {
namespace {

/// Returns the text for `key`, or `fallback` when the tables do not carry it.
///
/// @param key Localization key, including its leading '@'.
/// @param fallback English text to use instead.
/// @return The text to display.
std::string Resolve(const char* key, const char* fallback)
{
    // An unknown key comes back from the game unchanged, which is what tells a
    // miss from a hit.
    const std::string text = Localize(key);
    if (text.empty() || text == key)
        return fallback;
    return text;
}

}  // namespace

std::string Localize(const std::string& text)
{
    if (text.empty())
        return text;

    CryStringT<char> output;
    if (!wh::framework::C_LocalizedString::Localize(CryStringT<char>(text.c_str()), output))
        return text;
    return output.c_str();
}

Labels Get()
{
    // Resolved per call rather than cached: a language changed mid-session lands
    // on the next showing with nothing to invalidate.
    Labels labels;
    labels.title = Resolve("@ui_savegame_renamer_title", "Rename savegame");
    // The game's own words for the three actions it already names, so they read
    // as the surrounding menu does in every language and follow it if Warhorse
    // rewords them. Cancel comes from the inventory hint line because that one is
    // a key prompt like ours, not a button caption.
    labels.accept = Resolve("@ui_accept", "Accept");
    labels.cancel = Resolve("@ui_invhint_cancel", "Cancel");
    labels.reset = Resolve("@ui_reset", "Reset");
    labels.hint = Resolve("@ui_savegame_renamer_hint", "Rename save");
    return labels;
}

}  // namespace Strings
