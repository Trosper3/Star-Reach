#pragma once

#include <raylib.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

// engine/assets/ -- the LoadTexture seam (architecture.md section 6).
//
// Section 6 defers the asset PIPELINE -- atlases, UUID asset ids, audio banks, hot-reload -- and
// section 2.5 makes that deferral binding until a shipped feature demands one. The main menu
// background is that feature. This is the thin seam it demands and nothing more: load, cache,
// unload. Do not grow it speculatively. If audio arrives later it is a SIBLING type with this
// same shape, not a template generalized over resource kinds -- Law 11's promotion rule wants two
// real consumers before generalizing, and two sixty-line classes are cheaper than one generic one
// that is subtly wrong about both.
//
// raylib.h is included here, unlike in platform/Window.h. That header hides raylib because its
// interface genuinely does not need it (open/close/width/frametime), and hiding it stops raylib
// types leaking into components. A texture cache's interface IS raylib handles, so an opaque id
// would buy nothing here: nothing below sr_engine links sr_engine at all, and check_layers.py
// already forbids raylib.h in shared/ and core/ outright. Two guards cover that boundary already.
namespace sr::engine {

class TextureCache {
public:
    // `root` is the directory relative keys resolve against -- data/base_game/textures/.
    // Constructing this does not touch the GPU, so it is safe before a window exists and in a
    // headless test; only Get() and the unloads need a live context.
    explicit TextureCache(std::filesystem::path root);
    ~TextureCache();

    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;
    TextureCache(TextureCache&&) = delete;
    TextureCache& operator=(TextureCache&&) = delete;

    // Loads on first call, returns the cached handle after. `relativePath` is relative to the
    // root ("ui/main_menu.png"), never absolute -- that rule buys section 6's "no hardcoded paths
    // in gameplay code" goal without building an id system to get it, and it makes the eventual
    // switch to hashed asset ids a change of key type in this one file.
    //
    // Never throws and never returns a null handle: a missing or unreadable file logs ONCE and
    // yields a magenta placeholder, so a broken asset is visible in game rather than silently
    // invisible. raylib's own LoadTexture failure hands back id 0, which draws as nothing at all
    // -- the worst outcome, an invisible bug. Same never-partially-abort contract as
    // core::ContentLibrary::LoadFromDirectory.
    const Texture2D& Get(const std::string& relativePath);

    // Drops one texture. Worth calling only for something big and mode-scoped -- a menu
    // background on OnExit -- since everything else can wait for the destructor.
    void Unload(const std::string& relativePath);
    void UnloadAll();

    std::size_t LoadedCount() const { return textures_.size(); }

private:
    const Texture2D& Placeholder();

    std::filesystem::path root_;
    std::unordered_map<std::string, Texture2D> textures_;
    // Keys that already failed. Without this, a missing file re-hits the filesystem and re-logs
    // every frame, because a placeholder must not be cached under the real key (Unload would
    // then free a handle shared with every other failed key).
    std::unordered_set<std::string> failed_;
    Texture2D placeholder_{};
    bool placeholderLoaded_ = false;
};

// Pure: no raylib call, no filesystem access. Separated out because the relative-key rule above
// is the part actually worth pinning down in a test, and because every other entry point needs a
// live GL context that headless CI does not have -- shared/ui/HudTheme.h documents that same
// constraint for its Draw* functions.
std::filesystem::path ResolveAssetPath(const std::filesystem::path& root,
                                       const std::string& relativePath);

}  // namespace sr::engine
