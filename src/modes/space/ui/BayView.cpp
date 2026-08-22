#include "modes/space/ui/BayView.h"

#include <raylib.h>

#include <cctype>
#include <optional>
#include <string>

#include "modes/space/ui/CockpitHud.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::bay_view {
namespace {

constexpr float kPanelWidth = 560.0f;
constexpr float kPanelTop = 132.0f;
constexpr float kHeaderHeight = 44.0f;
constexpr float kSiblingStripHeight = 28.0f;
constexpr float kRosterHeight = 260.0f;

// First two uppercased, alphanumeric characters of a BlueprintId -- the same "no per-shell art
// exists yet" placeholder shape RefactorMenu.cpp's ShellGlyph uses.
void BlueprintGlyph(const BlueprintId& id, char (&out)[3]) {
    out[0] = '\0';
    out[1] = '\0';
    out[2] = '\0';
    int written = 0;
    for (const char c : id.str()) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            continue;
        }
        out[written] = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        ++written;
        if (written == 2) {
            break;
        }
    }
}

// The entity PlayerLocation currently names, or entt::null. Exactly one per registry
// (architecture.md 12.30.1).
entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

// The living Docking-kind facility hardpoint PlayerLocation currently names ("this bay"), or
// entt::null when the player is not currently viewing the Bay screen -- flying, docked but
// standing on a different facility hardpoint, or aboard a vessel rather than inside the station.
entt::entity CurrentBay(const entt::registry& registry, entt::entity shell) {
    if (shell == entt::null || registry.all_of<Destroyed>(shell)) {
        return entt::null;
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(shell);
    if (facility == nullptr || facility->kind != FacilityKind::Docking) {
        return entt::null;
    }
    return shell;
}

struct Layout {
    Rectangle content{};
    Rectangle header{};
    Rectangle siblingStrip{};  // Zero height when there is only one bay.
    Rectangle roster{};
};

Layout ComputeLayout(Rectangle bounds, bool showSiblingStrip) {
    const Rectangle content = sr::ui::PanelContentRect(bounds);
    Layout layout;
    layout.content = content;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    float y = content.y + kHeaderHeight;
    if (showSiblingStrip) {
        layout.siblingStrip = {content.x, y, content.width, kSiblingStripHeight};
        y += kSiblingStripHeight;
    }
    layout.roster = {content.x, y, content.width, kRosterHeight};
    return layout;
}

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float height =
        kHeaderHeight + kSiblingStripHeight + kRosterHeight + 2.0f * sr::ui::kPanelPadding;
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth, height};
}

}  // namespace

std::vector<BayRosterEntry> Roster(const entt::registry& registry, entt::entity bay,
                                   entt::entity playerControlled, const FactionId& playerFaction) {
    std::vector<BayRosterEntry> entries;
    for (auto [vessel, docked, faction] : registry.view<Docked, FactionRef>().each()) {
        if (docked.bay != bay) {
            continue;
        }
        BayRosterEntry entry;
        entry.vessel = vessel;
        entry.owned = faction.id == playerFaction;
        entry.occupied = vessel == playerControlled;

        if (const DisplayName* name = registry.try_get<DisplayName>(vessel)) {
            entry.row.label = name->value;
        }
        if (const BlueprintRef* blueprint = registry.try_get<BlueprintRef>(vessel)) {
            BlueprintGlyph(blueprint->id, entry.row.glyph);
        }
        entry.row.style.integrity = cockpit_hud::AggregateHullFraction(registry, vessel);
        if (entry.occupied) {
            entry.row.value = "LAUNCH";
        } else if (entry.owned) {
            entry.row.value = "BOARD";
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<entt::entity> SiblingBays(const entt::registry& registry, entt::entity station) {
    std::vector<entt::entity> bays;
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return bays;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(child);
        if (facility != nullptr && facility->kind == FacilityKind::Docking) {
            bays.push_back(child);
        }
    }
    return bays;
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

void Board(entt::registry& registry, entt::entity vessel) {
    const entt::entity shell = PlayerShell(registry);
    if (shell == entt::null || shell == vessel) {
        return;
    }
    registry.remove<PlayerLocation>(shell);
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});
}

void Launch(entt::registry& registry, entt::entity vessel) {
    registry.emplace_or_replace<UndockRequest>(vessel);
}

void Update(entt::registry& registry, const FactionId& playerFaction) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity thisBay = CurrentBay(registry, shell);
    if (thisBay == entt::null) {
        return;
    }
    const entt::entity station = registry.get<ParentRig>(thisBay).root;
    const entt::entity playerControlled = [&registry]() -> entt::entity {
        for (auto [entity] : registry.view<PlayerControlled>().each()) {
            return entity;
        }
        return entt::null;
    }();

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const std::vector<entt::entity> siblings = SiblingBays(registry, station);
    const Layout layout = ComputeLayout(PanelBounds(), siblings.size() > 1);

    if (siblings.size() > 1) {
        const std::optional<int> hit = sr::ui::TabStripHitTest(
            layout.siblingStrip, static_cast<int>(siblings.size()), input.cursor);
        if (hit.has_value()) {
            const entt::entity target = siblings[static_cast<std::size_t>(*hit)];
            if (target != thisBay) {
                registry.remove<PlayerLocation>(thisBay);
                registry.emplace<PlayerLocation>(target, PlayerLocation{target});
            }
            return;
        }
    }

    const std::vector<BayRosterEntry> roster =
        Roster(registry, thisBay, playerControlled, playerFaction);
    const std::optional<int> hit =
        sr::ui::ListViewRowAt(layout.roster, static_cast<int>(roster.size()), 0.0f, input.cursor);
    if (!hit.has_value() || *hit >= static_cast<int>(roster.size())) {
        return;
    }
    const BayRosterEntry& entry = roster[static_cast<std::size_t>(*hit)];
    if (entry.occupied) {
        Launch(registry, entry.vessel);
    } else if (entry.owned) {
        Board(registry, entry.vessel);
    }
}

