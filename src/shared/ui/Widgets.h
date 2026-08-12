#pragma once

#include <raylib.h>

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "shared/ui/Row.h"
#include "shared/ui/UiInput.h"

// shared/ui/ -- the shared widget layer (architecture.md 12.30). Immediate-mode, stateless, pure
// functions over POD: a widget takes its state and this frame's input as data, draws, and
// returns what the player did. No retained tree, no callbacks, no widget-owned state -- UI state
// lives on a lazily-created singleton entity (the CommsLog precedent), never here.
//
// Each widget splits on HudTheme.h's existing testability seam: layout and hit-testing are pure
// and unit-tested; Draw* is a thin, untested shell over them -- there is nothing headless CI can
// safely call before InitWindow has run.
namespace sr::ui {

// -- PanelFrame ---------------------------------------------------------------------------

inline constexpr float kPanelPadding = 12.0f;

// Pure -- the inset content rect DrawPanelFrame's bezel leaves inside `bounds`. Never inverted:
// clamps to zero width/height rather than negative on a panel smaller than the inset.
Rectangle PanelContentRect(Rectangle bounds);

// Draws the bezel (HudTheme's DrawBracketPanel) and returns PanelContentRect(bounds) -- so a
// screen stops inventing its own padding.
Rectangle DrawPanelFrame(Rectangle bounds);

// -- ListView -------------------------------------------------------------------------------

inline constexpr float kListRowHeight = 20.0f;

// Pure -- clamps `scrollOffset` into [0, maxScroll]; maxScroll is zero (so this always returns
// zero) when every row already fits within `contentRect`.
float ClampListViewScroll(Rectangle contentRect, int rowCount, float scrollOffset);

// Pure -- the row index under `cursor` given a view scrolled by `scrollOffset`, or nullopt if
// `cursor` is outside `contentRect` or past the last row.
std::optional<int> ListViewRowAt(Rectangle contentRect, int rowCount, float scrollOffset,
                                 Vector2 cursor);

// Pure -- `rows` verbatim, or a single disabled row reading `emptyMessage` if `rows` is empty.
// features.md 8.3: absence must never look like emptiness -- a ListView given nothing to draw
// still draws a row, never a blank panel.
std::vector<Row> ListViewEffectiveRows(std::span<const Row> rows, const std::string& emptyMessage);

// Draws ListViewEffectiveRows(rows, emptyMessage) inside `contentRect`, scrolled by
// `scrollOffset` and clipped to it. Row::fill >= 0 draws a fill bar behind that row first
// (architecture.md 12.30.6's per-row progress).
void DrawListView(Rectangle contentRect, std::span<const Row> rows, float scrollOffset,
                  const std::string& emptyMessage);

// -- Button ---------------------------------------------------------------------------------

// Pure -- true if `cursor` is inside `bounds`.
bool ButtonHitTest(Rectangle bounds, Vector2 cursor);

// Pure -- true the one frame `input.clicked` lands inside `bounds`. HudTheme's
// DrawChamferedButton already draws a button; this is the hit-test half it lacked.
bool ButtonClicked(Rectangle bounds, const UiInput& input);

// -- TabStrip -------------------------------------------------------------------------------

// Pure -- `tabCount` equal-width tabs laid out left to right across `bounds`; the index `cursor`
// is over, or nullopt outside all of them.
std::optional<int> TabStripHitTest(Rectangle bounds, int tabCount, Vector2 cursor);

// Draws TabStripHitTest's layout as chamfered tabs, `selected` highlighted.
void DrawTabStrip(Rectangle bounds, std::span<const std::string> labels, int selected);

// -- Gauge ----------------------------------------------------------------------------------

// Pure -- clamps `fraction` into [0, 1] and returns the filled, left-anchored sub-rect of
// `bounds`.
Rectangle GaugeFillRect(Rectangle bounds, float fraction);

// A labelled bar: GaugeFillRect filled, `label` overlaid. features.md 3.4's mandatory
// per-screen facility-health readout; job progress on Research/Manufacturing; CockpitHud's
// existing hull bar.
void DrawGauge(Rectangle bounds, const std::string& label, float fraction, Color fillColor);

}  // namespace sr::ui
