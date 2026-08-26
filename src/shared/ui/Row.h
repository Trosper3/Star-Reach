#pragma once

#include <string>

// shared/ui/ -- the one row type every docked screen and both flight overlays draw through
// ListView (architecture.md 12.30, "the shared widget layer"), replacing InventoryGrid and the
// four hand-rolled row loops it never actually superseded.
namespace sr::ui {

// features.md 3.9: colour carries condition, glyph carries identity, everywhere. `integrity`
// drives ListView's green -> yellow -> orange -> red row tint; `disabled` is the greyed-out
// state features.md 3.10's degrade-never-remove rule needs -- an undeletable hardpoint, an
// empty-state placeholder row -- shown, never omitted.
struct RowStyle {
    float integrity = 1.0f;  // 0..1
    bool disabled = false;
};

// A screen builds a std::vector<Row> and hands it to ListView; grouping (header rows) stays in
// the screen, not the widget, which is what keeps ListView dumb enough to serve a job queue, a
// bay roster, and a fifty-element manifest without branching on which it is.
struct Row {
    std::string label;
    std::string value;
    char glyph[3] = {'\0', '\0', '\0'};  // monogram placeholder (features.md 3.9)
    RowStyle style;
    float fill = -1.0f;    // negative = no progress bar (architecture.md 12.30.6)
    std::string subtitle;  // empty = the classic one-line row; set = a two-line card row
                           // (issue #225's visual-chrome pass, first drawn by BayView's roster).
};

}  // namespace sr::ui
