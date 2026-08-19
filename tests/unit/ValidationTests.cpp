#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_map>

#include "shared/blueprints/Validation.h"
#include "shared/math/Angle.h"

namespace {

// A hand-built library. tests/ is one of the three places allowed to construct content types in
// C++ (Law 10) precisely so a validation test does not need a data file to run.
class FakeLibrary final : public sr::DefLibrary {  // NOLINT(bugprone-exception-escape)
public:
    void AddShell(const char* id, sr::ShellKind kind, float mass = 10.0f, int slots = 1,
                  std::vector<sr::ModuleKind> acceptsKinds = {}, float radius = 8.0f) {
        sr::ShellDef def;
        def.id = sr::ShellId(id);
        def.displayName = id;
        def.kind = kind;
        def.hull = 100.0f;
        def.mass = mass;
        def.moduleSlots = slots;
        def.acceptsKinds = std::move(acceptsKinds);
        def.radius = radius;
        shells_[id] = def;
    }

    void AddModule(const char* id, sr::ModuleKind kind, float mass = 10.0f, float draw = 0.0f,
                   float generation = 0.0f) {
        sr::ModuleDef def;
        def.id = sr::ModuleId(id);
        def.displayName = id;
        def.kind = kind;
        def.mass = mass;
        def.powerDraw = draw;
        def.powerGeneration = generation;
        modules_[id] = def;
    }

    const sr::ShellDef* FindShell(const sr::ShellId& id) const override {
        const auto it = shells_.find(id.str());
        return it == shells_.end() ? nullptr : &it->second;
    }

