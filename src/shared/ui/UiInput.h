#pragma once

#include <raylib.h>

// shared/ui/ -- polled once per frame by the caller (architecture.md 12.30) and handed down to
// every widget in Widgets.h, so no widget ever calls IsMouseButtonPressed itself. That is what
// keeps hit-testing headless-testable, what makes features.md 3.6's "all bindings are
// rebindable" possible at all, and what keeps a future network intent source (architecture.md
// 2.3) a change of producer rather than a rewrite of every menu.
namespace sr::ui {

struct UiInput {
    Vector2 cursor{};
    bool clicked = false;
    float scroll = 0.0f;
};

}  // namespace sr::ui
