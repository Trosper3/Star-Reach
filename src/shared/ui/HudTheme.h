#pragma once

#include <raylib.h>

#include <array>

// shared/ui/ -- the HUD theme (architecture.md section 3; section 2.1's sr_shared_ui note).
//
// This is the one library that sits above BOTH sr_shared and sr_engine: presentation code that
// needs raylib's drawing primitives directly, in a layer engine/platform/Window.h deliberately
// keeps raylib.h out of (see that header's comment -- the point is stopping raylib types from
// leaking into sr_shared and sr_core, where a headless test or tools/economy_sim would break).
// shared/ui/ is the seam where that trade-off gets paid back: it is the exception the layer
// table names, not a hole in it.
//
// This is a fresh rewrite, not a port. The old SpaceFlight HUD -- bracket panels, chamfered
// buttons, a chrome/glass palette, Orbitron/Exo2 fonts -- is prior art for the LOOK, not code
// carried over line for line.
//
// Header-only: there is no state to own here, only palette constants and draw-time math, the
// same shape the legacy file had. modes/space/ui/ (CockpitHud, AvionicsMenu, BridgeView -- a
// separate, dependent issue) is the first real consumer of the Draw* functions below;
// tests/unit/HudThemeTests.cpp exercises the pure-math half in the meantime.
namespace sr::ui {

// -- Palette ------------------------------------------------------------------------------
// A cockpit chrome-and-glass look: dark translucent panel glass, a lit chrome border, and a
// status triad shared by every gauge or readout that needs one.
inline constexpr Color kPanelGlass = {9, 14, 20, 218};
inline constexpr Color kPanelChrome = {90, 150, 190, 210};
inline constexpr Color kLabelDim = {95, 125, 150, 255};
inline constexpr Color kValueBright = {210, 230, 245, 255};
inline constexpr Color kDivider = {45, 68, 88, 150};
inline constexpr Color kStatusGood = {60, 210, 130, 255};
inline constexpr Color kStatusCaution = {235, 175, 60, 255};
inline constexpr Color kStatusCritical = {230, 70, 70, 255};

// -- Geometry -----------------------------------------------------------------------------
// The vertex ring for a chamfered ("cut corner") rectangle, used for panel tabs and buttons.
// Pure math, no raylib drawing call -- unlike everything below it, this is unit-testable
// without a live GL context. raylib's default font and render batch are not valid until
// InitWindow has run, which is why the Draw* functions here (and WorldRenderer) have no
// automated coverage: there is nothing headless CI can safely call.
inline std::array<Vector2, 6> ChamferedRectPoints(Rectangle bounds, float cut) {
    return {
        Vector2{bounds.x + cut, bounds.y},
        Vector2{bounds.x + bounds.width, bounds.y},
        Vector2{bounds.x + bounds.width, bounds.y + bounds.height - cut},
        Vector2{bounds.x + bounds.width - cut, bounds.y + bounds.height},
        Vector2{bounds.x, bounds.y + bounds.height},
        Vector2{bounds.x, bounds.y + cut},
    };
}

// -- Drawing ------------------------------------------------------------------------------
// A glass panel with corner brackets instead of a full border box -- reads as an instrument
// bezel rather than a filled debug rectangle.
inline void DrawBracketPanel(Rectangle bounds, Color glass, Color chrome,
                             float cornerLength = 16.0f, float thickness = 2.0f) {
    DrawRectangleRec(bounds, glass);
    auto Bracket = [&](Vector2 origin, Vector2 horizontal, Vector2 vertical) {
        DrawLineEx(origin, {origin.x + horizontal.x, origin.y + horizontal.y}, thickness, chrome);
        DrawLineEx(origin, {origin.x + vertical.x, origin.y + vertical.y}, thickness, chrome);
    };
    Bracket({bounds.x, bounds.y}, {cornerLength, 0}, {0, cornerLength});
    Bracket({bounds.x + bounds.width, bounds.y}, {-cornerLength, 0}, {0, cornerLength});
    Bracket({bounds.x, bounds.y + bounds.height}, {cornerLength, 0}, {0, -cornerLength});
    Bracket({bounds.x + bounds.width, bounds.y + bounds.height}, {-cornerLength, 0},
            {0, -cornerLength});
}

// A chamfered rect, filled and outlined -- used for HUD tabs and buttons.
inline void DrawChamferedRect(Rectangle bounds, float cut, Color fill, Color border,
                              float thickness = 1.0f) {
    std::array<Vector2, 6> points = ChamferedRectPoints(bounds, cut);
    DrawTriangleFan(points.data(), static_cast<int>(points.size()), fill);
    for (std::size_t i = 0; i < points.size(); ++i) {
        DrawLineEx(points[i], points[(i + 1) % points.size()], thickness, border);
    }
}

// A labeled chamfered button: DrawChamferedRect plus centered text.
inline void DrawChamferedButton(Rectangle bounds, const char* label, const Font& font,
                                float fontSize, Color fill, Color border, Color textColor) {
    DrawChamferedRect(bounds, 6.0f, fill, border, 1.5f);
    const Vector2 textSize = MeasureTextEx(font, label, fontSize, 1.0f);
    DrawTextEx(font, label,
               {bounds.x + (bounds.width - textSize.x) / 2.0f,
                bounds.y + (bounds.height - textSize.y) / 2.0f},
               fontSize, 1.0f, textColor);
}

}  // namespace sr::ui
