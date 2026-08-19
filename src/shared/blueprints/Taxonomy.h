#pragma once

#include <cstdint>
#include <string_view>

// The vocabulary shared by authored content and live components.
//
// Both forms of every object (Law 3) speak these enums: a ModuleDef in JSON declares a
// DamageType, and so does the Projectile entity it eventually spawns. Keeping the vocabulary in
// one header is what stops the blueprint side and the component side from drifting into two
// parallel enums that need a translation table.
namespace sr {

// features.md section 3.1. Matching type is absorbed by a shield; a mismatched type bypasses it
// and lands directly on the hardpoint beneath -- Ion is the one exception, absorbed by every
// shield type regardless of match (architecture.md 12.33). Shield-matching itself stays exactly
// {Kinetic, Energy}: Ion is a weapon type, not a shield type, and expanding the shield roster is
// still the open design question this enum's growth does not reopen.
enum class DamageType : std::uint8_t {
    Kinetic,
    Energy,
    Ion,
};

// What a shell is structurally for. Drives Template validation (features.md section 2.3) and
// which role components the factory attaches.
enum class ShellKind : std::uint8_t {
    Chassis,    // The rig's root. Carries the structural mass threshold.
    Armor,      // Pure hull, no function.
    PowerCell,  // Generation.
    Engine,     // Thrust.
    Weapon,     // Turret / fixed mount.
    Shield,     // Shield generator housing.
    Facility,   // Repair, manufacturing, research, docking bays.
};

// What a module does once mounted. A module is only legal in a shell whose kind accepts it;
// ModuleDef::compatible lists that explicitly rather than inferring it from the name.
enum class ModuleKind : std::uint8_t {
    Weapon,
    ShieldGenerator,
    PowerCell,
    Engine,
    Armor,
    Facility,
    // architecture.md 12.23's four new kinds: each is a built system with no module feeding it.
    // Do not add ModuleKind::Auxiliary as a catch-all -- grouping is a display category on
    // ModuleDef for the menus, never a type; a single kind cannot tell ModuleAttachment's switch
    // which components to attach.
    Sensor,       // Max-aggregates SensorRange (DiscoverySystem's read side already exists).
    CargoBay,     // slotCount x slotCapacity, total derived -- P0-10 wires CargoHold onto it.
    FireControl,  // Drives the co-mounted Weapon's FiringArc::turnRatePerSecond.
    Hyperdrive,   // WarpSystem's fuel/jump gate lands in P9-06; this just makes the kind exist.
};

// Which capability a Facility shell provides once powered. features.md section 4 --
// component-driven menus read this to decide which Bridge tabs exist.
enum class FacilityKind : std::uint8_t {
    Repair,
    Manufacturing,
    Research,
    Docking,
    Storage,
    // Grants EngineerMenu (module merging) and RefactorMenu (hardpoint deletion) at whatever
    // station/ship carries this facility. FacilityStats::level (ModuleDef.h) is only meaningful
    // for this kind -- the engineer's skill tier, 1-5, scales EngineerSystem's merge formula.
    Engineering,
};

std::string_view ToString(DamageType value);
std::string_view ToString(ShellKind value);
std::string_view ToString(ModuleKind value);
std::string_view ToString(FacilityKind value);

// Parse helpers return false on an unknown token rather than defaulting, so a typo in JSON
// surfaces as a registry load error naming the file and key (Law 10) instead of silently
// becoming whatever the first enumerator happens to be.
bool FromString(std::string_view text, DamageType& out);
bool FromString(std::string_view text, ShellKind& out);
bool FromString(std::string_view text, ModuleKind& out);
bool FromString(std::string_view text, FacilityKind& out);

// True if a module of this kind may legally sit in a shell of this kind.
bool IsMountable(ModuleKind module, ShellKind shell);

}  // namespace sr
