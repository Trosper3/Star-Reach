#pragma once

#include <raylib.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

// engine/assets/ -- the LoadFontEx seam, a sibling of TextureCache.h with the same shape (see
// that header's comment: "if audio arrives later it is a sibling type with this same shape, not
// a template generalized over resource kinds" -- fonts are that same case, arriving now that
// MainMenu needs the Orbitron/Exo2 pair instead of GetFontDefault()).
//
// Fonts differ from textures in one respect LoadFontEx bakes in: the glyph atlas is rasterized at
// a fixed base size at load time, so the cache key is (path, size) rather than just path -- the
// same .ttf loaded at two sizes is two distinct Font handles, not one shared texture.
namespace sr::engine {

class FontCache {
public:
    // `root` is the directory relative keys resolve against -- data/base_game/fonts/. Constructing
    // this does not touch the GPU, so it is safe before a window exists and in a headless test;
    // only Get() and the unloads need a live context. Reuses TextureCache.h's ResolveAssetPath --
    // the relative-key rule is identical, so the anchoring logic does not need a second copy.
    explicit FontCache(std::filesystem::path root);
    ~FontCache();

    FontCache(const FontCache&) = delete;
    FontCache& operator=(const FontCache&) = delete;
    FontCache(FontCache&&) = delete;
    FontCache& operator=(FontCache&&) = delete;

    // Loads on first call at `baseSize` (glyph atlas resolution), returns the cached handle after.
    // `relativePath` is relative to the root ("Orbitron-VariableFont_wght.ttf"), never absolute --
    // same rule as TextureCache::Get, same reason.
    //
    // Never throws and never returns a font with id 0: a missing or unreadable file logs ONCE and
    // falls back to raylib's built-in default font, so a broken asset is a visibly different (but
    // still legible) font in game rather than an unrenderable one.
    const Font& Get(const std::string& relativePath, int baseSize = 96);

    void Unload(const std::string& relativePath, int baseSize = 96);
    void UnloadAll();

    std::size_t LoadedCount() const { return fonts_.size(); }

private:
    static std::string Key(const std::string& relativePath, int baseSize);
    // GetFontDefault() returns Font by value, so `return GetFontDefault();` from a const Font&
    // function would bind the reference to a temporary destroyed at the end of the return
    // statement -- a dangling reference the caller reads next frame. This gives the fallback a
    // stable address to return a reference to instead, the same role TextureCache::Placeholder()
    // plays for a missing texture.
    static const Font& DefaultFont();

    std::filesystem::path root_;
    std::unordered_map<std::string, Font> fonts_;
    // Keys that already failed. Without this, a missing file re-hits the filesystem and re-logs
    // every frame, same reasoning as TextureCache::failed_.
    std::unordered_set<std::string> failed_;
};

}  // namespace sr::engine
