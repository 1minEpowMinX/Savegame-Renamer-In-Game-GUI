#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace whs {

/// Field positions inside the pipe-separated UIDescription attribute.
enum class UiField {
    Type = 0,
    Id = 1,
    Quest = 2,
    Objective = 3,
    Location = 4,
    Timestamp = 5,
    Date = 6,
    Playtime = 7,
};

/// The description header of one .whs savegame.
///
/// Holds the header text only. The payload stays in the file on disk and is
/// copied through on write, so a save of any size costs one header in memory.
class Description {
public:
    /// Reads the header of the save at `path`.
    ///
    /// Accepts a file only when it carries the savegame magic, a header that
    /// fits inside it, a numeric SaveId and a UIDescription holding at least
    /// the quest and objective fields. Every other member relies on that.
    ///
    /// @param path Savegame to read.
    /// @return The parsed description, or an empty optional.
    static std::optional<Description> Read(const std::filesystem::path& path);

    /// Returns the save id declared by the SaveId attribute.
    int SaveId() const { return m_saveId; }

    /// Returns the name the load list shows for this save.
    std::string DisplayName() const;

    /// Returns the current header XML.
    const std::string& Xml() const { return m_xml; }

private:
    std::filesystem::path m_path;
    std::string m_xml;                 ///< Header XML, without the terminating NUL.
    std::uint64_t m_payloadOffset = 0; ///< First byte of the payload in m_path.
    int m_saveId = 0;

    std::string Attribute(const std::string& name) const;
    std::vector<std::string> UiFields() const;
};

}  // namespace whs
