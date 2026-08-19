#include "whs/Description.h"

#include <fstream>
#include <regex>

#include "whs/Sanitise.h"

namespace whs {
namespace {

/// The four bytes every .whs opens with.
constexpr std::uint32_t kMagic = 0xFFFFFFFFu;

/// Bytes before the header XML: the magic, then the header length.
constexpr std::size_t kPrefixSize = sizeof(std::uint32_t) + sizeof(std::int32_t);

/// Root attribute holding the quest and objective a rename replaced.
constexpr const char* kStashAttribute = "RenamerOriginal";

/// Returns true when `text` parses as a decimal integer, storing it in `out`.
bool ParseInt(const std::string& text, int& out)
{
    if (text.empty())
        return false;
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        if (consumed != text.size())
            return false;
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

/// Returns the offset of the root element's closing bracket.
///
/// A '>' standing inside an attribute value is passed over.
///
/// @param xml Header XML.
/// @return Offset of the bracket, or npos when the element is unterminated.
std::size_t RootTagEnd(const std::string& xml)
{
    // XML asks for no escaping of '>' inside a value and the game writes one
    // there: quest names and the UsedMods block both reach the header as typed.
    bool quoted = false;
    for (std::size_t i = 0; i < xml.size(); ++i) {
        if (xml[i] == '"')
            quoted = !quoted;
        else if (xml[i] == '>' && !quoted)
            return i;
    }
    return std::string::npos;
}

/// One root attribute where it stands in the header.
struct AttributeMatch {
    std::size_t position = 0;  ///< Offset of the separator before the name.
    std::size_t length = 0;    ///< Length from that separator to the closing quote.
    std::string value;         ///< The value as the header carries it, still escaped.
};

/// Returns root attribute `name` where it stands in `xml`.
///
/// @param xml Header XML.
/// @param name Attribute name.
/// @return Where the attribute stands, or nothing when it is absent.
std::optional<AttributeMatch> FindAttribute(const std::string& xml, const std::string& name)
{
    // Searched no further than the root element. The game writes child elements
    // carrying attributes of their own -- every entry of UsedMods and of DLCs has
    // some -- and a search over the whole header answers with one of those
    // whenever the root has none of that name.
    const std::size_t rootEnd = RootTagEnd(xml);
    if (rootEnd == std::string::npos)
        return std::nullopt;

    // The separator before the name is part of the match: the leading \s is what
    // tells a whole name from the tail of a longer one. The header carries SaveId
    // inside AutoSaveId, and GameReleaseVersion inside NewGameReleaseVersion.
    const std::regex pattern("\\s" + name + "=\"([^\"]*)\"");
    std::smatch m;
    if (!std::regex_search(xml.cbegin(), xml.cbegin() + rootEnd, m, pattern))
        return std::nullopt;
    // Positions are counted from the iterator the search began at, which is the
    // start of the header.
    return AttributeMatch{static_cast<std::size_t>(m.position(0)),
                          static_cast<std::size_t>(m.length(0)), m[1].str()};
}

}  // namespace

std::optional<Description> Description::Read(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto fileSize = std::filesystem::file_size(path, ec);
    if (ec || fileSize < kPrefixSize)
        return std::nullopt;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return std::nullopt;

    std::uint32_t magic = 0;
    std::int32_t length = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    f.read(reinterpret_cast<char*>(&length), sizeof(length));
    if (!f || magic != kMagic || length <= 1)
        return std::nullopt;
    if (kPrefixSize + static_cast<std::uint64_t>(length) > fileSize)
        return std::nullopt;

    std::string xml(static_cast<std::size_t>(length), '\0');
    f.read(xml.data(), length);
    if (!f)
        return std::nullopt;
    const std::size_t nul = xml.find('\0');
    if (nul != std::string::npos)
        xml.resize(nul);

    Description d;
    d.m_path = path;
    d.m_xml = std::move(xml);
    d.m_payloadOffset = kPrefixSize + static_cast<std::uint64_t>(length);

    if (!ParseInt(d.Attribute("SaveId"), d.m_saveId))
        return std::nullopt;
    // Every accessor indexes these two without checking, so a header short of
    // them is refused here rather than read past later.
    if (d.UiFields().size() <= static_cast<std::size_t>(UiField::Objective))
        return std::nullopt;
    return d;
}

std::string Description::Attribute(const std::string& name) const
{
    const auto found = FindAttribute(m_xml, name);
    if (!found.has_value())
        return {};
    return found->value;
}

std::vector<std::string> Description::UiFields() const
{
    std::vector<std::string> out;
    const std::string packed = Attribute("UIDescription");
    if (packed.empty())
        return out;

    std::size_t start = 0;
    while (true) {
        const std::size_t bar = packed.find('|', start);
        if (bar == std::string::npos) {
            out.push_back(packed.substr(start));
            break;
        }
        out.push_back(packed.substr(start, bar - start));
        start = bar + 1;
    }
    return out;
}

void Description::SetAttribute(const std::string& name, const std::string& value)
{
    const std::string written = " " + name + "=\"" + value + "\"";

    // Spliced rather than handed to regex_replace: the replacement argument is a
    // format string, and the value carries whatever the player typed. A name
    // holding "$&" would paste the matched attribute back into itself, quotes
    // and all, and leave the header unparseable.
    if (const auto found = FindAttribute(m_xml, name)) {
        m_xml.replace(found->position, found->length, written);
        return;
    }
    // A new attribute goes last on the root element, just before its closing
    // bracket. An unknown attribute is tolerated by the game's parser; an
    // unknown child element is not.
    const std::size_t end = RootTagEnd(m_xml);
    if (end != std::string::npos)
        m_xml.insert(end, written);
}

void Description::RemoveAttribute(const std::string& name)
{
    if (const auto found = FindAttribute(m_xml, name))
        m_xml.erase(found->position, found->length);
}

void Description::SetUiFields(const std::vector<std::string>& fields)
{
    std::string packed;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i)
            packed += '|';
        packed += fields[i];
    }
    SetAttribute("UIDescription", packed);
}

