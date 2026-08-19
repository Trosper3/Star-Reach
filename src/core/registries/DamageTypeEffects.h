#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>

#include "core/registries/JsonReader.h"
#include "shared/blueprints/Taxonomy.h"

namespace sr::core {

// One row per DamageType that diverges from the default absorb-or-bypass shape (architecture.md
// 12.33): a shield absorbs a matching type, a mismatch bypasses straight to the hull. Kinetic and
// Energy take this default row implicitly -- only a type that needs to differ, today just Ion,
// needs an entry in data/base_game/damage_types.json (Law 10).
struct DamageTypeEffect {
    bool alwaysAbsorbedByAnyShield = false;  // Ion: true -- absorbed regardless of Shield::absorbs.
    bool bypassStillDrainsShieldCharge = false;  // Ion: true -- "strips shields quickly".
    float hullDamageFraction = 1.0f;             // Ion: 0.0 -- deals no hull damage once through.
    float powerDrainFraction = 0.0f;  // Ion: 1.0 -- redirected to PowerSource generation.
};

// Authored damage-type effects, loaded from data/base_game/damage_types.json. Lookup never
// fails to resolve: an unauthored DamageType (Kinetic, Energy) returns the default row above.
class DamageTypeEffects {
public:
    LoadReport LoadFromFile(const std::filesystem::path& path);

    DamageTypeEffect Lookup(DamageType type) const;

private:
    std::unordered_map<std::uint8_t, DamageTypeEffect> rows_;
};

}  // namespace sr::core
