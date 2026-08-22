#include "modes/space/ui/AvionicsMenu.h"

#include <raylib.h>

#include <algorithm>
#include <array>

#include "modes/space/ui/BayView.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::avionics_menu {
namespace {

// architecture.md 12.24 step 2: moved off KEY_E now that E is strafe-right (features.md 3.6) --
// the two would otherwise fight over one key every time the player docks while moving.
constexpr int kDockKey = KEY_R;
constexpr float kPromptFontSize = 20.0f;
constexpr float kPromptMarginBottom = 56.0f;  // Sits just above CockpitHud's hull bar.

struct PowerKey {
    int key;
    PowerCategory category;
};

// features.md section 2.9's F/G/H/J -- weapons, shields, engines, facilities.
constexpr std::array<PowerKey, 4> kPowerKeys{{
    {KEY_F, PowerCategory::Weapons},
    {KEY_G, PowerCategory::Shields},
    {KEY_H, PowerCategory::Engines},
    {KEY_J, PowerCategory::Facilities},
}};

// Longer than this and a press reads as a hold (toggle Offline) rather than a tap (toggle
// Boosted).
constexpr float kHoldThresholdSeconds = 0.35f;

// Per-category key-hold tracking for the tap/hold gesture split. Presentation-layer input
// debounce, not simulation state -- it never needs to survive a save or replay deterministically,
// the same reasoning that already lets this file poll raylib directly rather than going through
// Law 9's IntentQueue.
std::array<float, kPowerKeys.size()> gHeldSeconds{};
std::array<bool, kPowerKeys.size()> gHoldFired{};

PowerLevel& LevelFor(PowerAllocation& allocation, PowerCategory category) {
    switch (category) {
        case PowerCategory::Weapons: return allocation.weapons;
        case PowerCategory::Shields: return allocation.shields;
        case PowerCategory::Engines: return allocation.engines;
        case PowerCategory::Facilities: return allocation.facilities;
    }
    return allocation.weapons;
}

// Reduced is never commanded directly (features.md 2.9) -- toggling either affordance always
// lands on Boosted, Offline, or back to Normal, never Reduced.
void ToggleLevel(PowerLevel& level, PowerLevel target) {
    level = (level == target) ? PowerLevel::Normal : target;
}

// Tap toggles Boosted; hold toggles Offline (features.md 2.9). Shift+tap instead moves that
// category to the end of PowerPriorityList's shed order -- protects it, sheds last. A minimal,
// keyboard-only stand-in for "the interesting configuration happens at leisure in a menu": the
// priority list itself is the tested mechanic (PowerSystem reads it), this is just one way to
// write to it.
void UpdatePower(entt::registry& registry, entt::entity player) {
    PowerAllocation& allocation = registry.get_or_emplace<PowerAllocation>(player);

    for (size_t i = 0; i < kPowerKeys.size(); ++i) {
        const PowerKey& powerKey = kPowerKeys[i];

        if (IsKeyPressed(powerKey.key) &&
            (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) {
            PowerPriorityList& priority = registry.get_or_emplace<PowerPriorityList>(player);
            auto& order = priority.order;
            const auto it = std::find(order.begin(), order.end(), powerKey.category);
            if (it != order.end()) {
                std::rotate(it, it + 1, order.end());
            }
            continue;  // Shift+tap configures priority, not boost.
        }

        if (IsKeyDown(powerKey.key)) {
            gHeldSeconds[i] += GetFrameTime();
            if (!gHoldFired[i] && gHeldSeconds[i] >= kHoldThresholdSeconds) {
                gHoldFired[i] = true;
                ToggleLevel(LevelFor(allocation, powerKey.category), PowerLevel::Offline);
            }
        } else {
            if (gHeldSeconds[i] > 0.0f && !gHoldFired[i]) {
                ToggleLevel(LevelFor(allocation, powerKey.category), PowerLevel::Boosted);
            }
            gHeldSeconds[i] = 0.0f;
            gHoldFired[i] = false;
        }
    }

    // Ctrl held is the momentary engine afterburner, tapping H the sustained toggle -- "two
    // affordances onto one state" (features.md 2.9). Momentary: forces Boosted every frame held
    // and back to Normal the instant it releases, regardless of whatever H had independently set
    // -- the exact interaction between the two is explicitly left open in the spec.
    static bool wasCtrlDown = false;
    const bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrlDown) {
        allocation.engines = PowerLevel::Boosted;
    } else if (wasCtrlDown) {
        allocation.engines = PowerLevel::Normal;
    }
    wasCtrlDown = ctrlDown;
}

// architecture.md 12.30.2's "R while docked means board and launch": `player` (PlayerLocation's
// entity) standing inside a facility hardpoint is not itself Docked -- the derived
// PlayerControlled is the station (12.30.1) -- so a naive UndockRequest there does nothing. This
// resolves the vessel that shortcut would launch: the player's own hull Docked at the station
// `player`'s ParentRig names, or entt::null if `player` is not a facility hardpoint or no such
// hull exists.
entt::entity ResolveOwnedDockedHull(const entt::registry& registry, entt::entity player,
                                    const FactionId& playerFaction) {
    if (!registry.all_of<FacilityRef>(player)) {
        return entt::null;
    }
    const ParentRig* parent = registry.try_get<ParentRig>(player);
    if (parent == nullptr) {
        return entt::null;
    }
    return space::ui::bay_view::OwnedVesselAt(registry, parent->root, playerFaction);
}

}  // namespace

