#include "modes/space/ui/StorageScreen.h"

#include <raylib.h>

#include <optional>
#include <string>

#include "modes/space/ui/BridgeView.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::storage_screen {
namespace {

constexpr float kPanelWidth = 640.0f;
constexpr float kPanelTop = 520.0f;  // Below BayView's panel -- both are visible while docked.
constexpr float kHeaderHeight = 44.0f;
constexpr float kListHeight = 220.0f;
constexpr float kColumnGap = 12.0f;

bool HasCargoHold(const entt::registry& registry, entt::entity station) {
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<CargoHold>(child) && !registry.all_of<Destroyed>(child)) {
            return true;
        }
    }
    return false;
}

struct Layout {
    Rectangle header{};
    Rectangle left{};
    Rectangle right{};
};

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float height = kHeaderHeight + kListHeight + 2.0f * sr::ui::kPanelPadding;
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth, height};
}

Layout ComputeLayout(Rectangle bounds) {
    const Rectangle content = sr::ui::PanelContentRect(bounds);
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    const float columnWidth = (content.width - kColumnGap) * 0.5f;
    const float y = content.y + kHeaderHeight;
    layout.left = {content.x, y, columnWidth, kListHeight};
    layout.right = {content.x + columnWidth + kColumnGap, y, columnWidth, kListHeight};
    return layout;
}

std::string FormatMass(float mass) {
    return std::to_string(static_cast<int>(mass));
}

}  // namespace

std::vector<StorageRow> Rows(const entt::registry& registry, entt::entity rigRoot,
                             entt::entity destination) {
    std::vector<StorageRow> rows;
    const float destCapacity = cargo_view::Capacity(registry, destination);
    const float destUsed = cargo_view::TotalMass(registry, destination);
    const float destRoom = destCapacity - destUsed;

    for (const ItemStack& stack : cargo_view::Merged(registry, rigRoot)) {
        StorageRow entry;
        entry.kind = stack.kind;
        entry.id = stack.id;
        entry.quantity = stack.quantity;
        entry.unitMass = stack.unitMass;
        entry.fits = destRoom >= static_cast<float>(stack.quantity) * stack.unitMass;

        entry.row.label = stack.id;
        entry.row.value = std::to_string(stack.quantity);
        entry.row.style.disabled = !entry.fits;
        if (!entry.fits) {
            entry.row.value += "  FULL";
        }
        rows.push_back(std::move(entry));
    }
    return rows;
}

TransferItemRequest BuildDepositRequest(const StorageRow& row) {
    TransferItemRequest request;
    request.kind = row.kind;
    request.id = row.id;
    request.quantity = row.quantity;
    request.toStation = true;
    return request;
}

TransferItemRequest BuildWithdrawRequest(const StorageRow& row) {
    TransferItemRequest request = BuildDepositRequest(row);
    request.toStation = false;
    return request;
}

entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction) {
    for (auto [vessel, docked, faction] : registry.view<Docked, FactionRef>().each()) {
        if (docked.station == station && faction.id == playerFaction) {
            return vessel;
        }
    }
    return entt::null;
}

entt::entity ActiveStation(const entt::registry& registry, const FactionId& playerFaction) {
    const entt::entity station = bridge_view::DockedStation(registry);
    if (station == entt::null || !HasCargoHold(registry, station)) {
        return entt::null;
    }
    const FactionRef* faction = registry.try_get<FactionRef>(station);
    if (faction == nullptr || faction->id != playerFaction) {
        return entt::null;  // architecture.md 12.30.3: Deposit/Withdraw only within one owner.
    }
    return station;
}

void Update(entt::registry& registry, const FactionId& playerFaction) {
    const entt::entity station = ActiveStation(registry, playerFaction);
    if (station == entt::null) {
        return;
    }
    const entt::entity vessel = OwnedVesselAt(registry, station, playerFaction);
    if (vessel == entt::null) {
        return;
    }

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const Layout layout = ComputeLayout(PanelBounds());

    const std::vector<StorageRow> yours = Rows(registry, vessel, station);
    if (const std::optional<int> hit =
            sr::ui::ListViewRowAt(layout.left, static_cast<int>(yours.size()), 0.0f, input.cursor);
        hit.has_value() && *hit < static_cast<int>(yours.size())) {
        const StorageRow& row = yours[static_cast<std::size_t>(*hit)];
        if (row.fits) {
            registry.emplace_or_replace<TransferItemRequest>(vessel, BuildDepositRequest(row));
        }
        return;
    }

    const std::vector<StorageRow> theirs = Rows(registry, station, vessel);
    if (const std::optional<int> hit = sr::ui::ListViewRowAt(
            layout.right, static_cast<int>(theirs.size()), 0.0f, input.cursor);
        hit.has_value() && *hit < static_cast<int>(theirs.size())) {
        const StorageRow& row = theirs[static_cast<std::size_t>(*hit)];
        if (row.fits) {
            registry.emplace_or_replace<TransferItemRequest>(vessel, BuildWithdrawRequest(row));
        }
    }
}

void Draw(const entt::registry& registry, const FactionId& playerFaction) {
    const entt::entity station = ActiveStation(registry, playerFaction);
    if (station == entt::null) {
        return;
    }
    const entt::entity vessel = OwnedVesselAt(registry, station, playerFaction);
    if (vessel == entt::null) {
        return;  // No owned hull docked here yet to trade with.
    }

    const Layout layout = ComputeLayout(PanelBounds());
    sr::ui::DrawPanelFrame(PanelBounds());

    std::string stationName = "STATION";
    if (const DisplayName* name = registry.try_get<DisplayName>(station)) {
        stationName = name->value;
    }
    DrawText(stationName.c_str(), static_cast<int>(layout.header.x),
             static_cast<int>(layout.header.y), 18, sr::ui::kValueBright);

    const std::string subtitle =
        "YOUR HOLD " + FormatMass(cargo_view::TotalMass(registry, vessel)) + "/" +
        FormatMass(cargo_view::Capacity(registry, vessel)) + "    STATION HOLD " +
        FormatMass(cargo_view::TotalMass(registry, station)) + "/" +
        FormatMass(cargo_view::Capacity(registry, station));
    DrawText(subtitle.c_str(), static_cast<int>(layout.header.x),
             static_cast<int>(layout.header.y + 20.0f), 14, sr::ui::kLabelDim);

    const std::vector<StorageRow> yours = Rows(registry, vessel, station);
    std::vector<sr::ui::Row> yourRows;
    yourRows.reserve(yours.size());
    for (const StorageRow& entry : yours) {
        yourRows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.left, yourRows, 0.0f, "HOLD EMPTY");

    const std::vector<StorageRow> theirs = Rows(registry, station, vessel);
    std::vector<sr::ui::Row> theirRows;
    theirRows.reserve(theirs.size());
    for (const StorageRow& entry : theirs) {
        theirRows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.right, theirRows, 0.0f, "STATION HOLD EMPTY");
}

}  // namespace sr::space::ui::storage_screen
