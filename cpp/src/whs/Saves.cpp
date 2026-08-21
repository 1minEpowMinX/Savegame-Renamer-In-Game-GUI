#include "whs/Saves.h"

#include <limits>

#include "whs/Description.h"

namespace whs {
namespace {

/// The character type the filesystem spells names in.
///
/// Directory names are compared in it rather than through path::string(), which
/// converts, and throws on a name it cannot convert.
using Char = std::filesystem::path::value_type;

/// Returns `c` in the path's own character type.
///
/// @param c ASCII character.
/// @return The same character, widened.
constexpr Char Native(char c)
{
    return static_cast<Char>(c);
}

/// What every playline directory's name opens with.
constexpr char kPlaylinePrefix[] = "playline";

/// Returns the playline index a directory name declares.
///
/// @param name Directory name in the path's own character type.
/// @return The index, or -1 when the name is not a playline's.
int PlaylineIndex(const std::filesystem::path::string_type& name)
{
    constexpr std::size_t prefixLength = sizeof(kPlaylinePrefix) - 1;
    if (name.size() <= prefixLength)
        return -1;

    for (std::size_t i = 0; i < prefixLength; ++i) {
        // Windows keeps the name as it was created and answers to either case.
        // Setting the case bit maps the letters onto lower case, and maps
        // nothing else onto a letter.
        const Char lower = static_cast<Char>(name[i] | Native(' '));
        if (lower != Native(kPlaylinePrefix[i]))
            return -1;
    }

    int index = 0;
    for (std::size_t i = prefixLength; i < name.size(); ++i) {
        if (name[i] < Native('0') || name[i] > Native('9'))
            return -1;
        // A name runs as long as the filesystem allows, so the digits are
        // checked against the accumulator rather than counted.
        if (index > (std::numeric_limits<int>::max() - 9) / 10)
            return -1;
        index = index * 10 + static_cast<int>(name[i] - Native('0'));
    }
    return index;
}

}  // namespace

std::filesystem::path PlaylineDir(const std::filesystem::path& root, int index)
{
    if (index < 0)
        return {};

    std::error_code ec;
    std::filesystem::path found;
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (!entry.is_directory(ec))
            continue;
        if (PlaylineIndex(entry.path().filename().native()) != index)
            continue;
        // "playline1" and "playline01" are two directories to the filesystem and
        // one playline to the game, and nothing here can tell which it reads.
        if (!found.empty())
            return {};
        found = entry.path();
    }
    return found;
}

std::optional<Description> FindSave(const std::filesystem::path& root, int index,
                                    const std::string& fileName, int saveId)
{
    const auto dir = PlaylineDir(root, index);
    if (dir.empty())
        return std::nullopt;

    std::error_code ec;
    const auto candidate = dir / fileName;
    if (!std::filesystem::is_regular_file(candidate, ec))
        return std::nullopt;

    auto header = Description::Read(candidate);
    if (!header.has_value() || header->SaveId() != saveId)
        return std::nullopt;
    return header;
}

}  // namespace whs
