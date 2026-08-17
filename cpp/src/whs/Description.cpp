#include "whs/Description.h"

#include <fstream>
#include <regex>

namespace whs {
namespace {

constexpr std::uint32_t kMagic = 0xFFFFFFFFu;
constexpr std::size_t kPrefixSize = 8;

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

std::string Description::DisplayName() const
{
    return UiFields()[static_cast<std::size_t>(UiField::Quest)];
}

}  // namespace whs
