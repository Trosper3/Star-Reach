#pragma once

#include <string>
#include <vector>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/math/Vec2.h"

namespace sr {

// The per-ShellKind default when content doesn't author an explicit ShellDef::drawLayer
// (features.md section 3.5's five-layer stack: 1 ventral, 2 hull, 3 hull detail, 4 dorsal,
// 5 overlay). Engine reads ventral and Weapon/Facility read dorsal because that's literally what
// the layer table names as their contents; everything else -- internal hardware with no more
// specific home -- defaults to the hull layer alongside the chassis. Layer 3 has no default
// owner yet: it's reserved for decal-only detail nothing currently authors.
inline int DefaultDrawLayer(ShellKind kind) {
    switch (kind) {
        case ShellKind::Engine: return 1;
        case ShellKind::Weapon:
        case ShellKind::Facility: return 4;
        case ShellKind::Chassis:
        case ShellKind::Armor:
        case ShellKind::PowerCell:
        case ShellKind::Shield: return 2;
    }
    return 2;
}

// The authored definition of a shell: the graphical/structural housing half of the
// Shell -> Component -> Module model (Law 4). One shell becomes exactly one hardpoint entity
// when a rig is instantiated.
struct ShellDef {
    ShellId id;
    std::string displayName;
    ShellKind kind = ShellKind::Armor;

    // Empty for unfactioned/universal content -- ModuleDef.h's `faction` field carries the same
    // meaning and the same "Codex is the only reader today" caveat.
    FactionId faction;

    // ModuleDef.h's `tier` field, mirrored here for the same reason: the Codex tags every
    // ContentLibrary def uniformly (architecture.md 12.30.6), and a shell needs the same field a
    // module has or the Codex's Shells section has nothing to tag rows with.
    int tier = 1;

    // Hull of the hardpoint entity itself, before module bonuses. When this reaches zero the
    // hardpoint entity is destroyed and its capability is gone permanently
    // (features.md section 3.2) -- the engine shell dying is what stalls the ship.
    float hull = 0.0f;
    float mass = 0.0f;

    // How many modules may occupy this shell. Zero is legal: a pure armor plate.
    int moduleSlots = 0;

    // How many Crew-kind modules this shell can carry, on top of moduleSlots above rather than
    // counted against it (features.md section 2.7: "one authored field carries all of it") -- a
    // turret keeps its own weapon slot whether or not a Crew module also rides along. Zero is the
    // default for every shell that is not an authored cockpit, bridge, or turret (section 2.7's
    // crew-slots-by-shell-type table: Bridge 2, Cockpit 1, Turret 1 always, everything else 0).
    int crewSlots = 0;

    // Collision/targeting radius of this hardpoint in world units. Targeting picks an aim point
    // per hardpoint, so this has to be per-shell rather than per-craft.
    float radius = 8.0f;

    // Meaningful on a Chassis shell only: the hull's overall size in world units (features.md
    // section 3.5). Rule 12 -- hull envelope, bounding every mount's centre-plus-radius within
    // this -- is a later validation pass; today it's the one authored number the scale table's
    // ring-capacity math (chassis radius, peripheral radius, how many rings fit) is worked from.
    float hullRadius = 0.0f;

    // Optional baked collision polygon overriding the circle `radius` above draws (features.md
    // section 3.5). Empty means "use the circle" -- the only shape that exists until an asset
    // pipeline can trace one from real art (architecture.md section 6, deferred). Local to the
    // shell, convex, wound consistently. Defining the field now avoids a data-model change when
    // that pipeline lands; nothing bakes or reads this yet.
    std::vector<Vec2> collisionPolygon;

    // Texture layer id for the separated top-down layer stack (features.md section 2.2). This is
    // an asset *key*, never a path -- engine/assets/ owns resolution (architecture.md section 6).
    std::string spriteLayer;

    // Render order within one rig -- features.md section 3.5's five-layer stack (1 ventral..5
    // overlay). Separate from spriteLayer above: that's which image, this is what order. Zero
    // means "unset"; ParseShellDef fills it from DefaultDrawLayer(kind) when content doesn't
    // author one explicitly, so a live ShellDef always carries a valid 1-5 value.
    int drawLayer = 0;

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
