#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// architecture.md 4's NpcAiSystem inventory row. Patrol/Chase/Attack fall out of Target plus
// engagement range exactly as the pre-P2-10 approach-and-fire code already computed them; Flee
// and Escort are the two states that task added. Harvest is a non-combat third addition: closing
// on and draining a non-hostile entity (an asteroid, say) rather than a hostile Target -- see
// AiBehavior::harvestTarget below for why it cannot reuse Target.
enum class AiState : std::uint8_t { Patrol, Chase, Attack, Flee, Escort, Harvest };

// On a rig root driven by NpcAiSystem. `state` is recomputed from scratch every tick rather than
// carrying hysteresis -- the same "derived, not stored" shape Rig.h's own Uncrewed tag already
// uses -- so it can never drift from the Target/integrity/escortTarget/harvestTarget it
// describes.
struct AiBehavior {
    AiState state = AiState::Patrol;

    // Valid only while state == Escort: the friendly rig this one station-keeps on. Deliberately
    // not PartyMember (Party.h) -- that component is a Defend order's own to write (P8-02); this
    // is a narrower "who to follow" relationship with no formation offset and no shared
    // retaliation. Not yet written by anything (no order producer exists), so Escort never
    // triggers in the shipped game today -- NpcAiSystem only reads it, the same "producer doesn't
    // exist yet, reader is still correct to build" shape Commander::orders had before
    // CommanderSystem landed.
    entt::entity escortTarget = entt::null;

    // Valid only while state == Harvest: the non-hostile entity this rig is closing on and
    // draining (e.g. an asteroid). Deliberately not Target (Targeting.h) -- TargetingSystem's own
    // Tick runs over every entity carrying a Target component, nulls it out the instant
    // IsValidTarget fails, and unconditionally re-acquires the nearest hostile; an asteroid is
    // never Targetable/hostile, so it would never survive there even for one tick. Same
    // "producer doesn't exist yet, reader is still correct to build" shape as escortTarget above
    // -- nothing in the shipped game writes this yet.
    entt::entity harvestTarget = entt::null;
};

}  // namespace sr
