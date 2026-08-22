#include <catch2/catch_test_macros.hpp>

#include <unordered_map>

#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/ui/CustomizeMenu.h"

namespace customize_menu = sr::space::ui::customize_menu;

namespace {

// Same shape as ValidationTests.cpp's FakeLibrary -- tests/ is one of the three places allowed
// to construct content types in C++ (Law 10).
class FakeLibrary final : public sr::DefLibrary {  // NOLINT(bugprone-exception-escape)
public:
    void AddShell(const char* id, sr::ShellKind kind, float mass = 10.0f, int slots = 1,
                  std::vector<sr::ModuleKind> acceptsKinds = {}) {
        sr::ShellDef def;
        def.id = sr::ShellId(id);
        def.displayName = id;
        def.kind = kind;
        def.hull = 100.0f;
        def.mass = mass;
        def.moduleSlots = slots;
        def.acceptsKinds = std::move(acceptsKinds);
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
    library.AddModule("plate", sr::ModuleKind::Armor);
    library.AddModule("cell", sr::ModuleKind::PowerCell, 10.0f, 0.0f, 100.0f);
    library.AddModule("engine", sr::ModuleKind::Engine, 10.0f, 20.0f);
    return library;
}

// MakeLibrary()'s shells all keep ShellDef::radius at its 8.0f default, so 16 units (r(A) + r(B))
// from the chassis root satisfies rule 11's attachment bound exactly, and opposite sides of the
// chassis keep rule 10's separation bound comfortably clear (features.md 3.5).
sr::ShipBlueprint MakeValidDraft() {
    sr::ShipBlueprint draft = customize_menu::NewDraft();
    draft.id = sr::BlueprintId("player_design");
    draft.displayName = "Player Design";
    draft.mobile = true;
    draft.structuralMassLimit = 500.0f;

    customize_menu::AddMount(draft, sr::MountId("core"), sr::ShellId("chassis"), sr::MountId(""),
                             sr::Vec2{});
    customize_menu::EquipModule(draft, sr::MountId("core"), sr::ModuleId("plate"));

    customize_menu::AddMount(draft, sr::MountId("reactor"), sr::ShellId("power_bay"),
                             sr::MountId("core"), sr::Vec2{16.0f, 0.0f});
    customize_menu::EquipModule(draft, sr::MountId("reactor"), sr::ModuleId("cell"));

    customize_menu::AddMount(draft, sr::MountId("thruster_main"), sr::ShellId("thruster"),
                             sr::MountId("core"), sr::Vec2{-16.0f, 0.0f});
    customize_menu::EquipModule(draft, sr::MountId("thruster_main"), sr::ModuleId("engine"));

    return draft;
}

}  // namespace

TEST_CASE("NewDraft is an empty, unnamed blueprint", "[customize-menu]") {
    const sr::ShipBlueprint draft = customize_menu::NewDraft();
    CHECK(draft.rig.mounts.empty());
    CHECK(draft.id.empty());
}

TEST_CASE("AddMount appends a mount with the given fields", "[customize-menu]") {
    sr::ShipBlueprint draft = customize_menu::NewDraft();
    customize_menu::AddMount(draft, sr::MountId("core"), sr::ShellId("chassis"), sr::MountId(""),
                             sr::Vec2{1.0f, 2.0f});

    REQUIRE(draft.rig.mounts.size() == 1);
    CHECK(draft.rig.mounts.front().id == sr::MountId("core"));
    CHECK(draft.rig.mounts.front().shell == sr::ShellId("chassis"));
    CHECK(draft.rig.mounts.front().localOffset == sr::Vec2{1.0f, 2.0f});
}

TEST_CASE("EquipModule attaches to the named mount and no-ops for an unknown one",
          "[customize-menu]") {
    sr::ShipBlueprint draft = customize_menu::NewDraft();
    customize_menu::AddMount(draft, sr::MountId("core"), sr::ShellId("chassis"), sr::MountId(""),
                             sr::Vec2{});

    customize_menu::EquipModule(draft, sr::MountId("core"), sr::ModuleId("plate"));
    customize_menu::EquipModule(draft, sr::MountId("missing"), sr::ModuleId("plate"));

    REQUIRE(draft.rig.mounts.front().modules.size() == 1);
    CHECK(draft.rig.mounts.front().modules.front() == sr::ModuleId("plate"));
}

TEST_CASE("CanSave is true for a well-formed draft and false for an empty one",
          "[customize-menu]") {
    const FakeLibrary library = MakeLibrary();
    CHECK(customize_menu::CanSave(MakeValidDraft(), library));
    CHECK_FALSE(customize_menu::CanSave(customize_menu::NewDraft(), library));
}

TEST_CASE("BuildSaveRequest carries the actor, network, and blueprint through",
          "[customize-menu]") {
    const sr::ShipBlueprint draft = MakeValidDraft();
    const sr::core::SaveTemplateIntent intent =
        customize_menu::BuildSaveRequest(sr::ActorId{7}, sr::KnowledgeNetworkId("net"), draft);

    CHECK(intent.actor.value == 7);
    CHECK(intent.targetNetwork == sr::KnowledgeNetworkId("net"));
    CHECK(intent.blueprint.id == sr::BlueprintId("player_design"));
    CHECK(intent.blueprint.rig.mounts.size() == draft.rig.mounts.size());
}

// ConsumeSaveTemplateRequests moved to modes/space/systems/ConstructionSystem.cpp
// (architecture.md 12.30.8) -- its tests now live in ConstructionSystemTests.cpp, next to the
// concrete core::ContentLibrary the Template overlay needs.
