#include "engine/assets/TextureCache.h"

#include <iostream>
#include <system_error>
#include <utility>

namespace sr::engine {

std::filesystem::path ResolveAssetPath(const std::filesystem::path& root,
                                       const std::string& relativePath) {
    const std::filesystem::path relative(relativePath);
    // operator/ REPLACES the left side when the right is absolute, which would silently
    // reintroduce exactly the hardcoded absolute path the relative-key rule exists to prevent.
    // Anchoring it back to the root turns that mistake into a wrong-but-contained lookup.
    const std::filesystem::path anchored =
        relative.is_absolute() ? relative.relative_path() : relative;
    return (root / anchored).lexically_normal();
}

TextureCache::TextureCache(std::filesystem::path root) : root_(std::move(root)) {}

TextureCache::~TextureCache() {
    UnloadAll();
}

const Texture2D& TextureCache::Get(const std::string& relativePath) {
    const auto cached = textures_.find(relativePath);
    if (cached != textures_.end()) {
        return cached->second;
    }
    if (failed_.find(relativePath) != failed_.end()) {
        return Placeholder();
    }

    const std::filesystem::path full = ResolveAssetPath(root_, relativePath);

    // Checked before handing the path to raylib so the failure message names the path WE
    // resolved, which is the one the author has to fix.
    std::error_code ec;
    if (!std::filesystem::exists(full, ec) || ec) {
        std::cerr << "STAR REACH: missing texture '" << relativePath << "' (looked in "
                  << full.string() << ")\n";
        failed_.insert(relativePath);
        return Placeholder();
    }

    const Texture2D texture = LoadTexture(full.string().c_str());
    if (texture.id == 0) {
        std::cerr << "STAR REACH: could not decode texture '" << relativePath << "'\n";
        failed_.insert(relativePath);
        return Placeholder();
    }
    return textures_.emplace(relativePath, texture).first->second;
}

void TextureCache::Unload(const std::string& relativePath) {
    failed_.erase(relativePath);
    const auto found = textures_.find(relativePath);
    if (found == textures_.end()) {
        return;
    }
    if (IsWindowReady()) {
        UnloadTexture(found->second);
    }
    textures_.erase(found);
}

void TextureCache::UnloadAll() {
    // Unloading after CloseWindow() has destroyed the GL context is undefined, so main() calls
    // this explicitly BEFORE Window::Close(). This guard keeps the destructor safe in any other
    // order too, including a headless test where no window ever existed.
    if (IsWindowReady()) {
        for (auto& entry : textures_) {
            UnloadTexture(entry.second);
        }
        if (placeholderLoaded_) {
            UnloadTexture(placeholder_);
        }
    }
    textures_.clear();
    failed_.clear();
    placeholder_ = Texture2D{};
    placeholderLoaded_ = false;
}

const Texture2D& TextureCache::Placeholder() {
    if (!placeholderLoaded_) {
        // Two-by-two magenta: the universal "this asset is missing" tell. Generated rather than
        // loaded so a missing placeholder can never itself be the missing asset.
        Image image = GenImageColor(2, 2, MAGENTA);
        placeholder_ = LoadTextureFromImage(image);
        UnloadImage(image);
        placeholderLoaded_ = true;
    }
    return placeholder_;
}

}  // namespace sr::engine
