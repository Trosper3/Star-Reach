#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "shared/ui/Widgets.h"

using Catch::Approx;
using sr::ui::ButtonClicked;
using sr::ui::ButtonHitTest;
using sr::ui::ClampListViewScroll;
using sr::ui::GaugeFillRect;
using sr::ui::kListRowHeight;
using sr::ui::ListViewEffectiveRows;
using sr::ui::ListViewRowAt;
using sr::ui::PanelContentRect;
using sr::ui::Row;
using sr::ui::TabStripHitTest;
using sr::ui::UiInput;

// -- PanelFrame -------------------------------------------------------------------------------

TEST_CASE("PanelContentRect insets the content rect from the panel bounds", "[widgets]") {
    const Rectangle bounds{10.0f, 20.0f, 200.0f, 100.0f};
    const Rectangle content = PanelContentRect(bounds);

    CHECK(content.x == Approx(bounds.x + sr::ui::kPanelPadding));
    CHECK(content.y == Approx(bounds.y + sr::ui::kPanelPadding));
    CHECK(content.width == Approx(bounds.width - sr::ui::kPanelPadding * 2.0f));
    CHECK(content.height == Approx(bounds.height - sr::ui::kPanelPadding * 2.0f));
}

TEST_CASE("PanelContentRect never inverts for a panel smaller than the inset", "[widgets]") {
    const Rectangle bounds{0.0f, 0.0f, 4.0f, 4.0f};  // Smaller than 2x kPanelPadding.
    const Rectangle content = PanelContentRect(bounds);

    CHECK(content.width >= 0.0f);
    CHECK(content.height >= 0.0f);
}

// -- ListView -----------------------------------------------------------------------------------

TEST_CASE("ListViewRowAt maps a cursor inside the content rect to a row index", "[widgets]") {
    const Rectangle content{0.0f, 0.0f, 100.0f, 100.0f};
    // Row 0 spans y in [0, 20), row 1 spans [20, 40).
    CHECK(ListViewRowAt(content, 5, 0.0f, Vector2{10.0f, 5.0f}) == 0);
    CHECK(ListViewRowAt(content, 5, 0.0f, Vector2{10.0f, 25.0f}) == 1);
}

TEST_CASE("ListViewRowAt returns none for a cursor outside the content rect", "[widgets]") {
    const Rectangle content{0.0f, 0.0f, 100.0f, 100.0f};
    CHECK_FALSE(ListViewRowAt(content, 5, 0.0f, Vector2{-1.0f, 5.0f}).has_value());
    CHECK_FALSE(ListViewRowAt(content, 5, 0.0f, Vector2{10.0f, 200.0f}).has_value());
}

TEST_CASE("ListViewRowAt returns none past the last row, inside the content rect", "[widgets]") {
    // 3 rows at 20 units each occupy [0, 60); the content rect is taller than that, so a cursor
    // at y=80 is inside the rect but below every row.
    const Rectangle content{0.0f, 0.0f, 100.0f, 100.0f};
    CHECK_FALSE(ListViewRowAt(content, 3, 0.0f, Vector2{10.0f, 80.0f}).has_value());
}

TEST_CASE("ListViewRowAt accounts for scroll offset", "[widgets]") {
    const Rectangle content{0.0f, 0.0f, 100.0f, 40.0f};
    // Scrolled down by one row height: the cursor at the top of the content rect now lands on
    // row 1, not row 0.
    CHECK(ListViewRowAt(content, 5, kListRowHeight, Vector2{10.0f, 0.0f}) == 1);
}

TEST_CASE("ClampListViewScroll does not move when every row fits", "[widgets]") {
    const Rectangle content{0.0f, 0.0f, 100.0f, 200.0f};  // Plenty tall for 3 rows at 20 each.
    CHECK(ClampListViewScroll(content, 3, 0.0f) == Approx(0.0f));
    CHECK(ClampListViewScroll(content, 3, 500.0f) == Approx(0.0f));
}

