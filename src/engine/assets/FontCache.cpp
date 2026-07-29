#include "engine/assets/FontCache.h"

#include <iostream>
#include <system_error>
#include <utility>

#include "engine/assets/TextureCache.h"

namespace sr::engine {

std::string FontCache::Key(const std::string& relativePath, int baseSize) {
    return relativePath + "@" + std::to_string(baseSize);
}

FontCache::FontCache(std::filesystem::path root) : root_(std::move(root)) {}

FontCache::~FontCache() {
    UnloadAll();
}

const Font& FontCache::Get(const std::string& relativePath, int baseSize) {
    const std::string key = Key(relativePath, baseSize);
    const auto cached = fonts_.find(key);
    if (cached != fonts_.end()) {
        return cached->second;
    }
    if (failed_.find(key) != failed_.end()) {
        // Raylib owns the default font's texture; it is never entered into fonts_/UnloadFont'd
        // by this cache, the same way TextureCache::Placeholder never mixes into `textures_`.
        return DefaultFont();
    }

    const std::filesystem::path full = ResolveAssetPath(root_, relativePath);

    std::error_code ec;
    if (!std::filesystem::exists(full, ec) || ec) {
        std::cerr << "STAR REACH: missing font '" << relativePath << "' (looked in "
                  << full.string() << ")\n";
        failed_.insert(key);
        return DefaultFont();
    }

    Font font = LoadFontEx(full.string().c_str(), baseSize, nullptr, 0);
    if (font.texture.id == 0) {
        std::cerr << "STAR REACH: could not decode font '" << relativePath << "'\n";
        failed_.insert(key);
        return DefaultFont();
    }
    // Fonts here are drawn at arbitrary on-screen sizes away from baseSize (buttons, titles,
    // body text each pick their own DrawTextEx fontSize), so mipmaps + trilinear filtering keep
    // the downscaled glyphs from aliasing the way a size-matched atlas wouldn't need to.
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    return fonts_.emplace(key, font).first->second;
}

void FontCache::Unload(const std::string& relativePath, int baseSize) {
    const std::string key = Key(relativePath, baseSize);
    failed_.erase(key);
    const auto found = fonts_.find(key);
    if (found == fonts_.end()) {
        return;
    }
    if (IsWindowReady()) {
        UnloadFont(found->second);
    }
    fonts_.erase(found);
}

void FontCache::UnloadAll() {
    // Same ordering hazard TextureCache::UnloadAll documents: unloading after CloseWindow() has
    // destroyed the GL context is undefined, so main() calls this before Window::Close().
    if (IsWindowReady()) {
        for (auto& entry : fonts_) {
            UnloadFont(entry.second);
        }
    }
    fonts_.clear();
    failed_.clear();
}

const Font& FontCache::DefaultFont() {
    // A function-local static, not `placeholder_`-style member: GetFontDefault() is cheap and
    // stateless (raylib copies its internal font struct out by value each call), so there is
    // nothing to construct-once-per-cache-instance the way TextureCache's GPU placeholder needs.
    // What this DOES need is a stable address for the reference above to point at -- see the
    // declaration comment in FontCache.h.
    static const Font kDefault = GetFontDefault();
    return kDefault;
}

}  // namespace sr::engine
