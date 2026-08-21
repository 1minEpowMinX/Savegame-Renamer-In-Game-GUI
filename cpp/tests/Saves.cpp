#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "TestFixture.h"
#include "whs/Saves.h"

namespace {

/// Returns an empty saves root of its own, clearing one left by an earlier run.
///
/// @param name Suffix telling the test's tree from every other one's.
/// @return The directory.
std::filesystem::path FreshRoot(const std::string& name)
{
    const auto root = std::filesystem::temp_directory_path() / ("renamer_saves_" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

/// Writes a savegame of `saveId` into directory `dir` of the tree.
///
/// @param root Saves root the directory sits in.
/// @param dir Name of the playline directory, created when absent.
/// @param fileName Name to give the savegame.
/// @param saveId Id its header declares.
/// @return The path written.
std::filesystem::path PutSave(const std::filesystem::path& root, const std::string& dir,
                              const std::string& fileName, int saveId)
{
    std::filesystem::create_directories(root / dir);
    const auto path = root / dir / fileName;
    MakeSave(path, SampleXml(saveId), std::string(32, '\xCD'));
    return path;
}

}  // namespace

TEST_CASE("PlaylineDir answers with the directory of the index asked for", "[saves]")
{
    const auto root = FreshRoot("pick");
    std::filesystem::create_directories(root / "playline0");
    std::filesystem::create_directories(root / "playline1");
    std::filesystem::create_directories(root / "playline2");

    CHECK(whs::PlaylineDir(root, 0) == root / "playline0");
    CHECK(whs::PlaylineDir(root, 1) == root / "playline1");
    CHECK(whs::PlaylineDir(root, 2) == root / "playline2");
}

TEST_CASE("PlaylineDir reads a padded name as the number it spells", "[saves]")
{
    const auto root = FreshRoot("padded");
    std::filesystem::create_directories(root / "playline01");

    CHECK(whs::PlaylineDir(root, 1) == root / "playline01");
}

TEST_CASE("PlaylineDir reads a name in any case", "[saves]")
{
    const auto root = FreshRoot("case");
    std::filesystem::create_directories(root / "PlayLine3");

    CHECK(whs::PlaylineDir(root, 3) == root / "PlayLine3");
}

TEST_CASE("PlaylineDir refuses an index two directories spell", "[saves]")
{
    const auto root = FreshRoot("ambiguous");
    std::filesystem::create_directories(root / "playline1");
    std::filesystem::create_directories(root / "playline01");

    CHECK(whs::PlaylineDir(root, 1).empty());
}

TEST_CASE("PlaylineDir passes over what is not a playline", "[saves]")
{
    const auto root = FreshRoot("neighbours");
    std::filesystem::create_directories(root / "screenshots");
    std::filesystem::create_directories(root / "playline");     // the prefix and nothing more
    std::filesystem::create_directories(root / "playline1x");   // digits it does not end in
    std::filesystem::create_directories(root / "myplayline1");  // the prefix, not at the front
    std::ofstream(root / "steam_autocloud.vdf") << "{}";
    std::ofstream(root / "playline2") << "a file, not a directory";

    CHECK(whs::PlaylineDir(root, 0).empty());
    CHECK(whs::PlaylineDir(root, 1).empty());
    CHECK(whs::PlaylineDir(root, 2).empty());
}

TEST_CASE("PlaylineDir answers with nothing when the index is not there", "[saves]")
{
    const auto root = FreshRoot("absent");
    std::filesystem::create_directories(root / "playline0");

    CHECK(whs::PlaylineDir(root, 4).empty());
    CHECK(whs::PlaylineDir(root, -1).empty());
    CHECK(whs::PlaylineDir(root / "no_such_root", 0).empty());
}

TEST_CASE("PlaylineDir passes over a run of digits no index holds", "[saves]")
{
    const auto root = FreshRoot("overflow");
    std::filesystem::create_directories(root / "playline99999999999999999999");

    CHECK(whs::PlaylineDir(root, 0).empty());
    CHECK(whs::PlaylineDir(root, 99999).empty());
}

TEST_CASE("FindSave tells apart one id shared by two playlines", "[saves]")
{
    // The reported case: a second playline numbers its saves from one again, so
    // the same file name and the same id stand in both.
    const auto root = FreshRoot("shared_id");
    const auto first = PutSave(root, "playline0", "autosave4547.whs", 4547);
    const auto second = PutSave(root, "playline1", "autosave4547.whs", 4547);

    const auto inFirst = whs::FindSave(root, 0, "autosave4547.whs", 4547);
    const auto inSecond = whs::FindSave(root, 1, "autosave4547.whs", 4547);
    REQUIRE(inFirst.has_value());
    REQUIRE(inSecond.has_value());
    CHECK(inFirst->Path() == first);
    CHECK(inSecond->Path() == second);
}

TEST_CASE("FindSave hands back the header it read", "[saves]")
{
    // What spares the caller a second read of the file just opened.
    const auto root = FreshRoot("carries_header");
    PutSave(root, "playline0", "permanent12.whs", 12);

    const auto found = whs::FindSave(root, 0, "permanent12.whs", 12);
    REQUIRE(found.has_value());
    CHECK(found->SaveId() == 12);
    CHECK(found->DisplayName() == "@qname_navsteva_lekare_sxxH");
}

TEST_CASE("FindSave refuses a file standing in another playline", "[saves]")
{
    const auto root = FreshRoot("elsewhere");
    PutSave(root, "playline0", "permanent12.whs", 12);
    std::filesystem::create_directories(root / "playline1");

    CHECK_FALSE(whs::FindSave(root, 1, "permanent12.whs", 12).has_value());
}

TEST_CASE("FindSave refuses a file whose header declares another id", "[saves]")
{
    const auto root = FreshRoot("wrong_id");
    PutSave(root, "playline0", "permanent12.whs", 99);

    CHECK_FALSE(whs::FindSave(root, 0, "permanent12.whs", 12).has_value());
}

TEST_CASE("FindSave refuses a file that is not a savegame", "[saves]")
{
    const auto root = FreshRoot("not_a_save");
    std::filesystem::create_directories(root / "playline0");
    std::ofstream(root / "playline0" / "permanent12.whs", std::ios::binary) << "junk";

    CHECK_FALSE(whs::FindSave(root, 0, "permanent12.whs", 12).has_value());
}

TEST_CASE("FindSave refuses an index two directories spell", "[saves]")
{
    const auto root = FreshRoot("save_ambiguous");
    PutSave(root, "playline1", "permanent12.whs", 12);
    PutSave(root, "playline01", "permanent12.whs", 12);

    CHECK_FALSE(whs::FindSave(root, 1, "permanent12.whs", 12).has_value());
}
