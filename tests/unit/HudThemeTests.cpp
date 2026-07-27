#include <catch2/catch_test_macros.hpp>

#include "shared/ui/HudTheme.h"

TEST_CASE("ChamferedRectPoints cuts all four corners equally", "[hudtheme]") {
    const Rectangle bounds{10.0f, 20.0f, 100.0f, 50.0f};
    const auto points = sr::ui::ChamferedRectPoints(bounds, 8.0f);

    // Six points, walking clockwise from the top-left cut. The two points straddling the
    // top-left corner are the tell that the chamfer actually removed a corner rather than
    // just tracing the rectangle -- five or four distinct corners here would mean the cut
    // collapsed to zero.
    CHECK(points[0].x == bounds.x + 8.0f);
    CHECK(points[0].y == bounds.y);
    CHECK(points[5].x == bounds.x);
    CHECK(points[5].y == bounds.y + 8.0f);

    // The bottom-right corner is cut the same way, diagonally opposite.
    CHECK(points[2].x == bounds.x + bounds.width);
    CHECK(points[2].y == bounds.y + bounds.height - 8.0f);
    CHECK(points[3].x == bounds.x + bounds.width - 8.0f);
    CHECK(points[3].y == bounds.y + bounds.height);
}

TEST_CASE("ChamferedRectPoints with a zero cut traces the bare rectangle", "[hudtheme]") {
    const Rectangle bounds{0.0f, 0.0f, 40.0f, 20.0f};
    const auto points = sr::ui::ChamferedRectPoints(bounds, 0.0f);

    CHECK(points[0].x == 0.0f);
    CHECK(points[0].y == 0.0f);
    CHECK(points[1].x == bounds.width);
    CHECK(points[1].y == 0.0f);
}
