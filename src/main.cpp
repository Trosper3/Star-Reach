#include <filesystem>
#include <iostream>
#include <optional>

#include "core/economy/FactionEconomy.h"
#include "core/galaxy/WreckRecord.h"
#include "core/registries/ContentLibrary.h"
#include "engine/assets/FontCache.h"
#include "engine/assets/TextureCache.h"
#include "engine/platform/Window.h"
#include "modes/IGameMode.h"
#include "modes/main_menu/MainMenu.h"
#include "modes/space/SpaceFlight.h"

namespace {

// data/ sits beside the source tree, not beside the executable, and the build directory depth
// varies by preset (build/debug/bin vs build/bin). Walking up a few levels keeps `cmake --build`
// and a packaged run both working without a copy step. When the asset pipeline un-defers
// (architecture.md section 6) this is replaced by a proper resolver, not extended.
std::filesystem::path FindContentDirectory() {
    std::filesystem::path dir = std::filesystem::current_path();
    for (int depth = 0; depth < 5; ++depth) {
        const std::filesystem::path candidate = dir / "data" / "base_game";
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            break;
        }
        dir = dir.parent_path();
    }
    return {};
}

// Loads and validates data/base_game/, printing a one-line summary on success or an error on
// failure. Split out of RunGame below so this "load, validate, report" concern doesn't count
// against RunGame's own function-length cap (tools/ci/check_sizes.py) -- the twenty-minute
// extraction the cap exists to force, per that tool's own docstring.
std::optional<sr::core::ContentLibrary> LoadGameContent(const std::filesystem::path& contentDir) {
    sr::core::ContentLibrary content;
    const sr::core::LoadReport load = content.LoadFromDirectory(contentDir);
    if (!load.ok()) {
        std::cerr << "STAR REACH: " << load.Summary() << "\n";
        return std::nullopt;
    }

    // Authored content that cannot be instantiated is a build error, not a runtime surprise.
    // The same check runs in CI against the same files.
    const sr::core::LoadReport validation = content.ValidateAll();
    if (!validation.ok()) {
        std::cerr << "STAR REACH: " << validation.Summary() << "\n";
        return std::nullopt;
    }

    std::cout << "STAR REACH: loaded " << content.ShellCount() << " shells, "
              << content.ModuleCount() << " modules, " << content.ShipCount() << " ships\n";
    return content;
}

// The actual entry point, split from main() below so that function can stay exception-free
// (clang-tidy's bugprone-exception-escape, section 8's CI/CD table). std::filesystem calls in
// FindContentDirectory can throw std::filesystem_error on a genuinely broken environment (a
// permissions failure, a removed drive mid-walk) -- rare, but a clean "STAR REACH: ..." message
// beats an unhandled-exception crash for the same reason the load/validation failures above
// already return 1 with a message instead of letting ContentLibrary throw.
int RunGame() {
    const std::filesystem::path contentDir = FindContentDirectory();
    if (contentDir.empty()) {
        std::cerr << "STAR REACH: could not locate data/base_game\n";
        return 1;
    }

    const std::optional<sr::core::ContentLibrary> content = LoadGameContent(contentDir);
    if (!content.has_value()) {
        return 1;
    }

    sr::engine::Window window;
    if (!window.Open(1600, 900, "Star Reach")) {
        std::cerr << "STAR REACH: failed to open a window\n";
        return 1;
    }

    // Art lives beside the authored JSON under data/base_game/ so the same FindContentDirectory
    // walk resolves both, and so the eventual data/mods/ overlay covers textures for free with
    // the same precedence rules it will use for content. Owned here and passed by reference,
    // exactly like `content` and `economy` -- never a global (see MainMenu's constructor).
    sr::engine::TextureCache textures(contentDir / "textures");
    // Fonts live beside textures under data/base_game/ -- same FindContentDirectory walk, same
    // "not a global" ownership shape (see MainMenu's constructor comment).
    sr::engine::FontCache fonts(contentDir / "fonts");

    sr::core::economy::FactionEconomy economy;
    // Owned here and passed by reference, exactly like `content` and `economy` -- galaxy-wide
    // state a system warp (architecture.md section 12.5) demotes wrecks into and promotes them
    // back out of, never a global.
    sr::core::galaxy::WreckLedger wreckLedger;
    sr::modes::main_menu::MainMenu menu(textures, fonts);
    sr::space::SpaceFlight game(*content, economy, wreckLedger);

    // Which mode runs is main()'s job to track (Law 6/7 govern mode CLASSES, not this loop) --
    // IGameMode.h "lands with the second mode, not before" (architecture.md section 3), and
    // main_menu is that second mode.
    sr::modes::IGameMode* activeMode = &menu;
    activeMode->OnEnter();

    while (!window.ShouldClose()) {
        activeMode->Update(window.FrameTime());

        if (activeMode == &menu && menu.QuitRequested()) {
            break;
        }

        if (activeMode == &menu && menu.ShouldStartGame()) {
            activeMode->OnExit();
            activeMode = &game;
            activeMode->OnEnter();
        }

        window.BeginFrame();
        activeMode->Draw();
        window.EndFrame();
    }

    activeMode->OnExit();
    // Before Window::Close(), not after: unloading a texture/font once CloseWindow() has
    // destroyed the GL context is undefined. TextureCache/FontCache's destructors guard the same
    // hazard for other teardown orders, but this scope closes the window explicitly, so the
    // unload has to be explicit too.
    textures.UnloadAll();
    fonts.UnloadAll();
    window.Close();
    return 0;
}

}  // namespace

// The catch(...) below is exhaustive; the check's static analysis does not seem to trust that a
// catch-all closes every path here.
int main() {  // NOLINT(bugprone-exception-escape)
    try {
        return RunGame();
    } catch (const std::exception& e) {
        std::cerr << "STAR REACH: unhandled exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "STAR REACH: unhandled exception of unknown type\n";
        return 1;
    }
}
