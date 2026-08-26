#include "modes/space/ui/StorageMenu.h"

#include <raylib.h>

#include <algorithm>
#include <optional>

#include "shared/components/FlightOverlay.h"
#include "shared/rig/CargoView.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::storage_menu {
namespace {

// KEY_I: mnemonic for Inventory, and free -- features.md 3.6's input map is still marked 📋 and
// does not reserve a key for either overlay. Not a settled binding, just the placeholder this
// issue needs to make the overlay reachable at all; revisit if/when 3.6 is finished.
constexpr int kToggleKey = KEY_I;

constexpr float kPanelWidth = 480.0f;
constexpr float kPanelTop = 80.0f;
constexpr float kHeaderHeight = 54.0f;
constexpr float kGroupHeaderHeight = 24.0f;
constexpr float kItemRowHeight = 42.0f;
constexpr float kListHeight = kItemRowHeight * 6.0f;  // Fits six item rows; overflow scrolls.
constexpr float kFooterHeight = 40.0f;
constexpr float kSectionGap = 8.0f;
constexpr float kIconBoxSize = 26.0f;
constexpr float kStepperButtonSize = 22.0f;
constexpr float kJettisonButtonWidth = 120.0f;
constexpr float kQtyLabelWidth = 40.0f;  // Reserved footer space for the "QTY" caption.

// System.h's "one legitimate cache" exception (Law 6), the same SystemMenuState precedent
// (SystemMenu.cpp's FindOrCreateSingleton) -- duplicated locally per screen file on purpose
// (architecture.md 12.30's established rule: each of this batch's files stays independently
// buildable, and it is eight lines).
entt::entity EnsureSingleton(entt::registry& registry) {
    for (auto [entity] : registry.view<FlightOverlayStateSingleton>().each()) {
        return entity;
    }
    const entt::entity singleton = registry.create();
    registry.emplace<FlightOverlayStateSingleton>(singleton);
    registry.emplace<FlightOverlayState>(singleton);
    return singleton;
}

entt::entity FindSingleton(const entt::registry& registry) {
    for (auto [entity] : registry.view<FlightOverlayStateSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

struct Layout {
    Rectangle header{};
    Rectangle list{};  // The grouped row list -- what Update() hit-tests against.
    Rectangle footer{};
    Rectangle qtyMinus{};
    Rectangle qtyValue{};  // Text only, not a hit target.
    Rectangle qtyPlus{};
    Rectangle jettison{};
};

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float height = kHeaderHeight + kSectionGap + kListHeight + kSectionGap + kFooterHeight;
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth, height};
}

Layout ComputeLayout() {
    const Rectangle content = sr::ui::PanelContentRect(PanelBounds());
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    layout.list = {content.x, layout.header.y + kHeaderHeight + kSectionGap, content.width,
                   kListHeight};
    layout.footer = {content.x, layout.list.y + kListHeight + kSectionGap, content.width,
                     kFooterHeight};

    const float stepperY = layout.footer.y + (kFooterHeight - kStepperButtonSize) / 2.0f;
    layout.qtyMinus = {layout.footer.x + kQtyLabelWidth, stepperY, kStepperButtonSize,
                       kStepperButtonSize};
    layout.qtyValue = {layout.qtyMinus.x + kStepperButtonSize + 6.0f, stepperY, 30.0f,
                       kStepperButtonSize};
    layout.qtyPlus = {layout.qtyValue.x + layout.qtyValue.width + 6.0f, stepperY,
                      kStepperButtonSize, kStepperButtonSize};
    layout.jettison = {layout.footer.x + layout.footer.width - kJettisonButtonWidth,
                       layout.footer.y, kJettisonButtonWidth, kFooterHeight};
    return layout;
}

float RowHeight(const GroupedEntry& entry) {
    return entry.isHeader ? kGroupHeaderHeight : kItemRowHeight;
}

float GroupedContentHeight(const std::vector<GroupedEntry>& rows) {
    float total = 0.0f;
    for (const GroupedEntry& row : rows) {
        total += RowHeight(row);
    }
    return total;
}

// Pure -- clamps `offset` into [0, maxScroll]; maxScroll is zero (so this always returns zero)
// once every row already fits within `listHeight`. Mirrors ModulesMenu.cpp's own ClampScroll,
// generalised for GroupedRows' mixed header/item row heights.
float ClampStorageScroll(const std::vector<GroupedEntry>& rows, float listHeight, float offset) {
    const float maxScroll = std::max(0.0f, GroupedContentHeight(rows) - listHeight);
    return std::clamp(offset, 0.0f, maxScroll);
}

// Pure -- the GroupedRows index under `cursor` given a view scrolled by `scrollOffset`, or
// nullopt if `cursor` is outside `bounds` or past the last row. The caller checks `isHeader`
// before treating the hit as a selection -- a header row still occupies space in the list, it
// just is not clickable.
std::optional<std::size_t> GroupedRowAt(const std::vector<GroupedEntry>& rows, Rectangle bounds,
                                        float scrollOffset, Vector2 cursor) {
    if (!CheckCollisionPointRec(cursor, bounds)) {
        return std::nullopt;
    }
    float y = bounds.y - scrollOffset;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float h = RowHeight(rows[i]);
        if (cursor.y >= y && cursor.y < y + h) {
            return i;
        }
        y += h;
    }
    return std::nullopt;
}

