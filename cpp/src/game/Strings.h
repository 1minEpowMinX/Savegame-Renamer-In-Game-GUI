#pragma once

#include <string>

/// The mod's user-visible text, resolved through the game's localization tables.
///
/// Words the game already names are taken from its own keys; the rest come from
/// the tables built out of src/Localization/strings.xml.
namespace Strings {

/// Returns `text` with localization markup resolved.
///
/// A "@key" is looked up; anything else, including a name the player typed,
/// comes back unchanged.
///
/// @param text Authored string.
/// @return The readable text.
std::string Localize(const std::string& text);

/// Every label the dialog and the prompt put on screen.
struct Labels {
    std::string title;
    std::string accept;
    std::string cancel;
    std::string reset;
    std::string hint;
};

/// Returns the labels in the language the game is running in.
///
/// Resolved on each call rather than cached. A key the tables do not carry
/// falls back to English.
///
/// @return The labels.
Labels Get();

}  // namespace Strings
