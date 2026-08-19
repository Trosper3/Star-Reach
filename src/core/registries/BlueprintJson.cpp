#include "core/registries/BlueprintJson.h"

#include <string>
#include <vector>

namespace sr::core {
namespace {

void ParseWeaponStats(const JsonReader& reader, WeaponStats& out) {
    const JsonReader stats = reader.Child("weapon", "weapon");
    stats.Optional("damage", out.damage);
    stats.OptionalEnum("damageType", out.damageType);
    stats.Optional("fireIntervalSeconds", out.fireIntervalSeconds);
    stats.Optional("projectileSpeed", out.projectileSpeed);
    stats.Optional("rangeUnits", out.rangeUnits);
    stats.Optional("spreadRadians", out.spreadRadians);
    stats.Optional("projectilesPerShot", out.projectilesPerShot);
}

void ParseShieldStats(const JsonReader& reader, ShieldStats& out) {
    const JsonReader stats = reader.Child("shield", "shield");
    stats.Optional("capacity", out.capacity);
    stats.OptionalEnum("absorbs", out.absorbs);
    stats.Optional("rechargePerSecond", out.rechargePerSecond);
    stats.Optional("rechargeDelaySeconds", out.rechargeDelaySeconds);
    stats.OptionalEnum("coverage", out.coverage);
    stats.Optional("coverageRadius", out.coverageRadius);
}

void ParseEngineStats(const JsonReader& reader, EngineStats& out) {
    const JsonReader stats = reader.Child("engine", "engine");
    stats.Optional("thrustNewtons", out.thrustNewtons);
    stats.Optional("turnTorque", out.turnTorque);
    stats.Optional("maxSpeed", out.maxSpeed);
}

void ParseFacilityStats(const JsonReader& reader, FacilityStats& out) {
    const JsonReader stats = reader.Child("facility", "facility");
    stats.OptionalEnum("kind", out.kind);
    stats.Optional("ratePerSecond", out.ratePerSecond);
    stats.Optional("capacity", out.capacity);
}

void ParseSensorStats(const JsonReader& reader, SensorStats& out) {
    const JsonReader stats = reader.Child("sensor", "sensor");
    stats.Optional("range", out.range);
}

void ParseFireControlStats(const JsonReader& reader, FireControlStats& out) {
    const JsonReader stats = reader.Child("fireControl", "fireControl");
    stats.Optional("turnRatePerSecond", out.turnRatePerSecond);
}

void ParseCargoBayStats(const JsonReader& reader, CargoBayStats& out) {
    const JsonReader stats = reader.Child("cargoBay", "cargoBay");
    stats.Optional("slotCount", out.slotCount);
    stats.Optional("slotCapacity", out.slotCapacity);
}

MountBlueprint ParseMount(const JsonReader& reader) {
    MountBlueprint mount;

    std::string id;
    reader.Require("id", id);
    mount.id = MountId(id);

    std::string shell;
    reader.Require("shell", shell);
    mount.shell = ShellId(shell);

    // Absent on exactly one mount -- the rig root. Validation, not the parser, decides whether
    // the resulting root count is legal.
    std::string attachedTo;
    reader.Optional("attachedTo", attachedTo);
    mount.attachedTo = MountId(attachedTo);

    const JsonReader offset = reader.Child("localOffset", "localOffset");
    offset.Optional("x", mount.localOffset.x);
    offset.Optional("y", mount.localOffset.y);

    reader.Optional("localRotation", mount.localRotation);
    reader.Optional("traverseRadians", mount.traverseRadians);

    std::vector<std::string> modules;
    reader.Optional("modules", modules);
    for (const auto& moduleId : modules) {
        mount.modules.push_back(ModuleId(moduleId));
    }
    return mount;
}

}  // namespace

ModuleDef ParseModuleDef(const JsonReader& reader) {
    ModuleDef def;

    std::string id;
    reader.Require("id", id);
    def.id = ModuleId(id);

    reader.Require("displayName", def.displayName);
    reader.RequireEnum("kind", def.kind);
    reader.Optional("mass", def.mass);
    reader.Optional("powerDraw", def.powerDraw);
    reader.Optional("powerGeneration", def.powerGeneration);
    reader.Optional("hullBonus", def.hullBonus);

    ParseWeaponStats(reader, def.weapon);
    ParseShieldStats(reader, def.shield);
    ParseEngineStats(reader, def.engine);
    ParseFacilityStats(reader, def.facility);
    ParseSensorStats(reader, def.sensor);
    ParseFireControlStats(reader, def.fireControl);
    ParseCargoBayStats(reader, def.cargoBay);
    return def;
}

ElementDef ParseElementDef(const JsonReader& reader) {
    ElementDef def;

    std::string id;
    reader.Require("id", id);
    def.id = ElementId(id);

    reader.Require("displayName", def.displayName);
    reader.Optional("mass", def.mass);
    return def;
}

ShellDef ParseShellDef(const JsonReader& reader) {
    ShellDef def;

    std::string id;
    reader.Require("id", id);
    def.id = ShellId(id);

    reader.Require("displayName", def.displayName);
    reader.RequireEnum("kind", def.kind);
    reader.Require("hull", def.hull);
    reader.Optional("mass", def.mass);
    reader.Optional("moduleSlots", def.moduleSlots);
    reader.Optional("radius", def.radius);
    reader.Optional("spriteLayer", def.spriteLayer);

    // ModuleKind::Armor is never listed (ShellDef::Accepts checks it separately) -- an unknown
    // token here is still an error, the same as any other enum field, so a content typo surfaces
    // at load time rather than as a module silently refusing to mount.
    std::vector<std::string> acceptsKinds;
    reader.Optional("acceptsKinds", acceptsKinds);
    for (const std::string& token : acceptsKinds) {
        ModuleKind kind = ModuleKind::Armor;
        if (!FromString(token, kind)) {
            reader.Error("field 'acceptsKinds' has unknown value '" + token + "'");
            continue;
        }
        def.acceptsKinds.push_back(kind);
    }
    return def;
}

ShipBlueprint ParseShipBlueprint(const JsonReader& reader) {
    ShipBlueprint bp;

    std::string id;
    reader.Require("id", id);
    bp.id = BlueprintId(id);

    // Required, not defaulted. features.md validation rule 9: a blueprint with no version is
    // rejected rather than assumed current, so SaveMigrator always has something to dispatch on.
    reader.Require("schemaVersion", bp.schemaVersion);
    reader.Require("displayName", bp.displayName);

    std::string faction;
    reader.Optional("faction", faction);
    bp.faction = FactionId(faction);

    reader.Optional("mobile", bp.mobile);
    reader.Optional("structuralMassLimit", bp.structuralMassLimit);

    if (!reader.Has("mounts") || !reader.node().at("mounts").is_array()) {
        reader.Error("field 'mounts' must be an array");
        return bp;
    }

    const nlohmann::json& mounts = reader.node().at("mounts");
    for (size_t i = 0; i < mounts.size(); ++i) {
        const std::string context = "mounts[" + std::to_string(i) + "]";
        if (!mounts[i].is_object()) {
            reader.Error(context + " must be an object");
            continue;
        }
        const JsonReader mountReader(mounts[i], reader.file(), reader.context() + " > " + context,
                                     reader.report());
        bp.rig.mounts.push_back(ParseMount(mountReader));
    }
    return bp;
}

}  // namespace sr::core
