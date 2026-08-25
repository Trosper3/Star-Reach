#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"

using Catch::Approx;
using sr::CargoHold;
using sr::Destroyed;
using sr::ItemKind;
using sr::ItemStack;
using sr::MountId;
using sr::MountRef;
using sr::Rig;
namespace cargo_view = sr::cargo_view;

namespace {

entt::entity MakeBay(entt::registry& registry, entt::entity root, const std::string& mountId,
                     int slotCount, float slotCapacity) {
    const entt::entity bay = registry.create();
    registry.emplace<sr::ParentRig>(bay, root);
    registry.emplace<MountRef>(bay, MountId(mountId));
    registry.emplace<CargoHold>(bay, std::vector<ItemStack>{}, slotCount, slotCapacity);
    return bay;
}

}  // namespace

TEST_CASE("Capacity sums slotCount * slotCapacity across living bays, skipping Destroyed",
          "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bayA = MakeBay(registry, root, "bay_a", 4, 250.0f);
    const entt::entity bayB = MakeBay(registry, root, "bay_b", 2, 50.0f);
    const entt::entity deadBay = MakeBay(registry, root, "bay_dead", 10, 1000.0f);
    registry.emplace<Destroyed>(deadBay);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bayA, bayB, deadBay});

    // 4*250 + 2*50, not + 10*1000 -- the dead bay's capacity does not count.
    CHECK(cargo_view::Capacity(registry, root) == Approx(1100.0f));
}

TEST_CASE("Deposit fills an empty bay and TotalMass reflects it", "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bay = MakeBay(registry, root, "bay", 4, 250.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});

    const auto result =
        cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "iron", 10, 2.0f});

    CHECK(result == cargo_view::DepositResult::Deposited);
    CHECK(cargo_view::TotalMass(registry, root) == Approx(20.0f));
    REQUIRE(registry.get<CargoHold>(bay).stacks.size() == 1);
    CHECK(registry.get<CargoHold>(bay).stacks.front().quantity == 10);
}

TEST_CASE("Deposit tops up an existing matching Element stack before opening a new slot",
          "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bay = MakeBay(registry, root, "bay", 4, 250.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "iron", 3, 2.0f});

    const auto result =
        cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "iron", 2, 2.0f});

    CHECK(result == cargo_view::DepositResult::Deposited);
    const CargoHold& cargo = registry.get<CargoHold>(bay);
    REQUIRE(cargo.stacks.size() == 1);  // Still one stack, not two.
    CHECK(cargo.stacks.front().quantity == 5);
}

TEST_CASE("Deposit never merges Module stacks -- two of the same id occupy two slots",
          "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bay = MakeBay(registry, root, "bay", 4, 250.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});

    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 14.0f});
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 14.0f});

    CHECK(registry.get<CargoHold>(bay).stacks.size() == 2);
}

TEST_CASE("A deposit that fits in no single bay but fits across two succeeds", "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bayA = MakeBay(registry, root, "bay_a", 1, 10.0f);
    const entt::entity bayB = MakeBay(registry, root, "bay_b", 1, 10.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bayA, bayB});

    // 15 units at 1.0 mass each = 15 mass, but no single 10-capacity slot can hold it whole.
    const auto result =
        cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "carbon", 15, 1.0f});

    CHECK(result == cargo_view::DepositResult::Deposited);
    CHECK(cargo_view::TotalMass(registry, root) == Approx(15.0f));
}

TEST_CASE(
    "Deposit refuses HoldFull when a matching stack and every free slot still cannot fit "
    "the whole quantity",
    "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bay = MakeBay(registry, root, "bay", 1, 10.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});
    // Bay already has an "iron" stack filling the one slot -- no free slot, but a matching stack
    // exists (with no room left).
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "iron", 5, 2.0f});

    const auto result =
        cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "iron", 1, 2.0f});

    CHECK(result == cargo_view::DepositResult::HoldFull);
    // Refused whole -- nothing partially applied.
    CHECK(registry.get<CargoHold>(bay).stacks.front().quantity == 5);
}

