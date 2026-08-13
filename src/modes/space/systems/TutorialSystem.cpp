#include "modes/space/systems/TutorialSystem.h"

#include <algorithm>

#include "shared/components/Docking.h"
#include "shared/components/Loot.h"
#include "shared/components/Mining.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/components/Tutorial.h"
#include "shared/rig/CargoView.h"

namespace sr::space::tutorial_system {
namespace {

// Ported verbatim from legacy StarReach2's kTutorialMoveDistance.
constexpr float kTutorialMoveDistance = 300.0f;

void Advance(Tutorial& tutorial) {
    tutorial.step = static_cast<TutorialStep>(static_cast<int>(tutorial.step) + 1);
}

void TickPerRigSteps(entt::registry& registry) {
    for (auto [self, tutorial, xf] : registry.view<Tutorial, WorldTransform>().each()) {
        if (!tutorial.started) {
            tutorial.startPosition = xf.position;
            tutorial.started = true;
        }

        switch (tutorial.step) {
            case TutorialStep::Move:
                if (Distance(xf.position, tutorial.startPosition) >= kTutorialMoveDistance) {
                    Advance(tutorial);
                }
                break;
            case TutorialStep::Target: {
                const auto* target = registry.try_get<Target>(self);
                if (target != nullptr && target->rig != entt::null) {
                    Advance(tutorial);
                }
                break;
            }
            case TutorialStep::CollectElement: {
                // Merged across every living cargo bay, not a single root-level CargoHold --
                // P0-10 moved the hold onto the bay (architecture.md 12.23).
                const std::vector<ItemStack> merged = cargo_view::Merged(registry, self);
                const bool hasElement = std::any_of(
                    merged.begin(), merged.end(),
                    [](const ItemStack& stack) { return stack.kind == ItemKind::Element; });
                if (hasElement) {
                    Advance(tutorial);
                }
                break;
            }
            case TutorialStep::Dock:
                if (registry.all_of<Docked>(self)) {
                    Advance(tutorial);
                }
                break;
            default: break;  // DestroyAsteroid handled separately; Sell/EquipModule/Warp deferred.
        }
    }
}

// Ported from legacy StarReach2, which advanced on ANY asteroid destruction rather than
// attributing the kill to the tutorial rig specifically. Must run before MiningSystem, which
// destroys the entity outright the same tick DamageSystem tags it Destroyed.
void TickDestroyAsteroid(entt::registry& registry) {
    const auto destroyedAsteroids = registry.view<Asteroid, Destroyed>();
    if (destroyedAsteroids.begin() == destroyedAsteroids.end()) {
        return;
    }
    for (auto [self, tutorial] : registry.view<Tutorial>().each()) {
        if (tutorial.step == TutorialStep::DestroyAsteroid) {
            Advance(tutorial);
        }
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    TickPerRigSteps(registry);
    TickDestroyAsteroid(registry);
}

}  // namespace sr::space::tutorial_system
