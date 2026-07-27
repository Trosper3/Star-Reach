#include <filesystem>
#include <iostream>

#include "core/economy/FactionEconomy.h"
#include "core/registries/ContentLibrary.h"
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

    sr::core::ContentLibrary content;
    const sr::core::LoadReport load = content.LoadFromDirectory(contentDir);
    if (!load.ok()) {
        std::cerr << "STAR REACH: " << load.Summary() << "\n";
        return 1;
    }

    // Authored content that cannot be instantiated is a build error, not a runtime surprise.
    // The same check runs in CI against the same files.
    const sr::core::LoadReport validation = content.ValidateAll();
    if (!validation.ok()) {
        std::cerr << "STAR REACH: " << validation.Summary() << "\n";
        return 1;
    }

    std::cout << "STAR REACH: loaded " << content.ShellCount() << " shells, "
              << content.ModuleCount() << " modules, " << content.ShipCount() << " ships\n";

    sr::engine::Window window;
    if (!window.Open(1600, 900, "Star Reach")) {
        std::cerr << "STAR REACH: failed to open a window\n";
        return 1;
    }

    sr::core::economy::FactionEconomy economy;
    sr::modes::main_menu::MainMenu menu;
    sr::space::SpaceFlight game(content, economy);

    // Which mode runs is main()'s job to track (Law 6/7 govern mode CLASSES, not this loop) --
    // IGameMode.h "lands with the second mode, not before" (architecture.md section 3), and
    // main_menu is that second mode.
    sr::modes::IGameMode* activeMode = &menu;
    activeMode->OnEnter();

    while (!window.ShouldClose()) {
        activeMode->Update(window.FrameTime());

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
