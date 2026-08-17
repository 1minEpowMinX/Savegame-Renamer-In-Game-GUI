#include "whs/Sanitise.h"

#include <utility>

namespace whs {

std::string SanitiseName(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());

    bool pendingSpace = false;
    for (const char c : raw) {
        const auto byte = static_cast<unsigned char>(c);
        if (c == '|' || byte < 0x20 || byte == 0x7F)
            continue;
        if (c == ' ') {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }
        out.push_back(c);
    }
    return out;
}

std::string XmlEscape(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());

    for (const char c : raw) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

std::string XmlUnescape(std::string_view raw)
{
    // &amp; comes last so that "&amp;lt;" decodes to "&lt;" and not to "<".
    static constexpr std::pair<std::string_view, char> kEntities[] = {
        {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&amp;", '&'},
    };

    std::string out;
    out.reserve(raw.size());

    for (std::size_t i = 0; i < raw.size();) {
        bool matched = false;
        if (raw[i] == '&') {
            for (const auto& entity : kEntities) {
                if (raw.compare(i, entity.first.size(), entity.first) == 0) {
                    out.push_back(entity.second);
                    i += entity.first.size();
                    matched = true;
                    break;
                }
            }
        }
        if (!matched)
            out.push_back(raw[i++]);
    }
    return out;
}

}  // namespace whs
