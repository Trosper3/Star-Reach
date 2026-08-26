#pragma once

#include <cstdint>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <string>
#include <vector>

#include "shared/components/Loot.h"

// shared/rig/ -- beside ModuleAttachment.h, the same "one write path" promotion architecture.md
// 12.30.5 gives DockedFacility. CargoHold lives per cargo-bay hardpoint (Loot.h); every rig-level
// question about cargo -- how much room is there, does this deposit fit, what does the hold
// contain -- goes through here rather than each of the seven-plus callers walking Rig::children
// and CargoHold::stacks by hand.
//
// Like ModuleAttachment.h, this needs no ContentLibrary: a deposit arrives as an already-resolved
// ItemStack (kind, id, quantity, unitMass), because shared/ may not include core/ (architecture.md
// section 2.3). The caller (a modes/space/systems/ file, which does have ContentLibrary access)
// resolves the item's mass before calling Deposit -- the same denormalize-at-the-boundary split
// HardpointMass/EnginePropulsion already establish.
namespace sr::cargo_view {

// Sum of slotCount * slotCapacity across every living (non-Destroyed) cargo-bay hardpoint under
// rigRoot's Rig. Zero for a rig with no CargoBay module -- capability is emergent, never
// authored, the same rule RecomputeRigTotals applies to BodyMass/Propulsion/SensorRange. Never
// written back onto the root: the rig-level view stays derived, never a stored aggregate.
float Capacity(const entt::registry& registry, entt::entity rigRoot);

// Sum of every living bay's stacks' (quantity * unitMass).
float TotalMass(const entt::registry& registry, entt::entity rigRoot);

// Sum of slotCount across every living cargo-bay hardpoint -- Capacity's item-slot counterpart
// (Capacity is mass, this is the "34 / 50" count the inventory overlay's header wants alongside
// it, architecture.md 12.30.7). Zero for a rig with no CargoBay module, same as Capacity.
int SlotsTotal(const entt::registry& registry, entt::entity rigRoot);

// How many of SlotsTotal's slots are presently occupied -- every living bay's stacks.size(),
// summed. Never exceeds SlotsTotal outside of a bug: a bay only ever grows a new stacks() entry
// through Deposit, which already refuses once slotCount is full.
int SlotsUsed(const entt::registry& registry, entt::entity rigRoot);

// features.md's two named refusals (the row model's "why", not just a bool):
//   HoldFull    -- every slot that could take this item (a free slot, or an existing matching
//                  stack with room) is at capacity. The rig has room to spare elsewhere, just not
//                  for this deposit's full quantity.
//   NoFreeSlot  -- no free slot exists anywhere AND no existing stack of this item exists either,
//                  so a brand-new item type has nowhere at all to start.
enum class DepositResult : std::uint8_t { Deposited, HoldFull, NoFreeSlot };

// The one write path for adding cargo. Whole or nothing (architecture.md 12.30.3): if the full
// `stack.quantity` cannot be placed, nothing is written and the reason is reported. Tops up an
// existing matching stack with room first (Element only -- a Module stack never merges, see
// ItemStack's comment), then places into free slots emptiest-bay-first (tie-broken by MountId,
// never Rig::children order), splitting across slots and bays as needed.
DepositResult Deposit(entt::registry& registry, entt::entity rigRoot, const ItemStack& stack);

// The destination-choosing overload (architecture.md 12.30.3's "Sibling holds" follow-on,
// P4-11): places into `targetHold` alone -- never falling back to another living bay of
// `rigRoot`, which is the whole point of a chosen destination rather than an auto-routed one.
// Whole or nothing, same as the auto-routed overload above, restricted to one bay's own room:
// tops up a matching stack there first, then free slots there, splitting across `targetHold`'s
// own slots as needed but never spilling into a sibling bay. NoFreeSlot if `targetHold` is not
// presently one of `rigRoot`'s living CargoHold hardpoints (destroyed or already gone). The
// auto-routed overload stays the default for every caller but the Storage screen, which is the
// one place a player picks a specific hold.
DepositResult Deposit(entt::registry& registry, entt::entity rigRoot, entt::entity targetHold,
                      const ItemStack& stack);

// The other half of the one write path. Draws `quantity` of (kind, id) from rigRoot's living
// bays, fullest matching slot first, so bays drift back toward even and empty slots reappear for
// new item types. Whole or nothing: refuses and writes nothing if the rig does not hold at least
// `quantity`.
bool Withdraw(entt::registry& registry, entt::entity rigRoot, ItemKind kind, const std::string& id,
              int quantity);

// Every living bay's stacks, concatenated, for display (StorageMenu) -- deliberately NOT merged
// across bays: collapsing two same-id stacks from different bays into one row would hide which
// bay (and therefore which mass, on bay loss) a given unit actually lives in. Each returned copy
// carries ItemStack::hardpoint set to the bay it came from, so a caller can name it.
std::vector<ItemStack> Merged(const entt::registry& registry, entt::entity rigRoot);

}  // namespace sr::cargo_view
