#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <fstream>
#include <iterator>

#include "TestFixture.h"
#include "whs/Description.h"

namespace {

/// Returns the whole file as bytes.
std::string ReadAll(const std::filesystem::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

/// Returns the bytes after the description header.
std::string PayloadOf(const std::filesystem::path& p)
{
    const std::string all = ReadAll(p);
    std::int32_t length = 0;
    std::memcpy(&length, all.data() + 4, 4);
    return all.substr(8 + static_cast<std::size_t>(length));
}

}  // namespace

TEST_CASE("Write keeps the payload byte for byte", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_payload.whs";
    std::string payload;
    for (int i = 0; i < 5000; ++i)
        payload.push_back(static_cast<char>(i % 251));
    MakeSave(path, SampleXml(), payload);

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("A considerably longer name than the original");
    REQUIRE(d->Write());

    CHECK(PayloadOf(path) == payload);
}

TEST_CASE("Write updates the length prefix so the file re-reads", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_roundtrip.whs";
    MakeSave(path, SampleXml(), std::string(128, '\x02'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("Short");
    REQUIRE(d->Write());

    const auto reread = whs::Description::Read(path);
    REQUIRE(reread.has_value());
    CHECK(reread->DisplayName() == "Short");
    CHECK(reread->SaveId() == 3754);
    CHECK(reread->HasCustomName());
}

TEST_CASE("Write shrinks the header when the name gets shorter", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_shrink.whs";
    MakeSave(path, SampleXml(), std::string(128, '\x03'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("x");
    REQUIRE(d->Write());

    auto reloaded = whs::Description::Read(path);
    REQUIRE(reloaded.has_value());
    reloaded->ResetName();
    REQUIRE(reloaded->Write());

    const auto restored = whs::Description::Read(path);
    REQUIRE(restored.has_value());
    CHECK(restored->DisplayName() == "@qname_navsteva_lekare_sxxH");
    CHECK_FALSE(restored->HasCustomName());
    CHECK(PayloadOf(path) == std::string(128, '\x03'));
}

TEST_CASE("Write refuses when the file on disk is a different save", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_guard.whs";
    MakeSave(path, SampleXml(3754), std::string(64, '\x04'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("Stale");

    MakeSave(path, SampleXml(9999), std::string(64, '\x05'));   // the game reused the slot
    CHECK_FALSE(d->Write());

    const auto onDisk = whs::Description::Read(path);
    REQUIRE(onDisk.has_value());
    CHECK(onDisk->SaveId() == 9999);
}

TEST_CASE("Write refuses when the file is gone", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_deleted.whs";
    MakeSave(path, SampleXml(), std::string(64, '\x07'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("Vanished");
    std::filesystem::remove(path);

    CHECK_FALSE(d->Write());
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("Write leaves no temporary file behind", "[write]")
{
    const auto dir = std::filesystem::temp_directory_path() / "write_tmp_check";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto path = dir / "permanent0001.whs";
    MakeSave(path, SampleXml(), std::string(64, '\x06'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("Tidy");
    REQUIRE(d->Write());

    CHECK(std::distance(std::filesystem::directory_iterator(dir),
                        std::filesystem::directory_iterator{}) == 1);
}
