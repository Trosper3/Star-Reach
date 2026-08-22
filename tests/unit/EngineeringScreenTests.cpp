#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/EngineeringScreen.h"
#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"

using sr::CargoHold;
using sr::Destroyed;
using sr::DisplayName;
using sr::Docked;
using sr::FactionId;
using sr::FactionRef;
using sr::Health;
using sr::ItemKind;
using sr::ItemStack;
using sr::ModuleId;
using sr::MountBlueprint;
using sr::MountId;
using sr::MountRef;
using sr::ParentRig;
using sr::Rig;
using sr::RigBlueprint;
using sr::ShellRole;
using sr::StructuralAttachment;
namespace engineering_screen = sr::space::ui::engineering_screen;

TEST_CASE("ModuleRows lists only ItemKind::Module stacks across cargo bays", "[engineering]") {
    entt::registry registry;
    const entt::entity requester = registry.create();
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, requester);
    registry.emplace<CargoHold>(
        bay,
        std::vector<ItemStack>{ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 4.0f},
                               ItemStack{ItemKind::Element, "Fe", 3, 1.0f}},
        10, 100.0f);
    registry.emplace<Rig>(requester, std::vector<entt::entity>{bay});

    const auto rows = engineering_screen::ModuleRows(registry, requester);
    REQUIRE(rows.size() == 1);
    CHECK(rows.front().module == ModuleId("pulse_cannon_i"));
}

TEST_CASE("DeletableHardpoints excludes a hardpoint another hardpoint is attached to",
          "[engineering]") {
    entt::registry registry;
    const entt::entity leaf = registry.create();
    const entt::entity parent = registry.create();
    registry.emplace<StructuralAttachment>(leaf, parent);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root, std::vector<entt::entity>{parent, leaf});

    const auto deletable = engineering_screen::DeletableHardpoints(registry, root);
    REQUIRE(deletable.size() == 1);
    CHECK(deletable.front() == leaf);
}

TEST_CASE("RebuildableMounts names an authored mount with no live entity", "[engineering]") {
    entt::registry registry;
    const entt::entity core = registry.create();
    registry.emplace<MountRef>(core, MountId("core"));

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root, std::vector<entt::entity>{core});

    RigBlueprint blueprint;
    MountBlueprint coreMount;
    coreMount.id = MountId("core");
    blueprint.mounts.push_back(coreMount);
    MountBlueprint wingMount;
    wingMount.id = MountId("wing_port");
    wingMount.attachedTo = MountId("core");
    blueprint.mounts.push_back(wingMount);

    const auto missing = engineering_screen::RebuildableMounts(registry, root, blueprint);
    REQUIRE(missing.size() == 1);
    CHECK(missing.front() == MountId("wing_port"));
}

TEST_CASE("RebuildableMounts still names a mount whose entity is Destroyed", "[engineering]") {
    // A Destroyed hardpoint is not "absent" -- Delete must remove it first (RefactorSystem's own
    // FindMount-based refusal). RebuildableMounts here is the pure-function half of that same
    // rule: present-but-Destroyed does not count as missing.
    entt::registry registry;
    const entt::entity wingPort = registry.create();
    registry.emplace<MountRef>(wingPort, MountId("wing_port"));
    registry.emplace<Destroyed>(wingPort);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root, std::vector<entt::entity>{wingPort});

    RigBlueprint blueprint;
    MountBlueprint wingMount;
    wingMount.id = MountId("wing_port");
    blueprint.mounts.push_back(wingMount);

    CHECK(engineering_screen::RebuildableMounts(registry, root, blueprint).empty());
}

TEST_CASE("MountRows reports Living, Destroyed and Absent rows with the right verb",
          "[engineering]") {
    entt::registry registry;
    const entt::entity living = registry.create();
    registry.emplace<MountRef>(living, MountId("wing_port"));
    registry.emplace<Health>(living, 20.0f, 20.0f);
    registry.emplace<ShellRole>(living, sr::ShellKind::Weapon);

    const entt::entity destroyed = registry.create();
    registry.emplace<MountRef>(destroyed, MountId("emitter"));
    registry.emplace<Destroyed>(destroyed);
    registry.emplace<Health>(destroyed, 0.0f, 10.0f);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root, std::vector<entt::entity>{living, destroyed});

    RigBlueprint blueprint;
    for (const char* id : {"wing_port", "emitter", "cockpit"}) {
        MountBlueprint mount;
        mount.id = MountId(id);
        blueprint.mounts.push_back(mount);
    }

    const auto rows = engineering_screen::MountRows(registry, root, blueprint);
    REQUIRE(rows.size() == 3);

    CHECK(rows[0].hardpoint == living);
    CHECK_FALSE(rows[0].destroyed);
    CHECK(rows[0].row.value == "DELETE");
    CHECK_FALSE(rows[0].row.style.disabled);

    CHECK(rows[1].hardpoint == destroyed);
    CHECK(rows[1].destroyed);
    CHECK(rows[1].row.value == "DESTROYED");
    CHECK(rows[1].row.style.disabled);

    CHECK((rows[2].hardpoint == entt::null));
    CHECK(rows[2].row.value == "REBUILD");
    CHECK_FALSE(rows[2].row.style.disabled);  // No attachedTo -- nothing to be orphaned by.
}

TEST_CASE("MountRows greys out an Absent row whose attachedTo parent is also absent",
          "[engineering]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<Rig>(root, std::vector<entt::entity>{});

    RigBlueprint blueprint;
    MountBlueprint hold;
    hold.id = MountId("hold");
    blueprint.mounts.push_back(hold);
    MountBlueprint cockpit;
    cockpit.id = MountId("cockpit");
    cockpit.attachedTo = MountId("hold");
    blueprint.mounts.push_back(cockpit);

    const auto rows = engineering_screen::MountRows(registry, root, blueprint);
    REQUIRE(rows.size() == 2);
    CHECK(rows[1].mount == MountId("cockpit"));
    CHECK(rows[1].row.style.disabled);
}

TEST_CASE("OwnedVesselAt finds the docked vessel matching the player's faction", "[engineering]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity owned = registry.create();
    registry.emplace<Docked>(owned, station, entt::null);
    registry.emplace<FactionRef>(owned, FactionId("aegis_directorate"));
    const entt::entity foreign = registry.create();
    registry.emplace<Docked>(foreign, station, entt::null);
    registry.emplace<FactionRef>(foreign, FactionId("scrappers"));

    CHECK(engineering_screen::OwnedVesselAt(registry, station, FactionId("aegis_directorate")) ==
          owned);
    CHECK(
        (engineering_screen::OwnedVesselAt(registry, station, FactionId("nobody")) == entt::null));
}
