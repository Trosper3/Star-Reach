#pragma once

#include <raylib.h>

#include "engine/assets/FontCache.h"

// shared/ui/ -- the Orbitron/Exo2 pair HudTheme.h's own header comment already names as prior art
// for the look ("Orbitron/Exo2 fonts") but never wired up: every Draw* call in this layer and in
// modes/space/ui/ drew through raylib's blocky built-in bitmap font (DrawText/MeasureText) until
// the docked-screen visual-chrome pass (issues #224/#225) asked for something less pixelated. A
// separate file from HudTheme.h, which documents itself as "header-only... no state to own here,
// only palette constants and draw-time math" -- FontCache is real state (a GPU-backed cache with
// ownership), so it earns its own seam instead of quietly contradicting that comment.
namespace sr::ui {

inline constexpr const char* kHeadingFontAsset = "Orbitron-VariableFont_wght.ttf";
inline constexpr const char* kBodyFontAsset = "Exo2-VariableFont_wght.ttf";
// A shared glyph-atlas resolution both fonts load at -- engine::FontCache::Get's own doc notes
// "the same .ttf loaded at two sizes is two distinct Font handles," so every Draw*Ex call below
// draws smaller than this via its own fontSize argument, downscaled through the mipmapped,
// trilinear-filtered atlas FontCache::Get already builds, rather than reloading per drawn size.
inline constexpr int kFontBaseSize = 64;

// The two faces MainMenu.cpp already established: Orbitron for titles/headings (angular, sci-fi),
// Exo2 for everything read at body size (labels, values, hints, buttons) -- MainMenu's own comment
// that Exo2 "has no consumer here yet" is what this closes.
struct Fonts {
    const Font& heading;
    const Font& body;
};

inline Fonts LoadFonts(engine::FontCache& cache) {
    return Fonts{cache.Get(kHeadingFontAsset, kFontBaseSize),
                 cache.Get(kBodyFontAsset, kFontBaseSize)};
}

}  // namespace sr::ui
