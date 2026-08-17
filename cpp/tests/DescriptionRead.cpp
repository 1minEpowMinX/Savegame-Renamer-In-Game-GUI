#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include "TestFixture.h"
#include "whs/Description.h"

TEST_CASE("Read parses id and display name", "[description]")
{
    const auto path = std::filesystem::temp_directory_path() / "read_basic.whs";
    MakeSave(path, SampleXml(), std::string(64, '\xAB'));

    const auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    CHECK(d->SaveId() == 3754);
    CHECK(d->DisplayName() == "@qname_navsteva_lekare_sxxH");
}

TEST_CASE("Read rejects a file with the wrong magic", "[description]")
{
    const auto path = std::filesystem::temp_directory_path() / "read_bad_magic.whs";
    std::ofstream(path, std::ios::binary | std::ios::trunc) << "not a savegame at all";

    CHECK_FALSE(whs::Description::Read(path).has_value());
}

TEST_CASE("Read rejects a truncated header", "[description]")
{
    const auto path = std::filesystem::temp_directory_path() / "read_truncated.whs";
    MakeSave(path, SampleXml(), "");
    std::filesystem::resize_file(path, 20);   // header claims far more than the file holds

    CHECK_FALSE(whs::Description::Read(path).has_value());
}

TEST_CASE("Read rejects a missing file", "[description]")
{
    const auto path = std::filesystem::temp_directory_path() / "read_absent.whs";
    std::filesystem::remove(path);

    CHECK_FALSE(whs::Description::Read(path).has_value());
}