TEST_CASE("ClampListViewScroll clamps at both ends when rows overflow", "[widgets]") {
    const Rectangle content{0.0f, 0.0f, 100.0f, 50.0f};  // 50 tall; 10 rows at 20 = 200 content.
    CHECK(ClampListViewScroll(content, 10, -100.0f) == Approx(0.0f));
    // maxScroll = 200 - 50 = 150.
    CHECK(ClampListViewScroll(content, 10, 1000.0f) == Approx(150.0f));
    CHECK(ClampListViewScroll(content, 10, 75.0f) == Approx(75.0f));
}

TEST_CASE("ListViewEffectiveRows passes non-empty rows through unchanged", "[widgets]") {
    const std::vector<Row> rows = {Row{"a", "", {}, {}, -1.0f}, Row{"b", "", {}, {}, -1.0f}};
    const std::vector<Row> effective = ListViewEffectiveRows(rows, "EMPTY");

    REQUIRE(effective.size() == 2);
    CHECK(effective[0].label == "a");
    CHECK(effective[1].label == "b");
}

TEST_CASE("ListViewEffectiveRows renders one disabled placeholder row when given none",
          "[widgets]") {
    const std::vector<Row> effective = ListViewEffectiveRows({}, "NO ITEMS IN HOLD");

    REQUIRE(effective.size() == 1);
    CHECK(effective[0].label == "NO ITEMS IN HOLD");
    CHECK(effective[0].style.disabled);
}

// -- Button ---------------------------------------------------------------------------------

TEST_CASE("ButtonHitTest is true only inside the bounds", "[widgets]") {
    const Rectangle bounds{10.0f, 10.0f, 20.0f, 20.0f};
    CHECK(ButtonHitTest(bounds, Vector2{15.0f, 15.0f}));
    CHECK_FALSE(ButtonHitTest(bounds, Vector2{0.0f, 0.0f}));
}

TEST_CASE("ButtonClicked requires both the hit and the click this frame", "[widgets]") {
    const Rectangle bounds{10.0f, 10.0f, 20.0f, 20.0f};
    CHECK(ButtonClicked(bounds, UiInput{Vector2{15.0f, 15.0f}, true, 0.0f}));
    CHECK_FALSE(ButtonClicked(bounds, UiInput{Vector2{15.0f, 15.0f}, false, 0.0f}));
    CHECK_FALSE(ButtonClicked(bounds, UiInput{Vector2{0.0f, 0.0f}, true, 0.0f}));
}

// -- TabStrip ---------------------------------------------------------------------------------

TEST_CASE("TabStripHitTest maps a cursor to the equal-width tab beneath it", "[widgets]") {
    const Rectangle bounds{0.0f, 0.0f, 90.0f, 20.0f};  // Three 30-wide tabs.
    CHECK(TabStripHitTest(bounds, 3, Vector2{10.0f, 10.0f}) == 0);
    CHECK(TabStripHitTest(bounds, 3, Vector2{40.0f, 10.0f}) == 1);
    CHECK(TabStripHitTest(bounds, 3, Vector2{80.0f, 10.0f}) == 2);
}

TEST_CASE("TabStripHitTest returns none outside the strip", "[widgets]") {
    const Rectangle bounds{0.0f, 0.0f, 90.0f, 20.0f};
    CHECK_FALSE(TabStripHitTest(bounds, 3, Vector2{200.0f, 10.0f}).has_value());
}

// -- Gauge ----------------------------------------------------------------------------------

TEST_CASE("GaugeFillRect scales width by fraction, anchored to the left", "[widgets]") {
    const Rectangle bounds{10.0f, 10.0f, 100.0f, 20.0f};
    const Rectangle fill = GaugeFillRect(bounds, 0.25f);

    CHECK(fill.x == Approx(bounds.x));
    CHECK(fill.width == Approx(25.0f));
    CHECK(fill.height == Approx(bounds.height));
}

TEST_CASE("GaugeFillRect clamps fraction to [0, 1]", "[widgets]") {
    const Rectangle bounds{0.0f, 0.0f, 100.0f, 20.0f};
    CHECK(GaugeFillRect(bounds, -1.0f).width == Approx(0.0f));
    CHECK(GaugeFillRect(bounds, 5.0f).width == Approx(100.0f));
}
