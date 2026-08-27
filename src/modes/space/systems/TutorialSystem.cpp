#include "modes/space/systems/TutorialSystem.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "shared/components/Combat.h"
#include "shared/components/Comms.h"
#include "shared/components/Identity.h"
#include "shared/components/Party.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/components/Tutorial.h"
#include "shared/components/Warp.h"

namespace sr::space::tutorial_system {
namespace {

// Ported verbatim from legacy StarReach2's kTutorialMoveDistance.
constexpr float kTutorialMoveDistance = 300.0f;

// Mirrors CommsSystem.cpp's own kCommsLogCap -- this system pushes to the same CommsLog (the
// commander's hail), so it must trim to the identical cap or the two writers could disagree about
// how many entries survive.
constexpr std::size_t kCommsLogCap = 8;

// A pure function of `tick` rather than <random> -- Law 2's fast-forward determinism governs
// ordinary simulation state, not this scripted, one-time event (this file's own header comment:
// "the only scripted world event in the game"), but there is no reason to reach for a
// non-deterministic source when a deterministic one is exactly as unpredictable to the player and
// stays unit-testable. Identical to CommsSystem.cpp's own hail-response hash -- a small,
// deliberate duplication rather than a cross-system call (systems interact only through shared
// components).
std::uint32_t Hash32(std::uint32_t seed) {
    std::uint32_t hash = 2166136261u;
    for (int byte = 0; byte < 4; ++byte) {
        hash ^= (seed >> (byte * 8)) & 0xFFu;
        hash *= 16777619u;
    }
    return hash;
}

void Advance(Tutorial& tutorial) {
    tutorial.step = static_cast<TutorialStep>(static_cast<int>(tutorial.step) + 1);
}

int CountMountedModules(const entt::registry& registry, entt::entity root) {
    const auto* rig = registry.try_get<Rig>(root);
    if (rig == nullptr) {
        return 0;
    }
    int count = 0;
    for (const entt::entity hardpoint : rig->children) {
        if (const auto* mounted = registry.try_get<MountedModules>(hardpoint)) {
            count += static_cast<int>(mounted->ids.size());
        }
    }
    return count;
}

bool HasRecentlyFired(const entt::registry& registry, entt::entity root) {
    const auto* rig = registry.try_get<Rig>(root);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity hardpoint : rig->children) {
        if (const auto* weapon = registry.try_get<Weapon>(hardpoint)) {
            if (weapon->cooldown > 0.0f) {
                return true;
            }
        }
    }
    return false;
}

void TickPerRigSteps(entt::registry& registry) {
    for (auto [self, tutorial, xf] : registry.view<Tutorial, WorldTransform>().each()) {
        if (!tutorial.started) {
            tutorial.startPosition = xf.position;
            tutorial.startEquippedModules = CountMountedModules(registry, self);
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
            case TutorialStep::Fire:
                if (HasRecentlyFired(registry, self)) {
                    Advance(tutorial);
                }
                break;
            case TutorialStep::Equip:
                if (CountMountedModules(registry, self) > tutorial.startEquippedModules) {
                    Advance(tutorial);
                }
                break;
            default: break;  // Done: terminal.
        }
    }
}

// System.h's "one legitimate cache" exception: a singleton entity rather than mode-held state
// (Law 6). Mirrors CommsSystem.cpp's identical helper -- systems interact only through shared
// components (Comms.h), never by calling into each other, so this is a deliberate small
// duplication rather than a cross-system call.
entt::entity EnsureCommsLog(entt::registry& registry) {
    for (auto [entity] : registry.view<CommsLogSingleton>().each()) {
        return entity;
    }
    const entt::entity log = registry.create();
    registry.emplace<CommsLogSingleton>(log);
    registry.emplace<CommsLog>(log);
    return log;
}

void PushCommsEntry(entt::registry& registry, std::string text) {
    const entt::entity log = EnsureCommsLog(registry);
    CommsLog& commsLog = registry.get<CommsLog>(log);
    commsLog.entries.push_back(CommsEntry{std::move(text), false});
    if (commsLog.entries.size() > kCommsLogCap) {
        commsLog.entries.erase(commsLog.entries.begin());
    }
}

// The fleet commander's display name -- WorldGen.cpp's PopulatePrologueSystem is the only
// producer of a PartyLeader, so this resolves to the formation's leader rig. Falls back to a
// generic label rather than skipping the hail outright, in case a future authored scenario places
// an AnomalyField with no PartyLeader.
std::string CommanderName(const entt::registry& registry) {
    for (auto [entity, leader, name] : registry.view<PartyLeader, DisplayName>().each()) {
        (void)entity;
        (void)leader;
        return name.value;
    }
    return "Fleet Commander";
}

// The prologue's commander offer (features.md 1.2). Gated on an AnomalyField existing anywhere in
// this registry -- the one property that is only ever true in WorldGen.cpp's
// PopulatePrologueSystem, never in a procedurally generated system, so this never fires outside
// Act I's fixed scenario.
void TickOffer(entt::registry& registry) {
    bool hasAnomaly = false;
    for (auto [entity, field] : registry.view<AnomalyField>().each()) {
        (void)entity;
        (void)field;
        hasAnomaly = true;
        break;
    }
    if (!hasAnomaly) {
        return;
    }

    for (auto [self] : registry.view<PlayerControlled>().each()) {
        if (registry.any_of<Tutorial, TutorialOffer, TutorialDeclined>(self)) {
            continue;
        }
        PushCommsEntry(registry, CommanderName(registry) +
                                     " hails you: \"New to the fleet? I can walk you through "
                                     "the basics. [Y] Accept / [N] Decline.\"");
        registry.emplace<TutorialOffer>(self);
    }
}

// The cataclysm (features.md 1.2). Independent of Tutorial/TutorialOffer/TutorialDeclined --
// "declining changes nothing else... the objective still stands" -- so this only ever looks at
// AnomalyField and the player's own position.
void TickCataclysm(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    entt::entity anomaly = entt::null;
    Vec2 anomalyPosition{};
    float triggerRadius = 0.0f;
    for (auto [entity, field, xf] : registry.view<AnomalyField, WorldTransform>().each()) {
        anomaly = entity;
        anomalyPosition = xf.position;
        triggerRadius = field.triggerRadius;
        break;  // Exactly one anomaly exists in the one system that ever spawns one.
    }
    if (anomaly == entt::null) {
        return;
    }

    for (auto [self, xf] : registry.view<PlayerControlled, WorldTransform>().each()) {
        // Guards against re-firing every tick the player lingers inside the trigger radius before
        // SpaceFlight processes this same tick's request and tears the whole registry down --
        // what makes the event structurally one-shot rather than needing a separate "already
        // fired" flag.
        if (registry.all_of<SystemWarpRequest>(self)) {
            continue;
        }
        if (Distance(xf.position, anomalyPosition) > triggerRadius) {
            continue;
        }
        const std::string targetSystemId =
            "frontier-" + std::to_string(Hash32(static_cast<std::uint32_t>(ctx.tick)));
        registry.emplace<SystemWarpRequest>(self, targetSystemId, Vec2{0.0f, 0.0f}, 0.0f);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    TickOffer(registry);
    TickPerRigSteps(registry);
    TickCataclysm(ctx);
}

}  // namespace sr::space::tutorial_system