// The live stack `state`'s selection names, or nullptr if it no longer resolves -- jettisoned to
// zero and erased since it was picked, a different rig now active, or nothing selected at all.
// Matched by (hardpoint, kind, id) rather than a row index; see FlightOverlayState's own comment
// on why.
const ItemStack* FindSelectedStack(const std::vector<ItemStack>& stacks,
                                   const FlightOverlayState& state) {
    if (state.selectedHardpoint == entt::null) {
        return nullptr;
    }
    for (const ItemStack& stack : stacks) {
        if (stack.hardpoint == state.selectedHardpoint && stack.kind == state.selectedKind &&
            stack.id == state.selectedId) {
            return &stack;
        }
    }
    return nullptr;
}

// One-letter monogram per ModuleKind (features.md 3.9's glyph-carries-identity rule) -- the same
// placeholder ModulesMenu.cpp's own ModuleGlyph uses; duplicated locally rather than shared,
// since neither file may depend on the other and this is the whole of it.
void ModuleGlyph(ModuleKind kind, char (&out)[3]) {
    switch (kind) {
        case ModuleKind::Weapon: out[0] = 'W'; break;
        case ModuleKind::ShieldGenerator: out[0] = 'S'; break;
        case ModuleKind::PowerCell: out[0] = 'P'; break;
        case ModuleKind::Engine: out[0] = 'E'; break;
        case ModuleKind::Armor: out[0] = 'A'; break;
        case ModuleKind::Facility: out[0] = 'F'; break;
        case ModuleKind::Sensor: out[0] = 'R'; break;
        case ModuleKind::CargoBay: out[0] = 'C'; break;
        case ModuleKind::FireControl: out[0] = 'X'; break;
        case ModuleKind::Hyperdrive: out[0] = 'H'; break;
        case ModuleKind::Crew: out[0] = 'K'; break;
    }
    out[1] = '\0';
    out[2] = '\0';
}

struct RowText {
    std::string label;
    char glyph[3] = {'\0', '\0', '\0'};
};

// An element's glyph is its own id -- already a periodic abbreviation ("Fe", "Ir", "Xe"), per
// the Materials System's own convention -- and its label is ContentLibrary's displayName ("Iron")
// once resolved. A module's glyph is ModuleGlyph of its ModuleDef::kind. Both fall back to the
// raw id as the label if `content` has nothing registered under it (a crafted or removed-from-
// content item), the same fallback ModulesMenu.cpp's own row builders use.
RowText DescribeStack(const core::ContentLibrary& content, const ItemStack& stack) {
    RowText text;
    text.label = stack.id;
    if (stack.kind == ItemKind::Element) {
        if (const ElementDef* def = content.FindElement(ElementId(stack.id))) {
            text.label = def->displayName;
        }
        text.glyph[0] = !stack.id.empty() ? stack.id[0] : '\0';
        text.glyph[1] = stack.id.size() > 1 ? stack.id[1] : '\0';
    } else if (const ModuleDef* def = content.FindModule(ModuleId(stack.id))) {
        text.label = def->displayName;
        ModuleGlyph(def->kind, text.glyph);
    }
    return text;
}

}  // namespace

