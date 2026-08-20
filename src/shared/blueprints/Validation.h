#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "shared/blueprints/DefLibrary.h"
#include "shared/blueprints/ShipBlueprint.h"

namespace sr {

// The rules from features.md section 2.3, plus two mechanical checks that section implies but
// does not enumerate (MountCapacity, ModuleCompatibility -- see Validation.cpp).
//
// Validation runs on the BLUEPRINT, before instantiation (Law 3). An invalid design therefore
// cannot become a live entity, and a corrupt save or a hostile wire packet cannot inject one.
// That ordering is the whole point: there is no second validation pass on the entity side.
enum class ValidationRule : std::uint8_t {
    StructuralShell,      // 1. At least one Chassis/Armor shell carrying a module.
    PowerSource,          // 2. At least one PowerCell shell carrying a module.
    PowerBalance,         // 3. Total draw <= total generation.
    MassLimit,            // 4. Total mass <= chassis structural limit.
    Propulsion,           // 5. At least one Engine shell carrying a module. Mobile craft only.
    MountParent,          // 6. Every attachedTo resolves; exactly one root.
    Adjacency,            // 7. Every mount reachable from the root; no cycles, no islands.
    UnresolvedId,         // 8. Every shell and module id resolves in the library.
    Identity,             // 9. Non-empty blueprint id and a supported schemaVersion.
    MountCapacity,        //    Module count within the shell's slot count.
    ModuleCompatibility,  //    Module kind legal for the shell kind it occupies.
    CrewCapacity,         //    Crew-kind module count within the shell's own crewSlots budget --
                          //    separate from MountCapacity above (features.md 2.7).
    WeaponTraverse,       //    A weapon-shell mount carrying a module authors traverseRadians > 0
                          //    -- a fixed-forward gun must be authored as such, not reached by
                          //    an omitted field silently defaulting to zero (architecture.md
                          //    13.3 finding W, 13.4 decision 5).
    StructuralCoverage,   // 12. Chassis plus armour must cover the hull envelope: no non-
                          //     structural mount hangs directly off another non-structural one
                          //     with nothing structural backing it (features.md 3.2's "no bare
                          //     sections").
    Separation,           // 10. No two mounts closer than r(A) + r(B) -- hit circles stay
                          //     disjoint so manual aim has something to pick apart (features.md
                          //     3.5).
    Attachment,           // 11. Every mount within r(A) + r(B) of what it attaches to, so it
                          //     reads as physically joined rather than floating detached
                          //     (features.md 3.5).
};

std::string_view ToString(ValidationRule rule);

struct ValidationError {
    ValidationRule rule;
    // Specific enough to show in the Engineering UI verbatim. features.md section 2.3 calls out
    // that rules 6-9 must produce distinct messages, never a generic "invalid template".
    std::string message;
    // Empty when the error is about the blueprint as a whole rather than one hardpoint.
    MountId mount;
};

struct ValidationResult {
    std::vector<ValidationError> errors;

    bool ok() const { return errors.empty(); }
    bool HasRule(ValidationRule rule) const;
};

// Reports EVERY violated rule, not just the first. A player rebalancing a loadout wants the
// whole list; failing fast would turn one edit session into eleven.
ValidationResult Validate(const ShipBlueprint& blueprint, const DefLibrary& library);

// Aggregates used by both validation and the Engineering UI's live readout. Computed only over
// mounts whose ids resolve, so they stay meaningful on a partially-broken blueprint.
struct RigTotals {
    float mass = 0.0f;
    float powerDraw = 0.0f;
    float powerGeneration = 0.0f;
    float hull = 0.0f;

    float PowerBalance() const { return powerGeneration - powerDraw; }
};

RigTotals ComputeTotals(const ShipBlueprint& blueprint, const DefLibrary& library);

}  // namespace sr
