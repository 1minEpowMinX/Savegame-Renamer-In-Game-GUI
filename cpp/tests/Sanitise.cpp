#include <catch2/catch_test_macros.hpp>

#include "whs/Sanitise.h"

TEST_CASE("SanitiseName drops the field separator", "[sanitise]")
{
    CHECK(whs::SanitiseName("before|after") == "beforeafter");
}

TEST_CASE("SanitiseName drops control characters and collapses runs of spaces", "[sanitise]")
{
    CHECK(whs::SanitiseName("a\r\nb\tc") == "abc");
    CHECK(whs::SanitiseName("  two   words  ") == "two words");
}

TEST_CASE("SanitiseName keeps non-ASCII bytes untouched", "[sanitise]")
{
    const std::string cyrillic = "\xD0\x9F\xD0\xB8\xD1\x81\xD0\xB0\xD1\x80\xD1\x8C";
    CHECK(whs::SanitiseName(cyrillic) == cyrillic);
}

TEST_CASE("SanitiseName reduces a blank input to nothing", "[sanitise]")
{
    CHECK(whs::SanitiseName("   ").empty());
    CHECK(whs::SanitiseName("").empty());
}

TEST_CASE("XmlEscape escapes ampersand first", "[sanitise]")
{
    CHECK(whs::XmlEscape("a & b") == "a &amp; b");
    CHECK(whs::XmlEscape("\"<>\"") == "&quot;&lt;&gt;&quot;");
    CHECK(whs::XmlEscape("&lt;") == "&amp;lt;");
}

TEST_CASE("XmlUnescape is the inverse of XmlEscape", "[sanitise]")
{
    const std::string raw = "a & b < c > d \" e";
    CHECK(whs::XmlUnescape(whs::XmlEscape(raw)) == raw);
    CHECK(whs::XmlUnescape("&amp;lt;") == "&lt;");
}