TEST_CASE("Deposit refuses NoFreeSlot for a brand-new item type when no slot is free anywhere",
          "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bay = MakeBay(registry, root, "bay", 1, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});
    // The one slot is occupied by a different item entirely -- "mass to spare" on that slot
    // (only 5 of 100 used) does not help a new item type with nowhere to start.
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "iron", 1, 5.0f});

    const auto result =
        cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "carbon", 1, 1.0f});

    CHECK(result == cargo_view::DepositResult::NoFreeSlot);
}

TEST_CASE(
    "Deposit places into the emptiest bay first, tie-broken by MountId and independent of "
    "Rig::children order",
    "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    // bay_z is listed FIRST in Rig::children but has the alphabetically LATER MountId -- if
    // placement ever fell back to children order, this test would catch it. Both bays start
    // equally empty (0 of N slots used), so the tie-break is all that decides it.
    const entt::entity bayZ = MakeBay(registry, root, "bay_z", 2, 100.0f);
    const entt::entity bayA = MakeBay(registry, root, "bay_a", 2, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bayZ, bayA});

    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Element, "iron", 1, 1.0f});

    CHECK(registry.get<CargoHold>(bayA).stacks.size() == 1);
    CHECK(registry.get<CargoHold>(bayZ).stacks.empty());
}

TEST_CASE("Withdraw drains the fullest matching slot first", "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bayA = MakeBay(registry, root, "bay_a", 2, 100.0f);
    const entt::entity bayB = MakeBay(registry, root, "bay_b", 2, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bayA, bayB});
    registry.get<CargoHold>(bayA).stacks.push_back(ItemStack{ItemKind::Element, "iron", 3, 1.0f});
    registry.get<CargoHold>(bayB).stacks.push_back(ItemStack{ItemKind::Element, "iron", 8, 1.0f});

    const bool ok = cargo_view::Withdraw(registry, root, ItemKind::Element, "iron", 5);

    CHECK(ok);
    // bayB (fuller, 8) drains first: 8 - 5 = 3 remaining there; bayA's 3 untouched.
    CHECK(registry.get<CargoHold>(bayB).stacks.front().quantity == 3);
    CHECK(registry.get<CargoHold>(bayA).stacks.front().quantity == 3);
}

TEST_CASE("Withdraw refuses whole and writes nothing when the rig does not hold enough",
          "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bay = MakeBay(registry, root, "bay", 2, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});
    registry.get<CargoHold>(bay).stacks.push_back(ItemStack{ItemKind::Element, "iron", 3, 1.0f});

    const bool ok = cargo_view::Withdraw(registry, root, ItemKind::Element, "iron", 10);

    CHECK_FALSE(ok);
    CHECK(registry.get<CargoHold>(bay).stacks.front().quantity == 3);  // Untouched.
}

TEST_CASE("Withdraw removes a stack entirely once it empties", "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bay = MakeBay(registry, root, "bay", 2, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});
    registry.get<CargoHold>(bay).stacks.push_back(ItemStack{ItemKind::Element, "iron", 3, 1.0f});

    CHECK(cargo_view::Withdraw(registry, root, ItemKind::Element, "iron", 3));

    CHECK(registry.get<CargoHold>(bay).stacks.empty());
}

TEST_CASE("A Destroyed bay's capacity and contents drop out, and the other bay is untouched",
          "[cargo-view]") {
    // "Blowing off a cargo bay reduces capacity and spills nothing that was never there" --
    // CargoView's own half of that: Destroyed excludes a bay from every rig-level view.
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity survivingBay = MakeBay(registry, root, "bay_a", 2, 100.0f);
    const entt::entity destroyedBay = MakeBay(registry, root, "bay_b", 2, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{survivingBay, destroyedBay});
    registry.get<CargoHold>(survivingBay)
        .stacks.push_back(ItemStack{ItemKind::Element, "iron", 5, 1.0f});
    registry.get<CargoHold>(destroyedBay)
        .stacks.push_back(ItemStack{ItemKind::Element, "titanium", 5, 3.0f});

    CHECK(cargo_view::Capacity(registry, root) == Approx(400.0f));  // Both bays, still alive.

    registry.emplace<Destroyed>(destroyedBay);

    CHECK(cargo_view::Capacity(registry, root) == Approx(200.0f));  // Only the surviving bay.
    const std::vector<ItemStack> merged = cargo_view::Merged(registry, root);
    REQUIRE(merged.size() == 1);
    CHECK(merged.front().id == "iron");  // The destroyed bay's titanium is gone from the view.
    // architecture.md 12.30.3's "Sibling holds": the row names which bay it survived in.
    CHECK(merged.front().hardpoint == survivingBay);
}

