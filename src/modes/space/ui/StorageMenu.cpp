#include "modes/space/ui/StorageMenu.h"

#include <raylib.h>

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

constexpr float kPanelWidth = 420.0f;
constexpr float kPanelTop = 80.0f;
constexpr float kHeaderHeight = 36.0f;
constexpr float kListHeight = 220.0f;

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

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth,
                     kHeaderHeight + kListHeight};
}

}  // namespace

std::vector<std::string> Rows(const std::vector<ItemStack>& stacks) {
    std::vector<std::string> rows;
    rows.reserve(stacks.size());
    for (const ItemStack& stack : stacks) {
        if (stack.kind == ItemKind::Module) {
            rows.push_back(stack.id);
        }
    }
    for (const ItemStack& stack : stacks) {
        if (stack.kind == ItemKind::Element) {
            rows.push_back(stack.id + " x" + std::to_string(stack.quantity));
        }
    }
    return rows;
}

std::vector<ItemStack> OrderedStacks(const std::vector<ItemStack>& stacks) {
    std::vector<ItemStack> ordered;
    ordered.reserve(stacks.size());
    for (const ItemStack& stack : stacks) {
        if (stack.kind == ItemKind::Module) {
            ordered.push_back(stack);
        }
    }
    for (const ItemStack& stack : stacks) {
        if (stack.kind == ItemKind::Element) {
            ordered.push_back(stack);
        }
    }
    return ordered;
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

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const std::vector<ItemStack> ordered = OrderedStacks(cargo_view::Merged(registry, rigRoot));
    const Rectangle content = sr::ui::PanelContentRect(PanelBounds());
    const Rectangle list{content.x, content.y + kHeaderHeight, content.width, kListHeight};
    const std::optional<int> hit =
        sr::ui::ListViewRowAt(list, static_cast<int>(ordered.size()), 0.0f, input.cursor);
    if (!hit.has_value() || *hit >= static_cast<int>(ordered.size())) {
        return;
    }

    const ItemStack& stack = ordered[static_cast<std::size_t>(*hit)];
    registry.emplace_or_replace<JettisonRequest>(
        rigRoot, JettisonRequest{stack.kind, stack.id, stack.quantity});
}

void Draw(const entt::registry& registry, entt::entity rigRoot) {
    if (!IsOpen(registry) || rigRoot == entt::null) {
        return;
    }

    const Rectangle content = sr::ui::DrawPanelFrame(PanelBounds());
    const std::vector<ItemStack> stacks = cargo_view::Merged(registry, rigRoot);

    const float mass = cargo_view::TotalMass(registry, rigRoot);
    const float capacity = cargo_view::Capacity(registry, rigRoot);
    const std::string header = "MASS " + std::to_string(static_cast<int>(mass)) + " / " +
                               std::to_string(static_cast<int>(capacity));
    DrawText(header.c_str(), static_cast<int>(content.x), static_cast<int>(content.y), 16,
             sr::ui::kValueBright);
    DrawText("CLICK A ROW TO JETTISON", static_cast<int>(content.x),
             static_cast<int>(content.y + 18.0f), 12, sr::ui::kLabelDim);

    std::vector<sr::ui::Row> rows;
    for (const ItemStack& stack : OrderedStacks(stacks)) {
        sr::ui::Row row;
        row.label = stack.id;
        row.value = stack.kind == ItemKind::Element ? ("x" + std::to_string(stack.quantity)) : "";
        rows.push_back(row);
    }
    const Rectangle list{content.x, content.y + kHeaderHeight, content.width, kListHeight};
    sr::ui::DrawListView(list, rows, 0.0f, "NO ITEMS IN HOLD");
}

}  // namespace sr::space::ui::storage_menu