std::vector<GroupedEntry> GroupedRows(const std::vector<ItemStack>& stacks) {
    std::vector<ItemStack> elements;
    std::vector<ItemStack> modules;
    for (const ItemStack& stack : stacks) {
        (stack.kind == ItemKind::Element ? elements : modules).push_back(stack);
    }

    std::vector<GroupedEntry> rows;
    if (!elements.empty()) {
        rows.push_back(GroupedEntry{true, "ELEMENTS", {}});
        for (const ItemStack& stack : elements) {
            rows.push_back(GroupedEntry{false, "", stack});
        }
    }
    if (!modules.empty()) {
        rows.push_back(GroupedEntry{true, "MODULES", {}});
        for (const ItemStack& stack : modules) {
            rows.push_back(GroupedEntry{false, "", stack});
        }
    }
    return rows;
}

bool IsOpen(const entt::registry& registry) {
    const entt::entity singleton = FindSingleton(registry);
    return singleton != entt::null && registry.get<FlightOverlayState>(singleton).inventoryOpen;
}

void Update(entt::registry& registry, entt::entity rigRoot) {
    if (IsKeyPressed(kToggleKey)) {
        FlightOverlayState& state = registry.get<FlightOverlayState>(EnsureSingleton(registry));
        state.inventoryOpen = !state.inventoryOpen;
    }

    if (!IsOpen(registry) || rigRoot == entt::null) {
        return;
    }

    const Layout layout = ComputeLayout();
    FlightOverlayState& state = registry.get<FlightOverlayState>(EnsureSingleton(registry));
    const std::vector<ItemStack> stacks = cargo_view::Merged(registry, rigRoot);
    const std::vector<GroupedEntry> rows = GroupedRows(stacks);

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (input.scroll != 0.0f && CheckCollisionPointRec(input.cursor, layout.list)) {
        state.storageScroll -= input.scroll * kItemRowHeight;
    }
    state.storageScroll = ClampStorageScroll(rows, layout.list.height, state.storageScroll);

    // Re-validated every frame, not just after a jettison: the selected stack can vanish out from
    // under the selection by any route that mutates cargo (LootSystem picking up a merge, another
    // client in a shared session, and so on), not only this screen's own JettisonRequest.
    if (FindSelectedStack(stacks, state) == nullptr) {
        state.selectedHardpoint = entt::null;
    }

    if (!input.clicked) {
        return;
    }

    if (const std::optional<std::size_t> hit =
            GroupedRowAt(rows, layout.list, state.storageScroll, input.cursor);
        hit.has_value() && !rows[*hit].isHeader) {
        const ItemStack& stack = rows[*hit].stack;
        state.selectedHardpoint = stack.hardpoint;
        state.selectedKind = stack.kind;
        state.selectedId = stack.id;
        state.jettisonQuantity = 1;
        return;
    }

    const ItemStack* selected = FindSelectedStack(stacks, state);
    if (selected == nullptr) {
        return;
    }

    if (sr::ui::ButtonClicked(layout.qtyMinus, input)) {
        state.jettisonQuantity = std::max(1, state.jettisonQuantity - 1);
    } else if (sr::ui::ButtonClicked(layout.qtyPlus, input)) {
        state.jettisonQuantity = std::min(selected->quantity, state.jettisonQuantity + 1);
    } else if (sr::ui::ButtonClicked(layout.jettison, input)) {
        const int quantity = std::clamp(state.jettisonQuantity, 1, selected->quantity);
        registry.emplace_or_replace<JettisonRequest>(
            rigRoot, JettisonRequest{selected->kind, selected->id, quantity});
        state.selectedHardpoint = entt::null;
        state.jettisonQuantity = 1;
    }
}

