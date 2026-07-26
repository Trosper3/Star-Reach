#include <catch2/catch_test_macros.hpp>

#include <unordered_map>

#include "shared/blueprints/Validation.h"

namespace {

// A hand-built library. tests/ is one of the three places allowed to construct content types in
// C++ (Law 10) precisely so a validation test does not need a data file to run.
class FakeLibrary final : public sr::DefLibrary {
public:
    void AddShell(const char* id, sr::ShellKind kind, float mass = 10.0f, int slots = 1) {
        sr::ShellDef def;
        def.id = sr::ShellId(id);
        def.displayName = id;
        def.kind = kind;
        def.hull = 100.0f;
        def.mass = mass;
        def.moduleSlots = slots;
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
    library.AddShell("power_bay", sr::ShellKind::PowerCell);
    library.AddShell("thruster", sr::ShellKind::Engine);
    library.AddShell("gun", sr::ShellKind::Weapon);
    library.AddModule("plate", sr::ModuleKind::Armor);
    library.AddModule("cell", sr::ModuleKind::PowerCell, 10.0f, 0.0f, 100.0f);
    library.AddModule("engine", sr::ModuleKind::Engine, 10.0f, 20.0f);
    library.AddModule("cannon", sr::ModuleKind::Weapon, 10.0f, 15.0f);
    return library;
}

sr::MountBlueprint Mount(const char* id, const char* shell, const char* attachedTo,
                         std::vector<const char*> modules) {
    sr::MountBlueprint mount;
    mount.id = sr::MountId(id);
    mount.shell = sr::ShellId(shell);
    mount.attachedTo = sr::MountId(attachedTo);
    for (const char* module : modules) {
        mount.modules.push_back(sr::ModuleId(module));
    }
    return mount;
}

sr::ShipBlueprint MakeValidShip() {
    sr::ShipBlueprint bp;
    bp.id = sr::BlueprintId("test_fighter");
    bp.schemaVersion = sr::kBlueprintSchemaVersion;
    bp.displayName = "Test Fighter";
    bp.mobile = true;
    bp.structuralMassLimit = 500.0f;
    bp.rig.mounts = {
        Mount("core", "chassis", "", {"plate"}),
        Mount("reactor", "power_bay", "core", {"cell"}),
        Mount("thruster_main", "thruster", "core", {"engine"}),
        Mount("gun_nose", "gun", "core", {"cannon"}),
    };
    return bp;
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

    SECTION("a mobile craft still needs one") {
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
