#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/RefactorMenu.h"
#include "shared/components/Rig.h"

using sr::Rig;
using sr::StructuralAttachment;
namespace refactor_menu = sr::space::ui::refactor_menu;

TEST_CASE("DeletableHardpoints excludes a hardpoint another hardpoint is attached to",
          "[refactor-menu]") {
    entt::registry registry;
    const entt::entity leaf = registry.create();
    const entt::entity parent = registry.create();
    registry.emplace<StructuralAttachment>(leaf, parent);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root, std::vector<entt::entity>{parent, leaf});

    const auto deletable = refactor_menu::DeletableHardpoints(registry, root);
    REQUIRE(deletable.size() == 1);
    CHECK(deletable.front() == leaf);
}

TEST_CASE("DeletableHardpoints is empty for a rig with no Rig component", "[refactor-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    CHECK(refactor_menu::DeletableHardpoints(registry, root).empty());
}

TEST_CASE("BuildDeleteRequest carries the hardpoint through", "[refactor-menu]") {
    const entt::entity hardpoint = entt::entity{5};
    CHECK(refactor_menu::BuildDeleteRequest(hardpoint).hardpoint == hardpoint);
}
