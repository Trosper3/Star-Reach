#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "engine/assets/TextureCache.h"

// Only the headless-safe half of TextureCache is covered here. Get() uploads to the GPU, and
// raylib's texture path is not valid until InitWindow has run -- shared/ui/HudTheme.h documents
// that same constraint for its Draw* functions, and it is why WorldRenderer has no automated
// coverage either. Whether the menu background actually appears on screen is a verify-in-game
// check, not something a build log can honestly confirm.
//
// What IS testable is the relative-key rule, which is the part of this seam carrying an actual
// architectural commitment (section 6's "no hardcoded paths in gameplay code").

using sr::engine::ResolveAssetPath;
using sr::engine::TextureCache;

TEST_CASE("ResolveAssetPath anchors a relative key under the root", "[assets]") {
    const std::filesystem::path resolved =
        ResolveAssetPath("data/base_game/textures", "ui/main_menu.png");

    CHECK(resolved.filename() == "main_menu.png");
    CHECK(resolved.parent_path().filename() == "ui");
    CHECK(resolved.string().find("textures") != std::string::npos);
}

TEST_CASE("ResolveAssetPath normalises a dotted key", "[assets]") {
    CHECK(ResolveAssetPath("root", "./ui/../ui/bg.png") == ResolveAssetPath("root", "ui/bg.png"));
}

TEST_CASE("ResolveAssetPath does not let an absolute key discard the root", "[assets]") {
    // std::filesystem::operator/ REPLACES the left side when the right operand is absolute, so
    // an absolute key would silently reintroduce exactly the hardcoded machine-specific path the
    // relative-key rule exists to prevent. current_path() is used rather than a literal because
    // what counts as absolute differs between Windows (needs a drive letter) and POSIX.
    const std::filesystem::path root = "assets_root";
    const std::filesystem::path absoluteKey = std::filesystem::current_path() / "bg.png";

    const std::filesystem::path resolved = ResolveAssetPath(root, absoluteKey.string());

    CHECK(resolved.string().find("assets_root") != std::string::npos);
    CHECK(resolved.filename() == "bg.png");
}

TEST_CASE("A fresh cache is empty and tolerates unloading what it never held", "[assets]") {
    // Constructing without a window is the property that makes this test possible at all: the
    // constructor stores a path and touches nothing else.
    TextureCache cache("data/base_game/textures");
    CHECK(cache.LoadedCount() == 0);

    cache.Unload("ui/never_loaded.png");
    cache.UnloadAll();

    CHECK(cache.LoadedCount() == 0);
}
