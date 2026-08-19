#include "shared/blueprints/Taxonomy.h"

#include <array>
#include <utility>

namespace sr {
namespace {

// One table per enum, used for both directions. Adding an enumerator without adding its row
// here is a compile-time break in ToString's switch, which is the intent.
constexpr std::array<std::pair<std::string_view, DamageType>, 3> kDamageTypes{{
    {"kinetic", DamageType::Kinetic},
    {"energy", DamageType::Energy},
    {"ion", DamageType::Ion},
}};

constexpr std::array<std::pair<std::string_view, ShellKind>, 7> kShellKinds{{
    {"chassis", ShellKind::Chassis},
    {"armor", ShellKind::Armor},
    {"power_cell", ShellKind::PowerCell},
    {"engine", ShellKind::Engine},
    {"weapon", ShellKind::Weapon},
    {"shield", ShellKind::Shield},
    {"facility", ShellKind::Facility},
}};

constexpr std::array<std::pair<std::string_view, ModuleKind>, 10> kModuleKinds{{
    {"weapon", ModuleKind::Weapon},
    {"shield_generator", ModuleKind::ShieldGenerator},
    {"power_cell", ModuleKind::PowerCell},
    {"engine", ModuleKind::Engine},
    {"armor", ModuleKind::Armor},
    {"facility", ModuleKind::Facility},
    {"sensor", ModuleKind::Sensor},
    {"cargo_bay", ModuleKind::CargoBay},
    {"fire_control", ModuleKind::FireControl},
    {"hyperdrive", ModuleKind::Hyperdrive},
}};

constexpr std::array<std::pair<std::string_view, FacilityKind>, 6> kFacilityKinds{{
    {"repair", FacilityKind::Repair},
    {"manufacturing", FacilityKind::Manufacturing},
    {"research", FacilityKind::Research},
    {"docking", FacilityKind::Docking},
    {"storage", FacilityKind::Storage},
    {"engineering", FacilityKind::Engineering},
}};

template <typename Table, typename Enum>
bool LookupToken(const Table& table, std::string_view text, Enum& out) {
    for (const auto& [token, value] : table) {
        if (token == text) {
            out = value;
            return true;
        }
    }
    return false;
}

}  // namespace

std::string_view ToString(DamageType value) {
    switch (value) {
        case DamageType::Kinetic: return "kinetic";
        case DamageType::Energy: return "energy";
        case DamageType::Ion: return "ion";
    }
    return "kinetic";
}

std::string_view ToString(ShellKind value) {
    switch (value) {
        case ShellKind::Chassis: return "chassis";
        case ShellKind::Armor: return "armor";
        case ShellKind::PowerCell: return "power_cell";
        case ShellKind::Engine: return "engine";
        case ShellKind::Weapon: return "weapon";
        case ShellKind::Shield: return "shield";
        case ShellKind::Facility: return "facility";
    }
    return "chassis";
}

std::string_view ToString(ModuleKind value) {
    switch (value) {
        case ModuleKind::Weapon: return "weapon";
        case ModuleKind::ShieldGenerator: return "shield_generator";
        case ModuleKind::PowerCell: return "power_cell";
        case ModuleKind::Engine: return "engine";
        case ModuleKind::Armor: return "armor";
        case ModuleKind::Facility: return "facility";
        case ModuleKind::Sensor: return "sensor";
        case ModuleKind::CargoBay: return "cargo_bay";
        case ModuleKind::FireControl: return "fire_control";
        case ModuleKind::Hyperdrive: return "hyperdrive";
    }
    return "armor";
}

std::string_view ToString(FacilityKind value) {
    switch (value) {
        case FacilityKind::Repair: return "repair";
        case FacilityKind::Manufacturing: return "manufacturing";
        case FacilityKind::Research: return "research";
        case FacilityKind::Docking: return "docking";
        case FacilityKind::Storage: return "storage";
        case FacilityKind::Engineering: return "engineering";
    }
    return "storage";
}

bool FromString(std::string_view text, DamageType& out) {
    return LookupToken(kDamageTypes, text, out);
}
bool FromString(std::string_view text, ShellKind& out) {
    return LookupToken(kShellKinds, text, out);
}
bool FromString(std::string_view text, ModuleKind& out) {
    return LookupToken(kModuleKinds, text, out);
}
bool FromString(std::string_view text, FacilityKind& out) {
    return LookupToken(kFacilityKinds, text, out);
}

bool IsMountable(ModuleKind module, ShellKind shell) {
    switch (module) {
        case ModuleKind::Weapon: return shell == ShellKind::Weapon;
        case ModuleKind::ShieldGenerator: return shell == ShellKind::Shield;
        case ModuleKind::PowerCell: return shell == ShellKind::PowerCell;
        case ModuleKind::Engine: return shell == ShellKind::Engine;
        // Sensor/CargoBay/Hyperdrive share the same non-combat housing ModuleKind::Facility
        // already uses -- a facility bay can be an antenna, a cargo pod, or a warp core just as
        // easily as a repair bay, and all four shed power in the same priority band.
        case ModuleKind::Facility:
        case ModuleKind::Sensor:
        case ModuleKind::CargoBay:
        case ModuleKind::Hyperdrive: return shell == ShellKind::Facility;
        // FireControl shares the Weapon shell it augments -- a 2-slot turret mount carries both
        // (architecture.md 12.23); do not merge this into ModuleKind::Weapon itself.
        case ModuleKind::FireControl: return shell == ShellKind::Weapon;
        // Armor plating is the one module that is structurally universal -- it adds hull and
        // mass to whatever it is bolted to, which is what makes the mass budget a real trade.
        case ModuleKind::Armor: return true;
    }
    return false;
}

}  // namespace sr
