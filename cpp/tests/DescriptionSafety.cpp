#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

#include "TestFixture.h"
#include "whs/Description.h"

// The header is searched with a regular expression built around the attribute
// name, over text the mod does not author: the game writes the player's quest
// names, mod list and DLC list into it. These cover what that text can do to
// the search.

namespace {

/// Returns a UsedMods block of `count` entries, as the game writes one.
///
/// @param count Number of mods listed.
/// @return The block, with the game's own indentation.
std::string UsedMods(std::size_t count)
{
    std::string out = "\t<UsedMods>\n";
    for (std::size_t i = 0; i < count; ++i)
        out += "\t\t<wh::S_ModInfo Name=\"mod " + std::to_string(i)
             + "\" Author=\"somebody\" Version=\"1.0\" />\n";
    out += "\t</UsedMods>\n";
    return out;
}

/// Returns `xml` with `children` placed before the root's closing tag.
///
/// @param xml Header as SampleXml returns it.
/// @param children Elements to splice in.
/// @return The header carrying them.
std::string WithChildren(const std::string& xml, const std::string& children)
{
    const std::string close = "</C_SaveGameDescription>";
    return std::string(xml).replace(xml.find(close), 0, children);
}

/// Runs `work` and returns how long it took.
///
/// @param work Callable taking no arguments.
/// @return Elapsed milliseconds.
template <typename Fn>
long long Milliseconds(Fn work)
{
    const auto start = std::chrono::steady_clock::now();
    work();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count();
}

}  // namespace

TEST_CASE("A header carrying a long mod list is read and rewritten whole", "[safety]")
{
    // The header grows with the player's mod list, which the mod does not bound.
    // Five thousand entries is far past any real load order and puts the search
    // over a few hundred kilobytes.
    const auto path = std::filesystem::temp_directory_path() / "safety_long_header.whs";
    const std::string payload(4096, '\xCD');
    MakeSave(path, WithChildren(SampleXml(), UsedMods(5000)), payload);

    std::optional<whs::Description> d;
    const long long readMs = Milliseconds([&] { d = whs::Description::Read(path); });
    REQUIRE(d.has_value());
    CHECK(d->SaveId() == 3754);
    CHECK(d->DisplayName() == "@qname_navsteva_lekare_sxxH");

    // A search that backtracked would not come back in this. The pattern holds
    // one star over a negated class, so the cost is linear in the header.
    CHECK(readMs < 5000);

    d->SetDisplayName("A name on a heavily modded save");
    REQUIRE(d->Write());

    const auto again = whs::Description::Read(path);
    REQUIRE(again.has_value());
    CHECK(again->DisplayName() == "A name on a heavily modded save");
    CHECK(again->Xml().find("mod 4999") != std::string::npos);
    CHECK(std::filesystem::file_size(path) > payload.size());
}

TEST_CASE("A name far longer than the dialog accepts survives a round trip", "[safety]")
{
    // The movie stops the field at MAX_CHARS, and the plugin bounds nothing.
    // Anything that reaches the model has to come back out as it went in.
    const auto path = std::filesystem::temp_directory_path() / "safety_long_name.whs";
    MakeSave(path, SampleXml(), "payload");

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());

    const std::string name(100000, 'x');
    d->SetDisplayName(name);
    REQUIRE(d->Write());

    const auto again = whs::Description::Read(path);
    REQUIRE(again.has_value());
    CHECK(again->DisplayName() == name);
    CHECK(again->SaveId() == 3754);
}

TEST_CASE("A name that spells out an attribute cannot forge one", "[safety]")
{
    // The one string in the header the player writes. Escaped on the way in, so
    // the quote that would end the value never reaches the header.
    const auto path = std::filesystem::temp_directory_path() / "safety_injection.whs";
    MakeSave(path, SampleXml(), "payload");

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());

    d->SetDisplayName("hello\" SaveId=\"9999\" RenamerOriginal=\"forged|name");
    REQUIRE(d->Write());

    const auto again = whs::Description::Read(path);
    REQUIRE(again.has_value());
    CHECK(again->SaveId() == 3754);
    // The bar goes: it separates the UIDescription fields, and SanitiseName drops
    // it. Everything else comes back as typed, the quotes among it.
    CHECK(again->DisplayName() == "hello\" SaveId=\"9999\" RenamerOriginal=\"forgedname");
    // The stash the mod itself wrote, not the one spelled out in the name.
    CHECK(again->HasCustomName());
    d = again;
    d->ResetName();
    CHECK(d->DisplayName() == "@qname_navsteva_lekare_sxxH");
    CHECK(d->ObjectiveName() == "@jmena_obj_zacatek_questu_JJyn");
}

TEST_CASE("An attribute standing on a child element is not taken for the root's", "[safety]")
{
    // Every child element the game writes carries attributes of its own, so a
    // search reaching past the root element would answer with one of those.
    const auto path = std::filesystem::temp_directory_path() / "safety_child_attr.whs";
    MakeSave(path, WithChildren(SampleXml(),
                                "\t<UsedMods>\n"
                                "\t\t<wh::S_ModInfo RenamerOriginal=\"a|b\" />\n"
                                "\t</UsedMods>\n"),
             "payload");

    const auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    CHECK_FALSE(d->HasCustomName());
}

TEST_CASE("A stash on a child element does not survive a reset of the root's", "[safety]")
{
    const auto path = std::filesystem::temp_directory_path() / "safety_child_reset.whs";
    MakeSave(path, WithChildren(SampleXml(),
                                "\t<UsedMods>\n"
                                "\t\t<wh::S_ModInfo RenamerOriginal=\"a|b\" />\n"
                                "\t</UsedMods>\n"),
             "payload");

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());

    d->SetDisplayName("renamed");
    d->ResetName();
    CHECK(d->DisplayName() == "@qname_navsteva_lekare_sxxH");
    CHECK(d->Xml().find("<wh::S_ModInfo RenamerOriginal=\"a|b\" />") != std::string::npos);
}
