#include "shared/ui/Widgets.h"

#include <algorithm>

#include "shared/ui/HudTheme.h"

namespace sr::ui {
namespace {

constexpr int kFontSize = 12;
constexpr float kRowTextPadding = 8.0f;

unsigned char LerpChannel(unsigned char a, unsigned char b, float t) {
    return static_cast<unsigned char>(static_cast<float>(a) +
                                      (static_cast<float>(b) - static_cast<float>(a)) * t);
}

Color LerpColor(Color a, Color b, float t) {
    return Color{LerpChannel(a.r, b.r, t), LerpChannel(a.g, b.g, t), LerpChannel(a.b, b.b, t), 255};
}

// features.md 3.9's green -> yellow -> orange -> red gradient, over the three status stops
// HudTheme.h already carries (kStatusCritical/Caution/Good) rather than a fourth palette.
Color IntegrityColor(float integrity) {
    const float t = std::clamp(integrity, 0.0f, 1.0f);
    if (t < 0.5f) {
        return LerpColor(kStatusCritical, kStatusCaution, t * 2.0f);
    }
    return LerpColor(kStatusCaution, kStatusGood, (t - 0.5f) * 2.0f);
}

std::string RowText(const Row& row) {
    std::string text;
    if (row.glyph[0] != '\0') {
        text += row.glyph;
        text += ' ';
    }
    text += row.label;
    if (!row.value.empty()) {
        text += "  ";
        text += row.value;
    }
    return text;
}

}  // namespace

Rectangle PanelContentRect(Rectangle bounds) {
    const float width = std::max(0.0f, bounds.width - kPanelPadding * 2.0f);
    const float height = std::max(0.0f, bounds.height - kPanelPadding * 2.0f);
    return Rectangle{bounds.x + kPanelPadding, bounds.y + kPanelPadding, width, height};
}

Rectangle DrawPanelFrame(Rectangle bounds) {
    DrawBracketPanel(bounds, kPanelGlass, kPanelChrome, 10.0f, 2.0f);
    return PanelContentRect(bounds);
}

float ClampListViewScroll(Rectangle contentRect, int rowCount, float scrollOffset) {
    const float contentHeight = static_cast<float>(std::max(rowCount, 0)) * kListRowHeight;
    const float maxScroll = std::max(0.0f, contentHeight - contentRect.height);
    return std::clamp(scrollOffset, 0.0f, maxScroll);
}

std::optional<int> ListViewRowAt(Rectangle contentRect, int rowCount, float scrollOffset,
                                 Vector2 cursor) {
    if (!CheckCollisionPointRec(cursor, contentRect)) {
        return std::nullopt;
    }
    const float localY = cursor.y - contentRect.y + scrollOffset;
    const int index = static_cast<int>(localY / kListRowHeight);
    if (index < 0 || index >= rowCount) {
        return std::nullopt;
    }
    return index;
}

std::vector<Row> ListViewEffectiveRows(std::span<const Row> rows, const std::string& emptyMessage) {
    if (!rows.empty()) {
        return std::vector<Row>(rows.begin(), rows.end());
    }
    Row placeholder;
    placeholder.label = emptyMessage;
    placeholder.style.disabled = true;
    return {placeholder};
}

void DrawListView(Rectangle contentRect, std::span<const Row> rows, float scrollOffset,
                  const std::string& emptyMessage, const Font& font) {
    const std::vector<Row> effective = ListViewEffectiveRows(rows, emptyMessage);

    BeginScissorMode(static_cast<int>(contentRect.x), static_cast<int>(contentRect.y),
                     static_cast<int>(contentRect.width), static_cast<int>(contentRect.height));
    for (std::size_t i = 0; i < effective.size(); ++i) {
        const Row& row = effective[i];
        const float y = contentRect.y + static_cast<float>(i) * kListRowHeight - scrollOffset;
        if (y + kListRowHeight < contentRect.y || y > contentRect.y + contentRect.height) {
            continue;  // Scrolled out of view -- scissor alone would clip the draw, not skip it.
        }

        if (row.fill >= 0.0f) {
            const Rectangle rowBounds{contentRect.x, y, contentRect.width, kListRowHeight};
            DrawRectangleRec(GaugeFillRect(rowBounds, row.fill), kPanelChrome);
        }

        const Color textColor =
            row.style.disabled ? kLabelDim : IntegrityColor(row.style.integrity);
        DrawTextEx(font, RowText(row).c_str(), {contentRect.x + kRowTextPadding, y},
                   static_cast<float>(kFontSize), 1.0f, textColor);
    }
    EndScissorMode();
}

bool ButtonHitTest(Rectangle bounds, Vector2 cursor) {
    return CheckCollisionPointRec(cursor, bounds);
}

bool ButtonClicked(Rectangle bounds, const UiInput& input) {
    return input.clicked && ButtonHitTest(bounds, input.cursor);
}

std::optional<int> TabStripHitTest(Rectangle bounds, int tabCount, Vector2 cursor) {
    if (tabCount <= 0 || !CheckCollisionPointRec(cursor, bounds)) {
        return std::nullopt;
    }
    const float tabWidth = bounds.width / static_cast<float>(tabCount);
    const int index = static_cast<int>((cursor.x - bounds.x) / tabWidth);
    if (index < 0 || index >= tabCount) {
        return std::nullopt;
    }
    return index;
}

void DrawTabStrip(Rectangle bounds, std::span<const std::string> labels, int selected,
                  const Font& font) {
    const int count = static_cast<int>(labels.size());
    if (count == 0) {
        return;
    }
    const float tabWidth = bounds.width / static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
        const Rectangle tabBounds{bounds.x + tabWidth * static_cast<float>(i), bounds.y, tabWidth,
                                  bounds.height};
        const bool isSelected = i == selected;
        DrawChamferedRect(tabBounds, 6.0f, isSelected ? kPanelChrome : kPanelGlass, kPanelChrome,
                          1.5f);
        DrawTextEx(font, labels[static_cast<std::size_t>(i)].c_str(),
                   {tabBounds.x + kRowTextPadding, tabBounds.y + tabBounds.height / 2.0f - 6.0f},
                   static_cast<float>(kFontSize), 1.0f, isSelected ? kValueBright : kLabelDim);
    }
}

Rectangle GaugeFillRect(Rectangle bounds, float fraction) {
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    return Rectangle{bounds.x, bounds.y, bounds.width * f, bounds.height};
}

void DrawGauge(Rectangle bounds, const std::string& label, float fraction, Color fillColor,
               const Font& font) {
    DrawRectangleRec(bounds, kPanelGlass);
    DrawRectangleRec(GaugeFillRect(bounds, fraction), fillColor);
    DrawRectangleLinesEx(bounds, 1.5f, kPanelChrome);
    DrawTextEx(font, label.c_str(),
               {bounds.x + kRowTextPadding, bounds.y + bounds.height / 2.0f - 6.0f},
               static_cast<float>(kFontSize), 1.0f, kValueBright);
}

}  // namespace sr::ui