// PlayerLocation, not PlayerControlled: architecture.md 12.30.1 makes PlayerLocation the sole
// source of truth, and PlayerControlled is derived from it (modes/space/systems/
// PlayerLocationSystem.h) rather than read directly here -- this file only ever needs to know
// where the player is actually standing, which PlayerLocation already answers directly.
void Update(entt::registry& registry, const FactionId& playerFaction) {
    entt::entity player = entt::null;
    for (auto [entity, location] : registry.view<PlayerLocation>().each()) {
        (void)location;
        player = entity;
        break;  // Exactly one PlayerLocation entity.
    }
    if (player == entt::null) {
        return;
    }

    if (IsKeyPressed(kDockKey)) {
        if (const DockPrompt* prompt = registry.try_get<DockPrompt>(player)) {
            registry.emplace_or_replace<DockRequest>(player, prompt->bay);
        } else if (registry.all_of<Docked>(player)) {
            registry.emplace_or_replace<UndockRequest>(player);
        } else if (const entt::entity hull =
                       ResolveOwnedDockedHull(registry, player, playerFaction);
                   hull != entt::null) {
            registry.remove<PlayerLocation>(player);
            registry.emplace<PlayerLocation>(hull, PlayerLocation{hull});
            registry.emplace_or_replace<UndockRequest>(hull);
        }
    }

    UpdatePower(registry, player);
}

void Draw(const entt::registry& registry, const FactionId& playerFaction) {
    for (auto [entity, location] : registry.view<PlayerLocation>().each()) {
        (void)location;
        const char* label = nullptr;
        if (registry.all_of<DockPrompt>(entity)) {
            label = "[R] DOCK";
        } else if (registry.all_of<Docked>(entity)) {
            label = "[R] UNDOCK";
        } else if (ResolveOwnedDockedHull(registry, entity, playerFaction) != entt::null) {
            label = "[R] LAUNCH";
        }
        if (label == nullptr) {
            return;
        }

        const Font font = GetFontDefault();
        const Vector2 textSize = MeasureTextEx(font, label, kPromptFontSize, 1.0f);
        const float screenWidth = static_cast<float>(GetScreenWidth());
        const float screenHeight = static_cast<float>(GetScreenHeight());
        DrawTextEx(font, label,
                   {(screenWidth - textSize.x) * 0.5f, screenHeight - kPromptMarginBottom},
                   kPromptFontSize, 1.0f, sr::ui::kValueBright);
        return;  // Exactly one PlayerLocation entity.
    }
}

}  // namespace sr::space::ui::avionics_menu
