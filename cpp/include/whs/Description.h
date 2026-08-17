#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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
    ///
    /// A save the mod has not touched carries a localization key here rather
    /// than readable text.
    std::string DisplayName() const;

    /// Returns the objective the load list appends to the name after " - ".
    ///
    /// Empty on a renamed save: a custom name replaces both fields.
    std::string ObjectiveName() const;

    /// Returns true when the save carries a name written by this mod.
    bool HasCustomName() const;

    /// Replaces the displayed name, stashing the original quest and objective
    /// on the first call.
    ///
    /// An input that sanitises to nothing resets instead, so that clearing the
    /// field in the dialog restores the quest name rather than blanking the
    /// entry in the load list.
    ///
    /// @param name Text as typed by the player.
    void SetDisplayName(std::string_view name);

    /// Restores the quest and objective stashed by the first SetDisplayName
    /// call, and drops the stash. Does nothing when there is no stash.
    void ResetName();

    /// Returns the current header XML.
    const std::string& Xml() const { return m_xml; }

    /// Writes the header back, copying the payload through unchanged.
    ///
    /// Refuses when the file on disk no longer holds the save this object was
    /// read from, so a slot the game reused or deleted while a rename dialog
    /// was open is left alone. The new file is built beside the original and
    /// replaces it only once complete.
    ///
    /// @return True when the file was replaced.
    bool Write() const;

private:
    std::filesystem::path m_path;
    std::string m_xml;                 ///< Header XML, without the terminating NUL.
    std::uint64_t m_payloadOffset = 0; ///< First byte of the payload in m_path.
    int m_saveId = 0;

    std::string Attribute(const std::string& name) const;
    std::vector<std::string> UiFields() const;
    void SetAttribute(const std::string& name, const std::string& value);
    void RemoveAttribute(const std::string& name);
    void SetUiFields(const std::vector<std::string>& fields);
};

}  // namespace whs
