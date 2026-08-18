#include <catch2/catch_test_macros.hpp>

#include "TestFixture.h"
#include "whs/Description.h"
#include "whs/Sanitise.h"

namespace {

/// Writes a fixture save and returns its parsed description.
whs::Description Load(const std::string& xml, const char* file)
{
    const auto path = std::filesystem::temp_directory_path() / file;
    MakeSave(path, xml, std::string(32, '\x01'));
    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    return *d;
}

}  // namespace

TEST_CASE("SetDisplayName replaces the quest field and clears the objective", "[rename]")
{
    auto d = Load(SampleXml(), "rename_basic.whs");
    d.SetDisplayName("Before the duel");

    CHECK(d.DisplayName() == "Before the duel");
    CHECK(d.Xml().find("|Before the duel||location_suchdol|") != std::string::npos);
}

TEST_CASE("The first rename stashes the original quest and objective", "[rename]")
{
    auto d = Load(SampleXml(), "rename_stash.whs");
    CHECK_FALSE(d.HasCustomName());

    d.SetDisplayName("First");
    CHECK(d.HasCustomName());
    CHECK(d.Xml().find(
        "RenamerOriginal=\"@qname_navsteva_lekare_sxxH|@jmena_obj_zacatek_questu_JJyn\"")
        != std::string::npos);
}

TEST_CASE("A second rename keeps the originally stashed name", "[rename]")
{
    auto d = Load(SampleXml(), "rename_twice.whs");
    d.SetDisplayName("First");
    d.SetDisplayName("Second");

    CHECK(d.DisplayName() == "Second");
    CHECK(d.Xml().find("RenamerOriginal=\"@qname_navsteva_lekare_sxxH|") != std::string::npos);
    CHECK(d.Xml().find("RenamerOriginal=\"First") == std::string::npos);
}

TEST_CASE("ResetName restores both fields and drops the attribute", "[rename]")
{
    auto d = Load(SampleXml(), "rename_reset.whs");
    d.SetDisplayName("Temporary");
    d.ResetName();

    CHECK(d.DisplayName() == "@qname_navsteva_lekare_sxxH");
    CHECK(d.Xml().find("@jmena_obj_zacatek_questu_JJyn") != std::string::npos);
    CHECK(d.Xml().find("RenamerOriginal") == std::string::npos);
    CHECK_FALSE(d.HasCustomName());
}

TEST_CASE("ResetName on a save that was never renamed changes nothing", "[rename]")
{
    auto d = Load(SampleXml(), "rename_reset_noop.whs");
    const std::string before = d.Xml();
    d.ResetName();

    CHECK(d.Xml() == before);
}

TEST_CASE("SetDisplayName sanitises and escapes the input", "[rename]")
{
    auto d = Load(SampleXml(), "rename_escape.whs");
    d.SetDisplayName("Rock & \"roll\" |cut|");

    CHECK(d.Xml().find("Rock &amp; &quot;roll&quot; cut") != std::string::npos);
    CHECK(d.DisplayName() == "Rock & \"roll\" cut");
}

TEST_CASE("An empty name resets instead of blanking the entry", "[rename]")
{
    auto d = Load(SampleXml(), "rename_empty.whs");
    d.SetDisplayName("Something");
    d.SetDisplayName("   ");

    CHECK(d.DisplayName() == "@qname_navsteva_lekare_sxxH");
    CHECK_FALSE(d.HasCustomName());
}

TEST_CASE("Renaming leaves the other header attributes alone", "[rename]")
{
    auto d = Load(SampleXml(), "rename_untouched.whs");
    d.SetDisplayName("Quiet");

    CHECK(d.SaveId() == 3754);
    CHECK(d.Xml().find("BuildInfo=\"1.5.6-15693-release_1_5\"") != std::string::npos);
    CHECK(d.Xml().find("SaveType=\"PermanentSave\"") != std::string::npos);
    CHECK(d.Xml().find("<structwh::rpgmodule::S_LocationId>39a52acd") != std::string::npos);
}

TEST_CASE("A name holding regex replacement syntax is stored literally", "[rename]")
{
    // The value used to be handed to regex_replace as its replacement argument,
    // where "$&" pastes the whole match back in, quotes included, and "$1" is
    // dropped. The header came out unparseable and the save left the load list.
    for (const std::string name : {"Cost 100$", "Save $1 here", "A $& B", "$$ pure"}) {
        auto d = Load(SampleXml(), "rename_dollar.whs");
        d.SetDisplayName(name);

        CHECK(d.DisplayName() == name);
        CHECK(d.Xml().find("|" + whs::XmlEscape(name) + "||location_suchdol|")
              != std::string::npos);
        // The pasted match brought a quote of its own, which ended the attribute
        // early and left a second UIDescription in the element.
        CHECK(d.Xml().find("UIDescription=") == d.Xml().rfind("UIDescription="));
    }
}

TEST_CASE("An attribute whose name ends in another's is not mistaken for it", "[rename]")
{
    auto d = Load(SampleXml(3754, "@quest", "@objective", " AutoSaveId=\"9999\""), "rename_suffix.whs");
    CHECK(d.SaveId() == 3754);
}
