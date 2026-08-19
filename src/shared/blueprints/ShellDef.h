#pragma once

#include <string>
#include <vector>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/Taxonomy.h"

namespace sr {

// The authored definition of a shell: the graphical/structural housing half of the
// Shell -> Component -> Module model (Law 4). One shell becomes exactly one hardpoint entity
// when a rig is instantiated.
struct ShellDef {
    ShellId id;
    std::string displayName;
    ShellKind kind = ShellKind::Armor;

    // Hull of the hardpoint entity itself, before module bonuses. When this reaches zero the
    // hardpoint entity is destroyed and its capability is gone permanently
    // (features.md section 3.2) -- the engine shell dying is what stalls the ship.
    float hull = 0.0f;
    float mass = 0.0f;

    // How many modules may occupy this shell. Zero is legal: a pure armor plate.
    int moduleSlots = 0;

    // Collision/targeting radius of this hardpoint in world units. Targeting picks an aim point
    // per hardpoint, so this has to be per-shell rather than per-craft.
    float radius = 8.0f;

    // Texture layer id for the separated top-down layer stack (features.md section 2.2). This is
    // an asset *key*, never a path -- engine/assets/ owns resolution (architecture.md section 6).
    std::string spriteLayer;

    // Which ModuleKinds may mount here (architecture.md 12.22). Replaces the old
    // IsMountable(ModuleKind, ShellKind) switch in Taxonomy.cpp -- a content rule in code, which
    // Law 10 forbids -- with an authored list. ModuleKind::Armor is never listed: it is the one
    // universal exception (Taxonomy.h documents why), checked separately by Accepts() below so
    // content never has to repeat it on every shell.
    std::vector<ModuleKind> acceptsKinds;

    bool Accepts(ModuleKind moduleKind) const {
        if (moduleKind == ModuleKind::Armor) {
            return true;
        }
        for (const ModuleKind accepted : acceptsKinds) {
            if (accepted == moduleKind) {
                return true;
            }
        }
        return false;
    }
};

}  // namespace sr