TEST_CASE("Merged stamps each stack with the bay it came from", "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bayA = MakeBay(registry, root, "bay_a", 2, 100.0f);
    const entt::entity bayB = MakeBay(registry, root, "bay_b", 2, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bayA, bayB});
    registry.get<CargoHold>(bayA).stacks.push_back(ItemStack{ItemKind::Element, "iron", 3, 1.0f});
    registry.get<CargoHold>(bayB).stacks.push_back(ItemStack{ItemKind::Element, "iron", 8, 1.0f});

    const std::vector<ItemStack> merged = cargo_view::Merged(registry, root);

    REQUIRE(merged.size() == 2);
    CHECK(merged[0].hardpoint == bayA);
    CHECK(merged[1].hardpoint == bayB);
}

TEST_CASE("The destination-choosing Deposit overload lands in the chosen bay, not the emptiest",
          "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    // bayEmpty has nothing in it -- the auto-routed overload would pick it first. bayChosen
    // already holds something, making it the LESS empty of the two.
    const entt::entity bayEmpty = MakeBay(registry, root, "bay_empty", 4, 100.0f);
    const entt::entity bayChosen = MakeBay(registry, root, "bay_chosen", 4, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bayEmpty, bayChosen});
    registry.get<CargoHold>(bayChosen).stacks.push_back(
        ItemStack{ItemKind::Element, "titanium", 1, 3.0f});

    const auto result = cargo_view::Deposit(registry, root, bayChosen,
                                            ItemStack{ItemKind::Element, "iron", 5, 1.0f});

    CHECK(result == cargo_view::DepositResult::Deposited);
    CHECK(registry.get<CargoHold>(bayEmpty).stacks.empty());  // Never auto-routed here.
    const CargoHold& chosen = registry.get<CargoHold>(bayChosen);
    REQUIRE(chosen.stacks.size() == 2);
    CHECK(chosen.stacks[1].id == "iron");
    CHECK(chosen.stacks[1].quantity == 5);
}

TEST_CASE(
    "The destination-choosing Deposit overload refuses HoldFull without spilling into a sibling "
    "bay that has room",
    "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bayFull = MakeBay(registry, root, "bay_full", 1, 10.0f);
    const entt::entity bayRoomy = MakeBay(registry, root, "bay_roomy", 4, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bayFull, bayRoomy});
    // bayFull's one slot is already full of "iron" -- a matching stack exists, but with no room.
    registry.get<CargoHold>(bayFull).stacks.push_back(
        ItemStack{ItemKind::Element, "iron", 5, 2.0f});

    const auto result =
        cargo_view::Deposit(registry, root, bayFull, ItemStack{ItemKind::Element, "iron", 1, 2.0f});

    CHECK(result == cargo_view::DepositResult::HoldFull);
    CHECK(registry.get<CargoHold>(bayFull).stacks.front().quantity == 5);  // Refused whole.
    CHECK(registry.get<CargoHold>(bayRoomy).stacks.empty());  // Untouched -- the pick was final.
}

TEST_CASE(
    "The destination-choosing Deposit overload refuses a hold that is not living or not this rig's",
    "[cargo-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bay = MakeBay(registry, root, "bay", 4, 100.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});
    registry.emplace<Destroyed>(bay);

    const auto result =
        cargo_view::Deposit(registry, root, bay, ItemStack{ItemKind::Element, "iron", 1, 1.0f});

    CHECK(result == cargo_view::DepositResult::NoFreeSlot);
}