void Draw(const entt::registry& registry, const FactionId& playerFaction) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity thisBay = CurrentBay(registry, shell);
    if (thisBay == entt::null) {
        return;
    }
    const entt::entity station = registry.get<ParentRig>(thisBay).root;
    const entt::entity playerControlled = [&registry]() -> entt::entity {
        for (auto [entity] : registry.view<PlayerControlled>().each()) {
            return entity;
        }
        return entt::null;
    }();

    const std::vector<entt::entity> siblings = SiblingBays(registry, station);
    const bool showSiblingStrip = siblings.size() > 1;
    const Layout layout = ComputeLayout(PanelBounds(), showSiblingStrip);
    sr::ui::DrawPanelFrame(PanelBounds());

    // Header: bay name, occupancy, this bay hardpoint's integrity -- features.md 3.4's mandatory
    // per-screen facility-health readout.
    std::string bayName = "BAY";
    if (const DisplayName* stationName = registry.try_get<DisplayName>(station)) {
        bayName = stationName->value;
    }
    int occupied = 0;
    for (auto [vessel, docked] : registry.view<Docked>().each()) {
        (void)vessel;
        if (docked.bay == thisBay) {
            ++occupied;
        }
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(thisBay);
    const int capacity = facility != nullptr ? facility->capacity : 0;
    const std::string occupancy =
        capacity == 0 ? (std::to_string(occupied) + " / UNLIMITED")
                      : (std::to_string(occupied) + " / " + std::to_string(capacity));
    DrawText(bayName.c_str(), static_cast<int>(layout.header.x), static_cast<int>(layout.header.y),
             18, sr::ui::kValueBright);
    DrawText(occupancy.c_str(), static_cast<int>(layout.header.x),
             static_cast<int>(layout.header.y + 20.0f), 14, sr::ui::kLabelDim);

    float integrityFraction = 1.0f;
    if (const Health* health = registry.try_get<Health>(thisBay);
        health != nullptr && health->max > 0.0f) {
        integrityFraction = health->current / health->max;
    }
    const Rectangle gaugeBounds{layout.header.x + layout.header.width * 0.5f, layout.header.y,
                                layout.header.width * 0.5f, 20.0f};
    const Color gaugeColor = integrityFraction > 0.5f   ? sr::ui::kStatusGood
                             : integrityFraction > 0.2f ? sr::ui::kStatusCaution
                                                        : sr::ui::kStatusCritical;
    sr::ui::DrawGauge(gaugeBounds, "BAY INTEGRITY", integrityFraction, gaugeColor);

    // Sibling selector: one TabStrip entry per living Docking hardpoint on the host, absent when
    // there is only one bay.
    if (showSiblingStrip) {
        std::vector<std::string> labels;
        labels.reserve(siblings.size());
        int selected = -1;
        for (std::size_t i = 0; i < siblings.size(); ++i) {
            labels.push_back("BAY " + std::to_string(i + 1));
            if (siblings[i] == thisBay) {
                selected = static_cast<int>(i);
            }
        }
        sr::ui::DrawTabStrip(layout.siblingStrip, labels, selected);
    }

    // Roster: view<Docked> filtered on docked.bay == thisBay. Your own vessel is a row like any
    // other, marked via `value` ("LAUNCH" if you occupy it, "BOARD" if you own it and don't).
    const std::vector<BayRosterEntry> roster =
        Roster(registry, thisBay, playerControlled, playerFaction);
    std::vector<sr::ui::Row> rows;
    rows.reserve(roster.size());
    for (const BayRosterEntry& entry : roster) {
        rows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.roster, rows, 0.0f, "BAY EMPTY");
}

}  // namespace sr::space::ui::bay_view