    const sr::ModuleDef* FindModule(const sr::ModuleId& id) const override {
        const auto it = modules_.find(id.str());
        return it == modules_.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::string, sr::ShellDef> shells_;
    std::unordered_map<std::string, sr::ModuleDef> modules_;
};

FakeLibrary MakeLibrary() {
    FakeLibrary library;
    library.AddShell("chassis", sr::ShellKind::Chassis);
    library.AddShell("power_bay", sr::ShellKind::PowerCell, 10.0f, 1, {sr::ModuleKind::PowerCell});
    library.AddShell("thruster", sr::ShellKind::Engine, 10.0f, 1, {sr::ModuleKind::Engine});
    library.AddShell("gun", sr::ShellKind::Weapon, 10.0f, 1, {sr::ModuleKind::Weapon});
    library.AddModule("plate", sr::ModuleKind::Armor);
    library.AddModule("cell", sr::ModuleKind::PowerCell, 10.0f, 0.0f, 100.0f);
    library.AddModule("engine", sr::ModuleKind::Engine, 10.0f, 20.0f);
    library.AddModule("cannon", sr::ModuleKind::Weapon, 10.0f, 15.0f);
    return library;
}

sr::MountBlueprint Mount(const char* id, const char* shell, const char* attachedTo,
                         const std::vector<const char*>& modules, float traverseRadians = 0.0f,
                         sr::Vec2 localOffset = {}) {
    sr::MountBlueprint mount;
    mount.id = sr::MountId(id);
    mount.shell = sr::ShellId(shell);
    mount.attachedTo = sr::MountId(attachedTo);
    for (const char* module : modules) {
        mount.modules.push_back(sr::ModuleId(module));
    }
    mount.traverseRadians = traverseRadians;
    mount.localOffset = localOffset;
    return mount;
}

// Every shell MakeLibrary() adds keeps ShellDef::radius at its 8.0f default, so a ring of mounts
// each 16 units (r(A) + r(B)) from the chassis root, spaced further than 16 apart from each
// other, satisfies rules 10 and 11 with the lower and upper bound meeting exactly at the
// parent-child distance -- the same "touching, not overlapping" placement the scale table's own
// worked example authors (features.md 3.5).
sr::ShipBlueprint MakeValidShip() {
    sr::ShipBlueprint bp;
    bp.id = sr::BlueprintId("test_fighter");
    bp.schemaVersion = sr::kBlueprintSchemaVersion;
    bp.displayName = "Test Fighter";
    bp.mobile = true;
    bp.structuralMassLimit = 500.0f;
    bp.rig.mounts = {
        Mount("core", "chassis", "", {"plate"}),
        Mount("reactor", "power_bay", "core", {"cell"}, 0.0f, sr::Vec2{16.0f, 0.0f}),
        Mount("thruster_main", "thruster", "core", {"engine"}, 0.0f, sr::Vec2{-8.0f, 13.8564f}),
        Mount("gun_nose", "gun", "core", {"cannon"}, 0.35f, sr::Vec2{-8.0f, -13.8564f}),
    };
    return bp;
}

// A library with just a chassis and a peripheral shell at whatever radii the caller wants to
// check the ring-capacity formula against (features.md 3.5): peripherals per ring ~ pi(c+p)/p.
FakeLibrary MakeRingLibrary(float chassisRadius, float peripheralRadius) {
    FakeLibrary library;
    library.AddShell("core_shell", sr::ShellKind::Chassis, 10.0f, 0, {}, chassisRadius);
    library.AddShell("ring_shell", sr::ShellKind::Armor, 10.0f, 0, {}, peripheralRadius);
    return library;
}

// `count` peripherals evenly spaced on a ring of radius `chassisRadius + peripheralRadius`
// around a chassis root -- exactly the placement the scale table's ring-capacity formula
// describes, so rules 10 and 11 are the oracle for whether the documented count actually fits.
sr::ShipBlueprint MakeRingShip(float chassisRadius, float peripheralRadius, int count) {
    sr::ShipBlueprint bp;
    bp.id = sr::BlueprintId("ring_test");
    bp.schemaVersion = sr::kBlueprintSchemaVersion;
    bp.displayName = "Ring Test";
    bp.rig.mounts.push_back(Mount("core", "core_shell", "", {}));

    const float ringRadius = chassisRadius + peripheralRadius;
    for (int i = 0; i < count; ++i) {
        const float angle = sr::kTwoPi * static_cast<float>(i) / static_cast<float>(count);
        const sr::Vec2 offset = sr::Rotated(sr::Vec2{ringRadius, 0.0f}, angle);
        const std::string id = "p" + std::to_string(i);
        bp.rig.mounts.push_back(Mount(id.c_str(), "ring_shell", "core", {}, 0.0f, offset));
    }
    return bp;
}

bool RingGeometryValid(const sr::ValidationResult& result) {
    return !result.HasRule(sr::ValidationRule::Separation) &&
           !result.HasRule(sr::ValidationRule::Attachment);
}

}  // namespace

TEST_CASE("A well-formed blueprint validates clean", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    const sr::ValidationResult result = sr::Validate(MakeValidShip(), library);

    const std::string firstError =
        result.errors.empty() ? std::string{} : result.errors.front().message;
    INFO(firstError);
    CHECK(result.ok());
}

TEST_CASE("Rule 2 -- a rig with no power source is rejected", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts.erase(bp.rig.mounts.begin() + 1);

    const sr::ValidationResult result = sr::Validate(bp, library);
    CHECK_FALSE(result.ok());
    CHECK(result.HasRule(sr::ValidationRule::PowerSource));
}

TEST_CASE("Rule 3 -- draw above generation is rejected", "[validation]") {
    FakeLibrary library = MakeLibrary();
    library.AddModule("hungry_cannon", sr::ModuleKind::Weapon, 10.0f, 500.0f);

    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts.back().modules = {sr::ModuleId("hungry_cannon")};

    const sr::ValidationResult result = sr::Validate(bp, library);
    CHECK(result.HasRule(sr::ValidationRule::PowerBalance));
}

TEST_CASE("Rule 4 -- mass above the structural limit is rejected", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.structuralMassLimit = 10.0f;

    const sr::ValidationResult result = sr::Validate(bp, library);
    CHECK(result.HasRule(sr::ValidationRule::MassLimit));
}

TEST_CASE("Rule 5 -- stations are exempt from the engine requirement", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts.erase(bp.rig.mounts.begin() + 2);  // Remove the thruster.

    SECTION("a mobile vessel still needs one") {
        bp.mobile = true;
        CHECK(sr::Validate(bp, library).HasRule(sr::ValidationRule::Propulsion));
    }
    SECTION("a station does not") {
        bp.mobile = false;
        CHECK_FALSE(sr::Validate(bp, library).HasRule(sr::ValidationRule::Propulsion));
    }
}

TEST_CASE("Rule 6 -- a dangling parent is reported distinctly", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts.back().attachedTo = sr::MountId("nonexistent");

    const sr::ValidationResult result = sr::Validate(bp, library);
    CHECK(result.HasRule(sr::ValidationRule::MountParent));
}

TEST_CASE("Rule 6 -- exactly one root is required", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts[1].attachedTo = sr::MountId("");  // A second root.

    CHECK(sr::Validate(bp, library).HasRule(sr::ValidationRule::MountParent));
}

TEST_CASE("Rule 7 -- a floating island is unreachable from the root", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    // Two mounts attached to each other and to nothing else: a disconnected ring.
    bp.rig.mounts.push_back(Mount("island_a", "gun", "island_b", {"cannon"}));
    bp.rig.mounts.push_back(Mount("island_b", "gun", "island_a", {"cannon"}));

    const sr::ValidationResult result = sr::Validate(bp, library);
    CHECK(result.HasRule(sr::ValidationRule::Adjacency));
}

TEST_CASE("Rule 8 -- an unknown module id names the offending id", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts.back().modules = {sr::ModuleId("does_not_exist")};

    const sr::ValidationResult result = sr::Validate(bp, library);
    REQUIRE(result.HasRule(sr::ValidationRule::UnresolvedId));

    bool named = false;
    for (const auto& error : result.errors) {
        if (error.message.find("does_not_exist") != std::string::npos) {
            named = true;
        }
    }
    // features.md section 2.3: distinct, specific messages -- never a generic "invalid template".
    CHECK(named);
}

TEST_CASE("Rule 9 -- a missing schemaVersion is rejected, not assumed current", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.schemaVersion = 0;

    CHECK(sr::Validate(bp, library).HasRule(sr::ValidationRule::Identity));
}

TEST_CASE("A module may not mount in an incompatible shell", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts[1].modules = {sr::ModuleId("cannon")};  // A weapon in a power bay.

    CHECK(sr::Validate(bp, library).HasRule(sr::ValidationRule::ModuleCompatibility));
}

TEST_CASE("A weapon mount with no authored traverse fails validation, naming the mount",
          "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts.back().traverseRadians = 0.0f;  // gun_nose, omitted rather than authored.

    const sr::ValidationResult result = sr::Validate(bp, library);
    REQUIRE(result.HasRule(sr::ValidationRule::WeaponTraverse));

    bool named = false;
    for (const auto& error : result.errors) {
        if (error.rule == sr::ValidationRule::WeaponTraverse &&
            error.message.find("gun_nose") != std::string::npos) {
            named = true;
        }
    }
    CHECK(named);
}

TEST_CASE("Rule 12 -- a non-structural mount hanging off another non-structural mount is rejected",
          "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    // A second gun attached to gun_nose (Weapon kind) instead of to the chassis or armour --
    // nothing structural backs it.
    bp.rig.mounts.push_back(Mount("gun_second", "gun", "gun_nose", {"cannon"}, 0.35f));

    const sr::ValidationResult result = sr::Validate(bp, library);
    REQUIRE(result.HasRule(sr::ValidationRule::StructuralCoverage));

    bool named = false;
    for (const auto& error : result.errors) {
        if (error.rule == sr::ValidationRule::StructuralCoverage &&
            error.message.find("gun_second") != std::string::npos) {
            named = true;
        }
    }
    CHECK(named);
}

TEST_CASE("Rule 12 -- a non-structural mount backed by armour is accepted", "[validation]") {
    FakeLibrary library = MakeLibrary();
    library.AddShell("armor", sr::ShellKind::Armor);

    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts.push_back(Mount("flank", "armor", "core", {}));
    bp.rig.mounts.push_back(Mount("gun_second", "gun", "flank", {"cannon"}, 0.35f));

    const sr::ValidationResult result = sr::Validate(bp, library);
    CHECK_FALSE(result.HasRule(sr::ValidationRule::StructuralCoverage));
}

TEST_CASE("Rule 10 -- two mounts closer than the sum of their radii are rejected", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    // Both individually touch the chassis at exactly its attachment bound (16 units, rule 11 is
    // satisfied for each) but sit only 30 degrees apart on that ring -- too close to each other.
    bp.rig.mounts.push_back(
        Mount("gun_a", "gun", "core", {"cannon"}, 0.35f, sr::Vec2{16.0f, 0.0f}));
    bp.rig.mounts.push_back(
        Mount("gun_b", "gun", "core", {"cannon"}, 0.35f, sr::Vec2{13.8564f, 8.0f}));

    const sr::ValidationResult result = sr::Validate(bp, library);
    CHECK(result.HasRule(sr::ValidationRule::Separation));
}

TEST_CASE("Rule 11 -- a mount beyond its parent's extent is rejected", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    // 30 units from a chassis whose attachment bound (rule 11) is 16 (8 + 8), and far enough from
    // every other mount that separation (rule 10) stays clean -- isolates the failure to rule 11.
    bp.rig.mounts.push_back(
        Mount("drifter", "gun", "core", {"cannon"}, 0.35f, sr::Vec2{0.0f, 30.0f}));

    const sr::ValidationResult result = sr::Validate(bp, library);
    REQUIRE(result.HasRule(sr::ValidationRule::Attachment));
    CHECK_FALSE(result.HasRule(sr::ValidationRule::Separation));

    bool named = false;
    for (const auto& error : result.errors) {
        if (error.rule == sr::ValidationRule::Attachment &&
            error.message.find("drifter") != std::string::npos) {
            named = true;
        }
    }
    CHECK(named);
}

TEST_CASE("The scale table's fighter ring at p = 0.25R fits 9 peripherals, not 10",
          "[validation]") {
    const FakeLibrary library = MakeRingLibrary(12.0f, 6.0f);
    CHECK(RingGeometryValid(sr::Validate(MakeRingShip(12.0f, 6.0f, 9), library)));
    CHECK_FALSE(RingGeometryValid(sr::Validate(MakeRingShip(12.0f, 6.0f, 10), library)));
}

TEST_CASE("The scale table's fighter ring at p = 0.1R fits 18 peripherals, not 19",
          "[validation]") {
    const FakeLibrary library = MakeRingLibrary(12.0f, 2.5f);
    CHECK(RingGeometryValid(sr::Validate(MakeRingShip(12.0f, 2.5f, 18), library)));
    CHECK_FALSE(RingGeometryValid(sr::Validate(MakeRingShip(12.0f, 2.5f, 19), library)));
}

TEST_CASE("The scale table's capital-scale ring fits 16 peripherals, not 17", "[validation]") {
    const FakeLibrary library = MakeRingLibrary(625.0f, 150.0f);
    CHECK(RingGeometryValid(sr::Validate(MakeRingShip(625.0f, 150.0f, 16), library)));
    CHECK_FALSE(RingGeometryValid(sr::Validate(MakeRingShip(625.0f, 150.0f, 17), library)));
}

TEST_CASE("A mount may not exceed its shell's slot count", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.rig.mounts.back().modules = {sr::ModuleId("cannon"), sr::ModuleId("cannon")};

    CHECK(sr::Validate(bp, library).HasRule(sr::ValidationRule::MountCapacity));
}

TEST_CASE("Validation reports every violated rule, not just the first", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    sr::ShipBlueprint bp = MakeValidShip();
    bp.structuralMassLimit = 1.0f;
    bp.schemaVersion = 99;
    bp.rig.mounts.erase(bp.rig.mounts.begin() + 1);  // Also kills the power source.

    const sr::ValidationResult result = sr::Validate(bp, library);
    CHECK(result.HasRule(sr::ValidationRule::MassLimit));
    CHECK(result.HasRule(sr::ValidationRule::Identity));
    CHECK(result.HasRule(sr::ValidationRule::PowerSource));
}

TEST_CASE("ComputeTotals sums shells and modules together", "[validation]") {
    const FakeLibrary library = MakeLibrary();
    const sr::RigTotals totals = sr::ComputeTotals(MakeValidShip(), library);

    CHECK(totals.mass == 80.0f);  // 4 shells at 10 + 4 modules at 10.
    CHECK(totals.powerGeneration == 100.0f);
    CHECK(totals.powerDraw == 35.0f);  // engine 20 + cannon 15.
    CHECK(totals.PowerBalance() == 65.0f);
}
