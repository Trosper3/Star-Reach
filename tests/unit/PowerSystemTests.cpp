#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/PowerSystem.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"

using Catch::Approx;
using sr::Destroyed;
using sr::PowerBudget;
using sr::PowerLoad;
using sr::PowerShed;
using sr::PowerSource;
using sr::Rig;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace power_system = sr::space::power_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

entt::entity MakeHardpoint(entt::registry& registry, Rig& rig) {
    const entt::entity hardpoint = registry.create();
    rig.children.push_back(hardpoint);
    return hardpoint;
}

}  // namespace

TEST_CASE("PowerSystem gives full satisfaction when generation covers draw", "[power]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 100.0f);
    const entt::entity load = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(load, 60.0f, 2);
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);

    power_system::Tick(MakeContext(world, intents, content));

    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.generation == Approx(100.0f));
    CHECK(budget.draw == Approx(60.0f));
    CHECK(budget.satisfaction == Approx(1.0f));
    CHECK_FALSE(registry.all_of<PowerShed>(load));
}

TEST_CASE("PowerSystem throttles proportionally under a mild overdraw", "[power]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 100.0f);
    const entt::entity load = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(load, 110.0f, 2);  // Ratio 1.1 -- inside the 1.25 throttle band.
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);

    power_system::Tick(MakeContext(world, intents, content));

    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.satisfaction == Approx(100.0f / 110.0f));
    CHECK_FALSE(registry.all_of<PowerShed>(load));
}

TEST_CASE("PowerSystem sheds the lowest-priority load first under severe overdraw", "[power]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 100.0f);
    const entt::entity facility = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(facility, 90.0f, 0);  // Lowest priority -- sheds first.
    const entt::entity engine = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(engine, 60.0f, 3);  // Highest priority -- sheds last.
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);

    power_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<PowerShed>(facility));
    CHECK_FALSE(registry.all_of<PowerShed>(engine));
    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.satisfaction == Approx(1.0f));
}

TEST_CASE("PowerSystem ignores destroyed hardpoints' generation and draw", "[power]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity deadGenerator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(deadGenerator, 100.0f);
    registry.emplace<Destroyed>(deadGenerator);
    const entt::entity deadLoad = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(deadLoad, 500.0f, 2);
    registry.emplace<Destroyed>(deadLoad);
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);

    power_system::Tick(MakeContext(world, intents, content));

    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.generation == Approx(0.0f));
    CHECK(budget.draw == Approx(0.0f));
    CHECK(budget.satisfaction == Approx(1.0f));
}

TEST_CASE("PowerSystem clears a stale PowerShed tag once the budget recovers", "[power]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 100.0f);
    const entt::entity load = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(load, 60.0f, 2);
    registry.emplace<PowerShed>(load);  // Left over from a worse tick.
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);

    power_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<PowerShed>(load));
}