std::string Description::Field(UiField field) const
{
    return XmlUnescape(UiFields()[static_cast<std::size_t>(field)]);
}

std::string Description::DisplayName() const
{
    return Field(UiField::Quest);
}

std::string Description::ObjectiveName() const
{
    return Field(UiField::Objective);
}

bool Description::HasCustomName() const
{
    return !Attribute(kStashAttribute).empty();
}

void Description::SetDisplayName(std::string_view name)
{
    const std::string clean = SanitiseName(name);
    if (clean.empty()) {
        ResetName();
        return;
    }

    auto fields = UiFields();
    auto& quest = fields[static_cast<std::size_t>(UiField::Quest)];
    auto& objective = fields[static_cast<std::size_t>(UiField::Objective)];

    if (!HasCustomName())
        SetAttribute(kStashAttribute, quest + "|" + objective);

    quest = XmlEscape(clean);
    objective.clear();
    SetUiFields(fields);
}

bool Description::WriteTo(const std::filesystem::path& path) const
{
    std::ifstream src(m_path, std::ios::binary);
    std::ofstream dst(path, std::ios::binary | std::ios::trunc);
    if (!src || !dst)
        return false;

    const std::uint32_t magic = kMagic;
    const std::int32_t length = static_cast<std::int32_t>(m_xml.size() + 1);
    dst.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    dst.write(reinterpret_cast<const char*>(&length), sizeof(length));
    dst.write(m_xml.data(), static_cast<std::streamsize>(m_xml.size()));
    dst.put('\0');

    // Copied through a fixed buffer, so a save of any size costs one header and
    // one buffer in memory.
    src.seekg(static_cast<std::streamoff>(m_payloadOffset));
    std::vector<char> buffer(1u << 20);
    while (src) {
        src.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        dst.write(buffer.data(), src.gcount());
    }
    dst.flush();
    return dst.good() && !src.bad();
}

bool Description::Write() const
{
    // The file has to still hold the save this object was read from. The id
    // alone does not settle that: the payload is copied from a byte offset
    // measured at read time, so a header that has changed length since would
    // have the copy start in the middle of something.
    const auto current = Read(m_path);
    if (!current.has_value() || current->SaveId() != m_saveId
        || current->m_payloadOffset != m_payloadOffset)
        return false;

    const auto temp = m_path.parent_path() / (m_path.filename().string() + ".renamer-tmp");
    std::error_code ec;

    // A half-written temp file is not left beside the save: the game enumerates
    // the directory, and the next write would find it in the way. WriteTo has
    // closed both streams by the time this is decided.
    if (!WriteTo(temp)) {
        std::filesystem::remove(temp, ec);
        return false;
    }

    std::filesystem::rename(temp, m_path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}

void Description::ResetName()
{
    const std::string stash = Attribute(kStashAttribute);
    if (stash.empty())
        return;

    const std::size_t bar = stash.find('|');
    auto fields = UiFields();
    fields[static_cast<std::size_t>(UiField::Quest)] = stash.substr(0, bar);
    fields[static_cast<std::size_t>(UiField::Objective)] =
        bar == std::string::npos ? std::string{} : stash.substr(bar + 1);

    SetUiFields(fields);
    RemoveAttribute(kStashAttribute);
}

}  // namespace whs
