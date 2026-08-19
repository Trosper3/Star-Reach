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
using sr::PowerAllocation;
using sr::PowerBudget;
using sr::PowerCategory;
using sr::PowerLevel;
using sr::PowerLevels;
using sr::PowerLoad;
using sr::PowerPriorityList;
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

TEST_CASE("PowerSystem funds every category at Normal when generation covers draw", "[power]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 100.0f);
    const entt::entity load = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(load, 60.0f, PowerLevels{}, PowerCategory::Weapons);
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);

    power_system::Tick(MakeContext(world, intents, content));

    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.generation == Approx(100.0f));
    CHECK(budget.draw == Approx(60.0f));
    CHECK(budget.weapons == Approx(1.0f));
    CHECK_FALSE(registry.all_of<PowerShed>(load));
}

TEST_CASE("PowerSystem refuses a Boosted category without headroom rather than browning out",
          "[power]") {
    // features.md 2.9: "boost simply does not engage without headroom." The category clamps
    // back to Normal for the budget rather than PowerSystem shedding something else to fund it.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 100.0f);
    const entt::entity weapon = MakeHardpoint(registry, rig);
    PowerLevels weaponLevels;
    weaponLevels.boosted = {2.0f, 1.5f};  // Boosted draw (2x) alone already exceeds generation.
    registry.emplace<PowerLoad>(weapon, 60.0f, weaponLevels, PowerCategory::Weapons);
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);
    registry.emplace<PowerAllocation>(root, PowerLevel::Boosted, PowerLevel::Normal,
                                      PowerLevel::Normal, PowerLevel::Normal);

    power_system::Tick(MakeContext(world, intents, content));

    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.weapons == Approx(1.0f));  // Refused: reads as Normal's effect, not Boosted's.
    CHECK(budget.draw == Approx(60.0f));    // Normal draw, not the refused Boosted draw (120).
    CHECK_FALSE(registry.all_of<PowerShed>(weapon));
}

TEST_CASE("PowerSystem allows a Boosted category when the budget has headroom for it", "[power]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 200.0f);
    const entt::entity weapon = MakeHardpoint(registry, rig);
    PowerLevels weaponLevels;
    weaponLevels.boosted = {2.0f, 1.5f};
    registry.emplace<PowerLoad>(weapon, 60.0f, weaponLevels, PowerCategory::Weapons);
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);
    registry.emplace<PowerAllocation>(root, PowerLevel::Boosted, PowerLevel::Normal,
                                      PowerLevel::Normal, PowerLevel::Normal);

    power_system::Tick(MakeContext(world, intents, content));

    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.weapons == Approx(1.5f));
    CHECK(budget.draw == Approx(120.0f));
}

TEST_CASE("PowerSystem's Offline category stops output and frees exactly its authored draw",
          "[power]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 100.0f);
    const entt::entity engine = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(engine, 40.0f, PowerLevels{}, PowerCategory::Engines);
    const entt::entity weapon = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(weapon, 25.0f, PowerLevels{}, PowerCategory::Weapons);
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);
    registry.emplace<PowerAllocation>(root, PowerLevel::Normal, PowerLevel::Normal,
                                      PowerLevel::Offline, PowerLevel::Normal);

    power_system::Tick(MakeContext(world, intents, content));

    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.engines == Approx(0.0f));
    CHECK(budget.weapons == Approx(1.0f));
    // The engine's full 40.0 authored draw is freed -- only the weapon's 25.0 remains.
    CHECK(budget.draw == Approx(25.0f));
}

TEST_CASE(
    "PowerSystem sheds categories in the rig's configured PowerPriorityList order under a "
    "generation shortfall",
    "[power]") {
    // A severe shortfall even at every category's commanded (Normal) level -- the "dead power
    // cell" case architecture.md 12.16 item 18 keeps separate from an ordinary boost refusal.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity generator = MakeHardpoint(registry, rig);
    registry.emplace<PowerSource>(generator, 50.0f);
    const entt::entity shield = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(shield, 40.0f, PowerLevels{}, PowerCategory::Shields);
    const entt::entity engine = MakeHardpoint(registry, rig);
    registry.emplace<PowerLoad>(engine, 40.0f, PowerLevels{}, PowerCategory::Engines);
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);
    // Configured to shed engines before shields -- the reverse of the built-in default order.
    registry.emplace<PowerPriorityList>(
        root, std::array<PowerCategory, 4>{PowerCategory::Engines, PowerCategory::Shields,
                                           PowerCategory::Weapons, PowerCategory::Facilities});

    power_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<PowerShed>(engine));
    CHECK_FALSE(registry.all_of<PowerShed>(shield));
    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.engines == Approx(0.0f));
    CHECK(budget.shields == Approx(1.0f));
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
    registry.emplace<PowerLoad>(deadLoad, 500.0f, PowerLevels{}, PowerCategory::Weapons);
    registry.emplace<Destroyed>(deadLoad);
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);

    power_system::Tick(MakeContext(world, intents, content));

    const auto& budget = registry.get<PowerBudget>(root);
    CHECK(budget.generation == Approx(0.0f));
    CHECK(budget.draw == Approx(0.0f));
    CHECK(budget.weapons == Approx(1.0f));
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
    registry.emplace<PowerLoad>(load, 60.0f, PowerLevels{}, PowerCategory::Weapons);
    registry.emplace<PowerShed>(load);  // Left over from a worse tick.
    registry.emplace<Rig>(root, rig);
    registry.emplace<PowerBudget>(root);

    power_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<PowerShed>(load));
}
