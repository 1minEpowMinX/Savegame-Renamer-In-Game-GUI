#include "whs/Description.h"

#include <fstream>
#include <regex>

#include "whs/Sanitise.h"

namespace whs {
namespace {

constexpr std::uint32_t kMagic = 0xFFFFFFFFu;
constexpr std::size_t kPrefixSize = 8;

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
    f.read(reinterpret_cast<char*>(&magic), 4);
    f.read(reinterpret_cast<char*>(&length), 4);
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
    if (d.UiFields().size() <= static_cast<std::size_t>(UiField::Objective))
        return std::nullopt;
    return d;
}

std::string Description::Attribute(const std::string& name) const
{
    const std::regex re(name + "=\"([^\"]*)\"");
    std::smatch m;
    if (!std::regex_search(m_xml, m, re))
        return {};
    return m[1].str();
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
    const std::regex re(name + "=\"[^\"]*\"");
    if (std::regex_search(m_xml, re)) {
        m_xml = std::regex_replace(m_xml, re, name + "=\"" + value + "\"",
                                   std::regex_constants::format_first_only);
        return;
    }
    // A new attribute goes last on the root element, just before its closing
    // bracket. An unknown attribute is tolerated by the game's parser; an
    // unknown child element is not.
    m_xml.insert(m_xml.find('>'), " " + name + "=\"" + value + "\"");
}

void Description::RemoveAttribute(const std::string& name)
{
    const std::regex re(" " + name + "=\"[^\"]*\"");
    m_xml = std::regex_replace(m_xml, re, "", std::regex_constants::format_first_only);
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

std::string Description::DisplayName() const
{
    return XmlUnescape(UiFields()[static_cast<std::size_t>(UiField::Quest)]);
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
