#pragma once

namespace sr::core::economy {

// architecture.md 12.30.4's Repair screen: "price derives from what the hardpoint is made of" --
// but a hardpoint records no shell id today (RigFactory::CreateHardpoint emplaces no ShellId),
// and a ShellDef carries no Inert attribute either (Inert is a property of the *elements* a
// shell was made of, propagated onto an instance -- features.md 2.10 -- and a def is
// grade-neutral besides). Both land with section 12.19's ShellInstance/ItemInstance, at which
// point this becomes RepairCostPerHp(const ItemInstance& shell, int facilityGrade, const
// ContentLibrary&).
//
// Until then, this is deliberately the flat placeholder architecture.md 12.30.4's own scheduling
// table calls "honest here, since the rate and the gate are what the screen is for" -- price
// fidelity is not.
//
// `facilityGrade` is the authored efficiency divisor (features.md 2.10: "cost to build = recipe
// x facility grade," applied to its second consumer here) -- a higher-grade bay repairs more
// cheaply. Clamped to at least 1 so a misauthored grade of 0 cannot divide out to free repair.
int RepairCostPerHp(int facilityGrade);

}  // namespace sr::core::economy