namespace {

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

// "CARGO HOLD" over a divider, with the right-aligned two-line stat block the reference names:
// item-slot capacity bold and bright, total mass dim underneath it (architecture.md 12.30.7's
// layout table -- "Capacity 34/50 · total mass," the gap this issue's own header names).
void DrawHeader(Rectangle header, const sr::ui::Fonts& fonts, int slotsUsed, int slotsTotal,
                float mass) {
    DrawTextEx(fonts.heading, "CARGO HOLD", {header.x, header.y}, 20.0f, 1.0f,
               sr::ui::kValueBright);

    const std::string slots = std::to_string(slotsUsed) + " / " + std::to_string(slotsTotal);
    const std::string massLine = "MASS " + std::to_string(static_cast<int>(mass)) + " KG";
    const float slotsWidth = MeasureTextEx(fonts.heading, slots.c_str(), 20.0f, 1.0f).x;
    const float massWidth = MeasureTextEx(fonts.body, massLine.c_str(), 13.0f, 1.0f).x;
    DrawTextEx(fonts.heading, slots.c_str(), {header.x + header.width - slotsWidth, header.y},
               20.0f, 1.0f, sr::ui::kValueBright);
    DrawTextEx(fonts.body, massLine.c_str(),
               {header.x + header.width - massWidth, header.y + 24.0f}, 13.0f, 1.0f,
               sr::ui::kLabelDim);

    const float dividerY = header.y + header.height;
    DrawLineEx({header.x, dividerY}, {header.x + header.width, dividerY}, 1.0f, sr::ui::kDivider);
}

// A dim caption over a faint fill -- "ELEMENTS" / "MODULES" -- reading as a section divider
// rather than a fourth row style competing with the item rows around it.
void DrawGroupHeader(Rectangle bounds, const sr::ui::Fonts& fonts, const std::string& label) {
    DrawRectangleRec(bounds, sr::ui::kPanelGlass);
    DrawTextEx(fonts.body, label.c_str(), {bounds.x + 4.0f, bounds.y + bounds.height / 2.0f - 6.0f},
               11.0f, 1.0f, sr::ui::kLabelDim);
}

// One cargo row: a bordered icon chip (glyph carries identity), the resolved display name, and
// the stack's quantity right-aligned -- selected rows get a left accent bar and a tinted
// background, mirroring the reference's highlighted "Iridium" row.
void DrawStackRow(Rectangle bounds, const sr::ui::Fonts& fonts, const core::ContentLibrary& content,
                  const ItemStack& stack, bool selected) {
    if (selected) {
        DrawRectangleRec(bounds, Color{sr::ui::kPanelChrome.r, sr::ui::kPanelChrome.g,
                                       sr::ui::kPanelChrome.b, 40});
        DrawRectangleRec({bounds.x, bounds.y, 3.0f, bounds.height}, sr::ui::kStatusGood);
    }

    const RowText text = DescribeStack(content, stack);
    const Rectangle iconBox{bounds.x + 12.0f, bounds.y + (bounds.height - kIconBoxSize) / 2.0f,
                            kIconBoxSize, kIconBoxSize};
    DrawRectangleLinesEx(iconBox, 1.0f, sr::ui::kPanelChrome);
    if (text.glyph[0] != '\0') {
        const Vector2 glyphSize = MeasureTextEx(fonts.heading, text.glyph, 12.0f, 1.0f);
        DrawTextEx(fonts.heading, text.glyph,
                   {iconBox.x + (iconBox.width - glyphSize.x) / 2.0f,
                    iconBox.y + (iconBox.height - glyphSize.y) / 2.0f},
                   12.0f, 1.0f, sr::ui::kPanelChrome);
    }

    const float textX = iconBox.x + iconBox.width + 12.0f;
    DrawTextEx(fonts.heading, text.label.c_str(), {textX, bounds.y + bounds.height / 2.0f - 8.0f},
               14.0f, 1.0f, sr::ui::kValueBright);

    const std::string value = std::to_string(stack.quantity);
    const float valueWidth = MeasureTextEx(fonts.body, value.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(
        fonts.body, value.c_str(),
        {bounds.x + bounds.width - 12.0f - valueWidth, bounds.y + bounds.height / 2.0f - 7.0f},
        14.0f, 1.0f, sr::ui::kLabelDim);
}

void DrawGroupedRows(Rectangle bounds, const sr::ui::Fonts& fonts,
                     const core::ContentLibrary& content, const std::vector<GroupedEntry>& rows,
                     float scrollOffset, const ItemStack* selected) {
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                     static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    if (rows.empty()) {
        DrawTextEx(fonts.body, "NO ITEMS IN HOLD", {bounds.x, bounds.y}, 13.0f, 1.0f,
                   sr::ui::kLabelDim);
        EndScissorMode();
        return;
    }

    float y = bounds.y - scrollOffset;
    for (const GroupedEntry& entry : rows) {
        const float h = RowHeight(entry);
        if (y + h >= bounds.y && y <= bounds.y + bounds.height) {
            const Rectangle rowBounds{bounds.x, y, bounds.width, h};
            if (entry.isHeader) {
                DrawGroupHeader(rowBounds, fonts, entry.headerLabel);
            } else {
                const bool isSelected =
                    selected != nullptr && entry.stack.hardpoint == selected->hardpoint &&
                    entry.stack.kind == selected->kind && entry.stack.id == selected->id;
                DrawStackRow(rowBounds, fonts, content, entry.stack, isSelected);
            }
        }
        y += h;
    }
    EndScissorMode();
}

// A thin chrome-coloured thumb along the list's right edge once its rows outgrow `bounds` --
// mirrors ModulesMenu.cpp's own DrawScrollbar.
void DrawScrollbar(Rectangle bounds, const std::vector<GroupedEntry>& rows, float scrollOffset) {
    const float contentHeight = GroupedContentHeight(rows);
    const float maxScroll = contentHeight - bounds.height;
    if (maxScroll <= 0.0f) {
        return;
    }
    constexpr float kThumbWidth = 3.0f;
    const Rectangle track{bounds.x + bounds.width - kThumbWidth, bounds.y, kThumbWidth,
                          bounds.height};
    const float thumbHeight = std::max(16.0f, bounds.height * (bounds.height / contentHeight));
    const float thumbY = track.y + (bounds.height - thumbHeight) * (scrollOffset / maxScroll);
    DrawRectangleRec(track, sr::ui::kPanelGlass);
    DrawRectangleRec({track.x, thumbY, track.width, thumbHeight}, sr::ui::kPanelChrome);
}

// QTY label, -/+ stepper, and the JETTISON button -- all dimmed and inert (ButtonClicked never
// fires against them; Update() already guards every branch on `selected != nullptr`) while
// nothing is selected, matching the reference's own greyed-out footer in that state.
void DrawFooter(const Layout& layout, const sr::ui::Fonts& fonts, const ItemStack* selected,
                int jettisonQuantity) {
    const bool active = selected != nullptr;
    const Color chrome = active ? sr::ui::kPanelChrome : sr::ui::kLabelDim;
    const Color text = active ? sr::ui::kValueBright : sr::ui::kLabelDim;

    DrawTextEx(fonts.body, "QTY", {layout.footer.x, layout.footer.y + kFooterHeight / 2.0f - 6.0f},
               12.0f, 1.0f, sr::ui::kLabelDim);

    sr::ui::DrawChamferedButton(layout.qtyMinus, "-", fonts.body, 13.0f, sr::ui::kPanelGlass,
                                chrome, text);
    const int shownQuantity = active ? std::clamp(jettisonQuantity, 1, selected->quantity) : 0;
    const std::string quantityText = std::to_string(shownQuantity);
    const float quantityWidth = MeasureTextEx(fonts.body, quantityText.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(fonts.body, quantityText.c_str(),
               {layout.qtyValue.x + (layout.qtyValue.width - quantityWidth) / 2.0f,
                layout.qtyValue.y + 3.0f},
               14.0f, 1.0f, text);
    sr::ui::DrawChamferedButton(layout.qtyPlus, "+", fonts.body, 13.0f, sr::ui::kPanelGlass, chrome,
                                text);

    sr::ui::DrawChamferedButton(layout.jettison, "JETTISON", fonts.body, 13.0f, sr::ui::kPanelGlass,
                                chrome, text);
}

}  // namespace

void Draw(const entt::registry& registry, entt::entity rigRoot, const core::ContentLibrary& content,
          const sr::ui::Fonts& fonts) {
    if (!IsOpen(registry) || rigRoot == entt::null) {
        return;
    }

    const Layout layout = ComputeLayout();
    sr::ui::DrawPanelFrame(PanelBounds());

    const std::vector<ItemStack> stacks = cargo_view::Merged(registry, rigRoot);
    const std::vector<GroupedEntry> rows = GroupedRows(stacks);

    const entt::entity singleton = FindSingleton(registry);
    const FlightOverlayState state = singleton != entt::null
                                         ? registry.get<FlightOverlayState>(singleton)
                                         : FlightOverlayState{};
    const ItemStack* selected = FindSelectedStack(stacks, state);

    DrawHeader(layout.header, fonts, cargo_view::SlotsUsed(registry, rigRoot),
               cargo_view::SlotsTotal(registry, rigRoot), cargo_view::TotalMass(registry, rigRoot));
    DrawGroupedRows(layout.list, fonts, content, rows, state.storageScroll, selected);
    DrawScrollbar(layout.list, rows, state.storageScroll);
    DrawFooter(layout, fonts, selected, state.jettisonQuantity);
}

}  // namespace sr::space::ui::storage_menu
