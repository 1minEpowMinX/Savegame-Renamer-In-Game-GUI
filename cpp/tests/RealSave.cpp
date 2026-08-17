#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>

#include "whs/Description.h"

// KCD2_TEST_SAVE points at a COPY of a real .whs. The test renames it, resets
// it and checks the file came back byte-identical in size; without the variable
// it is skipped, so the suite still runs on a machine without the game.
TEST_CASE("A real savegame survives a rename and a reset", "[real]")
{
    const char* env = std::getenv("KCD2_TEST_SAVE");
    if (!env)
        SKIP("KCD2_TEST_SAVE is not set");

    const std::filesystem::path path{env};
    REQUIRE(std::filesystem::exists(path));
    const auto originalSize = std::filesystem::file_size(path);

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    const std::string before = d->DisplayName();
    CHECK_FALSE(d->HasCustomName());

    d->SetDisplayName("Renamer smoke test");
    REQUIRE(d->Write());

    auto renamed = whs::Description::Read(path);
    REQUIRE(renamed.has_value());
    CHECK(renamed->DisplayName() == "Renamer smoke test");
    CHECK(renamed->HasCustomName());

    renamed->ResetName();
    REQUIRE(renamed->Write());

    const auto restored = whs::Description::Read(path);
    REQUIRE(restored.has_value());
    CHECK(restored->DisplayName() == before);
    CHECK_FALSE(restored->HasCustomName());
    CHECK(std::filesystem::file_size(path) == originalSize);
}
