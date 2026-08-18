#pragma once

#include <string>
#include <string_view>

namespace whs {

/// Returns `raw` with the UIDescription field separator and control characters
/// removed, runs of spaces collapsed and the edges trimmed.
///
/// Bytes outside ASCII pass through untouched, so UTF-8 text survives.
///
/// @param raw Text as typed by the player.
/// @return The cleaned name, possibly empty.
std::string SanitiseName(std::string_view raw);

/// Returns `raw` with the characters that would corrupt an XML attribute value
/// replaced by entities.
///
/// @param raw Text to place inside a double-quoted attribute.
/// @return The escaped text.
std::string XmlEscape(std::string_view raw);

/// Returns `raw` with XML entities replaced by the characters they stand for.
///
/// @param raw Text taken from an attribute value.
/// @return The decoded text.
std::string XmlUnescape(std::string_view raw);

}  // namespace whs
