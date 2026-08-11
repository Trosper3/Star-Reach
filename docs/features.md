# Star Reach: Game Design Document

**Version:** 2.0

**Core Concept:** A systemic, top-down space sandbox that merges twitch-based tactical combat, deep
engineering, and macro-level fleet command within a living, autonomous economy.

**Companion documents:** `architecture.md` (engineering laws, layering, enforcement, migration) ·
`lore.md` (narrative, faction psychology, simulation drivers)

---

## Status Legend

| Mark | Meaning |
|:---:|---|
| ✅ | **Built** in the current project |
| 📋 | **Designed** — spec agreed, not yet built |
| 🧊 | **Deferred** — intentionally out of scope for now |
| ❓ | **Open question** — needs a decision before implementation |

Nothing in this document is ✅ yet. The first vertical slice is scoped in `architecture.md` §10.

---

## 1. The Core Gameplay Loops 📋

An infinite progression loop scaling the player from solo pilot to faction-level engineer and
commander.

**Micro Loop (minute-to-minute)** — Fly, mine manually, fight tactically against *specific enemy
hardpoints*, salvage wrecks, explore derelicts and anomalies for rare tech.

**Meso Loop (hour-to-hour)** — Dock at vessels with Facility or Research modules. Deconstruct
salvage, reverse-engineer rare items into repeatable blueprints, engineer better modules. Balance
Mass and Power to optimize a personal vessel; save the result as a **Template**.

**Macro Loop (session-to-session)** — Sell Templates to AI factions. Watch a faction manufacture
your design and conquer territory with it. Accumulate enough wealth to equip a Command Module,
transition to the Bridge, and command automated fleets to establish or expand your own faction.

### 1.1 Time-Sliced LOD Simulation 📋

*Previous versions of this document left this as a one-line note to self. It is load-bearing for the
entire living-galaxy pitch, so it is specified here.*

The galaxy is simulated at three fidelities. This maps directly onto Law 2 in `architecture.md`
(**one `entt::registry` per star system**), which is what makes the model cheap: LOD is a question
of *which registries tick and how often*, not of filtering every query.

| | **Tier 1 — Active Sector** | **Tier 2 — Local Neighborhood** | **Tier 3 — Galaxy Background** |
|---|---|---|---|
| **Scope** | The player's current system | Systems within 2 warp jumps | Everything else |
| **Rate** | 60 Hz fixed | ~5 Hz | One macro tick (~30 s real time) |
| **Backing** | Full resident registry | Resident registry, coarse components | **No registry** — `core/galaxy/` records only |
| **Physics** | Full: thrust, momentum, collision hulls | None | None |
| **Projectiles** | Individually simulated | None | None |
| **Combat** | Per-hardpoint targeted damage | Fleet-strength attrition rolls | Outcome resolved as a single event |
| **Movement** | Continuous steering | Scheduled arrivals, interpolated | Origin + destination + ETA only |
| **Economy** | Live production ticks | Live production ticks | Aggregate stock accumulation |
| **AI** | Full state machine | Role tasks, no steering | 4-facet decision engine only |
| **Rendered** | Yes | Only on the galaxy map | Only on the galaxy map |

**Promotion and demotion.** Warping into a Tier 2 system promotes its registry to full fidelity.
Warping into a Tier 3 system *instantiates* a registry from its `core/galaxy/` record — station
ownership, fleet presence, and stock levels become entities via factories. The departed system
demotes: entities are collapsed back into a galaxy record and its registry is torn down.

**Deterministic catch-up.** Tier 3 systems bank elapsed macro ticks. On arrival, orbital positions,
production, and station rebuild timers fast-forward as a pure function of elapsed time — never by
replaying skipped frames. This is why `architecture.md` §5 requires a fixed timestep: without
determinism, catch-up and demotion do not round-trip.

**The boundary rule:** *Tier 3 must never require entity data.* Anything the macro simulation needs
to read or write lives in `core/galaxy/` as plain data. If a Tier 3 feature wants to inspect a
hardpoint, that feature is misplaced.

> **"Tier" always means simulation fidelity, and only that.** The navigation map (§8) has its own
> four-level scale, and the two do not correspond — map Zoom Level 3 is a detailed view *inside* one
> system, while simulation Tier 3 is the unrendered galactic background. Those are called **Zoom
> Levels**, never tiers.

---

## 2. Engineering, Customization & Reverse Engineering 📋

Engineering is the heart of progression. Objects are modular, built from a hierarchy of **Shells**
(the physical housings that hold modules — also called **components**), **Modules** (functional
stats), and **Elements**.

**There are two tiers, not three** (settled 2026-08-07). *Shell* and *component* are the same thing
under two names: the housing a module mounts into, which is also the unit of localized damage
(§3.2) and the thing a hardpoint *is*. `architecture.md` Law 4 previously described a
`Shell → Component → Module` model and §12.12 recorded that "no `ComponentDef` type exists" as a gap;
it is not a gap. **Nothing needs to be added to close it** — `ShellDef` and `ModuleDef` are the
complete set, and the Law 4 wording should be corrected to `Shell → Module` rather than a third type
being written.

*"Component" remains fine to say in conversation and in UI copy. It must not become a separate
authored type.*

> ✅ **The supply vocabulary, settled 2026-08-09. This supersedes the 2026-08-08 "craft" ruling
> below, which is retained because its *reasoning* still holds and only its nouns changed.**
>
> | Tier | Name | What it is | Source |
> |:---:|---|---|---|
> | **1** | **Element** | A periodic-table element. Iron, silicon, hydrogen | Mined, skimmed, traded |
> | **2** | **Material** | A manufactured intermediate — alloy plate, coil, wafer, fuel | Made from Elements |
> | **3** | **Module** / **Shell** | What bolts onto a rig | Made from Materials |
>
> **The word *craft* is retired entirely.** It was settled on 2026-08-08 to mean "a manufactured
> intermediate," specifically to stop it also meaning *vessel* — but needing to litigate a word is
> usually the word's fault, and *"an alloy plate is a material"* is plain English where *"an alloy
> plate is a craft"* is jargon. **A vessel is a *vessel*; an intermediate is a *material*.**
>
> ⚠️ **This flips the meaning of "material."** It used to mean the raw tier and now means the
> **manufactured** tier. Existing code carries the old sense — `CargoHold::materials`,
> `MaterialStack`, `MaterialDrop`, `MaterialChance`, `AsteroidComposition::materials`, and
> `MiningSystem`'s spawn path all hold what are now **Elements**. That rename is tracked in
> `architecture.md` §13.5 and must land in one pass; a half-migrated codebase where the same word
> means both things is worse than either name alone.
>
> ✅ **Three tiers, and the fourth was rejected.** A proposed
> `Element → Compound → Material → Module` chain was considered on 2026-08-09 and cut: a Compound
> tier asks the same question the Material tier already asks (steel is Fe+C, stainless is Fe+Cr+Ni —
> that choice *is* an alloy-plate recipe), and every extra hop averages attribute contributions
> further toward the mean, which is the mechanism §2.10's attribute system depends on. A tier that
> adds no decision and costs signal is friction.
>
> **This adds authored content that does not exist yet.** `data/base_game/` holds `modules.json`,
> `shells.json`, and `ships.json` — there is no `elements.json`, no `elements.json`, and no
> `ElementId`/`ItemId` in `shared/blueprints/Ids.h`. `architecture.md` §12.15 item 21 already
> tripped over the edge of this, typing `ResearchJob::item` as `ModuleId` because no item type
> exists. Manufacturing (§2.8) needs all of it.
>
> ⚠️ **This is not a third *rig* tier.** Rig composition remains **Shell → Module**, two tiers. The
> Element → Material → Module chain is a *manufacturing input* axis and is orthogonal to it. Do not
> reopen the tier question on the strength of this paragraph.

### 2.1 The Two Forms of a Ship

This mirrors Law 3 in `architecture.md`, and it matters to design, not just to code:

- A **Blueprint** is authored data — a stable ID, a list of shells, mounts, and module IDs. This is
  what a player Template *is*, what gets sold to a faction, what a save file stores, and what a
  faction manufactures from.
- A **live rig** is the instantiated object: a parent entity with one child entity per hardpoint.
  This is what takes damage, loses a turret, and stalls when its engine dies.

The practical consequence: **selling a Template sells data, not an object.** A faction that buys
your design can manufacture it indefinitely, in any system with a Manufacturing hardpoint, forever.
That is the intended power fantasy of the macro loop — seeing your silhouette in someone else's
fleet.

### 2.2 Physical Customization

**Modular assembly** — players snap shells and modules onto a base chassis. Separated top-down
texture layers mean custom designs retain a distinct, recognizable silhouette.

**The constraints puzzle** — every module demands **Power** (supplied by Power Cell modules) and
adds **Mass**, which directly degrades turn rate, acceleration, and top speed. There is no
strictly-best loadout; there is only a loadout suited to a role.

#### The mass and power model 📋

*Settled 2026-08-07. Restated here because it was agreed in conversation and never written down,
and because the codebase currently implements only half of it.*

| Tier | Adds mass | Draws power |
|---|:---:|:---:|
| **Shell / component** | ✅ | ❌ |
| **Module** | ✅ | ✅ *(may be zero — see §2.7)* |

**Mass and power are recomputed whenever a module or shell is added or removed.** Not on a timer,
not at spawn only — on every equip, unequip, install, and strip. This is what makes the constraints
puzzle a live decision at the workbench rather than a number printed on a blueprint.

##### Mass is authored, on everything that has it 📋

*Settled 2026-08-08. A derived model — `mass = k · density · radius²` for shells — was proposed and
withdrawn the same day.*

**Every authored item carries a `mass` field: shells, modules, Materials, and Elements.** Simple
addition, one field, no formula, and the same rule everywhere.

*Why the derived model was withdrawn.* It existed to make §3.5's hull envelope rule (rule 12)
self-enforcing — that rule bounds a hull's hardpoint count by its authored `hullRadius`, on the
reasoning that the lever granting capability is the same lever that makes something a capital, which
only holds if mass follows size. The exploit it guarded against turned out not to be reachable:

- **Players cannot author shells.** `CustomizeMenu` assembles from `shells.json`; nobody can create a
  huge-but-light chassis in normal play.
- **Modders are unconstrainable anyway.** Someone who can write `mass: 5` can already write
  `damage: 10000`. Deriving mass changes which field they edit and nothing else.
- **Authoring accidents are what validation is for**, not what a formula is for.
- **And authored mass is more consistent** — modules were always going to be authored, so deriving
  only shells meant two rules for one concept.

*Optional guard, if wanted:* a **rule 13 — mass sanity** band checking a shell's mass against
`radius²` within a wide tolerance (say 0.25× – 4× a reference density). Wide enough that a dense
armour plate and a light hangar frame both pass; narrow enough that a 250× typo fails at load.

##### Cargo has mass, and it is part of the vessel 📋

*Settled 2026-08-08, and it is why Elements and Materials need a mass field at all.*

> **A vessel's mass is its structure plus everything in its hold.**
> Loading cargo degrades turn rate, acceleration, and top speed exactly as bolting on a module does.

This costs almost nothing to build — `BodyMass` already exists and this section already requires mass
to be recomputed on every change — and it buys a surprising amount:

- **Haulers become a real vessel class with no class system.** A hull with large cargo capacity and
  strong engines *is* a freighter. Emergent from the fit, per Law 4, with no `ShipType` anywhere.
- **Mining becomes a decision** rather than a loop: fill up and crawl home, or make two fast trips.
- **Interdiction gets teeth.** A laden freighter cannot outrun an attacker, which gives The Forgotten
  (*wreck salvage, ambush, vanishing when outgunned*, §6.2) a target profile that follows from the
  fiction rather than from a spawn table.
- **The recovery run (§3.3 Tier 2) gains a shape** — you fly out light and fast, then have to escape
  heavy.
- **Stripping salvage on-site becomes tactical.** If a module's materials weigh less than the module,
  deconstructing a wreck before running is a real choice rather than a convenience.
- **Blockades and trade routes (§5) become physical.** Hauling capacity stops being a number on a
  faction record and becomes something with a speed cost attached.

**It applies to NPCs identically** (§6.3) — a laden NPC freighter is visibly slow, which is exactly
the "read the enemy by its behaviour" property that rule exists to protect.

**Two implementation notes**, both small:

- **Do not cache cargo mass.** Sum it in the same pass that already reads `BodyMass`; it is a short
  vector, and the alternative is an invalidation bug on every pickup, sale, mining tick, and loot
  collection. Same reasoning §4.3 applies to Military Weight.
- **It rides on an existing gap.** `ModuleEquipSystem.h` already documents that rig-wide
  `BodyMass`/`Propulsion` are *not* recomputed on mount or unmount (`architecture.md` §12.16 item 8).
  Cargo mass and that fix are one piece of work, not two.

⚠️ **Watch the early-game ratio.** A starter hull that crawls under ten units of ore is miserable.
Per-unit material mass wants to be negligible when the hold is light and meaningful only when it is
genuinely full.

##### Storage is a module, and capacity is a mass limit 📋

*Settled 2026-08-08. Storage had never been specified anywhere, and `CargoHold::capacity` existed as
a bare count added for `RefactorSystem`'s whole-or-nothing check.*

> **Cargo capacity comes from `CargoBay` modules mounted in shells, exactly as power comes from
> `PowerCell` modules. `CargoHold` is the rig-level aggregate, summed from *living* bays — the same
> shape `PowerBudget` already uses for `PowerSource`.**

Three things follow, and all three were wanted:

- **Capacity joins the fit puzzle.** More hold costs mass and module slots, so a freighter is a
  tradeoff rather than a hull property — and combined with cargo mass above, a hauler is simply a
  vessel that spent its budget on bays and engines. No `ShipType`, per Law 4.
- **It is upgradeable and expandable with no new mechanic** — a better bay is a better module,
  acquired, researched, and manufactured like any other.
- **Shooting a cargo bay spills its contents.** §3.2's localized damage applied to storage: destroy
  the hardpoint, lose what was in it as salvage. That gives piracy a reason to target a *specific*
  hardpoint rather than the hull, and it feeds §3.3's recovery run from a direction other than death.

**`capacity` measures mass, not slots.** A bay holds so much mass; slots in `StorageMenu`'s grid are
presentation, not a second constraint. Volume was considered — a hold full of feathers versus one full
of lead is a genuine distinction — but it needs a volume stat on every item to express, and the
gameplay it buys is a second number saying almost what the first one says. One physical limit, with
"fill up and crawl home" getting a hard stop rather than a soft one.

> ❌ **The paragraph above is out of date, and §2.11 is where it was already superseded.** *"Slots
> are presentation, not a second constraint"* was written while `CargoHold::capacity` was a bare
> entry count with no bay module behind it and nothing to author a second number on. **§2.11's
> `CargoBay` roster entry authors two, and gives each one a job:**
>
> | | | |
> |---|---|---|
> | **`slotCount`** | How many **distinct stacks** | Variety |
> | **`slotCapacity`** | **Mass per stack** | Bulk |
>
> — with the total derived, never authored, and a worked example showing the two are not
> interchangeable: *"4 × 250 is an ore hauler; 20 × 50 is a trade-goods runner; **both carry
> 1,000**."* `architecture.md` §12.23 authors the identical pair on the kind, and both are rollable
> pool entries (§2.7), so a bay can roll into variety or into bulk.
>
> **Two numbers, two constraints, two refusals** — a hold can be out of *slots* with mass to spare,
> or out of *mass* with slots to spare, and §3.10's degrade-never-remove wants a row that says which.
> The sentence above is corrected rather than the roster: **capacity is a mass budget *per slot*, and
> the slot count is a real cap on variety.**
##### The enforcement rule, and the code still counts entries 📋

*Added 2026-08-10. This section settles what capacity **measures** and never says how it is
**checked** — and the check that exists counts list entries:* `CargoHoldEntryCount` *returns*
`modules.size() + materials.size()`*, and* `CargoHoldHasRoomFor` *compares that count to* `capacity`.
**A mass limit measured in rows is not a mass limit**, and it becomes actively wrong under §12.19's
stacking rule, where one row is an unbounded quantity.

> **A hold accepts an incoming stack when its current total mass plus that stack's mass is at or under
> capacity, and refuses it whole otherwise.** One number, one comparison, and `capacity: 0` still
> means unlimited so every existing fixture behaves as before.

**The total is maintained on the write, never recomputed on the read.** §5.0 already sets this shape
for faction stock — *"there is exactly one `Deposit`/`Withdraw` API and it updates the totals as it
writes"* — and every reason carries over: the total is read by the refusal check on **every**
transfer, by §3.10's inventory overlay header, and by the rig's own mass, and re-summing a hold at
each of those is the iteration §9.1 warns about.

⚠️ **Cargo mass is part of the rig's mass**, which is the entire point of denominating the limit in
mass: a laden hauler accelerates and turns worse, so §2.2's constraints puzzle is played by what you
are carrying and not only by what you bolted on. It also gives `BodyMass` a **third** writer beside
mount and unmount — and §2.11 records that it is currently recomputed on neither.

##### 🐛 "Shooting a cargo bay spills its contents" cannot mean what it says

*Found 2026-08-10, by asking what a bay dying does to a hold that is over the new limit.* This
section settles two rules that do not fit together:

- **`CargoHold` is the rig-level aggregate**, summed from living bays — under which a bay contributes
  *capacity* and never storage.
- **"Destroy the hardpoint, lose what was in it as salvage."**

Under a rig-level pool **nothing is in any particular bay**, so a destroyed bay has no contents to
spill — no such set exists. Two ways out, and the second is the one this section's own promise was
already describing:

> ~~**A destroyed bay reduces capacity, and the hold spills whatever no longer fits — heaviest stack
> first — as recoverable `LootDrop`s.**~~ *Written 2026-08-10 and withdrawn the same day: it is a
> tiebreak rule invented to paper over a mismatch, and the loss it produces has nothing to do with
> which hardpoint was hit.*

> ✅ **The hold lives on the bay. `CargoHold` is a component on each cargo-bay hardpoint, holding
> `slotCount` stacks of at most `slotCapacity` each, and the rig-level hold is a *view* over the
> living ones.** Destroying a bay drops **that bay's stacks** and nothing else.

**This makes §2.11's aggregation law literally true for cargo** — *"every rig-level attribute is the
sum of contributions from living hardpoints; destroying a hardpoint removes its contribution"* —
instead of nearly true with a spill rule bolted on. There is no overflow, no ordering, and no
special case: the contribution *is* the contents.

Three properties follow, and all three were wanted:

- **Piracy gets a target with a legible payoff.** Shooting a specific bay takes what was in that bay,
  which is §3.2's localized damage meaning something for a hauler and not only for a warship.
- **More bays is a survivability axis, not just capacity.** Two 500 kg bays and one 1,000 kg bay carry
  the same cargo; the first loses half of it to a hit and the second loses all of it. That is a real
  fit decision costing no new mechanic.
- ❌ **Superseded 2026-08-11. The player *may* place things** — reversing this bullet, because
  distribution-as-survivability (above) is a strictly richer idea when the player can act on it
  deliberately instead of only inheriting whatever an algorithm happened to produce.
  **Auto-assignment stays the default** for routine deposits (mining, looting, buying) so the common
  case costs nothing extra; **manual bay choice and drag-and-drop between bays/storages** are
  available for a player who wants to be deliberate about risk distribution. This does reopen the
  surface `architecture.md` §12.30 previously ruled out — see below for what it costs.

  > **The auto-assignment algorithm: greedy, most-remaining-capacity-first, deterministic.** A
  > deposit goes into whichever *living* bay currently has the most remaining mass capacity; if it
  > doesn't fully fit, the remainder spills into the next-most-empty bay, following the existing
  > "split across bays as needed, refuse whole only if nothing fits" rule. Rejected alternatives:
  > **round robin** balances by deposit *count*, not mass, so a few large stacks landing on
  > whichever bay is "next in rotation" can still produce lopsided distribution regardless of how
  > full it already is; **random** is unpredictable in a way that reads as arbitrary even when the
  > outcome is fine, and a player who opts out of managing bays still deserves a legible reason for
  > where things landed. Greedy most-empty-first is self-balancing by construction, which is what
  > makes the survivability property above hold even for a player who never opens the storage
  > screen.

**The same path runs when a bay is unmounted rather than destroyed**, so there is one rule and not
two — and live refit being unrestricted (§2.7) makes unmounting a full bay mid-flight a reachable act
rather than a hypothetical.

*This also gives `LootDrop` and `MaterialDrop` a producer, which `architecture.md` §13.3 T records
they have never had — the second, after §12.30.7's jettison. The mechanics are in `architecture.md`
§12.23.*


✅ **Mass is conserved through the manufacturing chain, with a loss at each step** (settled 2026-08-08).
Elements in ≈ Material out, Materials in ≈ module out. Only Elements carry an authored mass;
everything above derives from its recipe. That gives recipe authoring a physical sanity check,
makes deconstruction-as-inverse fall out for free, and means "why does this cost that much" has an
answer an author can reason about rather than a number someone picked. **Base price derives the same
way.** See §2.10.

**A shell cannot be removed while it still holds modules.** Strip its modules first, then pull the
shell. There is no cascade that silently dumps a shell's contents into cargo, because a cascade
means the player can lose a fit they did not intend to break, and because "your cargo hold is full"
then has to unwind a partially-applied operation.

*This reverses existing behaviour:* hardpoint deletion currently returns a shell's modules to cargo
automatically. The new rule refuses the deletion instead.

#### Hull is a base value that armour modules add to 📋

*Settled 2026-08-07 — and it ratifies what is already built.* `ShellDef.hull` is the hardpoint's own
hull before module bonuses; armour modules add to it. The alternative considered was a mandatory,
non-removable armour module supplying all of a shell's hull.

**That alternative is rejected, and it contradicts a rule settled one section earlier.** A shell
cannot be removed while it holds modules (above). A module that "can never be removed, only replaced"
would therefore make its shell **permanently unremovable** — you could never strip it to pull the
hardpoint. The two rules cannot both hold.

Two further reasons:

- **`moduleSlots: 0` is explicitly legal** — `ShellDef` calls it out for "a pure armour plate." Under
  the module-supplies-hull model a zero-slot shell has nowhere to put its armour module and therefore
  no hull at all, which is incoherent.
- **"Cannot be removed, only replaced" is a special case in the equip rules**, and it would be the
  only module in the game that behaves that way. `ModuleEquipSystem` and `RefactorSystem` would both
  need to know about it.

Base hull also lets shells differ meaningfully on their own — a thin wing mount and a thick chassis
are not the same object with different cargo — which is what makes shell tier (§2.2) worth having.

> **This must be enforced in the system, not only in the menu.** The menu should grey the action out
> — but §2.3's own rationale is that validation exists so "a corrupt save or malicious wire packet
> cannot inject" an invalid state, and a UI-only rule is bypassed by both. The refusal belongs in
> the system that consumes the intent, exactly as `RefactorSystem` already refuses to delete a
> hardpoint another hardpoint depends on. The menu greying out is the courtesy; the system refusing
> is the rule.

**Template creation** — a finalized configuration is saved as a Template for personal reuse or sale.

### 2.3 Template Validation Rules 📋

*Also previously a note to self.* Validation runs on the **blueprint**, before instantiation — so an
invalid design can never become a live entity, and a corrupt save or malicious wire packet cannot
inject one.

A blueprint is valid if and only if:

| # | Rule | Rationale |
|---|---|---|
| 1 | ≥ 1 Armor/Chassis shell with an attached module | Something must exist to mount to |
| 2 | ≥ 1 Power Cell shell with an attached module | Nothing runs without generation |
| 3 | Net power balance ≥ 0 | Sum of module draw ≤ total generation |
| 4 | Total mass within the chassis structural threshold | Per-chassis cap from its blueprint |
| 5 | ≥ 1 Engine shell with an attached module — **mobile vessels only** | Stations are exempt |
| 6 | No orphaned mounts | Every declared mount references a shell that exists |
| 7 | Adjacency valid | Every shell connects to the rig graph; no floating islands |
| 8 | All module and shell IDs resolve in the registry | Blocks typos and missing mod content |
| 9 | Unique blueprint ID, present `schemaVersion` | Required for saves and migration |
| 10 | **Separation** — no two mounts closer than `r(A) + r(B)` | Hit circles stay disjoint, so §3.2's manual aim can pick one (§3.5) |
| 11 | **Attachment** — every mount within `A extent + B extent` of what it attaches to | A mount cannot satisfy rule 7 while floating visually detached (§3.5) |
| 12 | **Hull envelope** — every mount's centre plus its radius falls within the chassis's `hullRadius` | Bounds the hull. Without it, mounts chain outward indefinitely (§3.5) |

Rules 3 and 4 are the design-facing ones — they are the constraints puzzle. Rules 6–9 are integrity
checks and should produce distinct, specific error messages in the Engineering UI, never a generic
"invalid template." **Rules 10–12 are pure geometry** (§3.5) and together they are what make a
hull's hardpoint count emergent rather than authored — see §3.5's ring-capacity formula.

⚠️ **Rule 11 measures a turret's *base* extent, not its drawn extent.** A long barrel legitimately
sweeps over neighbouring hardpoints; validating against the sprite's full length rejects hulls that
are correctly built.

#### These same rules run against every JSON-authored def at load, not only a Template at save 📋

*Settled 2026-08-11.* This section already states the reason validation exists: *"a corrupt save or
malicious wire packet cannot inject"* an invalid state. That reasoning applies exactly as much to
`data/base_game/ships.json` (and to any mod's content directory) as it does to a Template a player
saves through `CustomizeMenu` — a hand-edited or malicious content file is a wire packet with a
different mail slot, not a different threat.

> **On load, every ship/shell/module def runs through the same twelve rules above.** A def that
> fails is **excluded from the game, not silently accepted and not a crash** — logged with which
> rule it failed and why, and surfaced to the player as a popup naming the specific def and the
> specific violation (*"the `raider_x` ship was not loaded: hardpoint `gun_wing_2` overlaps
> `gun_wing_1` — rule 10, separation"*), not a generic "invalid content" message. The same
> per-rule error reporting rules 6–9 already require for the Engineering UI is the reporting this
> needs — one mechanism, two call sites.

This closes two problems with one gate: a modder cannot construct a ship that breaks the same
mountability/mass/geometry rules a player is held to in `CustomizeMenu`, and the base game's own
content gets the same integrity check for free — a malformed def in `ships.json` is caught at load
with a specific reason, not discovered later as an unexplained crash or an exploitable edge case.

#### Mountability is authored on the shell, not hardcoded 📋

*Settled 2026-08-08, prompted by shields needing to sit somewhere other than a dedicated housing.*

Today mountability is a **hardcoded C++ table**:

```cpp
// Taxonomy.cpp
case ModuleKind::ShieldGenerator: return shell == ShellKind::Shield;
```

That is a content rule living in code, which is precisely what `architecture.md` Law 10 exists to
prevent — and it makes a design question ("which hulls can host a shield generator?") into a code
change.

> **`ShellDef` declares which `ModuleKind`s it accepts.**

A chassis def can accept `{Armor, ShieldGenerator}`; a wing mount accepts `{Weapon, Armor}`. So only
certain shells have a shield slot, authored per shell, and **positioning a Bubble generator becomes
possible and deliberate** (§3.1) — the content author decides which hulls can host one and where.

**This replaced a grade-gated proposal** that would have let any high-grade shell accept shield
modules. Authored acceptance is more controllable and avoids piling a second benefit onto shell grade,
which §2.7 already warns compounds badly.

*It also retires a hardcoded table in favour of JSON, which is a Law 10 improvement independent of
shields.*

### 2.4 Reverse Engineering & Research 📋

**From loot to blueprint** — rare high-tier modules, weapons, and alien components found via
exploration, quests, or salvage are inputs, not trophies.

**The research loop** — bringing a rare item to a station with a Research/Engineering Facility
module lets the player spend resources and time to reverse-engineer it. Success makes the item
permanently manufacturable and available for Template integration.

#### The three workbench mechanics are distinct 📋

*Decided 2026-08-07. Earlier drafts used "engineer better modules" (§1) and "reverse-engineer"
(this section) loosely enough that they read as one mechanic, and the codebase built them as two
systems with no stated relationship. They are three, they share only a facility gate, and each has
its own inputs, costs, and output.*

| Mechanic | Input | Cost | Output | Home |
|---|---|---|---|---|
| **Research** | An *undiscovered* module | Time **+** Elements/Materials **and/or** credits | The module becomes **manufacturable** by the researching player or faction | `ResearchSystem` |
| **Engineering** | Two *alike* modules the actor already owns | Credits **and/or** Elements/Materials | **One** module combining both their attributes, at a loss | `EngineerSystem` |
| **Deconstruction** | Any one module | — | Raw **materials** | `EngineerSystem` (§12.12) |

**Research produces knowledge; engineering and deconstruction produce matter.** That split is the
§2.5 rule ("knowledge lives in networks, matter lives in ships and stations") applied to the
workbench: a research unlock lands in a knowledge network and survives the actor's death, while a
merged module and a pile of salvaged materials sit in a cargo hold and do not.

#### Manufacturing was already knowledge-gated; this section names every way to acquire the knowledge 📋

*Settled 2026-08-11.* §2.8 already states the gate — *"manufacturing requires... that the actor's
knowledge network actually holds the design"* — but until now this section only described one way to
satisfy it: reverse-engineering a physical instance you already hold. **Four paths, not one:**

| Path | How | Notes |
|---|---|---|
| **Reverse-engineering** | The existing Research mechanic, above — bring an undiscovered module to a facility, spend time + materials/credits | Unchanged. Needs a physical instance |
| **Research from scratch** | A **technology tree**, costing resources and time per node, no physical sample required | Same-faction or faction-less (rogue operator, §5.10) progression. **Tree shape (nodes, per-owner storage, how far a single unlock reaches) is agreed in principle and not yet specified** — see the forward pointer below |
| **Trade** | Acquiring another faction's already-unlocked recipe through them, gated on relations improved **dramatically** — the `Allied` band (§5.3), not merely `Friendly` | The same lump-sum/royalty negotiation §2.6 already specifies for Templates, widened to recipes and to non-player sellers |
| **Exploration** | "Mysterious sites" — anomalies, ruins, derelicts around stars, planets, and asteroid belts — have a chance to yield a recipe sample belonging to **any** faction, or none, reverse-engineerable via the existing mechanic once found | This is not a new setting element: `lore.md` §2 already names *"Deep-Space Anomalies... primary sources for rare technology, anomalies, and reverse-engineering components"* — this is the mechanical implementation of something already in the bible, unused until now |

**Successfully reverse-engineering a module doesn't only unlock that module — it grafts access to
the surrounding region of whichever tech tree it belongs to**, including a foreign faction's tree.
Capturing one piece of a rival's technology is what lets you catch up along *their* lineage through
normal research, rather than being permanently confined to researching only your own faction's
isolated tree from a standing start.

**This is also the first real computation source §6.1's Tech Superiority facet has ever had.**
`FactionDecisionEngine::ComputeFacets` currently holds it at a permanent placeholder — *"how much of
the tree a faction has unlocked, relative to rivals"* is exactly the kind of number that facet was
always meant to be, once the tree exists to measure.

> 📋 **Agreed in principle, not yet specified — the technology tree's internal shape.** Node
> structure, what a node actually is (one recipe? a bundle?), per-owner progress storage (extending
> `KnowledgeNetwork`, which already models per-owner state for `Player`/`Commander`/`Faction`), and
> the exact rule deciding how far one reverse-engineering success reaches into a tree are all open.
> Recorded here as a known commitment, the same way the signature/detection model was recorded in
> §9 before it had a shape — a design pass of its own, not decided inline with this list.

**The engineering loss is set by the facility's level** (decided 2026-08-07) — a better-equipped
Engineering facility preserves more of the secondary module's contribution. Skill (§2.7) is a
property of pilots and commanders and is deliberately *not* wired into the workbench: the facility
is the thing being upgraded here, and adding a second scaling axis to a merge makes the outcome
unpredictable to the player for no gain. `EngineerSystem` already scales by `FacilityRef::level`,
so this decision requires ~~**no code change** — it ratifies what is built~~ **only the change below.**

> 🐛 **Corrected 2026-08-10 (`architecture.md` §12.30.5).** *"Scales by `FacilityRef::level`"* is
> true; *"requires no code change"* is not, and the two claims were run together. **Two separate
> things are unbuilt:**
>
> - **The band clamp below does not exist.** `MergeField` is `primary + secondary × (level × 0.1)` —
>   **additive on raw stat values with no ceiling of any kind** — and it is applied per field, so a
>   merged weapon takes `damage`, `rangeUnits` and `projectileSpeed` from the formula and its other
>   four fields from the primary alone. §2.4's formula moves a *normalised quality* against `bandMax`;
>   the two share the `level × 0.1` factor and nothing else, and §2.4's version cannot be built until
>   §12.21's `Quality` type exists. **Merging is unbounded today, and nothing refuses it at a ceiling.**
> - **`FacilityRef::level` has never held a value other than `1`.** `ModuleAttachment.cpp:65` passes
>   only `module.facility.kind` when it emplaces the component, and `ParseFacilityStats` never reads
>   `level` from JSON at all (`architecture.md` §13.3 K). **So every merge in the game preserves
>   exactly 10% of the secondary**, and the facility-level axis this paragraph settles has never been
>   exercised.
>
> Merging also **consumes nothing but the two modules** — no credits and no Materials, contrary to the
> cost rule below. All three land together with §12.21 and §12.19.

**Merging is bounded by the grade band, and refused at the ceiling** (settled 2026-08-08, with
§2.7's quality band). A merge raises the primary module's quality *within* its grade and can never
push it into the next one — otherwise merging becomes the way to exceed tiers and the rarity ladder
stops meaning anything. Two consequences:

- **The gain is against the headroom, not the raw value:**
  `newQuality = primaryQ + (bandMax − primaryQ) × secondaryNorm × (facilityLevel × 0.1)`.
  A level-5 facility with a perfect secondary closes half the remaining gap. You approach the ceiling
  and never quite reach it, so the last few percent stay worth chasing and every merge costs a module.
- **A merge is refused when the primary is already at the ceiling**, so no resources are wasted. That
  joins `EngineerSystem`'s existing refusals — not docked, no living Engineering facility, not the
  same `ModuleKind`, either id not actually held.

*This gives merging an identity it previously lacked.* Under a flat stat model, merging was a
marginal bump on top of "keep the better one." Against a rolled quality it is **how you max out a
good roll**, which is a goal rather than a rounding error.

#### Facility grade drives all three 📋

*Settled 2026-08-08, replacing the open question of whether deconstruction yield was flat or scaled.*
`FacilityStats::level` (1–5) **folds into the seven-tier grade** (§2.7) — a facility is a
`ModuleKind::Facility` module and carries a grade like everything else, and two tier systems for one
concept would have drifted the moment either was tuned. Existing merge and research scaling constants
re-derive against seven steps instead of five.

**Deconstruction recovery is a random range, and the range is the facility's grade:**

| Facility grade | Materials recovered |
|---|---|
| Common | 20 – 45% |
| Uncommon | 30 – 55% |
| Unique | 40 – 65% |
| Rare | 50 – 75% |
| Epic | 60 – 85% |
| Legendary | 70 – 95% |
| **Mythic** | **80 – 100%** |

Bands overlap in the same shape as §2.7's quality bands, so a lucky Legendary deconstruct can beat an
unlucky Mythic one. Mythic tops out at exactly 100% — a *chance* at full conservation, never a
guarantee and never above it (§2.10's conservation-safe rule).

**Randomness belongs here and not in manufacturing.** A deconstructed item is already gone, so a variable
yield leaves nothing unpredictable in the player's hands — where a randomised *manufactured* mass would
mean equipping a module and watching your turn rate change by an amount you could not have read
beforehand. Quality (§2.7) is the per-instance variance; mass stays deterministic.

**Research consumes the sample — probably.** Facility grade sets both the chance the sample survives
and how long the job takes:

| Facility grade | Sample survives | Research time |
|---|---:|---:|
| Common | 5% | 100% |
| Uncommon | 15% | 88% |
| Unique | 30% | 76% |
| Rare | 45% | 64% |
| Epic | 60% | 52% |
| Legendary | 75% | 40% |
| Mythic | 90% | 30% |

> ⚠️ **The survival chance must be shown before the player commits.** Feeding your only Mythic into a
> Common bench has to be a gamble the player took, not a gotcha the game sprang. Mythic caps at 90%
> rather than 100% so late-game research keeps a little tension; a surviving sample is not reusable
> *for research* — the unlock is permanent after one success — so it is loot you keep, not a loop.

> 🐛 **None of the sample model exists, and neither does a way to start research at all** (verified
> 2026-08-10, `architecture.md` §12.30.6). `ResearchJob` carries an item id and nothing else — no
> lock, no roll, no consumption — and **there is no `StartResearchRequest` anywhere in the codebase.**
> `ResearchSystem` advances jobs nothing creates, on a `StationFacility` nothing emplaces, granting
> into a `KnowledgeStore` that is `nullptr`, keyed on a network id no entity carries. Five links, each
> built and tested; every test passes because each one constructs the job by hand.
>
> **The disclosure rule above is what decides the screen's shape** — it makes committing a sample a
> confirmation rather than a button, showing survival chance, duration, and target network before the
> click. It is the strongest UI requirement in the docked batch precisely because it is a settled rule
> with a stated reason.
>
> ⚠️ **One gap filled rather than quoted:** the table below gives the facility grade's *percentage* of
> research time and never says what it is a percentage **of**. §12.30.6 settles
> `duration = baseDuration(item.grade) × gradeTimeFactor(facility.grade)` — one ladder, no per-item
> authored time, the same discipline §2.10 applies to mass and price.

**Merging consumes Materials** as well as credits, scaled by the module's grade *and* by how close to
its band ceiling the merge is pushing it. Since the gain uses the diminishing-returns headroom
formula, cost and curve agree: **the last few percent are expensive in both directions, and maxing an
item is a project.**

**Merging applies to modules only, and each exclusion has its own reason:**

| | Mergeable | Why |
|---|:---:|---|
| **Modules** | ✅ | They carry a rolled quality, so there is a position within a band to move |
| **Shells** | ❌ | Position and children — two merged cockpits have no answer to where they sit or what was mounted in them (§2.4 above) |
| **Materials** | ❌ | They carry a grade but **no quality roll** (§2.10), so there is nothing to move |
| **Materials** | ❌ | Fungible. There is no instance to improve |

⚠️ **Every roll above is deterministic from `(item id, tick)`** — the FNV-1a idiom `MiningSystem` and
`CommsSystem` already use, never a stateful generator. Law 2's fast-forward requires a banked macro
tick to resolve identically to a played one, and a stateful RNG breaks that silently.



#### Research cannot fail — from a second roll. It can still be destroyed from outside 📋

*Asked and settled 2026-08-08: should a research job have a chance of outright failure? Scope
corrected 2026-08-12.*

**No — not from a second roll, because it already has one, and it is a better one.** The
sample-survival roll above *is* the failure mechanic. Adding a **second roll** would mean two
probabilities compounding into four outcomes, two of which read identically to the player, on top of
a material cost and a time cost. Four brakes on one action is the pattern that got upkeep cut (§2.7).
**This has always been a rule about not stacking a second probability, not a guarantee that nothing
can end a job.** A job still dies if the facility hosting it is destroyed or unmounted mid-run
(§2.8's fuel section) — that is an external cause with its own existing consequences elsewhere in
this design, not a random check bolted onto research.

The sample roll is also the better-designed risk on its own terms:

- **The odds are known before committing**, so it is a gamble the player took rather than one the
  game sprang.
- **The loss is a concrete object** — that Mythic module you found — rather than an abstract span of
  time, which makes it land.
- **It scales with facility grade in the direction that motivates upgrading.**

Whereas failing after sixty-four minutes is a pure loss with no decision attached anywhere in it. If
research ever needs *more* tension, the lever is the sample odds, not a second roll.

**Timing:** the sample is locked for the job's duration and the roll resolves on completion, so the
moment of resolution coincides with the payoff.

*One variant considered and set aside:* research succeeding at a **capped manufacturing quality**
when performed at a low-grade facility, liftable by re-researching later. Genuinely interesting —
degraded success beats binary failure — but it needs a per-unlock quality cap that nothing reads
today, and §2.4's own dead-abstraction rule says not to author it before its consumer.

#### Do the workbench mechanics apply to shells too? 📋

*Settled 2026-08-07, now that shells carry a grade (§2.7) and are therefore a progression axis of
their own rather than inert housings.*

| Mechanic | Applies to shells? | Why |
|---|:---:|---|
| **Research** | ✅ **Yes** | "Learn to manufacture this cockpit grade" is exactly parallel to a module. Nothing about it depends on the item being a stat block |
| **Deconstruction** | ✅ **Yes** | A salvaged shell breaking down into materials is the same operation on a different input |
| **Engineering (merge)** | ❌ **No** | Merging is only coherent for stat blocks. Shells have **position and children** — two cockpits merged into a better one has no answer to where it sits or what happens to what was mounted in it |

**Shell acquisition, for the record:** high-grade shells are found through exploration and salvage,
or obtained by dealing with factions that already hold them. Research adds a third path — turning a
single recovered example into something manufacturable — which is what makes finding one matter
beyond the one hull you bolt it to.

❓ *Open: whether a shell can be **upgraded in place** to a higher grade at an Engineering facility —
consuming materials, keeping its position and mounted modules.* This is the coherent shell-shaped
analogue of merging and it is appealing, but it is a new mechanic in `RefactorSystem` territory
rather than a reuse of an existing one. Deferred; research and deconstruction cover the need first.

❓ *Open, and it is load-bearing:* **nothing manufactures a module.** Research's stated payoff is
that an item becomes "manufacturable," and there is no manufacturing mechanic, system, or menu anywhere in
the design or the codebase — `ConstructionSystem` builds ships and stations from blueprints, not
modules. Until this is specified, research has no consumer and its reward is unreachable.

**Faction gates** — access to higher tiers of reverse engineering is influenced by faction alignment
and reputation tier. High standing with a tech-focused faction (Zenith Collective, AI Concordance)
unlocks advanced blueprints faster.

**Unlocks are save-scoped, not account-level.** A reverse-engineered blueprint is not a permanent
achievement unlocked on the player's profile — it is a record held in a **knowledge network** inside
that specific save (§2.5). Nothing carries between saves.

### 2.5 Knowledge Networks 📋

*This section replaces the "account-level meta-progression" model of earlier drafts, which granted
permanence from outside the fiction. Networks give the same protection from inside it, and make it
something that can be built, defended, and lost.*

**The governing rule, and it covers every case:**

> **Knowledge lives in networks. Matter lives in ships and stations.**
> Death destroys the matter you were carrying. It never touches a network.

A **knowledge network** is the persistent store for everything that is *data* rather than *object*:

| Stored in a network | Not stored in a network |
|---|---|
| Reverse-engineering unlocks — what you know how to manufacture | Physical modules, weapons, Elements |
| Saved Templates (§2.2) | Ships, stations, fleet assets |
| Discovered systems and sensor intel | Credits and stockpiled goods |
| Contract and diplomatic standing history | Anything in a cargo hold |

**Networks are owned, and there is more than one.** They are not a single global player inventory:

| Network | Held by | Notes |
|---|---|---|
| **Player network** | The player directly | Follows the player across ship loss and respawn |
| **Sub-commander networks** | Each AI sub-commander (§4.5) | Independent. A commander can hold designs the player does not, and vice versa |
| **Faction network** | The faction as an entity | What a faction manufactures from. Separate from the player's even when the player *leads* that faction |

Separation is the point. Selling a Template to a faction (§2.6) copies it from your network into
theirs — which is precisely why they can then manufacture it forever, and why destroying their
stations does not un-teach them the design. Symmetrically, an AI sub-commander that survives your
death still knows what it knew.

**Networks die with their host.** A network is not immortal — it is destroyed when the thing that
holds it is. Losing every station, every sub-commander, and the player at once destroys the player's
side of this entirely, which is what makes Hard Game Over (§3.3) total rather than cosmetic. The
protection against that is **redundancy** — more stations, more commanders — not a meta-progression
layer handed to the player for free.

✅ **Settled 2026-08-11: raiding exists, but only sub-commander networks can be raided, and it costs
no new mechanic.** Checked against `core/knowledge/KnowledgeNetwork.h`: a network has no physical
anchor of its own today — the only thing that ties one to a specific object is `NetworkOwner`, placed
on the player entity or a specific sub-commander entity. A **faction's** general network is not
anchored to any single object and **stays permanently un-raidable by design** — there is no "capture
the capital's archive" move, matching that a faction's institutional knowledge was never meant to have
one point of failure. A **sub-commander's** network *is* anchored, to that commander's own vessel, and
§3.2's capture mechanic already converts a hull's crew — commander included — when it changes hands.
Raiding a rival's designs is therefore just **capturing their commander's ship rather than destroying
it**: the standard reward for the harder, riskier version of a kill you were already going to attempt.
Gives Zenith Collective and AI Concordance the thematic reason §9 originally wanted, at zero
additional design cost.

*Merging a raided network's contents into the capturer's own is a `KnowledgeStore::Copy()`-shaped bulk
operation across all three categories (a set union — no conflict resolution needed), extending the
single-Template version §12.7 already uses for a sale.*

### 2.6 Template Negotiation & Royalties 📋

*This closes the "Template economics" open question of earlier drafts, which asked whether selling a
design pays once or accrues per unit. The answer is that the player chooses, and that the choice is
negotiated.*

**The pitch** — a player may present a Template from their network (§2.5) to any AI faction they are
on speaking terms with. The faction evaluates the design on its own terms: whether it fits their
archetype's task weighting (§6.2), whether it beats what they currently manufacture, and whether
they can afford the materials it demands.

**Two payment structures, player's choice:**

| Structure | Payoff | Reinforces |
|---|---|---|
| **Lump sum** | One-time payment on acceptance | The Meso loop — immediate capital for the next build |
| **Royalties** | Ongoing cut per unit the faction manufactures | The Macro loop — passive income scaling with how much they like the design |

Royalties are the stronger Macro-loop hook by a wide margin: income that grows as a faction wins wars
using your design is the clearest possible feedback that the macro simulation is real and that you
changed it. They are also considerably harder to balance, since payout is driven by a Tier 3
production rate the player never directly observes.

**Negotiation** — the player may push for a better rate. The attempt rolls against the faction's
disposition toward them (reputation tier, §5.3 relation band, and archetype). Success raises the
payout; failure can reduce the offer or see the pitch rejected outright. A rejected design is not
destroyed — it stays in the player's network and can be pitched to a different faction, or to the
same one after standing improves.

**Selling is not exclusive.** The same Template can be sold to multiple factions, including factions
at war with each other. Nothing prevents this and it should not — two rival fleets flying the same
silhouette because the player armed both is the macro loop working exactly as intended.

✅ **Settled 2026-08-11: royalties do not survive the seller's death.** Under §2.5 the design still
sits in the buyer's network and keeps being manufactured regardless — that was never in question.
What was open is where the payment stream goes, and the answer is: nowhere, once the seller is gone.
Royalty rate scale (the exact number, not the mechanism) is deferred with §9's other undecided
constants, pending `tools/economy_sim`.

#### The seller is not always the player, and neither is the buyer 📋

*Settled 2026-08-11.* Everything above describes the player pitching to an AI faction — accurate as
far as it goes, but not the only trade this section governs:

- **A Commander may be the seller, not only the player.** `KnowledgeNetwork` already supports
  `Commander` as an owner kind alongside `Player`/`Faction` (§2.5) — this is `TemplateMarketSystem`
  widening who can originate a pitch, not a new concept. **A Commander's death stops their royalty
  stream the same way the player's would**, per the rule above — the payment is tied to the seller
  as an actor, not to whichever faction they happened to belong to.
- **Player-to-player (or more generally, actor-to-actor) trade is genuinely new scope.** Nothing
  above covers it today. It needs its own negotiation surface — choosing lump sum vs. royalty and
  setting the value — mirroring the AI-faction pitch flow but without a disposition roll deciding the
  outcome, since two players negotiate directly. This matters more here than in most games given
  `architecture.md`'s multiplayer support. **Left as a follow-on design pass, not specified here.**
- **Trade between factions themselves — not just player-to-faction — is the third acquisition path
  §2.4's recipe-acquisition table above already names.** A faction with `Allied`-band relations to
  another can buy a finished item or license its recipe the same way a player can pitch a Template,
  using the identical lump-sum/royalty structure.

#### A faction's ship roster is preset and research-unlocked, not reverse-engineerable 📋

*Settled 2026-08-11.* Unlike modules and shells (§2.4, freely acquirable by any of the four paths),
**a faction's ship designs are curated content, not a free-for-all.** Each faction manufactures from
a fixed, thematically-authored list, unlocked the same way any recipe is researched from scratch
(§2.4) — never by reverse-engineering a captured hull. A faction can still acquire *another*
faction's ship design, but only through trade (above), the same as any other recipe.

**This makes the player, and whichever crew type eventually designs Templates for factions, the only
sources of genuinely new ship arrangements.** Everyone else — every faction, acting alone — works
from its preset list, its research unlocks, or what it can buy. See §2.7's crew section for the
Template-Designer and Trade Master roles this implies; **how an AI faction's Template-Designer
actually invents a new arrangement (the algorithmic equivalent of a player using `CustomizeMenu`) is
still open** — the natural hook is §6.2's archetype weighting biasing what gets designed, but that
facet has the same "not yet computed" status as Tech Superiority above.

### 2.7 Skill 📋

*Raised 2026-08-07, because two mechanics independently asked for it in one sitting and each was
about to invent its own private version.*

**Skill is carried by a module.** Not a hidden stat, not a menu entry, not a separate entity type —
a **crew module** that mounts into a component the same way a weapon or a power cell does. A common
crew module is the default fit; better ones are looted, researched, or manufactured like any other
module.

| Mechanic | Whose skill | What it scales | Section |
|---|---|---|---|
| Combat targeting | The pilot's | Whether hardpoint selection is random or deliberate | §3.2 |
| Fleet command | The sub-commander's | Standing-order quality and autonomous decisions | §4.5 |

**Engineering is deliberately not on this list.** Merge quality scales with the *facility's* level
(§2.4), not with a crew module. The workbench is a place you visit, not a person you carry.

**Why the module model is the right one, and not just the cheap one:**

- **It reuses machinery that already exists.** Mounting, unmounting, cargo, loot, research, and
  manufacturing all work on modules today. A crew module gets every one of those for free, and a
  skill *stat* would have needed a parallel path for each.
- **Localized damage kills officers for free** (§3.2). Blow up a capital's bridge component and the
  commander module mounted in it is gone with it. That is §4.5's "destroying the capital they
  command destroys them," made sharper and mechanical — you destroy the *bridge*, not the whole
  ship.
- **It preserves §4.5's "entities, not menu entries."** A module mounts on a hardpoint, and a
  hardpoint *is* an entity. The commander lives on that entity. Nothing about §4.5 needs rewriting.
- **Promotion becomes a physical act.** Moving your best pilot from a fighter's cockpit into a
  carrier's bridge is a module transfer, not a menu abstraction.
- **Capture and loss follow the same rules as any other cargo.** An officer can be lost in a wreck
  and salvaged by someone else, which §2.5's knowledge/matter split already governs: the crew
  module is matter.

#### One `ModuleKind::Crew`, and what it does is what it rolled 📋

> ⚠️ **Consolidated 2026-08-09. This replaces the two-kind model (`Operator` + `Commander`) settled
> 2026-08-07.** Both are now a single **`ModuleKind::Crew`** carrying **two rollable stats**.

| Stat | Consumer | A zero roll means |
|---|---|---|
| **`operation`** | `TargetingSystem` — biases NPC hardpoint selection (§3.2); flight and gunnery boosts | The crew cannot usefully work the shell they are in |
| **`command`** | `CommanderSystem` — standing orders and fleet dispatch (§4.5); **command authority** (§4.0) | **The rig cannot command at all**, whatever its comms |

**A rig needs one crew module, not two.** A single `Crew` module covers operating *and* commanding, so
no shell is ever required to hold a matched pair.

> ⚠️ **Floored 2026-08-11: neither stat may roll to exactly zero, and this reconciles a tension in
> the text above rather than adding a new rule.** The stat table says a zero `operation` roll means
> *"the crew cannot usefully work the shell they are in"* — stronger language than "officers multiply,
> never create" (below) would suggest, since a pure multiplier hitting zero should mean *no bonus*,
> not *unable to work the shell*. Whichever reading is correct, an explicit floor above zero on both
> `operation` and `command` removes the ambiguity and is required by an already-settled mechanic:
> §4.0/§6.5 sanction a commanding crew module sitting alone in a **1-crew-slot cockpit** (*"a fighter
> can now be a boss"*), and there is no second module possible there to cover for a zero. A build
> that dumps its whole budget into `command` still keeps a non-zero floor on `operation`, and
> vice versa — specialization stays real (§2.7's weighted-budget rule is unchanged), it just never
> reaches a stat that breaks an already-sanctioned scenario.

##### Why two kinds became one 📋

The 2026-08-07 model was argued on two grounds, and **both expired on 2026-08-09**:

| Original argument | Why it no longer holds |
|---|---|
| *"One kind with two ratings meant every crew module shipped a number that was dead everywhere except one shell type"* — a command rating was dead outside a bridge | §4.0 lets a **cockpit** command. A command rating is now live in every crew slot |
| Mountability falls out of `IsMountable(ModuleKind, ShellRole)` for free with one rating per kind | Both kinds now mount in exactly the same places, so the table distinguishes nothing |

With both objections gone, two kinds where one is a strict superset of the other is redundant
taxonomy. One kind is smaller in the enum, smaller in `ModuleAttachment`'s `switch`, and needs no
"which rating do I read here" rule — because the answer is always *both, and whichever is non-zero
does its job*.

##### The budget rule is what makes this work 📋

**A consolidated crew module is not strictly better than a specialised one**, and that is not an
accident of tuning — it falls out of §2.7's existing budget distribution. A grade sets a *total*
budget and caps how many stats it spreads across, so:

| The roll spent its budget on | You got |
|---|---|
| All operation | **An ace pilot.** Zero command — this module cannot command anything |
| All command | **A fleet admiral.** Baseline operation — brilliant strategist, flies like a rookie |
| Split | A capable both, excellent at neither |

Your "Operator or Commander" either/or survives exactly, but as a property of **the roll** rather than
of the taxonomy — and a Mythic that dumped everything into command genuinely does fly worse than a
good Common that dumped everything into operation. **Nothing is strictly better than anything.**

This is §2.7's own stated goal for the band arriving without a crew-specific rule: *"a Mythic that
rolls k = 1 is the most potent single-stat item in the game. Specialists stay relevant at every
tier."* The ace and the admiral are the two single-stat rolls of one kind.

##### What this revises 📋

- **"Two crew modules in the same shell buys nothing"** *(stated below, now false)*. It was true when
  both carried the same single rating. Two `Crew` modules in one bridge now cover **different
  functions** — the best `operation` roll flies the hull, the best `command` roll runs the fleet, and
  they need not be the same module.
- **The two-berth bridge is justified emergently, not by authored roles.** A bridge with two slots is
  *better* staffed by two specialists and *adequately* staffed by one generalist. That is a stronger
  position than the old one: crew stay **optional and authored, never mandatory**, which is what this
  section wanted from the start.
- **Each *function* takes the best available rating for that function**, across all living crew on the
  rig. Not "the highest-rated module operates" — that question is now ambiguous and the per-function
  reading is the correct one.

**"Crew" rather than "Pilot" or "Officer", deliberately.** The same person who flies a hull works a
turret and runs a fleet; *where it mounts and what it rolled* decide which of those it is doing. A
name tied to one job would misread in the other two.

**Promotion still works, and is now simpler.** Moving your best crew module from a fighter's cockpit
into a capital's bridge is the same physical transfer, and there is no second module to bring along.

**The operation rating does nothing on a ship the player is personally flying** — §3.2 makes the
player's aim manual, so their marksmanship *is* their piloting skill. It matters on every vessel the
player **owns but is not sitting in**, which under §4.5 is most of a developed fleet. *This is why the
player's own cockpit wants a high-`command`, low-`operation` roll: the operation half would be dead
weight there, and the command half is what lets them command while flying (§4.0).*

#### Four roles, and two of them are buildable today 📋

*Revised 2026-08-08. An earlier draft of this section rejected sensor, damage-control, and navigator
officers together, on the grounds that each needed a new mechanic first. **That assessment predates
§8.3's fog-of-war decision by one day and is now half wrong.***

"Consumer" means precisely this: **a system that reads the stat and changes behaviour.** §2.4's rule
is only that you do not author a number nothing reads. Against the code as it stands:

*Read "role" here as **a stat in `Crew`'s pool**, not as a separate `ModuleKind` — the two-kind model
this table was written against was consolidated on 2026-08-09 (above). Each row below is a rollable
stat one crew module may or may not have spent budget on.*

| Role | Consumer | Status |
|---|---|---|
| **`operation`** | `TargetingSystem`, `WeaponSystem`, `FiringArc` | ✅ Built or specified |
| **`command`** | `CommanderSystem`, and command authority (§4.0) | ✅ Specified (§4.5) |
| **Sensors** | `SensorRange` exists; `DiscoverySystem` reads it — and §8.3 just made sensor coverage **strategic infrastructure** | ✅ **Buildable now** |
| **Repair** | ~~`DockingSystem`'s dock-repair rate~~ → the **Repair facility's** `ratePerSecond` (`architecture.md` §12.30.4) | ✅ **Buildable now**, but see below |
| **Damage control** | Needs **in-flight repair**, which does not exist. Docking is the only heal | ❌ New mechanic first |
| **Navigator** | No skill hook in `WarpSystem` — would need warp charge time, jump accuracy, or fuel | ❌ New mechanic first |

**So there is real runway: four roles before a new mechanic is required.** That also retires the
claim below that a bridge beyond two berths has nothing to hold — `crewSlots` can scale past 2, and
each new role still lands in the same commit as its consumer.

> ⚠️ **The Repair role's consumer moves, and it nearly disappeared.** `architecture.md` §13.4
> decision 1 deletes `DockingSystem`'s free unconditional heal — the exact rate this table names as
> the Repair role's consumer, listed twice as ✅ *"Buildable now."* Deleting the heal without moving
> the rate would have turned this row into ❌ *new mechanic first* silently. **§12.30.4 moves the rate
> onto `FacilityStats::ratePerSecond`**, a field that is parsed, scaled by `EngineerSystem`'s merge,
> and read by nothing today. The role survives, and it now boosts a service the player pays for
> rather than one that was free — which is the version where a Repair officer is worth carrying.

The two blocked roles are the natural expansion path, and each is a small feature in its own right
rather than a stat: in-flight repair would also give §3.3's attrition a counter, and a warp hook
would give `WarpSystem` its first tuning surface.

> ❌ **Withdrawn 2026-08-09.** This paragraph previously read: *"Settled: two kinds, each gated by
> what it mounts into. Mountability does the work a single-kind design would have needed a special
> rule for: a `Commander` fits a bridge and nothing else, so a fleet admiral cannot be stuffed into a
> fighter cockpit for a targeting bonus."*
>
> **Both halves stopped being true on 2026-08-09.** §4.0 deliberately *wants* a commander in a
> fighter cockpit, and with both kinds mounting in the same slots, mountability distinguishes
> nothing. The concern the paragraph was protecting against — buying a targeting bonus you did not
> pay for — is now handled by the shared roll budget instead: a crew module that spent its budget on
> `command` has baseline `operation`, so a fleet admiral in a cockpit is a *worse* gunner, not a
> free one. See the consolidation above.

#### Three more roles raised 2026-08-11, at varying stages of settled 📋

**Facility manager — agreed, with a framing that avoids contradicting an already-settled rule.**
A crew stat that boosts a station facility's **automation/speed**, not its output quality —
`EngineerSystem`'s merge quality is already specified as scaling with *facility grade*, not crew
skill (above), and that stays exactly true. Facility grade remains the capability ceiling; a
facility-manager crew module modifies how fast a facility works within that ceiling, the same
relationship `operation`/`command` have to the modules they multiply rather than replace.

**Template Designer — agreed, and it's what answers §2.6's "who can create new ship arrangements."**
A crew type or facility letting a *faction* originate new Templates, the algorithmic equivalent of a
player using `CustomizeMenu`. **Still open: how an AI faction's designer actually invents an
arrangement** — the natural hook is §6.2's archetype weighting biasing what gets designed (Kore
builds rugged and cheap, Zenith builds sensor-heavy), but that facet is uncomputed today, same as
Tech Superiority (§2.4). Without a Template Designer, the player is the *only* source of new ship
arrangements in the entire simulation — every faction, acting alone, is otherwise limited to its
preset roster (§2.6).

**Trade Master — agreed, replacing Commander as the default template seller.** A dedicated crew role
for negotiating and orchestrating sales/royalties, separate from fleet command — Commander can still
be a seller (§2.6 already settles that Commander is a valid `KnowledgeNetwork` owner), but selling
stops being *specifically* a command function once this role exists to do it instead.

**Crew bribery — ✅ settled 2026-08-11.** A low base chance to convert **any** `Crew` module — not
only officers or commanders — to the briber's side, improvable by paying credits, with **Mythic-grade
crew simply immune regardless of offer** (the rarity ladder already gives every other tier a reason to
exist; this gives the top one a property beyond stats). A gamble, not a purchase: paying more raises
the odds, it never guarantees the outcome.

**A success is outright defection.** The crew module leaves its post — vacating a Bridge slot,
a turret, a facility role — and joins the briber's `Crew` roster. That post then sits empty until
replaced, which is the real cost: bribing a rival's commander is a strictly better outcome than
killing one (§3.2's capture already establishes this), but bribing a rank-and-file gunner is
sabotage with a side benefit. **Symmetric by default**, per Law 4: the player's own crew is exactly
as biddable as an NPC's, using the identical roll.

**The roll reuses `architecture.md` §12.7's Template-negotiation shape** rather than inventing a
second formula: a base chance, modified by the relation band between briber and current owner
(§5.3) and by credits committed, resisted by the target module's own grade (§2.7's seven-tier
ladder — a Legendary is harder to turn than a Common, a Mythic is impossible). No new pricing model,
just a second consumer of one already-specified curve.

**How each of the four directions actually happens, since the mechanism is one roll but the trigger
differs by who's attempting it:**

| Direction | Trigger | Resolution |
|---|---|---|
| **Player → NPC** | Requires proximity or comms contact with the target vessel — docked, boarded, or hailed, the same "speaking terms" gate §2.6's Template negotiation already uses | Player-initiated, resolves immediately against the roll |
| **NPC → Player** | A rival faction hails the player (existing `CommsSystem`/comms-hail channel) and names a target crew module | Resolves against the same roll; the attempt and its outcome post to `CommsLog` — the log's first real reader (§13.3 finding S notes it is write-only today) |
| **NPC → NPC** | Background: a Tier 3 macro-tick roll, the same shape as `FactionDecisionEngine::RaidDispatchChance` (§6.4), gated by relation band and paid from the acting faction's `FactionEconomy` stock | Resolves silently in the simulation; no player-facing UI required |
| **Player → Player** | Deferred with multiplayer (`net/`, 🧊) | Needs no separate design — the roll is already keyed by `FactionId`/relation, not by "is this the player," so it falls out for free once multiplayer exists, the same inheritance pattern every other Law-4 mechanic in this design uses |

No new UI surface for the Player→NPC direction beyond a "Bribe" action wherever the target's crew
roster is already visible (a docked/boarded interaction, not a new screen).

> 📋 **Also raised and deliberately deferred: crew training/leveling.** Whether a crew module's stats
> can improve after acquisition (reroll vs. flat increment, a new facility kind to host it, time and
> resource cost) is a real system with real interactions, not a one-line addition. Recorded as a
> known commitment, not decided here — its own design pass when picked up.

#### Fire control is its own `ModuleKind`, and there is no `Auxiliary` 📋

*Settled 2026-08-08 alongside the turret change above, which needs it.*

**`ModuleKind::FireControl`** — a module that supplies automated tracking. Its stat pool drives
`FiringArc::turnRatePerSecond` (traverse), lead accuracy, and target reacquisition, stacking with an
`Operator`'s boost when both are present.

**This retires a dead field.** `turnRatePerSecond` is read by `WeaponSystem` but **hardcoded to
`kPi`** in `ModuleAttachment.cpp` and authored nowhere — one of three boost targets §2.7 names that
have no authored input. Fire control is what should drive it.

> ❌ **No `ModuleKind::Auxiliary`.** A catch-all kind becomes a taxonomy dump folder, and it has two
> concrete costs: `ModuleAttachment`'s `switch (ModuleKind)` could not know which components to
> attach for an "Auxiliary," and mountability could not distinguish a fire-control unit from a
> tractor beam. **Each functional module is its own kind.**

**Grouping belongs to presentation, not taxonomy.** An optional *display category* on `ModuleDef` lets
`CustomizeMenu` and `StorageMenu` show an "Auxiliary" heading over fire control, sensors, and whatever
follows — so the UI groups freely while the type system stays exact.

#### Crew slots by shell type 📋

Assigned by what a shell *is*. **Grade does not move these** — the turret 0 → 1 tier gate was
withdrawn on 2026-08-08 (see below); every turret has one slot regardless of grade:

| Shell | `crewSlots` | Who sits there |
|---|:---:|---|
| **Bridge** (capital, station) | 2 | One `Crew` module suffices; two lets a specialist fly and another command (§2.7) |
| **Cockpit** (fighter) | 1 | One `Crew` module — flying, commanding, or a split of both, per its roll |
| **Chassis** | **0, always** | **Settled 2026-08-11 — no longer 0–1.** Chassis death is rig death (§3.7); a crew slot living on the chassis would always die in perfect lockstep with the ship itself, with none of the distinct exposure a Bridge/Cockpit crew slot has under localized damage (§3.2). No interesting variance, just an authored slot with nothing to say. A co-pilot berth belongs on a shell that can be lost *without* the ship dying, not on the one that guarantees it |
| **Turret** | **1, always** | Crewed = independent tracking; uncrewed = slaved, unless a fire-control module is fitted (§2.7) |
| Thruster, power bay, wing, armour | 0 | — |

#### Modules provide base stats; officers provide percentages 📋

*Settled 2026-08-07, and it is the rule that keeps crew from becoming stat-sticks.*

> **A module supplies a capability. An officer multiplies it. An officer never creates one.**

An `Operator` in a turret does not give that turret damage — the weapon module does. The officer
raises its fire rate, traverse, or accuracy by a percentage. **Nothing an officer boosts exists
without the module underneath it.**

Three properties follow, and all three are wanted:

- **Officers scale with the quality of the fit.** A great gunner on a Mythic turret is worth far more
  than the same gunner on a Common one. Crew quality and equipment quality reinforce each other
  instead of substituting.
- **Officer power is bounded by construction.** A percentage cannot conjure capability from an empty
  shell, so no amount of crew turns a stripped hull into a warship.
- **Placement is the decision.** The boost applies to **the shell the officer occupies** — which is
  what makes "where do I put my best operator" a real question rather than a formality. When that
  shell is a control shell (cockpit or bridge), the scope widens to the whole rig, which is exactly
  what a pilot or a commander should do.

**Magnitude comes from the quality band and is spent as a budget** (§2.7's "Budget distribution") —
an officer's grade sets a total boost budget and caps how many of its role's stats that budget may
be spread across. A specialist officer concentrates it; a generalist spreads it thinner. This is the
same mechanism every other module kind uses, not a crew-specific rule.

#### Living and artificial officers ✅

*Proposed 2026-08-08, settled 2026-08-13.* Officers come in two flavours, acquired two different
ways, and the choice between them is a real one that changes as the player's economy matures.

| | **Living** (hired) | **Artificial** (manufactured) |
|---|---|---|
| Acquired by | Credits, at a station | `Circuit Wafer` + credits, at a Manufacturing facility (§2.8) |
| Ongoing cost | **A recurring salary** | **None** — build it once, it works forever |
| Skill ceiling | Higher | Lower, but perfectly consistent |
| Mass | Lower | Higher — it is hardware |
| Researchable | No | **Yes** — it is manufactured content like any other module |

**Both cost credits, deliberately.** If Artificial were the free option, Living would only ever be
a cash-poor starting crutch rather than a standing choice. Charging credits for both keeps the
tradeoff live for the whole campaign: salaried-and-flexible vs. paid-once-and-fixed, not
cheap-now vs. expensive-later.

**The material is `Circuit Wafer` specifically**, not a generic "materials" placeholder — it is
already the family §2.10 tags "Feeds: Targeting, avionics, crew modules," and it is what every
entry in the Crew Roster (below) already builds from. An artificial officer is a crew module like
any other; it uses the crew recipe, plus the manufacturing facility and the one-time cost that
`Circuit Wafer`'s presence in a recipe always implies.

**The strategic shape this produces:** hire early, when credits are easier to come by than a
production base; build robots later, once you have the pipeline and a standing fleet whose salary
bill has become the thing limiting you. That is a decision that changes over a campaign rather than
being answered once.

**It also gives an existing rivalry mechanical teeth.** §5.6 describes Kore Industries ↔ AI
Concordance as *"labor vs. automation — Kore sees an intelligence that makes workers obsolete; the
Concordance sees hand-built hydraulics as gross inefficiency."* Today that is pure flavour. With two
officer flavours it becomes something the player *does*: crewing artificial pleases the Concordance
and offends Kore, and vice versa. A rivalry the design already committed to gains a lever without
inventing anything.

> 💡 **Candidate mechanism raised 2026-08-11, not yet adopted: Ion vulnerability as a third axis.**
> The table above already has two balance levers (upkeep vs. none, skill ceiling vs. consistency) —
> this would be a third, using a weapon type that already exists rather than a new stat. Ion is
> already specified as attacking power/electronics rather than hull (§3.1); making **artificial
> officers specifically vulnerable to Ion** (living crew unaffected) gives synthetic crew a real
> tactical downside beyond cost, gives Ion a second niche beyond shield-stripping, and reinforces the
> same Kore ↔ AI Concordance framing from a combat angle instead of only an economic one. Offered as
> a candidate for whichever pass finalizes this section, not asserted as settled.

#### Upkeep is a general module property, not a crew feature 🧊

*Specified 2026-08-08 and immediately **deferred** the same day — see the note at the end of this
subsection. Recorded in full because the reasoning is sound and it will be wanted later.*

Salary is built as **upkeep** — a recurring credit cost any module may declare, of which a living
officer's wage is simply the first user.

**Why general rather than crew-specific.** The machinery is identical whatever is being paid for, and
a crew-only version would have to be torn open the first time anything else wants a running cost.
Plausible future users are already visible in this document: facility maintenance, licensed or
leased technology, and faction tribute (§5).

| | |
|---|---|
| **Declared on** | `ModuleDef` — an upkeep figure in credits per period. Zero for almost everything |
| **Period** | The **macro tick** (§1.1, ~30 s). It already exists, it is the Tier 3 heartbeat, and it accrues identically whether or not the player is in the system |
| **Paid by** | The rig's `Wallet` for the player; `core/economy/FactionEconomy` for an AI faction |
| **On non-payment** | **The module is removed** — the officer leaves, the lease lapses. It returns to the market |

**Never accrue debt.** A player returning after a week away should find a thinned-out roster, not an
unrecoverable deficit. Removal is self-limiting: the bill shrinks as it goes unpaid, so the situation
always resolves rather than spiralling.

**Artificial officers have zero upkeep**, which is the whole point of them — the trade is a large
one-time material and research cost against a bill that never arrives.

> 🧊 **Deferred 2026-08-08, and for a good reason: the game already has upkeep.** Repairing hulls,
> refuelling, and assembling replacement ships are all recurring credit and material sinks that
> scale with fleet size — which is everything a salary system was going to provide. Adding an
> abstract bill on top would tax the player twice for the same thing and add a subsystem to
> maintain. Revisit if and when automation removes enough of those manual sinks that fleet size
> stops costing anything to sustain.
>
> **Living vs. artificial officers survive this intact**, because salary was never their most
> interesting difference. The real one is that **you cannot mass-produce people.** Living officers
> are found and hired one at a time; artificial officers are researched once and then manufactured
> as fast as the production base allows. That is the axis that matters to the Macro loop, and it is
> the one that makes the Kore ↔ AI Concordance rivalry (§5.6) legible in play.

**Multiple boosts per officer, drawn from one budget** (revised 2026-08-08). An earlier draft
allowed each kind a single boost, on the reasoning that an `Operator` improving fire rate *and*
traverse *and* shield regen is "the stat-stick failure wearing a percentage sign." **That failure
only occurs when each stat receives the full value** — then higher tier means strictly more, with no
decision attached. Under §2.7's shared budget it does not arise: spreading across three stats gives
roughly a third each, so breadth is paid for in depth at every tier.

What remains true is that **new *effects* come from new roles**, not from widening an existing role's
pool past what its consumer systems read.

*Boost targets with a consumer that already exists:* weapon cooldown (`WeaponSystem`), turret
traverse (`FiringArc`), shield regeneration (`DamageSystem`), thrust and turn rate
(`PhysicsSystem`), sensor range (`SensorRange`), and dock repair rate (`DockingSystem`). **In-flight
repair does not exist** — a "heals faster" officer boost needs the docking path or a new mechanic
first.

#### Officers are modules; the rest of the crew is a complement 📋

*Settled 2026-08-07.* A capital should feel crewed by hundreds, and a hundred crew modules would be
absurd. The split:

| | Represented as | Mechanically |
|---|---|---|
| **Officers** — `operation`, `command`, and future stats | `Crew` **modules** in `crewSlots` | Real. Each stat has exactly one consumer system |
| **General crew** — sensors, damage control, deck hands, gunners' mates | A **complement** number derived from the hull's shell count | Narrative only, for now |

**Only model a crew role once a system reads it.** That is §2.4 applied to content: a sensors rating
with no system varying `SensorRange` is a number in a tooltip, and it makes the fit screen longer
without making a single decision more interesting.

**The complement carries the fiction for free.** A hull's shell count already scales with its size,
so "this capital berths 340" is derivable rather than authored, appears in UI and comms, and costs
nothing. If a mechanic later wants it — boarding resistance during a capture (§3.2), or a casualty
figure when a hull is disabled — it is already there to read.

**The real risk of modelling everyone** is not performance, it is tedium: choosing between two or
three officers is a decision, and staffing eight posts on every hull is payroll. Each new role must
justify itself as a *choice the player wants to make*, not as a slot that wants filling.

#### Crew modules draw no power 📋

*Settled 2026-08-07.* A crew module's `powerDraw` is **zero**.

**This is a value, not an exception.** §2.2's rule reads "modules draw power," and the cleanest way
to keep that universal is for the draw to be a number that happens to be zero — not a carve-out in
`PowerSystem` for one module kind. A rig that browns out does not un-crew itself.

#### Life support: cut 🧊

*Considered and removed 2026-08-07, before anything was built.* A power-drawing Life Support module
with a draining reserve was specified in an earlier draft of this section. It is **out of scope** —
not deferred pending a trigger, simply cut.

**Why it was cut, and the reason is the interesting part.** It was not too complex on its own. It was
that it coupled to almost everything else decided in the same sitting: it added a mandatory shell to
every crewed hull (pushing on §3.5's scale question), a second clock for the player to watch, a new
failure mode to balance, and a new state for §2.9's power levels to mean something in.

**Cutting it deletes an entire state from the crew model.** Life support was the *only* mechanism
that could kill a crew module while its shell survived. Without it, **crew and shell always die
together** — which removes the "surviving turret loses its operator" case entirely, and with it the
fallback-to-slaved-on-crew-death path, from both the design and the code.

What is lost is real but small: cutting power no longer has a human cost, and "kill life support to
take an intact prize" disappears as a capture route. §2.9 keeps plenty of stakes without it.

#### What limits crew modules instead 📋

With life support gone, a crew module is an ordinary module: **it adds mass, draws no power, and
carries a skill rating.** Three limiters keep it in check, and they are complementary rather than
redundant:

| Limiter | Caps | Why it holds up |
|---|---|---|
| **`crewSlots`** | *How many* | A hard cap the player cannot buy past. A hull has the slots it has |
| **Mass** (§2.2) | *The cost of each* | Recurring, not one-time. A hull stuffed with reserve crew turns badly |
| **Credits / rarity** | *How good* | Gates skill tier, not availability |

**Credits should gate quality, never quantity.** A pure credit cost is a one-time gate, and in a game
whose §1 pitch is infinite progression, anything priced only in credits stops mattering the moment
the player is rich. Slots and mass do not evaporate with wealth — a rich player still has N slots and
still pays the turn rate. So: common crew modules cheap and plentiful, high-skill ones expensive,
rare, or research-gated.

#### What a crew module needs to actually function 📋

The minimum set, in dependency order. Nothing here should land ahead of its consumer (§2.4).

| # | Piece | Where |
|---|---|---|
| 1 | A `ModuleKind` for crew, plus its `ShellRole` mountability rules | `modules.json`, `Taxonomy` |
| 2 | A skill rating on the module — piloting and command | `ModuleDef` |
| 3 | **A first reader.** Smallest is `TargetingSystem` biasing its random hardpoint roll | §3.2 |
| 4 | **The uncrewed-hull rule** — what a rig does when its crew module is destroyed | §3.2, below |
| 5 | Crew is matter (§2.5): lootable from a wreck, tradeable, deconstructable | existing systems |

Items 1–3 are mechanical. **Item 4 is the design content**, and it is what makes crew modules
interesting rather than a stat bonus in a box.

#### Proposed: crew degrade, they do not switch off ❓

*Proposed 2026-08-07, pending sign-off. This is the model that answers all of the crew questions
consistently; each answer below falls out of the one rule rather than being decided separately.*

> **Every crewed function has an operator. If its operator dies, the function falls back to the
> next-best living crew module on the rig, at reduced effectiveness. Only when nothing is left does
> the function stop.**

| Situation | Result |
|---|---|
| **Cockpit / bridge destroyed** | Flight control falls to any other living crew module on the rig, at reduced skill. None left → uncrewed hull (§3.2) |
| **Two crew modules in the *same* shell** | **Each function takes the best rating for it** — one may fly while the other commands *(revised 2026-08-09; this row previously read "buys nothing," which was true only while a crew module carried a single rating)*. They still die together, so it buys capability, never survivability |
| **Two separate cockpits / bridges** | Real redundancy. Kill one, command falls to the other. Costs mass and the hardpoints that could have been guns |
| **Turret with its own operator** | Acts independently — its own target, its own arc |
| **Turret with no operator** | Slaved to the hull's aim point: the cursor for the player, the rig's `Target` for an NPC |
| **No crew anywhere** | Uncrewed hull. Drifts, nothing fires |

**The second row is the design teaching itself.** Stacking crew in one shell does nothing;
distributing them across the hull is what buys survival. That is §3.2's localized-damage lesson
delivered through the fit screen, and it is the same "redundancy, not permanence" principle §4.5
applies to sub-commanders — one rule at two scales.

**A destroyed bridge must not brick a station.** Facilities keep running when the bridge dies — a
manufacturing bay is a machine, not a decision. What stops is *coordination*: fleet dispatch,
standing orders, and any hardpoint that depended on the bridge as its fallback operator. Making
everything on a hull depend on one shell would recreate the **protected core that §3.2 explicitly
rejects**, merely inverted — one lucky shot disabling an entire station is a single point of failure
by another name.

#### Crew slots — one authored field carries all of it 📋

*Settled 2026-08-07.* **A shell declares how many crew slots it has.** That single number answers
every crew question that was still open:

| `crewSlots` | Meaning |
|:---:|---|
| **0** | No crew may mount here. A turret on this shell is always **slaved** to the hull's aim point |
| **≥ 1** | Crew may mount. On the control shell they *operate* it; elsewhere they are **reserve** |

**Reserve pilots are allowed only where a crew slot exists** — not on any shell at a penalty. This
is the refinement that makes the earlier "distributed crew" idea concrete: a chassis may declare a
slot for a co-pilot, a wing gun does not, and where redundancy is possible becomes an authored
property of the hull rather than a universal rule.

It resolves the operator question too. **Operators are optional and authored, never mandatory** — a
shell with `crewSlots: 0` is simply always slaved. Mandatory operators would mean a capital with
eight turrets needs eight gunners before it can fight, spending the whole mass budget on crew and
making a single-seat fighter absurd. Optional turns crew into a **fit decision with a real trade**:
pay mass and a slot for a gunner, and that turret covers angles you cannot.

#### Shell grade 📋

*Settled 2026-08-07.* Shells carry a **grade** on a shared rarity ladder (common → … → mythic),
matching the one modules use. **Grade never changes a shell's radius** — a better shell is
**better, not bigger**, which keeps §3.5's geometry (and therefore a hull's hardpoint count) a
function of the shell *type* rather than of what dropped. *This previously read "orthogonal to §3.5's
uniform hit radius"; that model was withdrawn on 2026-08-08, but the property it protected still
holds and now has to be stated directly rather than inherited.*

**A higher grade is lighter, not heavier.** High-grade shells are hard to find — §2.4's acquisition
paths are exploration, salvage, faction dealing, and research — and taxing that success with extra
mass punishes the player for succeeding. Better engineering meaning better strength-to-weight is also
the more plausible fiction.

*This corrects an earlier draft of this section, which argued grade must cost mass to satisfy §2.2's
"no strictly-best loadout." That was a misreading.* §2.2 governs **loadouts, not items** — it
promises there is no single best *configuration*, because finite slots and a finite mass budget force
a choice between guns, armour, speed, and crew. It does not promise that no item outclasses another.
Rarity-gated items that are simply better are normal, and the loadout puzzle survives them intact:
a mythic chassis still has finite slots, and you still cannot fit everything.

**What grade should scale, in order of how immediately useful each is:**

| Property | Field | Payoff |
|---|---|---|
| **Module capacity** | `moduleSlots` — **already exists on `ShellDef`** | Immediate. A mythic chassis carries four modules where a common one carries two. Zero new mechanics |
| **Mass** | `mass` | Immediate. Lighter at higher grade, per above |
| **Hull** | `hull` | Immediate. Tougher hardpoint, harder to strip |
| **Structural mass threshold** | `structuralMassCap` (new) — Rule 4's per-chassis cap | **Settled 2026-08-11.** Without this, `moduleSlots` growing with grade is a trap: a Mythic chassis gains a fourth slot it may not be able to afford to fill, since nothing raised what it can carry. See below — this steps with `moduleSlots`, not the quality band |
| **Crew slots** | `crewSlots` (new) | **Useful only up to two today** — see below |

#### How each property scales across the ladder 📋

**Three mechanisms, one per property, never two on the same property** (revised 2026-08-08 — see
the quality band below, which replaced what was previously a separate hand-authored hull curve):

| Property | Governed by | Why |
|---|---|---|
| **Every capability stat** — `hull`, weapon damage, shield capacity, thrust, officer boost | **The quality band** | One mechanism for everything a higher grade makes *better*. Gives per-item variance for free |
| **`mass`** | **The settled ladder** — 100% → 70% at Mythic | Must move *down* with grade, so it cannot ride the same band |
| **`moduleSlots`** | **Step** — +1 at Unique, Epic, Mythic | Discrete, legible, and it caps the compounding |
| **`structuralMassCap`** | **Step, same tiers as `moduleSlots`, same instant** — +*N*% at Unique, Epic, Mythic | Deliberately **not** the quality band — a structural budget rolling per-instance would let two Mythic chassis of the same type carry different maximum loadouts, which fights planning around a known chassis rather than rewarding it. Deliberately **not** an independent curve either — it steps exactly where `moduleSlots` steps because the two are causally linked: the extra slot and the capacity to use it should land together. Exact percentage is a tuning question, not decided here |
| **`crewSlots`** | **Authored per shell, not grade-scaled** — steps as new crew roles ship | See below |

> ⚠️ **Tier must be applied exactly once per property.** An earlier draft of this section carried a
> separate hull curve (100 → 325%) *alongside* the quality band. Those are the same curve written
> twice — a Mythic shell would have come out at `325% × 3.0–5.0` = **975–1,625%** of a Common's
> hull. The hull curve is deleted; `ShellDef.hull` is a base value and the band scales it.

The step axis gives the *"another slot!"* moments; the band fills the gaps and makes two items of
the same grade differ. Putting both on one property makes the curve unmanageable.

#### The quality band — one roll, every capability stat 📋

*Settled 2026-08-08. This closes a gap neither the ladder nor `ModuleDef` ever covered: the ladder's
columns are shell properties, and **nothing said what grade does to a module's stats.***

> **A grade defines a multiplier band. Every instance rolls a point in its band at creation.**

| Tier | Band |
|---|---|
| Common | ×0.90 – 1.10 |
| Uncommon | ×1.00 – 1.30 |
| Unique | ×1.15 – 1.55 |
| Rare | ×1.35 – 1.85 |
| Epic | ×1.50 – 2.50 |
| Legendary | ×2.00 – 3.50 |
| Mythic | ×3.00 – 5.00 |

**Every adjacent pair overlaps, and that is the load-bearing property.** A high-roll Rare (1.85)
beats a low-roll Epic (1.50). So every drop is worth reading, a good item stays good for a long
time, and the ladder does not obsolete itself one tier at a time.

**Why a 5× ceiling is acceptable here.** §2.4 already settled that research has no tier cap and that
**the gate is economic**. A Mythic is reachable and ruinous to field at scale — that is the same
decision applied again, not a new one. The brake lives in §2.8's manufacturing cost and input chain,
never in the ceiling.

**Manufacturing re-rolls quality** (§2.8). Without that, a single Mythic drop rolling 3.0 out of a
3.0–5.0 band would be permanent, and a 2.0-wide band on a 1-in-1,000 item would be punishing.
With it, approaching the ceiling becomes an *industrial* pursuit and the production base has a
reason to keep running past volume — which is §1's Macro loop.

**Merging clamps to the band** (§2.4) — it moves an item up *within* its grade and never out of it.
Otherwise merging becomes the way to exceed tiers and the ladder stops meaning anything.

~~**Implementation note:** the mechanism already exists. `ContentLibrary::RegisterCraftedModule` — the
runtime-registered overlay `FindModule` checks before the JSON set — was built for `EngineerSystem`'s
merged modules. A rolled instance registers through the identical path.~~

> ❌ **Withdrawn 2026-08-10 (`architecture.md` §12.19).** That path makes every rolled instance a
> **definition**: the overlay grows one permanent entry per manufactured unit, has to be saved, and
> is never collected. A merge is rare enough that nobody noticed; **manufacturing is a loop.**
> §2.7's own next section already chose the other answer — an instance stores its budget point and
> its distribution and recomputes its stat block from `def + quality` — and that is the one that
> scales. **A rolled instance is a value that travels in the hold, not an entry in a content table.**
> `RegisterCraftedModule`, `craftedModules_` and the merged module's generated id are all retired
> with it.

#### Budget distribution — how a roll is spent 📋

*Settled 2026-08-08, and it applies to **every** module kind, not only crew.*

A grade does two things, not one:

| | |
|---|---|
| **Band** | Sets the instance's **total budget** |
| **Grade** | Caps **how many stats** the budget may be spread across — 1 at Common, rising to the module kind's whole pool at Mythic |

Each `ModuleKind` declares the **stat pool** it may affect, authored in `modules.json`. On creation
the instance rolls `k` stats within its cap and distributes the budget across them.

> **More stats, less potent each. Fewer stats, more potent each.**

The properties this produces are all wanted:

- **A Mythic that rolls `k = 1` is the most potent single-stat item in the game.** Specialists stay
  relevant at every tier, and the god-roll is a genuine chase rather than "the biggest number."
- **A Mythic that rolls its whole pool is a versatile generalist.** Both are Mythic; which you got is
  the roll.
- **Higher tier buys a bigger budget *and* more places to put it** — never a strictly-better item, so
  §2.2's "no strictly-best loadout" survives the ladder intact.

*This supersedes an earlier line in this section warning that multi-stat officers are "the stat-stick
failure wearing a percentage sign."* That failure only occurs when each stat receives the **full**
value, making higher tiers strictly dominant with no decision attached. A shared budget removes it:
the specialist/generalist choice exists identically at every tier.

⚠️ **The one cost, worth entering deliberately:** §6.3's *"read the enemy by its loadout"* weakens
slightly, since two instances of the same module id can now differ. Grade colour (§2.7) and the
`ModuleKind`'s pool still read at a glance, so a weapon is always recognisably a weapon — what is no
longer exact is *how much* of one.

> **Note on "smaller mass per slot":** holding a shell's `mass` flat while `moduleSlots` rises
> **already** delivers lower mass-per-slot. Reducing `mass` as well is a second, independent buff —
> which is exactly why §2.2 checks the combined multiplier rather than each curve alone.

⚠️ **Crew slots were the weakest of the four, and are less so as of 2026-08-08.** Crew in the same
shell die together, so extra berths only pay off if the crew in them do *different jobs* — which
previously capped the useful count at two. With sensor and repair officers now having live consumers
(above), **four roles exist and a four-berth bridge has something to hold.** Berths beyond four still
wait on a new mechanic, so scale `crewSlots` a role at a time rather than ahead of the roster.

> ❌ **The turret `crewSlots` 0 → 1 tier gate is withdrawn (2026-08-08).** An earlier draft made a
> Common turret permanently slaved and gave it a crew slot only at Unique. That is **illogical and
> historically backwards**: nothing about a cheap turret prevents a person sitting in it, and in
> reality manned turrets are the *old* technology while remote and automated stations are the new one.

**Every turret has one crew slot. Independence comes from crew *or* from automation:**

| Turret has | Behaviour |
|---|---|
| Neither | **Slaved** to the hull's aim point |
| A fire-control module **or** a `Crew` module with a non-zero `operation` roll | **Independent** tracking |
| Both | Independent, and better — they stack |

Automation and crew become two ways to buy one capability, which is the right shape: a cheap turret
with a good gunner is worth about what an expensive automated one is, and a high-grade fire-control
module **frees your officer to be somewhere else**. That is a real fit decision rather than a power
gate.

**Grade still has a turret payoff** — automation *quality* rather than a seat — and crew now matter at
every tier instead of only above Unique, which is a stronger position for crew modules than the one
this section originally argued for.

#### Weighted budgets — every roll is worth the same 📋

*Settled 2026-08-08. Without this, the distribution rule above produces rolls that vary in **power**;
with it, they vary in **shape**, which is the difference between a loot system players read and one
they vendor from.*

**The problem, made concrete.** An Epic module rolls a bonus budget of +100% to distribute. Spent
evenly and unweighted:

> `{damage}` → **+100% damage** · `{spread}` → **perfect accuracy** · `{projectileSpeed}` → **+100% speed**

Three Epic weapons, identical to build, wildly different in power. Players would learn to discard
anything that did not roll damage, and Epic as a tier would become unbalanceable.

**The fix: each attribute carries a weight — the price of one unit of relative improvement.**

```
improvement(stat) = (share of budget) / weight(stat)
```

> ⚠️ **Weight is price, not importance.** A *high* weight means the stat is expensive, so a roll buys
> **less** of it. A *low* weight means cheap, so a roll buys **more**. A light stat is not a weak
> stat — it is one you get a lot of.

Same Epic budget, weights `damage 2.0`, `spread 1.5`, `range 1.0`, `projectileSpeed 1.8`:

| Roll | Picks | Result | Reads as |
|---|---|---|---|
| **A** | damage | +50% damage | A brawler |
| **B** | damage + spread | +25% damage, spread cut by a third | A precision cannon |
| **C** | range + speed | +50% range, +28% speed | A sniper |

**All three are worth exactly the same.** Nobody vendors any of them; you take the one that fits your
build. That is the good kind of variance.

**Two consequences worth having:**

- **Weights are the balance dial, and there is one place to turn it.** If damage proves too strong
  across the board, raise its weight — every roll then buys less of it. One number, one file, no
  content re-authoring.
- **Weights are never player-facing.** The player sees the *result* ("Epic · +50% damage"), so the
  weights stay invisible balance data. That settles what the item UI has to show.

##### Two more things a pool entry needs 📋

**Direction.** Some stats improve by going *down* — `fireInterval`, `spread`, `rechargeDelay`. A ×3.0
band on those makes a Mythic module worse. Every entry declares multiply or divide.

**Defaults per kind, overridable per def.** Some stats resist a single global weight:

- **Range is a threshold, not a slope.** Against an equal fighter at 900 range, +10% means firing
  from outside their reach — decisive. Past that it is worth nothing, because you close anyway. And
  §3.5 has capitals fighting *beyond visual range*, where range is everything, while fighters brawl
  inside it, where it barely matters.
- **Spread's value depends on `projectilesPerShot`.** On a single-projectile weapon it is pure
  downside. On a multi-projectile one it *is* the identity.

So weights default per `ModuleKind` and may be overridden per `ModuleDef`. A long-range lance prices
range expensively because it is already good at range; a brawler prices it cheap.

##### The band scales quality; it never changes identity 📋

Excluded from every pool, authored and never rolled:

| Excluded | Why |
|---|---|
| `damageType`, `Shield::absorbs`, `FacilityKind` | **Identity.** Rolling them produces a different item |
| `projectilesPerShot` and other integer fields | A ×1.37 band on an integer of 1 is still 1 — and rolling 1 → 3 turns a cannon into a shotgun, which is an identity change |

##### Pool size is a legibility budget, not a balance dial 📋

Per-stat power is `budget ÷ k`, so pool *size* does not affect balance — a five-stat kind rolling two
stats and a two-stat kind rolling two get identical treatment. **But a Mythic rolling across twenty
attributes moves each one imperceptibly and produces an item nobody can read at a glance.**

> **Keep rollable pools to roughly 4–6 entries.** Everything else about a module belongs in its
> identity attributes.

*This retires the "minimum pool of three" rule proposed earlier the same day.* `PowerCell`
(generation) and `Armor` (hull) genuinely do one thing, and forcing a third stat onto them would mean
inventing mechanics with no reader — exactly what §2.4 forbids. **A one-stat kind applies its band
directly with no distribution, and that is a statement about what the module is, not a degenerate
case.** The budget is the same either way; armour is simply a specialist by nature.

##### Weapon — the first pool 📋

Derived from what actually changes a weapon's contribution:

```
effective DPS ≈ damage/shot × shots/sec × hit rate × (1 − overkill waste)
```

| Stat | Dir | Weight | Reasoning |
|---|:---:|---:|---|
| `fireInterval` | ↓ | **2.2** | Linear in DPS **and** cuts overkill waste — see below |
| `damage` | ↑ | **2.0** | Linear in DPS, but diminishing against small hardpoints |
| `projectileSpeed` | ↑ | **1.8** | Manual aim (§3.2) makes lead the dominant source of misses |
| `spread` | ↓ | **1.5** | Gates whether you can hit a *chosen* hardpoint, not merely whether you hit |
| `range` | ↑ | **1.0** | Threshold rather than slope; expect per-def overrides |
| `knockback` | ↑ | **0.8** | Control rather than damage; scales with mass ratios |

**Two of these are weighted by decisions this design already made, and that is why they are not
guesses:**

- **Projectile speed is far more valuable here than in most games.** §3.2 removes target lock, so lead
  is where shots are lost. §3.5's numbers make that concrete: at 1,200–1,800 units/sec over 800–1,000
  range, time of flight is **0.44–0.83 s**, and a fighter crossing at ~200 units/sec travels
  **90–170 units** in that window — several hull-lengths, and far more than a 5-unit wing gun's
  radius.
- **Fire rate edges above damage because of localized damage.** Overkill is real here in a way it is
  not against a single health bar: a shot dealing 500 to a 50-hull wing gun wastes 450. High
  damage-per-shot has diminishing returns against small hardpoints; high fire rate does not. **So
  high-damage weapons strip chassis and station cores, while high-fire-rate weapons shred
  peripherals** — a real tactical distinction that falls out of §3.2 rather than being authored.

**These constants are placeholders**, in the same category as §2.6's negotiation-roll weights and
§2.4's merge scale: they make the system buildable and testable now, and get tuned later. The
*structure* is what is settled.

*There is also an empirical route to setting them, eventually:* **weight ∝ measured marginal DPS
contribution**, from a headless combat harness in the spirit of `tools/economy_sim`. Cheap to build,
because systems take a bare `SystemContext` with no window and no content file — and it would turn
this table from a judgement call into a measurement.

##### The remaining pools 📋

**§2.11 now holds the full roster**, including aggregation and power category per kind. Summarised here:

| Kind | Rollable pool |
|---|---|
| **Weapon** | ✅ above |
| **ShieldGenerator** | ✅ §3.1 — capacity · coverageRadius · rechargeDelay · boostMultiplier · rechargePerSecond · bleedThrough · ionResistance |
| **FireControl** | traverse rate · lead accuracy · reacquisition — *new kind, §2.7* |
| **Engine** | thrust ↑ · turnTorque ↑ · maxSpeed ↑ |
| **Facility** | ratePerSecond ↑ · capacity ↑ — *grade separately selects §2.4's recovery/survival/merge rows; not a double-dip, different stats* |
| **PowerCell** | generation ↑ *(one-stat kind)* |
| **Armor** | hullBonus ↑ *(one-stat kind)* |

##### Self pools and boost pools 📋

The mechanism is shared; the target is not.

| | Contains | Example |
|---|---|---|
| **Self pool** | The module's own stats | A weapon rolls its own damage |
| **Boost pool** | Stats on *other* modules, multiplied | An `Operator` raises the rig's weapon fire rate (§2.7) |

Same budget, same weights, same directions. **They stack multiplicatively** — a weapon that rolled
high fire rate, in a turret crewed by an officer who rolled into fire rate, compounds. That is §2.7's
stated intent ("a module supplies a capability, an officer multiplies it") rather than an accident,
but it is the sharpest power spike available in a fit and should be watched in tuning.


#### The rarity ladder 📋

*Settled 2026-08-07.* **One ladder, shared by shells and modules alike** — seven tiers:

| # | Tier | `moduleSlots` bonus | Mass |
|:---:|---|:---:|:---:|
| 1 | Common | +0 | 100% |
| 2 | Uncommon | +0 | 95% |
| 3 | Unique | **+1** | 90% |
| 4 | Rare | +1 | 85% |
| 5 | Epic | **+2** | 80% |
| 6 | Legendary | **+3** | 75% |
| 7 | Mythic | **+4** | 70% |

**The slot curve is back-loaded on purpose.** The top three tiers each add a slot, where the bottom
four add two between them — reward accelerating exactly where the drop rate becomes brutal. Matching
the reward curve to the rarity curve is the right shape; a linear ladder would make Legendary and
Mythic feel like rounding errors on a Rare.

*"Remarkable" was cut on 2026-08-07 — it read as an unranked adjective wedged between two ranked
ones.* **"Unique" still sits below Rare**, which runs against genre convention where the word means
one-of-a-kind. That is a deliberate choice, and the mitigation is that **tier colour is the primary
signal** in every UI surface; the words support the colour rather than the other way round.

**Slots are a bonus, not an absolute.** A shell's authored `moduleSlots` is its Common value and the
tier adds to it, so a 1-slot wing mount and a 3-slot chassis both improve without a 7 × N authored
matrix. *Note the proportional asymmetry this creates:* +3 quadruples a 1-slot shell and merely
doubles a 3-slot one. That favours small shells at high tier, which is probably desirable — it makes
a mythic wing mount genuinely exciting — but it is a real effect, not an accident.

**Nothing of the sort exists in the codebase today** — not on `ShellDef`, not on `ModuleDef`. The only
"tier" present is the Engineering facility's unrelated 1–5 skill tier. New construction for both,
which is why it is specified once here rather than twice later.

#### The two curves multiply — check the combined number, not each one

Against a base-2 shell:

> capacity per unit mass = (6 slots / 70 mass) ÷ (2 slots / 100 mass) = **4.3× a Common shell**

**Slots reaching +4 does not break the scaling — it is gentler overall than the earlier +3 at 50%
mass**, which came out at 5×. Pulling mass back to 70% more than pays for the extra slot. The extra
slot is also the better half of the trade to spend on: it is a *visible* reward at exactly the tiers
that are hardest to reach, where a 5-point mass difference would go unnoticed.

**The remaining thing to watch is the smallest shells.** On a 1-slot wing mount, Mythic gives 5 —
a 5× rather than a 3×. That concentration is largely self-correcting under §3.2: five modules stacked
in one hardpoint is five modules lost to one well-aimed shot. Localized damage taxes concentration
without a balance rule having to.

**Obsolescence is acceptable here specifically.** A player does not choose between a Common and a
Mythic chassis for *role* reasons — structural shells are not a variety mechanic the way weapons are,
so "use the best you have" is the correct behaviour and not a failure of §2.2.

#### Drop rates: steep, and safe to make steep 📋

Each tier roughly **one third** as likely as the one below it, reconciled to sum to exactly 100%:

| Tier | Share | Roughly |
|---|---:|---|
| Common | 66.70% | 2 in 3 |
| Uncommon | 22.20% | 1 in 5 |
| Unique | 7.40% | 1 in 14 |
| Rare | 2.50% | 1 in 40 |
| Epic | 0.82% | 1 in 122 |
| Legendary | 0.28% | 1 in 357 |
| Mythic | 0.10% | **1 in 1,000** |

*These are seven authored numbers, not a derived ratio — the tail is nudged slightly off a strict
third so the column totals 100% exactly. That is the right trade: a table of seven values is easier
to reason about and to hand-tune than a formula plus a correction term.*

**Research is what makes rates this steep safe** (§2.4). One recovered Mythic can be
reverse-engineered into something manufacturable forever, so a drop rate gates **first acquisition**,
not ongoing supply. A player does not need Mythics to drop often; they need to find *one*. That is
also the strongest possible reason to explore.

#### No tier cap on research — the gate is economic 📋

*Settled 2026-08-07.* Research can unlock **any** tier. What stops a single Mythic drop from
trivialising the top of the ladder is that **building one costs substantially more materials and
credits than a lower tier** — enough that manufacturing Mythics *at scale* is uneconomical until the
player has established a real resource pipeline.

**This is a better gate than the facility-tier cap it replaces**, which was the obvious alternative:

- A hard cap is binary and says *"you cannot, come back later."* An economic gate says *"you can, and
  here is what it will cost you"* — the player chooses, and the answer changes as they grow.
- It makes the top of the ladder an **industrial achievement rather than a lucky drop**. Finding a
  Mythic is the beginning; fielding a squadron of them is the accomplishment, and it requires exactly
  the production base §1's Macro loop is about building.
- It needs no new mechanism at all. Manufacturing (§2.8) already consumes materials and credits;
  this is a number on a curve, not a rule.

**Manufacturing cost must therefore climb steeply with tier** — steeper than the stat benefit does,
or mass-producing Mythics becomes correct as soon as it is possible. ⚠️ **Corrected 2026-08-11: not
by scaling against the drop-rate curve.** This line originally said cost scaling roughly against
§2.7's drop-rate curve (~3×/tier) was "the natural starting point" — §2.10 tried exactly that,
reversed it the next day, and this line was never swept to match. Mirroring rarity's steepness on the
cost side defeats §2.10's own governing principle (*"exploration and combat give you the first of a
thing, industry gives you more of it"*) — confirmed concretely by `tools/economy_sim`, which puts a
literal drop-rate-matched curve at a **2.48-million-fold** Common→Mythic module cost against a ~5×
combat-value gain. See §2.10 for the reasoning and the settled ~1× value.

#### Tier colour 📋

*Settled 2026-08-07.* Colour is the **primary** rank signal, because the ladder's own names do not
sort on sight — "Unique" reads as top-tier to anyone arriving with genre expectations, and it sits
third.

The palette is constrained by what `shared/ui/HudTheme.h` has already claimed: `kStatusGood`
(green `60,210,130`), `kStatusCaution` (amber `235,175,60`), `kStatusCritical` (red `230,70,70`), and
`kPanelChrome` (steel blue `90,150,190`) — which is the ambient colour of the entire UI. The
conventional genre ladder collides with three of those four, and its blue would sink into the chrome
and read as inert panel furniture.

| # | Tier | Colour | RGB |
|:---:|---|---|---|
| 1 | Common | Steel | `140, 150, 160` |
| 2 | Uncommon | Jade | `80, 200, 140` |
| 3 | Unique | **Cyan** | `60, 205, 215` |
| 4 | Rare | Azure | `80, 150, 250` |
| 5 | Epic | Violet | `165, 110, 245` |
| 6 | Legendary | Gold | `255, 190, 70` |
| 7 | Mythic | Incandescent | `255, 245, 220` + shimmer |

**Cyan at tier 3 is the load-bearing choice.** It sits naturally between green and blue, so the eye
reads Unique as *between* Uncommon and Rare without being told — which is exactly the ordering
problem the name creates.

**Red is deliberately excluded.** `kStatusCritical` owns it and so does hostile targeting; a red top
tier in a combat HUD would read as "enemy" more strongly than "best." **Mythic as a near-white**
lets Legendary keep the gold players expect while still escalating past it, and it cannot be confused
with any status colour.

**Three rules that matter more than the hues:**

1. **Colour must never be the only rank signal.** Perceived luminance across this ramp is *not*
   monotonic — it dips at Azure and Violet — and forcing monotonicity would push Common toward black,
   unreadable on the `9,14,20` panel glass. A seven-step ramp cannot be both convention-matching and
   greyscale-legible. Pair every rarity with a **non-colour cue** — pips, a numeral, or the tier name
   always visible. That fixes colour-blind legibility and the Unique-ordering problem at once.
2. **Separate rarity from status by *shape*, not hue.** Jade sits near `kStatusGood` and Gold near
   `kStatusCaution`; that is unavoidable while matching convention. Render rarity as **name text plus
   a thin frame or glow, never a filled swatch**, while status stays on bars, needles, and readouts.
   Same colours, different register, no confusion.
3. **Give Legendary and Mythic motion.** A slow shimmer on those two and nothing else — no status
   element ever animates. Motion is the most legible "this is special" channel available, and it
   makes the top tiers unmistakable at a glance in a crowded cargo list.

*This is the first concrete entry in what §9 still lists as the largest missing document — the UI/UX
specification. It lives here rather than in a UI section because it is inseparable from the ladder it
encodes.*

*This also revises the "two crew in the same shell buys nothing" line above:* it buys nothing
**against that shell being destroyed**, which is still the point. A shell with two slots holds a
pilot and a spare who die together; a chassis slot and a cockpit slot are what actually survive a
hit.

**A destroyed turret takes its gunner with it**, so there is no awkward state where a surviving
turret loses its operator and silently reverts to the player. **Crew and shell always die together** —
that falls out of there being no sub-hardpoint damage (§3.2), and it is now unconditional since life
support was cut. A live shell always has whatever crew it was built with, or was destroyed.

The only way a shell loses crew without dying is the player unmounting it — a deliberate act that
needs no runtime handling at all.

> ⚠️ **Corrected 2026-08-09: this previously read "unmounting it *at a station* … at a workbench,"
> which described a gate that does not exist and should not.** `ModuleEquipSystem` enforces no
> location requirement, and that is **deliberate and settled**: any swap is legal at any time,
> including mid-combat. The cost is not a rule but §3.4 — the simulation never pauses, so time in
> the fit screen is time spent drifting and targetable, and unmounting your own engine strands you
> on the spot. What makes the act "deliberate" is that it is unpaused, not that it is stationary.
>
> **This has a hard prerequisite the design was not previously paying for:** `ModuleEquipSystem`
> currently does **not** recompute rig-wide `BodyMass` or `Propulsion` on mount or unmount, so a
> live refit today changes what a hull carries without changing how it flies. Tolerable while the
> menu was unreachable; broken the moment live refit is a sanctioned combat action. See §2.11's
> aggregation rule and `architecture.md` §12.23's `RecomputeRigTotals`.

❓ *Open: does the player have a skill rating?* §3.2's manual aim says the player's piloting skill
**is their actual marksmanship** — a crew module that biased the player's own shots would contradict
it. So a player-piloted vessel likely ignores the pilot rating on its crew module, and the player's
crew modules matter only for work they *delegate*: commanders running fleets elsewhere, and NPC
pilots flying vessels the player owns but is not sitting in.

> ✅ **That last clause got its mechanism on 2026-08-10** (`architecture.md` §12.30.2). A hull you park
> in a docking bay is flyable again by exactly two routes and no third: **go back and board it**, or
> **crew it** — assign a `Crew` module and it becomes an asset an operator or commander flies for you.
> There is no remote recall and no fleet teleport, so parking is a real decision with a real cost,
> which is what §3.4 asks of every other choice. It is the first mechanic that makes a `command` or
> `operation` roll matter on a hull the player is *not* in.
>
> ⚠️ Blocked twice over: `ModuleKind::Crew` does not exist (`architecture.md` §13.3 Z — zero
> occurrences of "crew" in `src/`), and **warping currently destroys the parked hull outright**, since
> `WarpToSystem` tears the whole world down and re-spawns the player from their blueprint. The second
> is `architecture.md` §12.31's `RigState` — which also means **every jump today is a free full repair
> and rolls back every live refit this section sanctions.**

### 2.8 Manufacturing 📋

*Raised 2026-08-07. §2.4 states that research makes an item "manufacturable," and until now nothing in
this document or the codebase could Material anything except a whole vessel. Research's payoff was
unreachable.*

**Manufacturing turns knowledge plus materials plus time into matter.** It is the consumer that
makes research worth doing, and it is the only path by which a design in a knowledge network (§2.5)
becomes a physical object.

| Output | Is it entity assembly? | Home |
|---|---|---|
| **Vessel** (ship, station) | Yes — a composite rig is spawned | `ConstructionSystem` ✅ already built |
| **Shell** | No — an item lands in a cargo hold | Manufacturing (new) |
| **Module** | No — an item lands in a cargo hold | Manufacturing (new) |
| **Material** (intermediate, §2) | No — an item lands in a cargo hold | Manufacturing (new) |

**`ConstructionSystem` is the right home for vessels and the wrong home for the other three.** Building
a vessel *is* assembly — it spawns a composite entity through a factory, which is why that system
carries the codebase's one narrow, named exemption to the layering rule. Manufacturing a module
produces **inventory**, not an entity; routing it through the same file would widen that exemption
to cover work that never needed it. Keep the exemption as narrow as it is.

**Gated by facility and by knowledge, both.** Manufacturing requires a living Manufacturing facility
hardpoint (§4's "a Manufacturing hardpoint enables ship construction," generalized) *and* requires
that the actor's knowledge network actually holds the design. A faction that bought your Template
(§2.6) can manufacture it forever precisely because the design sits in their network — that is the
same gate, applied to them.

**AI factions use this path, not a private one** (§6.3). A faction manufacturing your design must
pay the same materials and take the same time, or the macro loop is simulating something the player
cannot reason about.

#### Manufacturing is a queued job 📋

*Settled 2026-08-08.* Structurally it is `ResearchSystem`'s sibling — a facility-hosted job queue
advanced against `dt` — but it is a **separate queue**, not a shared one. The two mechanics share a
shape, not a resource.

| | |
|---|---|
| **Concurrent slots** | `FacilityStats::capacity`, authored per facility. This retroactively gives `ResearchSystem` a slot limit it currently lacks — today a station can run unbounded concurrent research |
| **Speed** | `FacilityStats::ratePerSecond` |
| **Gates** | A living Manufacturing facility hardpoint **and** the design present in the actor's knowledge network (§2.5) |
| **Survives the player leaving** | A `core/galaxy/ManufacturingRecord`, exactly parallel to the existing `ResearchRecord` — demotion writes it, promotion re-instantiates with elapsed time banked (§1.1) |

**Manufacturing re-rolls quality.** Each unit produced rolls a fresh point in its grade's band
(§2.7). This is what makes wide top-end bands safe: a single Mythic drop that rolled poorly is not a
permanent verdict, and approaching a band's ceiling becomes an *industrial* pursuit rather than a
lottery result. It is also the production base's reason to keep running past raw volume.

#### The cost gate is a supply chain, not a multiplier 📋

*Settled in shape 2026-08-08; the numbers wait on the materials and recipe pass (§9).*

§2.4 requires manufacturing cost to climb **steeper than the stat benefit does**, or mass-producing
Mythics becomes correct the moment it is possible. Two levers together, and the second matters more:

| Lever | Shape |
|---|---|
| **Quantity** | Material cost scales steeply with grade — roughly against the inverse of the drop-rate curve (§2.7), which is ~3× per tier |
| **Input grade** | A grade-*N* module requires **Materials of grade ≥ N−1**, which must themselves be manufactured |

**The input chain is the interesting half.** It means you cannot build a Mythic weapon until you can
mass-produce Legendary Materials, which needs Epic Materials beneath them. Mythic production becomes a
*pipeline* problem rather than a large number — which is exactly the industry §1's Macro loop is
about building, and a far better reason to hold territory than a cost multiplier is.

It also does not reintroduce the facility-tier cap §2.4 rejected. Nothing says *you may not*; it says
*here is the industry you will need*. That distinction is the whole point of §2.4's economic gate.

**And it is what stops quality re-rolling from becoming a slot machine.** An unconstrained player
would spam Mythic production fishing for a 5.0 roll; each attempt costing a full Legendary-Material
pipeline run is the brake.

❓ *Open, and deliberately deferred: the actual numbers.* Material quantities, Material recipes, and the
time curve per grade all depend on the Elements and Materials content pass that has not happened yet —
`data/base_game/` has no `elements.json` or `materials.json` at all (§2). Specify the recipe base
first, then the scaling; doing it the other way round produces numbers with nothing to multiply.


#### The time curve 📋

*Settled 2026-08-08.*

**The framing that decides the whole curve: jobs progress while you play.** A job banks elapsed time
across demotion (§1.1), so a ten-minute build is not ten minutes of waiting — it is ten minutes of
flying somewhere else. The curve is therefore tuned against session pacing, not against patience.

**Time scales far more gently than materials, and deliberately so.** Material cost climbs ~3× per
grade (§2.10) and that is already §2.4's economic gate. Making time climb at the same rate taxes the
player twice for one thing — **the exact reasoning that cut upkeep** (§2.7: repair, refuel, and
assembly already provide the sink). Steep materials make this a logistics game; steep time makes it a
waiting game, and §1's Macro loop wants the former.

> **Base build time doubles per grade. Facility grade divides it by up to ~3.3× (§2.4).**

| Grade | Material *(base 5s)* | Module / Shell *(base 10s)* |
|---|---:|---:|
| Common | 5s | 10s |
| Uncommon | 10s | 20s |
| Unique | 20s | 40s |
| Rare | 40s | 1m 20s |
| Epic | 1m 20s | 2m 40s |
| Legendary | 2m 40s | 5m 20s |
| **Mythic** | **5m 20s** | **10m 40s** |

Sixty-fold across the whole ladder, one rule to remember, and a Mythic bench turns out a Mythic
module in about three minutes.

**Vessels derive rather than scale.** A vessel is not a grade, it is an assembly:

> **Vessel build time = Σ(its parts' build times) × an assembly factor.**

So **mass, base price, and build time all derive from the recipe by the same rule** (§2.10) — the
third use of one anchor. A dreadnought takes an hour because it is made of a great many expensive,
heavy, high-grade parts. Nothing is authored and nothing can drift.

⚠️ **This means `ConstructionSystem` grows a build timer**, which it does not have today. That is a
feature rather than an oversight to patch quietly: a capital under construction is a window for a
raid, it gives §6.1's facets something to react to, and it is what makes a shipyard worth defending.

**Research is the one that should feel like an investment**, since its payoff is permanent — the same
2×-per-grade shape from a six-fold higher base:

| Research target | Base | At a Mythic facility |
|---|---:|---:|
| Common | 1m | ~18s |
| **Mythic** | **~64m** | **~19m** |

Nineteen minutes of *play* to permanently unlock manufacture of the best item in the game is
proportionate; sixty-four minutes at a crude bench is the reason to build a better one.

**Throughput is the second axis and it already exists.** `FacilityStats::capacity` sets concurrent
job slots, so a better facility runs more jobs *and* runs each one faster — two multipliers on one
upgrade, which is what makes investment in industry compound.

#### Two fuel sources, not one — corrected 2026-08-12 📋

*Originally settled 2026-08-11 as a single shared pool; corrected the next day. The original version
routed Manufacturing, Research, Engineering, and Deconstruction through the same fuel `WarpSystem`'s
jumps consume. That conflated two things that shouldn't share a resource: a jump needs **reaction
mass**, physically expended for thrust, no substitute possible. A stationary facility running a job
doesn't need reaction mass at all — it needs **electricity to run its equipment**, which is a
completely different resource with a completely different model already built.*

| Fuel type | Powers | Resource | Model |
|---|---|---|---|
| **Propellant** (material) | Hyperdrive jumps only | Energetic elements → manufactured Propellant, physically consumed per jump | Unchanged, already specified above |
| **Power** (energy) | Manufacturing, Research, Engineering, Deconstruction — running the facility's equipment | Whatever the station generates | Draws against the **Facilities** power category §2.9 already defines (one of the four keys, `F`/`G`/`H`/`J`) — not a new mechanism, the existing one gains a new consumer |

This also cleanly separates from *materials/elements consumed as recipe inputs* — what a manufactured
item is physically made **from**, already fully specified above — from *power drawn to run the
process* — what keeps the equipment operating **while** it runs. Three different consumption
patterns (becomes-the-output, runs-the-equipment, spent-as-reaction-mass), not one undifferentiated
bucket.

**Engineering and Deconstruction still gain a time-to-complete component they've never had**, and it
is still mass-derived rather than grade-derived — unchanged from the original proposal. Manufacturing
and Research keep their existing grade-based time curve untouched.

**A player or faction may substitute Propellant/material requirements with their credit equivalent,
at a markup, plus additional time** — reusing `Pricing.h::BaseValue` rather than a second pricing
model. This applies to the **materials/Propellant side only**; it does not extend to power, which
isn't purchased externally, it's generated locally, so there is nothing to substitute it with.

#### A facility process stalls, not fails, if its power draw cannot be sustained 📋

*Settled 2026-08-12.* If the Facilities category cannot sustain a running job's draw — Offline, or
Reduced below what the job needs — the job **stalls**: it stops progressing and resumes the instant
power is available again, the same "stalls, resumes when the constraint clears, nothing lost" shape
already used elsewhere in this exact design (Manufacturing already stalls when its destination hold
is full; Research already freezes rather than erases when `ctx.knowledge` is null). This is not a
second failure roll and does not reopen *"research cannot fail"* above (§2.4) — it is fully
deterministic and player-visible (build more generation, or wait), not a probability.

#### Power generation gains storage, and a second and third source type 📋

*Settled 2026-08-12.* Power generation today is a pure instantaneous ratio — generation vs. draw,
recomputed fresh every tick, no buffer. Shields already work differently: `capacity` (a pool) +
`rechargePerSecond` (a refill rate). Giving `PowerSource` the same two-stat shape is not new
machinery, it is the same stat-pool pattern applied a second place, and it gives **"boost refuses
without headroom"** (§2.9, below) a literal meaning instead of an abstract one — headroom becomes the
stored buffer, and a boost can draw it down instead of only shedding other categories in real time.
A generator built for a deep reserve and one built for sustained rate are now genuinely different
fits, not the same stat under two names.

**Three ways to generate power, each a real tradeoff rather than a strictly-better option:**

| Source | Cost | Output | Constraint |
|---|---|---|---|
| **Standard `PowerCell`** (existing) | None ongoing | Fixed by grade/quality | None — works anywhere |
| **Fuel-consuming generator** (new) | Consumes Energetic elements/materials over time | Higher ceiling than a standard cell | Needs resupply — a second real sink for Energetic materials, beyond Propellant. Not a new resource relationship: the Energetic attribute already explicitly feeds `PowerSource::generation` *and* hyperdrive fuel (§2.10) — this makes that dual role literal instead of only a composition bonus |
| **Solar panel** (new) | None ongoing | Scales with proximity to a star | Position-dependent — negligible in the outer system, strong parked near a sun. Gives the star-luminosity/system-radius work an actual gameplay hook instead of a purely visual one |

#### A workbench job also fails outright if the facility hosting it is destroyed or removed 📋

*Settled 2026-08-12, correcting "research cannot fail" above (§2.4) to the scope it actually has.* That rule
rejects a **second random roll** stacked on the existing sample-survival roll — it says nothing about
external causes, which already have their own consequences everywhere else in this design. A
research, manufacturing, engineering, or deconstruction job runs *on* a specific facility hardpoint;
if that hardpoint dies in combat or is unmounted mid-job, the job — and whatever sample or
in-progress materials it held — is destroyed with it. This is the same cascade-of-destruction rule
already governing everything else attached to a hardpoint (§2.2: shooting a cargo bay spills its
contents; §3.7: destroying a shell destroys everything attached to it) applied to one more thing a
hardpoint can carry. Not a new mechanic — the existing one gains a new kind of contents.

This is also precisely what `architecture.md` §13.5 group 2's research-defects bullet already tracks
from the wiring side (*"`ResearchSystem::Tick` gains a `FacilityKind::Research` gate... blowing the
lab off a station does not stop the jobs in it"*) — that fix and this rule are the same requirement
seen from two directions: the gate stops a dead facility from *starting* new work, this stops a dead
facility from *continuing* work already in progress.

**One related question this does not resolve: what happens to a research job if the hosting station
changes owner mid-job (capture)?** Left open — it depends on §3.2's capture state, which is itself
still blocked on the crew shell (§13.3 Z) and not built yet. Worth revisiting once capture exists,
not decided here.

### 2.9 Power Allocation 📋

*Settled 2026-08-07. §6.3 has always promised that ships "dynamically reallocate power mid-combat —
dumping shield regeneration into a weapon burst, or cutting engines to hold a shield up." This is
that mechanic specified. It is also the **counterplay to hardpoint fragility**: the more a single
lost shell hurts (§3.2), the more the player needs a lever to compensate mid-fight.*

**Every power category runs at one of four levels**, not on/off:

| Level | Draw | Effect |
|---|---|---|
| **Offline** | none | Nothing. Run silent, or free the budget entirely |
| **Reduced** | below nominal | Degraded output |
| **Normal** | nominal | Baseline |
| **Boosted** | well above nominal | Above baseline — an overdrive |

**The draw and effect multipliers at each level are module attributes** (Law 10, authored in
`modules.json`), not global constants. A cheap thruster's boost is a nudge; a military one's is an
afterburner. Two ships with the same four levels can have completely different boost characters.

**Set per category, not per hardpoint.** The player controls weapons, shields, engines, and
facilities — four switches, not one per turret. This matches the shed order `PowerSystem` already
uses and keeps the control surface usable mid-fight. *(Life support was cut, §2.7 — crew draw no
power and are not a category.)*

#### Only two levels are ever commanded directly 📋

*Settled 2026-08-08, and it collapses sixteen states into four keys.*

**The player boosts. The priority list decides who pays.** Pressing a category's key boosts it; the
power to fund that boost is taken from the other categories automatically, in the order set by a
**power priority list** the player configures out of combat. `Reduced` is therefore never commanded —
it is what happens to something else when you boost.

| Input | Effect |
|---|---|
| **Tap** a category key | Toggle **Boosted** |
| **Hold** a category key | Toggle **Offline** — the deliberate "run silent" or "cut engines" act |
| — | **Normal** and **Reduced** are results, never inputs |

Four keys, two gestures, all four levels reachable. The interesting configuration happens at leisure
in a menu; the in-combat action is a single keypress, which is what §4.4's timing constraint demands.

**The four keys are `F` / `G` / `H` / `J`** — weapons, shields, engines, facilities (§3.6). *Until
2026-08-09 §3.6 listed only three, omitting engines, which made this section's "cut engines" act
unreachable; that was a defect in the input map rather than a disagreement about the model.*

**The priority list already exists in code.** `PowerLoad::priority` is authored per hardpoint today
and `PowerSystem` already sheds ascending — *"facilities before shields before engines."* The menu
exposes and reorders what is already there rather than introducing a parallel concept. Its natural
home is the avionics surface, alongside the other ship-configuration readouts.

❓ *Open: whether multiple categories may be Boosted simultaneously.* Permitting it is more
expressive and means the priority list has to fund two demands at once; forbidding it keeps the
budget arithmetic trivial and the HUD unambiguous.

#### Boost costs nothing extra — and that is the whole design

There is deliberately **no heat, wear, or overdrive resource.** The cost of boosting is that the
budget must still balance: *power spent boosting shields is power not available to weapons or
engines*. Reduce something, or you cannot boost anything.

> 🔁 **Re-tested 2026-08-09 and upheld.** §2.10's materials pass introduced a **Thermal** attribute,
> and a heat-buildup system was the obvious way to give it depth. Rejected on this paragraph's own
> reasoning — *"adding a second one would only obscure it"* — and because an attribute being thin is
> not grounds to reverse a settled decision. **Thermal drives `Weapon::fireIntervalSeconds`
> instead**: a tungsten-barrelled autocannon sustains a higher rate of fire, which is legible
> without a gauge and needs no new system.

That constraint is sufficient on its own, and adding a second one would only obscure it. It also
makes the decision continuous and legible — every boost is visibly a trade against the other four
switches, which is exactly the §2.2 constraints puzzle carried from the workbench into live combat.

**Boost simply does not engage without headroom.** It is not a request that browns out the rest of
the ship; the UI shows it unavailable and nothing happens.

#### The afterburner is not a special case

Holding **`Ctrl`** for an engine boost is engines set to *Boosted* — the same mechanism, the same
budget, the same trade. It draws far more than regular flight, so sustained running compromises
shields or guns. This is the clearest possible demonstration of the system, which is why it should be
the first thing built on it and the first thing the player meets.

*The afterburner moved from `Shift` to `Ctrl` on 2026-08-09 (§3.6), so `Shift` could take its
conventional "add to selection / append to queue" meaning for the command system (§4.3). Nothing
about the mechanism depends on the key.* **`Ctrl` held is the momentary boost; tapping the engines
key (`H`) is the sustained toggle** — two affordances onto one state, deliberately.

**AI ships use this identically** (§6.3). No AI-only reallocation, no hidden multiplier — an enemy
that suddenly outruns the player has paid for it somewhere the player can read off its behaviour.

❓ *Open: whether Offline is selectable for every category or only some.* Cutting engines dead
mid-fight is a legitimate desperate move; cutting them dead by mis-click while being chased is not.
The likely answer is that Offline is available everywhere but requires a deliberate input rather than
a single tap on a four-position cycle.

### 2.10 Elements, Materials & Recipes 📋

*Settled 2026-08-08; **substantially rewritten 2026-08-09** — the supply tiers were renamed (§2), the
element rarity bands were deleted, elements gained an eight-attribute vector, and recipes now demand
attribute roles rather than named elements.*

*`data/base_game/` holds `modules.json`, `shells.json`, and `ships.json` and nothing else — there is
no Element or Material content anywhere, which is why research's payoff has been unreachable and why
every cost curve in §2.4 and §2.8 has had nothing to be denominated in.*

**The chain is two hops, and no more:**

> **Elements → Materials → Modules / Shells / Vessels**

Depth comes from **grade**, not from stacking intermediate layers: a Legendary module wants Epic
materials, which want Rare materials, and so on (§2.8's input-grade chain). One intermediate type
gives arbitrarily deep pipelines; a second would multiply content for the same effect — which is the
reasoning that also rejected a `Compound` tier on 2026-08-09 (§2).

#### The governing principle 📋

> **Exploration and combat give you the *first* of a thing. Industry gives you *more* of it.**

This is §2.4's "a drop rate gates first acquisition, not ongoing supply," generalised to the whole
content set, and it keeps the two gates from ever overlapping:

| Gated by | What it gates |
|---|---|
| **Reach** — where you can get to and hold | **Elements.** All of them are gatherable somewhere |
| **Achievement** — what you can find, beat, or complete | **High-grade finished items** — materials, modules, shells, chassis, whole vessels |

**Research is the bridge between them.** It converts a thing you *found* into a thing you can
*make*, which is what stops the achievement gate from becoming a grind and the reach gate from
becoming the only progression.

#### Elements — the periodic-table floor, and genuinely no tiers 📋

*Rewritten 2026-08-09. **This section previously contradicted itself in consecutive paragraphs:** it
opened with "elements are never tiered" and then tiered all fourteen into Common / Uncommon / Rare /
Anomalous, with the grade table gating recipes on "rarest permitted." The principle was right and the
implementation was its opposite. The bands are deleted; the principle stands.*

> **No element is rarer than another. Scarcity is entirely a property of where you are and what you
> hold — and it is therefore *perceived*, differing between two factions looking at the same galaxy.**

Real elements are used deliberately: they supply free intuition (iron is structural, copper
conducts), free icons (the periodic abbreviation), and — the useful part — **free mass numbers**,
since relative density settles what would otherwise be an argument.

##### Eight attributes, and every one has a consumer

A element is a **vector of contributions**, never a quality score. That distinction is load-bearing:
a single "how good is it" number reintroduces the rarity ladder through the back door, because the
best element per role would strictly dominate every other.

| Attribute | Real basis | What reads it |
|---|---|---|
| **Structure** | Tensile strength | `Health.max` on the hardpoint |
| **Conductive** | Electrical conductivity | `PowerSource`/`PowerLoad` efficiency, engine output |
| **Semiconductive** | Band gap | `Weapon::spreadRadians` (accuracy), sensor strength |
| **Energetic** | Energy density, fissile yield | `PowerSource::generation`, hyperdrive `fuelPerJump` (§2.11) |
| **Magnetic** | Permeability | `Shield::capacity` / `rechargePerSecond` |
| **Optical** | Reflectivity, transmission | Energy-weapon damage, sensor strength |
| **Thermal** | Melting point | `Weapon::fireIntervalSeconds` — sustained rate of fire |
| **Inert** | Corrosion resistance | Corona/hazard resistance (`architecture.md` §12.28), repair cost |
| *(**Density**)* | g/cm³, real | Mass — the universal cost paid against all eight |

⚠️ **An attribute with no reader is the same defect as a system with no producer** — the failure
class `architecture.md` §13 catalogues a dozen times. The right-hand column is a requirement, not a
convenience: **do not add a ninth attribute without naming what consumes it.** Two were nearly added
and rejected on exactly this test — a heat/wear model (§2.9 cut it deliberately) and a durability
bar (§2.7 cut upkeep deliberately).

##### The roster

**Real elements only.** The target was **~50**; eleven of the twenty candidates below survived
`element_check` reasoning on 2026-08-12 (nine cut for dominance or Pareto failure — see the
candidates section), landing the roster at **41**. §2.10 itself authorizes stopping short: *"keep
whatever survives `element_check`; cut the rest without argument, even if the roster lands under
50."* Ratings 0–3; **ρ** is real g/cm³. New rows from the 2026-08-12 pass are marked 🆕.

| | Element | ρ | Str | Cnd | Sem | Eng | Mag | Opt | Thm | Inr | Character |
|---|---|---:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|---|
| **Li** | Lithium | 0.53 | 0 | 1 | 0 | 3 | 0 | 0 | 0 | 0 | Featherweight power — corrodes on sight |
| **Mg** | Magnesium | 1.74 | 1 | 2 | 0 | 1 | 0 | 0 | 0 | 0 | The cheapest mass saving there is |
| **Be** | Beryllium | 1.85 | 1 | 1 | 0 | 0 | 0 | 3 | 2 | 2 | Optics that weigh nothing. Brittle |
| **C** | Carbon | 2.27 | 2 | 1 | 1 | 0 | 0 | 0 | 3 | 3 | Light, inert, near-unmeltable. No specialism |
| **Si** | Silicon | 2.33 | 0 | 0 | 3 | 0 | 0 | 1 | 1 | 2 | The logic substrate, and it is light |
| **B** | Boron | 2.34 | 2 | 0 | 2 | 1 | 0 | 0 | 3 | 2 | Stiff fibre, neutron sponge |
| **Al** | Aluminium | 2.70 | 2 | 2 | 0 | 0 | 0 | 2 | 0 | 2 | Good at four things, best at none |
| **Sc** 🆕 | Scandium | 2.99 | 2 | 1 | 0 | 0 | 0 | 0 | 1 | 2 | Titanium's lighter, weaker cousin — aerospace-grade lift |
| **Y** | Yttrium | 4.47 | 1 | 0 | 1 | 0 | 1 | 3 | 2 | 1 | Laser and sensor glass |
| **Ti** | Titanium | 4.51 | 3 | 0 | 0 | 0 | 0 | 0 | 2 | 3 | Strength per kilo that never corrodes |
| **Se** 🆕 | Selenium | 4.81 | 0 | 0 | 3 | 0 | 0 | 2 | 0 | 1 | The original photocell element — light in, current out |
| **Ge** | Germanium | 5.32 | 0 | 0 | 3 | 0 | 0 | 3 | 1 | 2 | Sensors and energy optics both |
| **V** 🆕 | Vanadium | 6.11 | 2 | 1 | 0 | 1 | 0 | 0 | 2 | 2 | A workhorse alloy metal with a flow-battery side job |
| **Te** 🆕 | Tellurium | 6.24 | 0 | 0 | 2 | 1 | 0 | 1 | 1 | 1 | Thermoelectric — turns waste heat into a trickle of power |
| **Zr** | Zirconium | 6.52 | 2 | 0 | 0 | 1 | 0 | 0 | 3 | 3 | Reactor cladding |
| **Nd** | Neodymium | 7.01 | 0 | 0 | 0 | 0 | 3 | 1 | 1 | 0 | The magnet. Oxidises fast |
| **Zn** 🆕 | Zinc | 7.14 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 1 | Cheap, light, mildly everything |
| **Cr** | Chromium | 7.19 | 3 | 1 | 0 | 0 | 0 | 2 | 2 | 3 | Hard, bright, corrosion-proof |
| **Mn** 🆕 | Manganese | 7.21 | 2 | 0 | 0 | 1 | 1 | 0 | 1 | 1 | Toughens steel, powers a battery on the side |
| **Sn** | Tin | 7.31 | 1 | 2 | 0 | 0 | 0 | 0 | 0 | 2 | Joins things cheaply |
| **In** 🆕 | Indium | 7.31 | 0 | 2 | 1 | 0 | 0 | 2 | 0 | 1 | Transparent conductor — electronics you can see through |
| **Sm** | Samarium | 7.52 | 0 | 0 | 0 | 1 | 3 | 0 | 2 | 1 | Magnets that survive heat |
| **Fe** | Iron | 7.87 | 3 | 1 | 0 | 0 | 3 | 0 | 2 | 0 | Strong and magnetic — and it rusts |
| **Nb** | Niobium | 8.57 | 2 | 1 | 0 | 0 | 1 | 0 | 3 | 3 | Refractory and inert |
| **Cd** 🆕 | Cadmium | 8.65 | 0 | 1 | 2 | 1 | 0 | 2 | 0 | 2 | Thin-film photovoltaics — Optical Array's second real option |
| **Co** | Cobalt | 8.90 | 2 | 1 | 0 | 0 | 3 | 0 | 3 | 2 | Magnetism that holds under heat |
| **Ni** | Nickel | 8.91 | 2 | 1 | 0 | 0 | 2 | 0 | 2 | 3 | The dependable alloy |
| **Cu** | Copper | 8.96 | 1 | 3 | 0 | 0 | 0 | 1 | 1 | 1 | The default conductor |
| **Mo** | Molybdenum | 10.28 | 3 | 2 | 0 | 0 | 0 | 0 | 3 | 2 | Structure that shrugs off heat |
| **Ag** | Silver | 10.49 | 0 | 3 | 0 | 0 | 0 | 3 | 1 | 1 | Best conductor and best mirror — tarnishes |
| **Th** | Thorium | 11.72 | 1 | 0 | 0 | 2 | 0 | 0 | 3 | 1 | Fuel that tolerates heat |
| **Pd** 🆕 | Palladium | 12.02 | 1 | 2 | 0 | 0 | 0 | 1 | 1 | 3 | The compact noble conductor — catalytic and clean |
| **Rh** 🆕 | Rhodium | 12.41 | 2 | 2 | 0 | 0 | 0 | 3 | 2 | 3 | The mirror metal — reflects almost everything, corrodes in nothing |
| **Hf** | Hafnium | 13.31 | 2 | 0 | 0 | 2 | 0 | 0 | 3 | 3 | Reactor control |
| **Ta** | Tantalum | 16.65 | 2 | 2 | 0 | 0 | 0 | 0 | 3 | 3 | Capacitors, extreme service. Soft |
| **U** | Uranium | 19.05 | 2 | 0 | 0 | 3 | 0 | 0 | 2 | 0 | Raw fission. Dangerous to store |
| **W** | Tungsten | 19.25 | 3 | 1 | 0 | 0 | 0 | 0 | 3 | 2 | Nothing melts it |
| **Au** | Gold | 19.30 | 0 | 3 | 0 | 0 | 0 | 2 | 1 | 3 | Heavy, but still working in a century |
| **Re** 🆕 | Rhenium | 21.02 | 3 | 1 | 0 | 0 | 0 | 0 | 3 | 3 | The lightest way to get iridium-grade refractory strength |
| **Pt** | Platinum | 21.45 | 1 | 2 | 0 | 2 | 0 | 1 | 2 | 3 | Catalyst and noble all-rounder |
| **Ir** | Iridium | 22.56 | 3 | 1 | 0 | 0 | 0 | 1 | 3 | 3 | The premium hull element |

⚠️ **`Cd` was cut in the first pass of this validation and reinstated the same day** — the original
rating (`Sem1/Opt1`) undersold real cadmium chalcogenide chemistry (CdTe thin-film photovoltaics run
near 22% efficiency; CdSe is a foundational quantum-dot optical material). Corrected to `Sem2/Opt2`,
it survives dominance cleanly and gives **Optical Array** (§2.10, below) a second real
Semiconductive+Optical option instead of Germanium alone — previously this roster's single worst
role-coverage bottleneck.

**`Zr`'s existing `Eng1/Thm3` rating already answers a question this section almost got wrong.**
`Power Core` (wants Energetic + Thermal, below) was flagged as a two-element bottleneck — Uranium
and Thorium — until re-examining Zirconium's own row: real zirconium alloy (zircaloy) is *the*
nuclear fuel-cladding material specifically for its combination of low neutron absorption and high
heat tolerance. It was never added for this pass; it was already sitting in the original thirty,
undercounted because the check only looked for elements rated ≥2 on **both** attributes at once.
The honest Power Core pool is **U, Th, and Zr** — three, not two.

##### Volatiles are Elements too 📋

*Settled 2026-08-09. An earlier proposal in this session made gases and liquids a **separate resource
class** with their own model. **Withdrawn** — the three-tier rename dissolved the problem: hydrogen is
an Element exactly as iron is, and `Propellant` is a Material exactly as Alloy Plate is. One model,
not two.*

| | Element | ρ *(stored)* | Str | Cnd | Sem | Eng | Mag | Opt | Thm | Inr | Character |
|---|---|---:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|---|
| **H** | Hydrogen | 0.071 | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | The best fuel by mass, and nothing else |
| **He** | Helium | 0.125 | 0 | 0 | 0 | 2 | 0 | 0 | 3 | 3 | Cryogenic coolant, utterly inert |
| **N** | Nitrogen | 0.807 | 0 | 0 | 0 | 1 | 0 | 0 | 1 | 3 | Inert working fluid |
| **O** | Oxygen | 1.14 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | Oxidiser. Corrodes everything |
| **Ar** | Argon | 1.40 | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 3 | Shielding gas, arc media |
| **Xe** | Xenon | 3.06 | 0 | 0 | 0 | 1 | 0 | 3 | 0 | 3 | Ion-drive propellant, arc optics |

⚠️ **Densities are for the *stored* liquid, not the gas** — and that is what keeps them honest.
Gas-phase hydrogen is 0.00009 g/cm³, which against density-as-universal-cost would let it dominate
lithium **and** uranium at Energetic 3. Liquid hydrogen at 0.071 sits in the same range as lithium's
0.53. You store volatiles liquefied because that is what you would actually do, and the mass you
carry is the mass that counts.

Hydrogen still comes out the best fuel by mass, which is simply **true** — and it is paid for by
being Energetic 3 and **zero at all seven other attributes**, where lithium at least conducts.

> 🔭 **Xenon is not the "anomalous" material any more.** The deleted availability table had `Xe` as a
> working-name exotic gated to anomalous systems. It is now just xenon: a real noble gas, the real
> ion-thruster propellant, and a natural fit beside Ion being one of §3.1's three damage types.

##### Where Elements come from — three gathering activities 📋

*New 2026-08-09. Volatiles being Elements means they need a source, and the sources give two things
that currently have no purpose a reason to exist.*

| Activity | Source | Yield per visit | Distance | Gives purpose to |
|---|---|---|---|---|
| **Mining** | Asteroids — many, small | Low | **Close** — the belt | Already built (`MiningSystem`) |
| **Skimming** 📋 | Gas giants — few, huge | **High** | Far — outer orbits | **Planets**, currently pure non-colliding scenery (`architecture.md` §12.28) |

**Yield per visit scales with the size of the body**, which is what balances fuel against its own burn
rate: propellant is cheap per unit but expensive in **logistics**. You consume it constantly and refill
it in bulk, and the refill is a trip to the outer system. That makes fuel a **supply-line** problem
rather than a scarcity one — the more interesting version, and it feeds the territory economy directly
rather than just taxing the player.

> ⚠️ **There is deliberately no third "harvesting" verb.** An earlier draft made nebulae a *gathering
> site* — a dangerous resource node yielding richer volatiles. **Withdrawn 2026-08-09**, because
> "dangerous resource" is a trade most players simply decline: if the reward for flying into damage is
> more of a volatile you can get safely elsewhere, the rational answer is no, and the content is dead
> on arrival. **A nebula is now a *place*, not a node** — see §3.8. Mining and skimming work normally
> inside one; the nebula is a modifier on *where* they happen, not a third way of doing them.

**This is the payoff for §12.28's "planets are background" decision.** That section gave planets no
collision and no role beyond landmarks, and flagged the deferral honestly as partly scope control.
Gas giants as the volatile supply give them an economic reason to exist **without** needing collision,
occlusion or sheltering — you fly to one and skim it, exactly as you fly to an asteroid and mine it.

**A nebula is a hazard and a resource in the same volume**, which is the tension worth having: the
richest volatile fields sit inside something that is damaging you while you harvest. `HazardSystem`
(§12.28) already exists to carry the damage half, so a nebula is **content for a built system** rather
than a new one.

⚠️ **This needs one new module kind and one authored planet property, and neither exists.** A
**gathering module** — §2.11's roster has no extraction module at all, and `architecture.md` §13
already records that *"`MiningSystem` reads no module stat"* — and a planet **type** flag, since
`WorldGen.cpp`'s own comment concedes *"every planet below is mechanically identical aside from its
orbit."*

**Tested against `element_check`'s four rules by hand, 2026-08-12 — eleven survived, nine were cut.**
This is the pass the roster above already reflects; recorded here so the reasoning is auditable
rather than just the outcome.

**Survived (11):** Sc, Se, V, Te, Zn, Mn, In, Cd, Pd, Rh, Re — all now in the roster above.

**Cut (9), and the specific failure each one hit:**

| Element | Failure |
|---|---|
| **Pb** | Dominated outright by **Ni** — same-or-better on every attribute at lower density. Its real niche (radiation shielding) isn't one of this roster's eight consumers |
| **Os** | Dominated outright by **Ir** — near-identical profile, Ir is lighter |
| **Bi** | Survives dominance narrowly, fails Pareto — no weighting of its thin Sem/Opt edge over Sn beats the density it costs |
| **Gd** | Dominated by **Sm** — same magnetic-lanthanide shape, Sm is lighter with more Thermal/Energetic |
| **Eu**, **Er** | Both dominated outright by **Y** — the *"lanthanides collapse"* effect this section already predicted by name |
| **Sb** | Survives narrowly, too thin a differentiator from **V** to earn a slot |
| **La**, **Ce** | Same lanthanide-collapse failure as Gd/Eu/Er, folding into Y's already-covered territory |

**Net result: 41 elements, not 50.** This section already authorizes that outcome explicitly —
*"keep whatever survives `element_check`; cut the rest without argument, even if the roster lands
under 50"* — and the cut list matches what this section predicted: most of the losses are exactly
the lanthanide collapse it named as the expected failure mode, not a surprise.

##### Why not the whole periodic table

*Asked and answered 2026-08-09.* Because **the table's organising principle is similarity within a
group**, so 118 elements do not contain 118 profiles:

- **~47 are not bulk elements at all** — ~28 synthetic or never-stable (everything past uranium;
  elements 104+ have existed as a handful of atoms), 11 gases at STP, 2 liquids, 6 radioactive traces.
- **The lanthanides collapse.** Fifteen elements, densities 5.24–9.84, chemically near-identical —
  which is exactly why separating them is hard in reality. Against these eight attributes they yield
  roughly three profiles, so **twelve fail Pareto by construction** and cannot be fixed without
  inventing differences, which forfeits the free-intuition argument entirely.
- **Same collapse, smaller:** Na/K/Rb/Cs are lithium but heavier with no compensating gain — *dominated
  by definition*. The platinum group is six noble dense catalysts yielding perhaps three profiles.

That leaves **~45–50 genuinely distinct profiles**, which is where the target comes from.

> **Elements are the periodic-table floor. Materials are where variety lives.** Reaching for more
> elements to get richness is solving a Material-layer problem at the Element layer — and the Element
> layer is the one place the cost is paid in hand-authored numbers. Steel, carbon fibre and tungsten
> carbide are not elements; they are **Materials**, and the Material layer generates them
> combinatorially rather than by hand.

##### Uneven attribute coverage is the design working, not a gap to fill

Counting the drafted thirty by how many elements rate **3** in each attribute: Inert 10 · Thermal 9 ·
Structure 6 · Magnetic 4 · Optical 4 · Conductive 3 · **Energetic 2** · **Semiconductive 2**.

*(Recount across the full 41-element roster, 2026-08-12: Inert 13 · Thermal 10 · Structure 7 ·
Optical 5 · Magnetic 4 · Conductive 3 · **Semiconductive 3** · **Energetic 2**. Semiconductive
gained a third element (`Se`); **Energetic is now the roster's sole worst-covered attribute**,
unchanged by this pass — see the `Power Core`/`Field Emitter` discussion below for why that's judged
a real chemistry limit rather than a gap this section should try to close by inventing a rating.)*

Refractory and corrosion-proof elements are everywhere; top-grade reactor fuel and top-grade logic
substrate were two elements each out of thirty, one of which (Semiconductive) has since gained a
third. **That is the pressure point, and it arrived without a rarity tier being invented.** Systems
holding germanium or uranium are worth fighting over because of what grows there, not because a table
said "rare." Do not even this out.

#### Attributes propagate. Quality is rolled once. 📋

*These are two different mechanisms and they must not be confused — this section already forbade the
confusion for grades and now states it generally.*

| | Behaviour | Where |
|---|---|---|
| **Attributes** | Propagate deterministically — sums and weighted averages | Every link: Element → Material → Module |
| **Quality** (§2.7's band roll) | Rolled **once** | The finished module or shell only |

A Conductive Coil made of silver genuinely *is* a better coil than one made of aluminium —
predictably, inspectably, before any dice are involved. The finished module then rolls its quality on
top. **Two knobs the player reasons about separately: what you made it from is a choice; how well it
came out is a roll.** If both compounded across the chain, the output distribution would be
impossible to balance or to explain.

> ⚠️ **This chain has no home in code.** Element → Material → Module attribute propagation must be
> *computed* somewhere, and that somewhere is **`ManufacturingSystem`** (`architecture.md` §12.18),
> which does not exist. Not circular — the content lands first — but none of this is visible in play
> until §12.18 is built. Recorded in §11.9.

> ⚠️ **There are three axes, not two, and `architecture.md` §12.19 names the third** (2026-08-10).
> Attributes and quality are the two this table separates; **composition** is the one it assumes.

| Axis | Set by | Rolled? |
|---|---|:--:|
| **Grade** — how *broad* the recipe is | The builder: which recipe you ran | No |
| **Composition** — which elements filled its role slots | The builder's stock, resolved at manufacture | No |
| **Quality** — §2.7's band | Creation | **Yes** |

> **Grade buys breadth. Composition buys depth. Quality buys magnitude.** No two of them touch the
> same number, which is what lets all three be tuned independently.

**And the propagation rule needs one word this section never supplies.** *"Sums and weighted
averages"* does not say which is which, and the answer is forced by what the numbers mean:
**attributes average, mass sums.** An attribute is a property of the substance — four units of silver
are not four times as conductive as one — while mass is a property of the quantity. That split is
what makes density-as-universal-cost actually cost something: reaching for iridium raises the
Structure average **and** the summed mass. Under a summing rule the choice would collapse, because
more units would mean more of everything at once.

#### Recipes demand roles, never named elements 📋

*Settled 2026-08-09, and it is what makes the whole model safe.*

> **A Field Emitter recipe does not ask for "5 × Neodymium." It asks for five units of a
> **magnetic** element — and Nd, Sm, Co and Fe all qualify, with different results.**

Three consequences, all wanted:

- **A player can never be hard-stuck.** Any element of the right role builds *something*. Being
  short of good ones costs quality, not access — which lands precisely on §2.7's band.
- **Trade is about *better*, not *possible*.** A gate makes a missing element a wall; substitution
  makes it a price.
- **It answers the softlock directly.** You need elements to build a hyperdrive and a hyperdrive to
  reach new elements. With named-element recipes that is a real dead end; with roles it cannot occur.

##### 🐛 A recipe is therefore a demand, not a bill of materials

*Found 2026-08-10 while scoping `architecture.md` §12.19 against the six specified docked screens.*

If a slot names a role and *"whatever the builder holds that scores highest in that role fills it,"*
then **the recipe cannot say what any particular item is made of.** Two Common Alloy Plates off one
recipe are iron in a poor system and iridium in a rich one, and the paragraph above requires them to
differ. Three settled mechanics read what actually went in — attribute propagation, an item's mass,
and `architecture.md` §12.30.4's repair cost reading **Inert** — and none of them can get it from a
def.

> **The recipe is the demand. What an item is made of is a fact about the instance, and it is not
> derivable from the design.**

⚠️ **What an instance stores is what its composition *produced*, never the composition itself** —
eight attributes and one mass (`architecture.md` §12.19). The inputs are consumed, so the derivation
is not repeatable and there is nothing for a stored value to drift against; that is the one case
where this document's derive-never-store discipline does not apply, and it needs saying or someone
will delete the fields as redundant.

**And *"deconstruction reads the recipe backwards"* needs a definition to resolve to.** Reading roles
backwards yields roles, and roles are not matter. §12.19 supplies it: **the nominal fill of a role is
the roster's lowest-density element scoring ≥ 1 in it** — cheapest available, which is exactly what
generic reclaimed stock should be. It has three consumers, so it is not an abstraction ahead of its
readers: deconstruction yield, the attributes of an item that was **found** rather than built, and
the base value of the same. **Recovery is clamped by mass**, so reclaiming a lithium-built module at
a heavier nominal fill can never return more mass than the module held.

##### The seeding invariant

> **Every system rolls at least one element in every role.**

That is what makes "cannot be hard-stuck" an *enforced property* rather than a hope — it is checkable
in a test. Systems stay distinctive because *which* element fills each role varies, and abundance
varies on top.

**The player therefore does not start with a hyperdrive** (settled 2026-08-09) — they build one, from
whatever their starting system happens to hold, at whatever quality that implies. Drives are also
lootable and buyable, so credits are always a second path.

#### Distribution — two rolls, and abundance carries the variety 📋

Seed-derived, pure functions of the coordinate (§7.1):

1. **Presence** — does this system hold this element at all? **The probability is the same for every
   element**, subject to the seeding invariant above. There is no availability band.
2. **Abundance** — how rich is it, if present?

**Abundance is what the deleted bands used to do.** A system is not "a tungsten system"; it is a
**rich** system or a **poor** one, crossed with which elements it happens to hold. Rich systems
become worth fighting over for a legible economic reason, which is what §5.1's Three Pillars and §6's
expansion facets want and currently lack.

**One mining tool, and the asteroid decides the yield.** Element-specific extraction gear would be a
second progression axis on a system that already has several, and it would not earn its place.

#### `tools/element_check` — the roster's gate 📋

Thirty rows × eight attributes is exactly the size where a human misses one and a ten-line script
never does. **Four dominance failures were caught by hand while drafting the thirty above** — Nb over
Cu (niobium only superconducts below 9 K; its room-temperature conductivity is poor), Ta over W
(tantalum is soft, tungsten is the strongest metal there is), Th over U (thorium is fertile, uranium
is fissile), and Ga, cut outright because germanium beat it on every axis while being lighter. **There
is no reason to believe that was all of them.**

| Check | Fails when |
|---|---|
| **No pairwise dominance** | A element is ≤ another on all eight attributes while being ≥ its density |
| **Pareto validity** | A element is optimal under *no* weighting of the eight against density — the subtler death, losing to no single rival but to every combination |
| **Role coverage** | Some attribute has too few elements at some level for the seeding invariant to be satisfiable |
| **Density spread** | An attribute is available only at one end of the mass range |

It runs in CI beside the four existing structural checks (`architecture.md` §2.2–2.4), and it is the
first time that section's *"boundaries enforced by a format, a parser, or a linker survive"* principle
reaches **content** rather than code. The same shape validates the module roster later.

#### Materials — eleven manufactured families 📋

*Renamed from "Crafts" 2026-08-09 (§2's vocabulary block). Weightings are now expressed as
**attribute roles** rather than named elements, which is what makes substitution work. Grew from
eight to ten families 2026-08-12 (`Refractory Plate`, `Transparent Composite`), then to eleven the
same day (`Radiation Shielding`) — see the new rows below. All eleven are universal; none carry
faction specificity — see below.*

Materials are manufactured intermediates. They **carry a grade** (§2.8's input chain requires it) but
**do not roll quality** — quality is rolled once, at the finished module or shell. If every link
rolled, the output distribution would compound across the chain and become impossible to balance or
to explain.

| Material | Wants | Feeds |
|---|---|---|
| **Alloy Plate** | Structure | Chassis, armour shells |
| **Composite Housing** | Structure, low density | Shells generally |
| **Conductive Coil** | Conductive | Power routing, engines |
| **Circuit Wafer** | Semiconductive | Targeting, avionics, crew modules |
| **Optical Array** | Optical, Semiconductive | Sensors, energy weapons |
| **Power Core** | Energetic, Thermal | Power cells, reactors |
| **Field Emitter** | Magnetic, Conductive | Shields, Ion weapons |
| **Propellant** 📋 | Energetic, low density | Hyperdrive fuel (§2.11), thrusters |
| **Refractory Plate** 📋 | Structure, Thermal | Weapon barrels, engine linings, reactor housings |
| **Transparent Composite** 📋 | Structure, Optical | Cockpit canopies, sensor domes, viewports |
| **Radiation Shielding** 📋 | Inert, low density | Hull plating and hazard-rated modules exposed to Corona/nebula radiation (`architecture.md` §12.28) |

⚠️ **A material's "wants" is a weighting, never a gate.** Every material exists at every grade, and a
Common Field Emitter built from iron is a crude thing that works. **Nothing is locked behind a
particular element** — the weighting decides what the recipe *reaches for*, and what you actually
hold decides what it *gets*. This is §2.10's role-substitution rule seen from the recipe side.

**`Radiation Shielding` is new, 2026-08-12, and it closes a real coverage gap rather than a
faction-flavor one.** Inert is the best-covered attribute at the element level (13 of 41 elements
rate it 3), but none of the other ten families want it at all — a whole attribute with excellent
element support and zero material consumer. It also gives `architecture.md` §12.28's Corona/nebula
hazards something concrete to feed a resistance stat from, which nothing in the content model
currently provides.

✅ **Settled 2026-08-12: materials carry no faction specificity, ever — exclusivity lives entirely at
the module/shell/vessel design level, which is where it was already specified.** §2.8 already gates
manufacturing on *"the actor's knowledge network actually holds the design"* — for modules, shells,
and vessels, never for the materials they're built from. A faction-exclusive material *variant*
(a proprietary bonus recipe, considered and rejected the same day) would have been a second gating
mechanism sitting one layer below where gating already lives, solving the same problem twice and
needing its own tracking for which variant a given instance used. **Any actor who can manufacture at
all can manufacture any of these eleven families, at any grade, from whatever they hold** — the
same "cannot be hard-stuck" guarantee §2.10 already promises for elements, extended down to the
layer built from them. Faction identity — a signature alloy, a proprietary reactor design — is
expressed by *what a faction builds*, not by *what they're allowed to build it from*, and lands when
the module/shell roster is designed, not here.

**`Propellant` is new**, and it closes a real gap: §2.11 settled that hyperdrives consume fuel and
**nothing anywhere said what fuel is made of**. It is a manufactured material like any other, drawn
from Energetic elements — which is what lets hydrogen, helium and the other volatiles be ordinary
Elements rather than a parallel resource class.

**`Refractory Plate` and `Transparent Composite` are new, and neither is a named-element recipe
wearing a material's name.** Both were raised alongside a batch of exotic, fictional element
proposals (Vitreous-Silicon, Phason-Gas) that couldn't be adopted as Elements — §2.10 is explicit
that layer is real elements only — but the underlying gaps they pointed at were real: nothing in the
original eight wants Structure **and** Thermal together (weapon barrels and engine linings currently
have no material that reaches for both), and nothing wants Structure **and** Optical together
(transparent armor — a shatter-resistant, see-through hull material — has no family to draw from
either). Both slot into the existing generation rule with no change to the mechanism: role-weighted
slots, substitutable, no element named. `Refractory Plate` feeds weapon barrels, engine linings, and
reactor housings; `Transparent Composite` feeds cockpit canopies, sensor domes, and viewports —
giving the Armored Observation Canopy idea (visibility traded for a lower health pool than solid
plating) a real material to be made of.

⚠️ **This changes §2.10's "~50-material roster" arithmetic, restated there — twice now, since
`Radiation Shielding` landed the same day.** Eleven families × seven grades is closer to 77 than 50;
two Mythic recipes drawing 8 distinct materials each now share roughly **0.8 of them, not 1.3** —
the same argument that section already makes, strengthened by having three more families to draw
from, not weakened.

##### Five materials want a combination the roster can't fully satisfy — checked by hand, 2026-08-12

*A material wanting two attributes at once is only as well-served as the elements that score decently
on **both simultaneously**, not the elements that score well on either alone. Checked against the
full 41-element roster:*

| Material | Wants (combined) | Elements serving both decently | Verdict |
|---|---|---|---|
| **Optical Array** | Semiconductive + Optical | `Ge`, and now `Cd` (above) | Was a single-element bottleneck before `Cd`'s correction — the worst case found |
| **Power Core** | Energetic + Thermal | `U`, `Th`, and `Zr` (above, previously undercounted) | Three, not two |
| **Transparent Composite** | Structure + Optical | `Cr`, `Rh` | Two — a real physical tension, not a gap |
| **Field Emitter** | Magnetic + Conductive | `Fe`, `Co`, `Ni` | Three — these are the *only* three elements ferromagnetic at room temperature, full stop |
| **Circuit Wafer** | Semiconductive (alone) | `Si`, `B`, `Ge`, `Se` | Single-attribute, not a dual-intersection — thin but not a bottleneck |

**Deliberately not padded further.** `Field Emitter` and `Transparent Composite` stay at their
narrow counts on purpose: real chemistry doesn't offer more elemental ferromagnets, and structural
strength and optical transmission are close to opposing properties in an element's own bonding —
metallic bonds give strength, non-metallic bonds give transparency, rarely both. Inventing a fourth
"pretty good at both" element for either would mean either picking something already dominated (the
same failure `Ga`, `Gd`, `Eu`, and `Er` already hit above) or authoring a rating past what real
chemistry supports, which undermines the free-intuition argument this whole layer is built on.
**Composition averaging across a material's multiple slots is the intended answer to a narrow pool,
not a workaround for one** — a Mythic `Power Core` blending `U`+`Th`+`Zr` slots gets a genuinely
strong blend of both wanted attributes without needing a single perfect element to exist.

#### What a grade actually costs 📋

> **Grade sets how many *distinct* materials a recipe demands, and how much of each. It does not
> restrict *which*.**

| Grade | Distinct materials |
|---|:---:|
| Common | 2 |
| Uncommon | 3 |
| Unique | 4 |
| Rare | 5 |
| Epic | 6 |
| Legendary | 7 |
| **Mythic** | **8** |

*The **"rarest permitted"** column was deleted 2026-08-09 along with the availability bands. It could
not survive them: with no material rarer than another there is nothing for it to restrict. Grade now
controls **breadth and quantity only**, which is enough — 8 distinct materials is a territory problem
however common each one is individually.*

**Two ladders used to share three words**, which would have collided the moment either became a JSON
enum: material availability was Common/Uncommon/Rare/Anomalous while item grade is
Common/Uncommon/Unique/Rare/Epic/Legendary/Mythic. This table previously had a row labelled **Rare**
(a grade) whose cell read **"≤ uncommon"** (a band). **Deleting the bands removes the collision
entirely** — `Grade` is now the only ladder in the content model.

Against a ~77-material roster (eleven families × seven grades, up from fourteen under the old
pre-rename system), two Mythic recipes drawing 8 distinct materials each share roughly **0.8 of them
rather than 4.6** — so recipes read as genuinely different parts lists rather than as the same one
reshuffled. That, not scarcity, is what the roster size buys.

*(Was ~50/1.3 against eight families; revised 2026-08-12 twice — first to ~70/0.9 when
`Refractory Plate` and `Transparent Composite` brought the count to ten, then to ~77/0.8 when
`Radiation Shielding` brought it to eleven — see below.)*

**Quantity scales on top of variety**, steeply — roughly against the inverse of §2.7's drop-rate
curve (~3× per grade). §2.4 requires cost to climb faster than the stat benefit does, or
mass-producing Mythics becomes correct the moment it is possible.

#### Recipes are authored on the def, and Material recipes are generated 📋

- **A module's or shell's recipe is a field on its own def** (`ModuleDef.recipe`, `ShellDef.recipe`),
  so one item is one entry and there is no second file to keep in sync.
- **Material recipes are generated** from the grade table above plus the per-material attribute
  weighting — never authored as fifty-six rows. Hand-authoring them guarantees that someone edits one
  grade and not its neighbours, and the curve develops a kink nobody notices.

**The generation rule, stated** *(2026-08-09 — this was an unspecified gap; the section said recipes
were generated without saying how)*:

> A Material's recipe is **`n` slots**, where `n` is the grade's distinct count (2→8). Slots are
> filled by **attribute role**, drawn in order of the material's weighting — its signature roles
> first, then breadth. **No slot ever names an element**; it names a role, and whatever the builder
> holds that scores highest in that role fills it.

A Common Alloy Plate is 2 slots, both Structure — iron if that is what you have, iridium if you are
rich. A Mythic one is 8, reaching well past Structure into Thermal and Inert. **Grade decides
breadth; your holdings decide quality; nothing decides availability.**

#### Mass and price both derive from the recipe 📋

*This is the same anchor applied twice, and it is what keeps the content set honest as it grows.*

| | Authored | Derived |
|---|---|---|
| **Mass** | Per **Element** only | Material = inputs − loss · Module/shell = inputs − loss · Vessel = its parts |
| **Base price** | Per **Element** only | Material = inputs + margin · Module = its Materials + margin · Vessel = its parts |

**One authored mass per element, one authored price *for all of them*, and everything else follows.**
A module is expensive *because* its recipe is brutal, not because someone typed a large number — and
adding a new module never requires guessing either figure. §3.5's "manufacturing cost scales with
total mass" stops being a separate rule and becomes the same rule seen from the other end.

**Price is a function, never stored.** Base value is computed once at content load; local supply and
demand modulate it on query. This is the same discipline §3.5 applies to system radius — *"seed-derived
and must never be stored, because caching it invites the two to drift."*

> ⚠️ **Corrected 2026-08-10 (`architecture.md` §12.19): mass derives from the *composition*, not from
> the recipe.** The recipe fixes how many units of which roles; the composition fixes which elements
> filled them, and those have different densities. Two instances of one def at one grade can weigh
> different amounts, and **that is the design** — density is this section's universal cost, so a
> silver coil has to weigh more than an aluminium one or the cost is not paid. §2.4's *"mass stays
> deterministic"* survives intact: mass is deterministic **given the composition**, and composition
> is a choice, never a roll.
>
> **Base value gains a second term for a different reason.** Priced on the recipe alone, a high-roll
> and a low-roll instance of one def cost the same — and with §12.30.3's settled no-spread rule
> (buying and selling at one station nets exactly zero) a player buys ten Mythics, keeps the 5.0
> roll, and sells nine back for what they paid. That is a **free reroll machine**, and it bypasses
> the brake §2.8 built against exactly this: *"each attempt costing a full Legendary-Material
> pipeline run is the brake."* **`BaseValue = recipeValue(id, grade) × the instance's quality
> multiplier.`** The recipe measures what went in, quality measures what came out, and a market that
> prices only the first lets rolls be laundered at zero cost. *Value remains a property of the design
> rather than of where it was built — a facility grade still changes nothing.*

#### Pricing — one authored number, three separate layers 📋

*Settled 2026-08-09. These three are conflated constantly, and keeping them apart is what stops the
economy from contradicting the material model.*

| Layer | What it is | Driven by |
|---|---|---|
| **Base value** | What a thing is inherently worth | Its recipe, recursively down to elements |
| **Cost to build** | What you actually consume making it | Recipe **×** facility grade (the efficiency axes below) |
| **Local price** | What it trades for **here** | Base value **×** local scarcity |

A Mythic facility building an item consumes less than a Common one, but **the item's base value is
identical** — value is a property of the design, not of where it was built. Same discipline this
section already applies to mass.

##### Every element has the same base value

> **One unit of any element is 1 credit. All price differentiation is local.**

⚠️ **This revises a proposal made earlier the same day** that base price should be anchored on
**density**. It was wrong, and the reason matters: **density is already the universal cost** in
§2.10's attribute model — what you pay for high attributes. Pricing on it too charges twice, and it
would have let titanium (ρ 4.51, Str 3 / Thm 2 / Inr 3) **economically dominate** iridium (ρ 22.56,
Str 3 / Thm 3 / Inr 3): one extra Thermal point for five times the mass *and* five times the money.
`element_check`'s Pareto test would not have caught it, because that test reads attributes and
density, not price. **The dead-weight problem would have come back through the economy.**

Uniform base value sounds flat until you look at what moves local stock:

- **Abundance rolls** — a system rich in germanium and poor in uranium prices them differently on day
  one, with no authored rarity anywhere.
- **Consumption** — and this is what carries it. **Elements are consumed at wildly different rates.**
  Propellant burns every jump; iridium sits in a hull for years. High-consumption elements deplete
  local stock continuously, so they run scarce and price high **everywhere** — without a number.

> **Fuel is expensive because it is *burned*, not because it is rare.** That is the whole
> volatile-pricing answer, and it needs no special case. A busy trade hub burns more than a backwater
> and prices accordingly.

##### The three knobs, because derivation with no lever cannot be corrected

| Knob | Lives in | Effect |
|---|---|---|
| **Quantity per grade** (**~2×**, revised 2026-08-09) | The grade table above | Dominant — compounds six times across the ladder |
| **Refinement loss** | §2.7's mass ladder, 100% → 70% | You consume more than you get |
| **Margin per manufacturing step** | Applied twice: Element→Material, Material→Module | Modest, roughly linear |

##### ⚠️ The quantity curve was ~3× and is revised down to ~2×

*This section previously set quantity at ~3×/grade, chosen to mirror §2.7's drop-rate curve so that
manufacturing cost matched what finding was worth. **Two problems, and the second is decisive.***

**It made swarms strictly correct.** 3× compounds to **3⁶ = 729×** across the full ladder, against a
Mythic's actual delivery of roughly **×6** combat value per hull (§2.7's quality bands reach ×2.00–3.50
by Legendary, plus `moduleSlots` stepping +1 at Unique, Epic and Mythic). A player choosing between one
Mythic fighter and a thousand Common ones for the same credits is not making an interesting decision —
and nothing else caps fleet size, since §2.7 cut upkeep and §9.1's 100-vessel figure is a performance
budget rather than a game rule.

**It contradicted this section's own governing principle**, three subsections above:

> *"Exploration and combat give you the **first** of a thing. Industry gives you **more** of it."*

If manufacturing a Mythic costs exactly what its rarity implies, **industry does not give you more of it** —
it gives you precisely as few, at a different kind of expense. Mirroring the drop curve reproduces the
scarcity that drops already impose, and §2.4's *"a drop rate gates first acquisition, not ongoing
supply"* is the rule it breaks.

**~2×/grade → 64× across the ladder** keeps §2.4's cost-outpaces-benefit constraint satisfied (64×
cost against ~6× value) while leaving industry the genuine advantage over scavenging that the
principle promises. **It is a working value pending measurement, not a settled one** — this is exactly
the question `tools/economy_sim` exists to answer, and it should be read off the curve rather than
argued.

**One check that still passes, and one that was retired:**

- ✅ **§2.4's hardest constraint holds.** *"Cost must climb faster than the stat benefit does, or
  mass-producing Mythics becomes correct."* At 2×/grade: cost ×64, combat value ~×6.
- ❌ **"The curve matches the drop rate" is no longer a goal.** It was the reasoning behind 3×, and it
  is the thing that broke. Matching drop rates was never the requirement — *outpacing stat benefit*
  is, and that is a much weaker constraint with far more room in it.

##### ⚠️ The curve compounds three knobs, and the check above counts one

*Found 2026-08-10 (`architecture.md` §12.19), by running the derivation rather than the argument.*

| Term | Across the seven tiers | Inside the ×64? |
|---|---:|:--:|
| Quantity per slot, ~2×/grade | ×64 | ✅ |
| **Distinct slots, 2 → 8** — this section's own grade table | **×4** | ❌ |
| **The input-grade chain** — §2.8: a grade-*N* item needs grade ≥ *N*−1 Materials, themselves ×256 | **×10²ish** | ❌ |

Multiplied out, a Mythic module costs on the order of **10⁴** Common modules rather than 64. §2.4's
constraint is satisfied by an enormous margin — **which is its own failure**, because a 10,000×
multiplier is the scarcity ladder rebuilt on the cost side, and that is precisely what the 3× → 2×
revision was made to avoid. *"Industry gives you more of it"* does not survive ×10,000 any better
than it survived ×729.

> **The ~2× figure was chosen against a one-knob model. Against three it is probably nearer ~1×** —
> breadth alone already delivers ×4 per rung, and the input chain delivers the rest.

✅ **Confirmed 2026-08-11 by `tools/economy_sim`, which now exists.** The tool prints the compounded
Common→Mythic module cost multiplier at each candidate quantity-per-grade value:

| Quantity/grade | Module cost multiplier | Cost/combat-value ratio at Mythic |
|---|---:|---:|
| 1.0× | **14×** | 3.08 (peaks ~3.3× at Epic/Legendary) |
| 1.5× | 1,211× | 266× |
| 2.0× (the prior working value) | 28,672× | 6,308× |
| 3.0× (mirroring the drop-rate curve) | 2,480,058× | 545,613× |

**The quantity-per-grade multiplier is settled at ~1.0×** — i.e., the quantity knob contributes
nothing beyond breadth and the input-grade chain, which alone already produce a real, tier-scaling
14× cost climb against a ~5× combat-value gain. This satisfies §2.4's constraint (cost outpaces
benefit) without rebuilding the drop-rate's scarcity on the cost side, which every value above 1.0×
does by a widening margin. **This is also why cost must not scale against the drop-rate curve** — see
the correction above at "No tier cap on research."

*Do not re-tune this by argument alone going forward — this section's own rule is that prices are
outputs. `tools/economy_sim` (`tools/economy_sim/EconomyModel.h`, tested in
`tests/unit/EconomyModelTests.cpp`) is the standing tool for re-running this derivation if any of the
three knobs' authored values change.*

##### Nobody decides what a ship costs

*The obvious next question — "so what does a starter fighter cost?" — is the wrong question, and
answering it by fiat would invert this whole section.* Prices are **outputs**. You author the content,
run the derivation, and read the curve.

**That is `tools/economy_sim`'s job** (🧊 `architecture.md` §3, and §9 already names it as what would
settle manufacturing pacing). It now has a concrete definition: run the derivation across the authored
content set and print the resulting curve, so a bad shape is *observed* rather than argued about.

⚠️ **It does not exist** — `tools/` holds only `ci/`. Until it does, every pacing figure in this
document is an estimate, and should be written as one.

#### The three efficiency axes, and why they must not be confused 📋

*Settled 2026-08-08, after an earlier model attached mass loss to the facility and inverted itself:
if a better facility "loses less," it produces **heavier** items, and since lighter is better at equal
quality, the worst workshop would make the best goods.*

| Axis | Driven by | Direction |
|---|---|---|
| An item's **mass** | The **item's grade** (§2.7's 100% → 70% ladder) | Higher grade = lighter |
| **Materials consumed** to build it | **Facility grade** | Better facility = less waste |
| **Materials recovered** on deconstruct | **Facility grade** | Better facility = more back |

**Facility grade is always material efficiency; item grade is always lighter and better.** Nothing
inverts, and an item's mass stays a property of *the design* rather than of where it happened to be
built — which matters, because a mass that varied by workshop would mean the same id weighing
different amounts and a fit screen the player cannot predict.

**§2.7's mass ladder is the refinement loss.** "Mythic is 70% mass" and "30% burned off in the
making" are one number seen twice, not two mechanisms to reconcile. A Common facility burns 120 units
to produce that same 70-mass item where a Mythic burns 100; the difference is waste, and waste was
never in the item.

**Deconstruction is conservation-safe:** it returns up to **the item's own mass**, never the mass
originally consumed. Refinement loss is permanent, build → deconstruct → build always loses, and
there is no loop.


### 2.11 The Module Roster 📋

*Settled 2026-08-08. `ModuleKind` currently holds six values — Weapon, ShieldGenerator, PowerCell,
Engine, Armor, Facility — and several built systems have no module feeding them at all. This section
is the full roster, each kind's rollable pool (§2.7), and how per-hardpoint contributions become
rig-level attributes.*

#### Rig attributes aggregate from living hardpoints 📋

> **Every rig-level attribute is the sum — or the maximum — of contributions from *living*
> hardpoints. Destroying a hardpoint removes its contribution. Nothing is all-or-nothing.**

⚠️ **Sharpened 2026-08-11: this is a recompute rule, not a zero-out rule, and the two are easy to
conflate.** A dying hardpoint's *own* contribution correctly drops to zero the instant it dies —
that part is simple and already intended. What this section actually requires is broader: every
**other** module's rig-level effect that depends on the *living set as a whole* — `PowerSystem`'s
draw-vs-generation satisfaction is the clearest example, since it is computed across every
power-consuming module, not just the one that changed — must be **recomputed**, not left stale,
whenever that set changes. And "changes" means all four ways it can: a player equipping or
unmounting a module, an NPC doing the same, a hardpoint dying in combat, and a hardpoint being
repaired back to life. A fix that only handles deliberate player equip/unmount is not this rule; it
is one quarter of it.

This is not a new pattern; it is **the pattern one system already follows and the others do not.**
`PowerSystem` recomputes `PowerBudget` from living hardpoints every tick. Four things currently break
it in different ways:

| Attribute | Today |
|---|---|
| `Propulsion` | **All-or-nothing** — zeroed only when the *last* engine dies |
| `BodyMass` | **Never recomputed** on mount or unmount |
| Shields | Contribution never extends past its own mount (§3.1's defect) |
| Cargo capacity | Unspecified until now |

*⚠️ The row above read "cargo **slots**" until 2026-08-10. §2.2 settled on 2026-08-08 that capacity is
a **mass** budget and that slots are presentation; this table was the last place the rejected word
survived, and it is the one a schema would have been written from.*

**Each attribute declares whether it sums or maxes**, and the distinction is not cosmetic:

| Aggregation | Attributes |
|---|---|
| **Sum** | thrust · turnTorque · power generation · **cargo mass capacity** · shield capacity · hull bonus · fuel capacity |
| **Max** | **maxSpeed** · sensor range · jump range |

**`maxSpeed` is the instructive one.** Two engines should not double your top speed, but they *should*
double your acceleration. Sum for thrust and max for top speed delivers exactly that, and it creates a
real fit distinction: **more engines makes you nimble, better engines makes you fast.** Sensors follow
the same logic — two radars do not see twice as far, the better one dominates. And multiple
hyperdrives are **redundancy rather than range**: fuel sums, range maxes, so shooting one out does not
strand you.

⚠️ **This corrects §3.2's wording.** It reads "destroy a thruster shell and the ship stalls." Under
proportional contribution a multi-engine hull *slows*; it stalls only when the last engine dies.

> ⚠️ **The `mobile` flag contradicts this rule outright, and it is going** (settled 2026-08-09).
> `RigFactory` reads a blueprint-authored `mobile` boolean and, when false, **emplaces no
> `Propulsion` component at all** so that `PhysicsSystem`'s view excludes the rig structurally. The
> consequence is that a station can never move no matter what is bolted to it — equipping engine
> shells at runtime changes nothing, because the component physics reads was never created.
>
> **Movement must be emergent from living hardpoints like every other rig attribute**, not decided by
> an authored flag. A rig with engines moves; a rig without them does not; a station that mounts
> engines becomes mobile. There is no static/mobile vessel class, which is Law 4 — the same rule that
> removed every other vessel-type branch.
>
> `mobile` need not be deleted outright: it still usefully records which factory built a thing and
> drives `Validation`'s "a mobile craft needs an engine" authoring check. It simply stops deciding
> whether physics applies. See `architecture.md` §12.25.

#### Costs follow grade; capability follows the band 📋

*Settled 2026-08-08, resolving where `powerDraw` belongs.*

| Property | Governed by | Direction |
|---|---|---|
| Every capability stat | **The quality band** (§2.7) | Up |
| **`mass`** | **The grade ladder** — 100% → 70% | Down |
| **`powerDraw`** | **The grade ladder** — gentler, ~100% → 85% | Down |
| `moduleSlots` | Step curve | Up |

**`powerDraw` is not a pool entry on any kind.** It and `mass` are the two costs of §2.2's constraints
puzzle and should behave identically — having one rollable and the other not was the real
inconsistency. Keeping it out also stops every pool gaining the same undifferentiated entry, which
would have pushed Weapon to 7 and ShieldGenerator to 8 against §2.7's 4–6 legibility budget.

**The draw curve is deliberately gentler than mass.** Capability up to ×5, mass at 70%, and draw at
70% would compound hard against §2.3 rule 3's power balance — and that rule is a hard validation gate
that blocks a Template from being saved, so it must stay binding.

*What this costs, stated plainly:* you can never find an unusually efficient instance — every Mythic
weapon draws the same power. That is one build axis less in the loot, traded for a fit whose power
balance is computable from grades alone rather than by inspecting every instance. If it ever reads as
flat, the reversible move is to give **one** kind efficiency as its identity, not to open it
everywhere.

#### The roster 📋

| Kind | Pool | Aggregation | Power category |
|---|---|---|---|
| **Weapon** ✅ | fireInterval ↓ · damage ↑ · projectileSpeed ↑ · spread ↓ · range ↑ · knockback ↑ | per-hardpoint | Weapons |
| **ShieldGenerator** ✅ | capacity ↑ · coverageRadius ↑ *(Bubble)* · rechargeDelay ↓ · boostMultiplier ↑ · rechargePerSecond ↑ · bleedThrough ↓ | sum within a pool | Shields |
| **Engine** ✅ | thrust ↑ · turnTorque ↑ · maxSpeed ↑ · boostMultiplier ↑ | sum / **max** for maxSpeed | Engines |
| **PowerCell** ✅ | generation ↑ · surgeMultiplier ↑ | sum | — |
| **Armor** ✅ | hullBonus ↑ *(+ flatReduction on one family — see below)* | per-hardpoint | — |
| **Facility** ✅ | ratePerSecond ↑ · capacity ↑ | per-hardpoint | Facilities |
| **FireControl** 📋 | traverse ↑ · lead accuracy ↑ · reacquisition ↑ | per-hardpoint | Weapons |
| **Sensor** 📋 | range ↑ | **max** | Facilities |
| **CargoBay** 📋 | slotCount ↑ · slotCapacity ↑ | sum | Facilities |
| **Hyperdrive** 📋 | jumpCountdown ↓ · jumpRange ↑ · cooldown ↓ · fuelCapacity ↑ · fuelPerJump ↓ | sum / **max** for range | Facilities |
| **Comms** 📋 | commsRange ↑ | **max** | Facilities |
| **Crew** 📋 | `operation` ↑ · `command` ↑ · boost pools (§2.7). Replaced `Operator`/`Commander` on 2026-08-09 | per-hardpoint | Facilities *(zero draw)* |

#### `Comms` — the kind that closes two gaps at once 📋

*Settled 2026-08-09, added by the command system (§4.0/§4.3).*

**`commsRange` gates command reach**, and it has a second consumer that already exists: `CommsSystem`
is built and gates hailing on **`SensorRange`** — which conflates two different things. Sensors
*detect*; comms *talk*. `CommsSystem` uses sensor range only because it was the one range stat that
existed when it was written.

So one module supplies command reach (new), fixes a conflation in built code (`CommsSystem`'s hail
check moves to `commsRange`), and gains a §2.4 justification from two independent readers rather than
one. **Aggregation is `max`**, for the same reason sensors are: two radios do not talk twice as far.

**It also gives ECM a better target.** §2.11 already lists ECM/jamming as a planned kind that
suppresses enemy sensor range; suppressing enemy *comms* range means jamming a hostile commander's
ability to issue orders at all, which is a far more interesting thing to attack than their radar.

#### `FacilityKind::Construction` — what gates building 📋

*Settled 2026-08-09.* A new `FacilityKind`, carrying **`buildRange`**, on an ordinary Facility module.

- **It gates the player's build mode.** The B key opens placement only if a living Construction
  hardpoint exists on the player's rig; otherwise the player orders a unit that has one (§4.3).
- **It gates the Build order** for any unit, per §4.3's emergent-order rule.
- **`buildRange` bounds placement around the *builder*, not the player** — which is what makes a
  constructor's position matter and what makes escorting one to the frontier a real task.

**Distinct from `Manufacturing`, deliberately.** §2.8 already split the two systems on principle:
construction produces *entities*, manufacturing produces *inventory*. Two systems, two gates, two
kinds. Reusing `Manufacturing` would also couple build mode to §2.8's manufacturing system, which
does not exist yet.

**No new `ModuleKind` is needed.** Repair, Research, Docking, Storage, and Engineering are all already
`FacilityKind`s on Facility modules; §2.11's "each functional module is its own kind" rule targets an
`Auxiliary` catch-all, not the facility sub-taxonomy.

**§2.9's four power categories need no fifth.** `PowerPriorityFor(ModuleKind)` already maps kinds to
weapons/shields/engines/facilities, and every new kind slots in: **FireControl → weapons** (it is part
of the gunnery chain), **Sensor, CargoBay, Hyperdrive → facilities** (shed first, non-combat). Worth
stating, or the next kind added will prompt someone to invent a category.

#### The three kinds that close existing gaps 📋

Each of these has a **built system with nothing feeding it** — the same shape as the shield defect,
found by grepping rather than by reading the docs.

**`Sensor`.** `SensorRange` exists in `Targeting.h` and `DiscoverySystem` reads it. §8.3 already
demands this module by name: *"Sensor modules, picket ships, and stations are radar you build,
position, and defend."* Thin pool today (range); it grows when there is something to detect *against*.

> ⚠️ **Corrected 2026-08-09: this previously said "nothing produces it."** `RigFactory` emplaces
> `SensorRange` hardcoded at `2000.0f` on every rig root. So it is not an unproduced stat — it is
> the same shape as `FiringArc::turnRatePerSecond = kPi`: a **hardcoded producer with nothing
> authored behind it.** The module is still needed and the gap is still real; it is one rung less
> broken than recorded, and the fix is to make the value come from a module rather than to invent a
> producer from nothing.

**`CargoBay`.** §2.2 specifies capacity as coming from mounted bays and `CargoHold` aggregating from
living ones — but the kind was never added.

- `slotCount` — how many distinct stacks. **Variety.**
- `slotCapacity` — mass per stack. **Bulk.**
- **Total is derived** (`slotCount × slotCapacity`) and never authored. A hold with 4 × 250 is an ore
  hauler; 20 × 50 is a trade-goods runner; both carry 1,000.

**`FireControl`.** Supplies automated tracking, driving `FiringArc::turnRatePerSecond` — read by
`WeaponSystem` but **hardcoded to `kPi`** and authored nowhere.

> **It stays separate from `Weapon`, and merging them would undo §2.7's turret decision.** If tracking
> were baked into the gun, a cheap weapon could never be independent and the withdrawn tier gate would
> be back. Separate modules are what make "cheap turret + good gunner ≈ expensive automated turret"
> true.

**And the tier progression returns properly, through the general mechanic:** a 1-slot turret fits only
the weapon and is slaved unless crewed; a higher-grade turret with 2 slots (§2.7's step curve) fits
weapon **plus** fire control and tracks on its own. Grade buys independence — not by a special
`crewSlots` rule, but because `moduleSlots` steps up.

#### `Hyperdrive` and fuel 📋

*Settled 2026-08-08.* `WarpSystem` performs local, system, and galaxy warp today with **no module and
no fuel concept at all.**

> **A hyperdrive is required to jump between systems.**

- **`WarpSystem` gains a gate it does not have** — a behaviour change to built code.
- **Shooting out a hyperdrive prevents escape.** A real tactical objective, and it makes
  `NpcAiSystem`'s flee behaviour counterable rather than an automatic out.
- **`jumpCountdown` is the vulnerability window** — the drive spins up, then you are gone. Distinct
  from `cooldown`, which is the wait *after* a jump. Interdiction becomes "kill them before the
  countdown ends."
- **Every hull can carry one, and it costs a slot and mass.** A fighter trades a gun for independence:
  take the drive and self-deploy, skip it and be a system-defence fighter that rides in a bay (§4.5).
  Both playstyles exist and neither is mandated.
- It also creates the **navigator crew role** §2.7 rejected for having "no skill hook in `WarpSystem`."

**Fuel exists, and the design already assumed it did.** §2.7 cut upkeep on the grounds that *"repairing
hulls, **refuelling**, and assembling replacement vessels are all recurring credit and material sinks."*
Without refuelling, that argument loses a leg.

> **Power is the tactical resource. Fuel is the strategic one.**

- **Only hyperdrives consume fuel — never engines.** Engines burning fuel puts a second clock in every
  dogfight and lets you run dry while manoeuvring, which is pure tedium. Jumps burning fuel means you
  run dry from **over-extending**, which is a decision you made an hour ago.
- Consumption scales with jump distance and hull mass.
- **Refuel becomes `StationServicesMenu`'s fourth service**, alongside buy, sell, and repair.
- **Running dry strands you**, which `DistressSystem` already exists to make interesting.

#### Armour: flat reduction is one family's identity, not a universal stat 📋

*Settled 2026-08-08 after being flagged as potentially overpowered — it is, as a universal stat.*

Flat reduction subtracts a fixed amount from every hit. Its effect is **nonlinear**: against a
50-damage shot, 5 is a 10% nerf and 25 is a 50% nerf, and beyond that everything floors. It hard-
counters entire weapon families, which makes it far swingier than its single number suggests.

So it is scoped rather than dropped:

- **One armour family carries it** — ablative or composite plating. Most armour modules are
  `hullBonus` only, which makes flat reduction rare by construction rather than by a grade gate.
- **The floor is ~25%**, not 10%. A mismatched weapon should be heavily penalised but **visibly
  working** — an absolute block means you shoot and nothing happens, with no feedback and no way to
  read why.
- **Percentage resistance is rejected** as redundant: it does the same job on a duller curve, and
  having both means every hit runs shields, then flat reduction, then a percentage — three mitigation
  steps to reason about, with a multiplicative stacking problem that then needs a cap.

**It earns its place because it completes a two-axis targeting decision.** §3.2's localized damage
already means high damage-per-shot *wastes* against small hardpoints, favouring fire rate. Flat
reduction pushes the other way on armoured ones, favouring per-shot damage. Small targets want rate;
armoured targets want punch.

#### Planned kinds, not yet specified 📋

| Kind | Consumer status | Cost |
|---|---|---|
| **Mining laser** | `MiningSystem` is built and scheduled and reads **no module stat** — mining rate is authored nowhere | Small — the same gap-closing shape as Sensor |
| **Tractor / salvage** | `LootSystem::FindCollectorInRange` already takes an `extraRadius` parameter **that nothing supplies** | Small — the hook exists |
| **ECM / jamming** | Suppresses enemy sensor range. Genuinely strategic once §8.3's fog is real | Small mechanic, real depth. Wants fog first |
| **Cloak / stealth** | ⚠️ **The one on this list that is not a module.** Sensors carry only a range; there is nothing to *detect against*. Cloak needs a signature stat on every hull and a detection check — a system, not an attribute | Large |

#### The Weapon Roster — per-faction and general 📋

*Settled 2026-08-12. First content pass on `ModuleKind::Weapon` — ten factions plus a faction-less
General roster, five weapons each (55 total). Crew and Facility modules stay universal, not
per-faction (§2.10's precedent for Materials, applied here) — the variety that's worth authoring per
faction lives in Weapon, ShieldGenerator, PowerCell, Engine, Armor, FireControl, and CargoBay.*

**Two axes stay independent, and getting them confused is the mistake this pass corrected twice.**
`DamageType` (Kinetic/Energy/Ion) is what happens **on impact**, and it decides shield interaction —
unaffected by anything below. A weapon's **recipe** — which Material(s) it's built from — is decided
by its **role**, not its damage type, and a role can imply a real second physical subsystem the way
`§3.1`'s "weapon behaviour is a separate, unbounded axis from damage type" already said in the
abstract. This section is that principle made concrete:

> **A role that implies storing or routing energy in a specific way earns a second material. A role
> that's just aim-and-fire stays on one.**

| Role | Implies | Recipe |
|---|---|---|
| **Kinetic — Sustained, Spread** | A barrel/housing surviving repeated firing. Nothing else | `Refractory Plate` alone |
| **Kinetic — Penetration** | The same barrel, **plus** a magnetic accelerator — real railguns achieve penetration velocity electromagnetically, not chemically | `Field Emitter` + `Refractory Plate` |
| **Energy — Precision** | Beam focusing and targeting. Nothing else — `Optical Array`'s own Optical+Semiconductive want already covers aim | `Optical Array` alone |
| **Energy — Burst** | Beam focusing, **plus** a capacitor to store a charge and release it fast | `Optical Array` + `Power Core` |
| **Energy — Sustained** | Beam focusing, **plus** efficient continuous power routing so a long beam doesn't waste itself as heat | `Optical Array` + `Conductive Coil` |
| **Ion — Disable (standard)** | A crude-but-functional EMP burst. Nothing else | `Field Emitter` alone |
| **Ion — Disable (heavy)** | The same field generator, **plus** a capacitor for a stronger pulse — the same logic as Energy Burst | `Field Emitter` + `Power Core` |

**This is the rule to carry into every other per-faction module kind**, not a Weapon-specific
exception: ask what a role's own description physically requires before authoring its recipe, the
same way a railgun's recipe was wrong until "penetration" was read as "electromagnetic," not just as
a bigger number.

Stat bias below is qualitative — exact tuning is `tools/economy_sim` territory (§2.10), not decided
by inspection. Each faction's **weakest weapon category is named explicitly** rather than pretended
away; nobody is equally good at everything, per §2.2's no-strictly-best-loadout rule applied to
faction identity instead of only to individual fits.

##### Aegis Directorate — disciplined, standard-issue, by-the-book

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Sentinel Autocannon | Kinetic | Sustained | Refractory Plate | High Thermal tolerance → high sustained rate, modest per-shot damage |
| Bulwark Railgun | Kinetic | Penetration | Field Emitter + Refractory Plate | High Structure bias → armor-piercing, slow fire rate |
| Directorate Beam Array | Energy | Precision | Optical Array | Low spread, steady damage, moderate power draw |
| Containment Lance | Energy | Burst | Optical Array + Power Core | High burst damage, real cooldown window |
| Suppressor EMP Caster | Ion | Disable | Field Emitter | Standard Ion — strips shields, cuts power, no hull damage |

##### Meridian Star Corps — cost-efficient, mass-produced, mercenary-adjacent

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Ledger-Line Cannon | Kinetic | Sustained | Refractory Plate | Cheap recipe, average everything — the "good enough" gun |
| Contractor Flak Battery | Kinetic | Spread | Refractory Plate | Wide spread, low per-hit damage — point-defense-adjacent |
| Q3 Pulse Emitter | Energy | Precision | Optical Array | Low power draw, prioritizes cost over ceiling |
| Liquidator Beam | Energy | Burst | Optical Array + Power Core | Higher-tier option — bought, not built, for the client who pays |
| Repossession Caster | Ion | Disable | Field Emitter | Standard Ion, marketed as a "peaceful" disable-not-kill weapon |

##### Kore Industries — repurposed mining tools, rugged, blunt

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Drillhead Mass Driver | Kinetic | Penetration | Field Emitter + Refractory Plate | Repurposed mining tool — extreme Structure bias, slow |
| Foreman's Scattergun | Kinetic | Spread | Refractory Plate | Close-range, heavy per-hit, short effective range |
| Slag-Cutter Laser | Energy | Sustained | Optical Array + Conductive Coil | Industrial cutting laser repurposed — steady, unglamorous |
| Blast-Charge Launcher | Energy | Burst | Optical Array + Power Core | Mining-charge derived — high burst, real self-risk at close range |
| Demag Field Caster | Ion | Disable | Field Emitter | Standard Ion, framed as "clearing a jammed rig" tech |

##### The Forgotten — jury-rigged, salvaged, high-variance

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Scrapgun | Kinetic | Sustained | Refractory Plate | Wide quality-roll variance — could be great, could be junk |
| Patchwork Ripper | Kinetic | Spread | Refractory Plate | Whatever scrap filled the recipe slots — unpredictable damage |
| Jury-Rig Laser | Energy | Precision | Optical Array | Cobbled together, high variance, cheap when it works |
| Overcharged Splicer | Energy | Burst | Optical Array + Power Core | Deliberately overdriven — real risk of self-damage on overheat |
| Salvaged EMP Coil | Ion | Disable | Field Emitter | Standard Ion, stolen tech rather than built |

##### AI Concordance — precise, optimized, post-biological

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Calculus Autocannon | Kinetic | Sustained | Refractory Plate | Their weakest category, by design — still computer-tuned |
| Precision Beam Array | Energy | Precision | Optical Array | Best-in-class low spread — the archetype energy weapon |
| Recursive Pulse Caster | Energy | Burst | Optical Array + Power Core | Optimized burst timing, minimal wasted energy |
| Statistical Disruptor | Ion | Disable | Field Emitter | Enhanced Ion — the faction that most wants "disable, don't destroy" |
| Entropy-Minimizing Lance | Ion | Disable (heavy) | Field Emitter + Power Core | Second Ion option — signature weapon, high power cost |

##### Pyre Ascendancy — fire/plasma, aggressive, high-risk

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Cinder Cannon | Kinetic | Sustained | Refractory Plate | Their weakest category — kept only for tradition |
| Sacred Flame Lance | Energy | Burst | Optical Array + Power Core | Highest burst damage of any faction's Energy weapon, short range |
| Purification Beam | Energy | Sustained | Optical Array + Conductive Coil | Continuous-beam plasma, heavy Thermal demand, real overheat risk |
| Immolator Pulse Caster | Energy | Burst | Optical Array + Power Core | Second Energy option — signature weapon |
| Cleansing EMP Font | Ion | Disable | Field Emitter | Standard Ion, framed narratively as "burning out" the target's power |

##### Voidwalkers — exotic, anomaly-touched, cryptic

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Rift-Edge Driver | Kinetic | Penetration | Field Emitter + Refractory Plate | Their weakest category — used only when cornered |
| Umbral Beam | Energy | Precision | Optical Array | Low signature, low spread — matches their stealth leaning |
| Anomalous Pulse Caster | Energy | Burst | Optical Array + Power Core | Erratic-but-strong, "borrowed" physics flavor |
| Dissonance EMP Font | Ion | Disable | Field Emitter | Standard Ion — the faction most thematically aligned with disabling |
| Silence Caster | Ion | Disable (heavy) | Field Emitter + Power Core | Second Ion option, signature weapon |

##### Zenith Collective — precision instruments, sensor-integrated

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Archivist Autocannon | Kinetic | Sustained | Refractory Plate | Their weakest category |
| Cataloguing Beam Array | Energy | Precision | Optical Array | Best-in-class low spread, shares tech with their sensor suite |
| Study Pulse Caster | Energy | Burst | Optical Array + Power Core | Second Energy option |
| Discovery Lance | Energy | Sustained | Optical Array + Conductive Coil | Third Energy option, tuned for prolonged engagement over specimens |
| Preservation EMP Font | Ion | Disable | Field Emitter | Standard Ion — "disable and recover intact," matches their ethos |

##### Edenian Pact — bio-integrated, defensive-leaning, corrosive

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Thornbranch Driver | Kinetic | Sustained | Refractory Plate | Their weakest category |
| Bloomspore Caster | Energy | Burst | Optical Array + Power Core | Organic-themed area denial |
| Verdant Beam | Energy | Precision | Optical Array | Second Energy option |
| Blight Lance | Energy | Burst | Optical Array + Power Core | Corrosive-flavored third Energy option — "protecting the garden" |
| Symbiotic EMP Font | Ion | Disable | Field Emitter | Standard Ion |

##### The Reapers — brutal, mass-destruction, entropic

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Harvest Driver | Kinetic | Penetration | Field Emitter + Refractory Plate | Highest raw Structure bias of any faction's Kinetic weapon |
| Culling Scattergun | Kinetic | Spread | Refractory Plate | Second Kinetic option — brutal close range |
| Unmaking Beam | Energy | Sustained | Optical Array + Conductive Coil | Continuous, remorseless, high Thermal demand |
| Entropy Lance | Energy | Burst | Optical Array + Power Core | Second Energy option |
| Decay Font | Ion | Disable | Field Emitter | Standard Ion — matches their power-suppression identity |

##### General — faction-less baseline, always available

| Weapon | Type | Role | Recipe | Bias |
|---|---|---|---|---|
| Standard Autocannon | Kinetic | Sustained | Refractory Plate | The flat baseline every other Kinetic weapon is judged against |
| Standard Railgun | Kinetic | Penetration | Field Emitter + Refractory Plate | Baseline penetration option |
| Standard Beam Emitter | Energy | Precision | Optical Array | The flat baseline Energy weapon |
| Standard Pulse Caster | Energy | Burst | Optical Array + Power Core | Baseline burst option |
| Standard EMP Caster | Ion | Disable | Field Emitter | Baseline Ion — what every faction's Ion weapon is a variant of |

#### The Shield Roster — per-faction and general 📋

*Settled 2026-08-12. `ModuleKind::ShieldGenerator` has three independent variety axes — shield-matching
type (Kinetic/Energy, §3.1's deliberate "stays two"), coverage (Personal/Bubble/Conformal, §3.1), and
recharge archetype (Regenerative/Capacitor, §3.1) — not one, which is why this roster is less
combinatorially uniform than Weapon's.*

**The Weapon roster's rule extends here directly, across two axes instead of one:**

| Property | Implies | Adds |
|---|---|---|
| **Personal** coverage | No extra subsystem — the field never leaves its own hardpoint | — |
| **Bubble** coverage | Projecting the field outward over a radius — a routing/amplification problem | `Conductive Coil` |
| **Conformal** coverage | Computing and maintaining a field against the whole hull's changing geometry in real time | `Circuit Wafer` |
| **Regenerative** recharge | Continuous low-rate trickle — the baseline behavior a Field Emitter already has | — |
| **Capacitor** recharge | Storing a large charge before releasing it — the same logic as Energy Burst weapons | `Power Core` |

**These stack.** A Conformal **and** Capacitor shield wants `Field Emitter` + `Circuit Wafer` +
`Power Core` — three distinct materials, which a Common-grade recipe's 2-slot cap can't afford. That
gives §3.1's *"Conformal is the premium mode and should be priced and gated as such"* a concrete
economic reason rather than just an authored label.

**Investment varies by faction on purpose, the same as Weapon's weakest-category rule** — shields
aren't every faction's focus, so the count below isn't uniform: 25 across 11 owners, not 5 each.

##### Aegis Directorate

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Sentinel Aegis | Kinetic | Personal | Regenerative | Field Emitter | Basic backup, always-on |
| Bulwark Field | Kinetic | Bubble | Capacitor | Field Emitter + Conductive Coil + Power Core | Their doctrine shield — absorb the big hit |
| Perimeter Ward | Energy | Bubble | Regenerative | Field Emitter + Conductive Coil | Secondary coverage |

##### Meridian Star Corps

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Standard Coverage Plan | Kinetic | Personal | Regenerative | Field Emitter | Cheapest possible, insurance-flavored |
| Premium Coverage Plan | Energy | Personal | Regenerative | Field Emitter | "Premium" in name only — still baseline hardware |

##### Kore Industries

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Drillhard Plating Shield | Kinetic | Personal | Capacitor | Field Emitter + Power Core | Their only shield — tank one big hit, no finesse |

##### The Forgotten

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Scav-Ward | Kinetic | Personal | Regenerative | Field Emitter | Salvaged, unreliable |
| Patched Barrier | Energy | Personal | Regenerative | Field Emitter | Whatever they scavenged — Bubble/Conformal are beyond their means |

##### AI Concordance

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Recursive Envelope | Energy | Conformal | Regenerative | Field Emitter + Circuit Wafer | Signature — elegant, computed, whole-hull |
| Calculated Bubble | Kinetic | Bubble | Capacitor | Field Emitter + Conductive Coil + Power Core | Secondary |
| Baseline Field | Energy | Personal | Regenerative | Field Emitter | Basic backup |

##### Pyre Ascendancy

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Faithguard | Kinetic | Personal | Regenerative | Field Emitter | Their only shield — offense is the doctrine, this is the bare minimum |

##### Voidwalkers

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Long-Drift Ward | Energy | Bubble | Regenerative | Field Emitter + Conductive Coil | Signature — long-endurance deep-space coverage |
| Wayfarer's Field | Kinetic | Personal | Regenerative | Field Emitter | Basic |
| Rift-Cloak Envelope | Energy | Conformal | Regenerative | Field Emitter + Circuit Wafer | Premium — full-hull protection for isolated runs |

##### Zenith Collective

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Archive Envelope | Energy | Conformal | Regenerative | Field Emitter + Circuit Wafer | Signature — protects the whole specimen-carrying hull |
| Cataloguer's Bubble | Kinetic | Bubble | Capacitor | Field Emitter + Conductive Coil + Power Core | Secondary |
| Study Field | Kinetic | Personal | Regenerative | Field Emitter | Basic |

##### Edenian Pact

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Symbiotic Ward | Energy | Bubble | Regenerative | Field Emitter + Conductive Coil | Signature — self-sustaining, "living" recharge flavor |
| Garden Envelope | Kinetic | Conformal | Regenerative | Field Emitter + Circuit Wafer | Whole-ship protection, matches their protective doctrine |
| Root Field | Energy | Personal | Regenerative | Field Emitter | Basic |

##### The Reapers

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Pressure Bulwark | Kinetic | Bubble | Capacitor | Field Emitter + Conductive Coil + Power Core | Signature — absorbs sustained assault, matches "absorbs the most pressure" (§5.7) |
| Husk Field | Energy | Personal | Regenerative | Field Emitter | Basic backup |

##### General — faction-less baseline, always available

| Shield | Type | Coverage | Recharge | Recipe | Bias |
|---|---|---|---|---|---|
| Standard Kinetic Shield | Kinetic | Personal | Regenerative | Field Emitter | The flat baseline every Kinetic-matching shield is judged against |
| Standard Energy Shield | Energy | Personal | Regenerative | Field Emitter | The flat baseline Energy-matching shield |

#### The Power Cell Roster — per-faction and general 📋

*Settled 2026-08-12. `ModuleKind::PowerCell` has three generation archetypes — standard cell,
fuel-consuming generator, and solar panel — established a few sessions prior alongside the storage
(`storageCapacity`) + rate (`productionRate`) model. Unlike Weapon and Shield, the three archetypes
turn out to want genuinely different primary materials, not just additions to a shared one.*

| Archetype | Why | Recipe |
|---|---|---|
| **Standard cell** | Baseline reactor/battery — no ongoing cost, fixed output by grade | `Power Core` alone |
| **Fuel-consuming generator** | Actively burning/reacting fuel for a higher ceiling needs **containment**, not just generation | `Power Core` + `Radiation Shielding` |
| **Solar panel** | Not thermal/nuclear generation at all — photovoltaic conversion is `Optical Array`'s domain, not `Power Core`'s | `Optical Array` alone |

`Radiation Shielding` (§2.10) gets its first real module consumer here, beyond hull plating — an
active reactor genuinely needs containment against what it's producing, which is exactly the Inert
role that material was added to fill.

**Investment varies by archetype fit, not uniformly** — 20 across 11 owners. Factions built around
consuming something (industrial, zealous, entropic) lean fuel-burning; factions built around
preservation or ecology lean solar; Voidwalkers stay self-sufficient rather than sun-dependent,
since "near a star" isn't where a deep-space nomad chooses to be.

##### Aegis Directorate

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Standard Reactor Core | Standard | Power Core | Reliable baseline, no fuss |
| Sentinel Fusion Plant | Fuel-consuming | Power Core + Radiation Shielding | Backup for heavy weapon/shield draw |

##### Meridian Star Corps

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Ledger Power Cell | Standard | Power Core | Cost-efficient baseline |
| Margin Array | Solar | Optical Array | Zero ongoing cost — maximizes margin, very on-brand |

##### Kore Industries

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Furnace Core | Fuel-consuming | Power Core + Radiation Shielding | Signature — industrial-scale burning, matches their mining doctrine |
| Rugged Power Cell | Standard | Power Core | Backup |

##### The Forgotten

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Scrap Cell | Standard | Power Core | Salvaged baseline |
| Anything-Burner | Fuel-consuming | Power Core + Radiation Shielding | Burns whatever fuel they scavenge — high variance, unreliable |

##### AI Concordance

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Optimized Core | Standard | Power Core | Algorithmically tuned baseline |
| Photonic Array | Solar | Optical Array | Computed stellar tracking for maximum conversion efficiency |

##### Pyre Ascendancy

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Sacred Furnace | Fuel-consuming | Power Core + Radiation Shielding | Their only power cell — "sacred fire" is the whole doctrine |

##### Voidwalkers

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Drift Reserve | Standard | Power Core | Storage-heavy, self-sufficient — no dependency on stellar proximity or fuel resupply for long isolated runs |

##### Zenith Collective

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Solar Array Mk. | Solar | Optical Array | Signature — sustainable, matches their preservation ethos |
| Archive Power Cell | Standard | Power Core | Backup |

##### Edenian Pact

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Photosynth Array | Solar | Optical Array | Signature — non-depleting generation, matches their ecological identity directly |
| Bio-Power Cell | Standard | Power Core | Backup |

##### The Reapers

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Consumption Core | Fuel-consuming | Power Core + Radiation Shielding | Signature — matches their entropic, consuming nature |
| Husk Cell | Standard | Power Core | Backup |

##### General — faction-less baseline, always available

| Power Cell | Archetype | Recipe | Bias |
|---|---|---|---|
| Standard Power Cell | Standard | Power Core | The flat baseline every reactor-type cell is judged against |
| Standard Solar Array | Solar | Optical Array | The flat baseline photovoltaic option |

#### The Engine Roster — per-faction and general 📋

*Settled 2026-08-13. `ModuleKind::Engine`'s pool (thrust ↑ · turnTorque ↑ · maxSpeed ↑ ·
boostMultiplier ↑) splits along one axis with two families — chemical combustion and
electromagnetic (Ion) — the same duality `modules.json`'s placeholder `ion_thruster_i` already
gestured at in its name before a roster existed to back it up.*

**The Weapon roster's rule applies unchanged: ask what the role physically needs before writing its
recipe.**

| Role | Implies | Recipe |
|---|---|---|
| **Chemical — Thrust** | A burn chamber and nozzle. Nothing else | `Propellant` alone |
| **Chemical — Maneuvering** | The same burn chamber, **plus** plumbing to split it across vectoring nozzles for turn authority | `Propellant` + `Conductive Coil` |
| **Ion — Cruise** | No combustion at all — a sustained electromagnetic acceleration field | `Conductive Coil` alone |
| **Ion — Boost** | The same field, **plus** a capacitor for a burst multiplier — the same logic as Energy Burst weapons | `Conductive Coil` + `Power Core` |

**Investment isn't uniform** — chemical thrust is the one every faction can build; Ion is the one
that separates them, the same shape as Weapon's Ion split.

##### Aegis Directorate

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Sentinel Drive | Chemical — Thrust | Propellant | Standard-issue, reliable, unremarkable |
| Formation Thruster | Chemical — Maneuvering | Propellant + Conductive Coil | Tuned for holding formation, not top speed |
| Directorate Ion Core | Ion — Cruise | Conductive Coil | Disciplined efficiency over raw thrust |

##### Meridian Star Corps

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Ledger Thruster | Chemical — Thrust | Propellant | Cheapest possible, average everything |
| Contractor Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Sold as an upgrade package |
| Premium Ion Drive | Ion — Cruise | Conductive Coil | "Premium" in name only, still baseline hardware |

##### Kore Industries

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Strip-Mine Thruster | Chemical — Thrust | Propellant | Signature — repurposed mining-rig burn engine, extreme thrust |
| Foreman's Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Blunt, functional |
| Rugged Ion Core | Ion — Cruise | Conductive Coil | Their weakest category — finesse isn't the doctrine |

##### The Forgotten

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Scrap Thruster | Chemical — Thrust | Propellant | Wide quality-roll variance |
| Patchwork Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Whatever scrap filled the recipe slots |
| Salvaged Ion Core | Ion — Cruise | Conductive Coil | Stolen tech, their weakest category, unreliable |

##### AI Concordance

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Calculated Thruster | Chemical — Thrust | Propellant | Their weakest category, kept minimal |
| Statistical Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Computer-tuned turn authority |
| Recursive Ion Core | Ion — Cruise | Conductive Coil | Signature — smooth, computed, efficient |
| Optimized Boost Array | Ion — Boost | Conductive Coil + Power Core | Second Ion option, minimal wasted energy |

##### Pyre Ascendancy

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Cinder Thruster | Chemical — Thrust | Propellant | Signature — the biggest burn of any faction's thruster |
| Zealot Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Aggressive close-in turning |
| Ember Ion Core | Ion — Cruise | Conductive Coil | Their weakest category — kept only for tradition |

##### Voidwalkers

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Wayfarer Thruster | Chemical — Thrust | Propellant | Their weakest category, basic |
| Umbral Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Low-signature turning, matches their stealth leaning |
| Drift Ion Core | Ion — Cruise | Conductive Coil | Signature — long-endurance deep-space efficiency |
| Rift Boost Array | Ion — Boost | Conductive Coil + Power Core | Second Ion option, erratic-but-strong |

##### Zenith Collective

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Archivist Thruster | Chemical — Thrust | Propellant | Their weakest category |
| Cataloguing Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Precise turn control for close specimen work |
| Study Ion Core | Ion — Cruise | Conductive Coil | Signature — sustainable, matches their preservation ethos |

##### Edenian Pact

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Rootwalker Thruster | Chemical — Thrust | Propellant | Their weakest category |
| Bloomdrift Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Organic-themed, matches their defensive doctrine |
| Symbiotic Ion Core | Ion — Cruise | Conductive Coil | "Living" efficiency, low signature |

##### The Reapers

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Harvest Thruster | Chemical — Thrust | Propellant | Signature — highest raw thrust of any faction's chemical drive |
| Culling Vector Jets | Chemical — Maneuvering | Propellant + Conductive Coil | Brutal, close-in turning |
| Decay Ion Core | Ion — Cruise | Conductive Coil | Matches their power-suppression identity |

##### General — faction-less baseline, always available

| Engine | Role | Recipe | Bias |
|---|---|---|---|
| Standard Chemical Thruster | Chemical — Thrust | Propellant | The flat baseline every other chemical drive is judged against |
| Standard Vector Thruster | Chemical — Maneuvering | Propellant + Conductive Coil | Baseline maneuvering option |
| Standard Ion Drive | Ion — Cruise | Conductive Coil | Baseline Ion — what every faction's Ion Core is a variant of |

#### The Armor Roster — per-faction and general 📋

*Settled 2026-08-13. `ModuleKind::Armor`'s pool is `hullBonus` alone for most of the family, with
flat reduction scoped to one family per "Armour: flat reduction is one family's identity, not a
universal stat" (above) — that scoping is what makes Armor a three-family roster rather than a
one-line stat.*

| Family | Implies | Recipe |
|---|---|---|
| **Standard Plating** | Pure structural bulk. Nothing else | `Alloy Plate` alone |
| **Ablative/Composite Plating** | The same structural bulk, **plus** a sacrificial layer that blunts each hit — this is the flat-reduction family, and it stays rare by construction | `Alloy Plate` + `Composite Housing` |
| **Hazard-Rated Plating** | The same structural bulk, **plus** containment against Corona/nebula radiation (`architecture.md` §12.28) | `Alloy Plate` + `Radiation Shielding` |

**Not every faction fields all three** — the same weakest-category rule as Weapon and Shield.

##### Aegis Directorate

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Sentinel Plating | Standard | Alloy Plate | Standard-issue baseline |
| Bulwark Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Signature — "absorb the big hit," same doctrine as their shield |
| Directorate Hazard Plating | Hazard-Rated | Alloy Plate + Radiation Shielding | Standard-issue hazard tolerance |

##### Meridian Star Corps

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Standard-Issue Plating | Standard | Alloy Plate | Cheapest possible baseline |
| Contractor Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Sold as an upgrade package |

##### Kore Industries

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Drillhard Plating | Standard | Alloy Plate | Rugged, repurposed mining-hull baseline |
| Foreman's Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Blunt, functional |
| Shaft-Rated Hazard Plating | Hazard-Rated | Alloy Plate + Radiation Shielding | Signature — mining rigs already need radiation tolerance |

##### The Forgotten

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Scrap Plating | Standard | Alloy Plate | Salvaged, unreliable |
| Patchwork Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Whatever scrap filled the recipe slots |

##### AI Concordance

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Calculated Plating | Standard | Alloy Plate | Their weakest category — precision and avoidance substitute for bulk |
| Recursive Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Computer-tuned absorption layer |

##### Pyre Ascendancy

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Cinder Plating | Standard | Alloy Plate | Their weakest category — offense is the doctrine |
| Sacred Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Signature — "endure the flame" |

##### Voidwalkers

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Wayfarer Plating | Standard | Alloy Plate | Basic |
| Rift Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Absorbs the unpredictable |
| Drift Hazard Plating | Hazard-Rated | Alloy Plate + Radiation Shielding | Signature — built for anomaly-laced deep space |

##### Zenith Collective

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Archivist Plating | Standard | Alloy Plate | Basic |
| Cataloguing Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Protects the specimen-carrying hull |
| Study Hazard Plating | Hazard-Rated | Alloy Plate + Radiation Shielding | Signature — for fieldwork near hazardous specimens |

##### Edenian Pact

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Rootwalker Plating | Standard | Alloy Plate | Basic |
| Bloomshield Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Signature — "protecting the garden," matches their protective doctrine |

##### The Reapers

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Harvest Plating | Standard | Alloy Plate | Basic |
| Culling Composite Armor | Ablative/Composite | Alloy Plate + Composite Housing | Signature — absorb and grind, not preserve |

##### General — faction-less baseline, always available

| Armor | Family | Recipe | Bias |
|---|---|---|---|
| Standard Plating | Standard | Alloy Plate | The flat baseline every hullBonus-only armor is judged against |
| Standard Composite Plating | Ablative/Composite | Alloy Plate + Composite Housing | Baseline flat-reduction option |
| Standard Hazard Plating | Hazard-Rated | Alloy Plate + Radiation Shielding | Baseline hazard-tolerance option |

#### The Fire Control Roster — per-faction and general 📋

*Settled 2026-08-13. `ModuleKind::FireControl` is still 📋 planned in code — `WeaponSystem` reads
`FiringArc::turnRatePerSecond` hardcoded to `kPi` (above, "The three kinds that close existing
gaps") — but the roster is written ahead of the pool, the same way Sensor and CargoBay's gaps were
described before their implementation.*

| Method | Implies | Recipe |
|---|---|---|
| **Radar-Directed** | Basic electronic tracking — targeting logic alone | `Circuit Wafer` alone |
| **Optically-Guided** | The same targeting logic, **plus** a sighting system it slaves to | `Circuit Wafer` + `Optical Array` |
| **Predictive** | The same targeting logic, **plus** a heavier compute core to solve intercepts and re-lock faster | `Circuit Wafer` + `Power Core` |

**This is the thinnest roster of the seven** — several factions field only one, which is honest:
fire control is a force multiplier on a weapon that already exists, not every faction's identity.

##### Aegis Directorate

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Sentinel Fire Director | Radar-Directed | Circuit Wafer | Standard-issue, disciplined |
| Directorate Predictive Array | Predictive | Circuit Wafer + Power Core | Signature — first-shot accuracy is the doctrine |

##### Meridian Star Corps

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Ledger Tracking Unit | Radar-Directed | Circuit Wafer | Cheapest possible baseline |
| Contractor Optical Sight | Optically-Guided | Circuit Wafer + Optical Array | Sold as an upgrade package |

##### Kore Industries

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Foreman's Tracking Rig | Radar-Directed | Circuit Wafer | Their only option — blunt doctrine, no finesse budget |

##### The Forgotten

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Scrap Tracker | Radar-Directed | Circuit Wafer | Their only option, unreliable |

##### AI Concordance

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Baseline Tracking Unit | Radar-Directed | Circuit Wafer | Barely needed — everything else compensates |
| Calculated Optical Sight | Optically-Guided | Circuit Wafer + Optical Array | Computer-tuned aim |
| Recursive Predictive Core | Predictive | Circuit Wafer + Power Core | Signature — computed lead-solving is their whole identity |

##### Pyre Ascendancy

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Zealot Tracking Rig | Radar-Directed | Circuit Wafer | Their only option — aggression, not precision |

##### Voidwalkers

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Umbral Optical Sight | Optically-Guided | Circuit Wafer + Optical Array | Signature — low-signature lock, matches their stealth leaning |
| Rift Predictive Core | Predictive | Circuit Wafer + Power Core | Erratic-but-strong |

##### Zenith Collective

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Archivist Tracking Unit | Radar-Directed | Circuit Wafer | Their weakest category |
| Cataloguing Optical Sight | Optically-Guided | Circuit Wafer + Optical Array | Signature — shares tech with their sensor suite |
| Study Predictive Core | Predictive | Circuit Wafer + Power Core | Second option, tuned for prolonged engagement |

##### Edenian Pact

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Bloomwatch Tracking Rig | Radar-Directed | Circuit Wafer | Their only option — defensive doctrine, not offense-optimized |

##### The Reapers

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Culling Tracking Rig | Radar-Directed | Circuit Wafer | Basic |
| Harvest Predictive Core | Predictive | Circuit Wafer + Power Core | Signature — brutal efficiency |

##### General — faction-less baseline, always available

| Fire Control | Method | Recipe | Bias |
|---|---|---|---|
| Standard Tracking Unit | Radar-Directed | Circuit Wafer | The flat baseline every fire control is judged against |
| Standard Optical Sight | Optically-Guided | Circuit Wafer + Optical Array | Baseline optical option |
| Standard Predictive Core | Predictive | Circuit Wafer + Power Core | Baseline predictive option |

#### The Cargo Bay Roster — per-faction and general 📋

*Settled 2026-08-13. `ModuleKind::CargoBay` is still 📋 planned in code (above, "The three kinds
that close existing gaps"). Its pool is exactly the two-word split already named there —
`slotCount` is **Variety**, `slotCapacity` is **Bulk** — which makes the roster's axis pick itself.*

| Family | Implies | Recipe |
|---|---|---|
| **Variety Bay** (slotCount) | A modular rack of small containers — structural housing alone | `Composite Housing` alone |
| **Bulk Hold** (slotCapacity) | The same housing, reinforced and enlarged for mass over count | `Composite Housing` + `Alloy Plate` |

**The thinnest roster of the seven by design** — cargo is a utility every faction can build, not a
combat identity, so most factions field exactly one.

##### Aegis Directorate

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Standard-Issue Hold | Variety | Composite Housing | Their only option — military doctrine doesn't prioritize freight |

##### Meridian Star Corps

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Ledger Freight Rack | Variety | Composite Housing | Cheapest possible baseline |
| Contractor Bulk Hold | Bulk | Composite Housing + Alloy Plate | Signature — mercenary/trade-adjacent hauling |

##### Kore Industries

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Foreman's Bulk Hold | Bulk | Composite Housing + Alloy Plate | Signature — ore hauling, matches their mining doctrine directly |

##### The Forgotten

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Scrap Rack | Variety | Composite Housing | Whatever fits |
| Patchwork Bulk Hold | Bulk | Composite Housing + Alloy Plate | Salvaged, unreliable capacity |

##### AI Concordance

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Calculated Freight Rack | Variety | Composite Housing | Their only option — not a trade-focused faction |

##### Pyre Ascendancy

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Zealot Cargo Rack | Variety | Composite Housing | Their only option — raiders keep only what they can burn through fast |

##### Voidwalkers

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Wayfarer Freight Rack | Variety | Composite Housing | Basic |
| Drift Bulk Hold | Bulk | Composite Housing + Alloy Plate | Signature — long isolated runs need bulk supply reserves |

##### Zenith Collective

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Cataloguing Freight Rack | Variety | Composite Housing | Signature — specimen variety over bulk |
| Archive Bulk Hold | Bulk | Composite Housing + Alloy Plate | Second option |

##### Edenian Pact

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Rootwalker Freight Rack | Variety | Composite Housing | Their only option |

##### The Reapers

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Harvest Bulk Hold | Bulk | Composite Housing + Alloy Plate | Signature — hauling the spoils of destruction |

##### General — faction-less baseline, always available

| Cargo Bay | Family | Recipe | Bias |
|---|---|---|---|
| Standard Freight Rack | Variety | Composite Housing | The flat baseline every Variety bay is judged against |
| Standard Bulk Hold | Bulk | Composite Housing + Alloy Plate | The flat baseline Bulk hold |

#### The Facility Roster — general only 📋

*Settled 2026-08-13, revised 2026-08-13 to track `architecture.md`'s two facility-taxonomy
decisions. Facility modules stay universal by design (above — "the variety that's worth authoring
per faction lives in Weapon, ShieldGenerator, PowerCell, Engine, Armor, FireControl, and CargoBay"
deliberately left Facility off that list). The roster targets **seven** `FacilityKind`s, not the
six currently in code: §12.26 (2026-08-09) adds **`Construction`**, carrying `buildRange`, gating
the `B` build-mode key and Build orders — not a docked screen, but a real facility hardpoint. §12.30
(2026-08-10) **deletes `Storage`** — deposit/withdraw is gated on the `CargoHold` component, not a
facility kind — and reuses that enumerator slot for **`Trade`**, which gates Buy/Sell (the Market
screen). `data/base_game/modules.json` currently authors exactly one of the seven (`docking_bay_i`,
Docking); this roster fills the other six.*

| Facility | Kind | Recipe | Bias |
|---|---|---|---|
| Docking Bay | Docking | Composite Housing | Existing (`docking_bay_i`) — simple structural bay, no tooling |
| Trading Post | Trade | Circuit Wafer + Composite Housing | Market concourse plus transaction logic — feeds Buy/Sell, the Market screen (§12.30.3) |
| Repair Bay | Repair | Alloy Plate + Conductive Coil | Structural handling plus powered tooling to patch hulls — feeds §12.30.4's `ratePerSecond` |
| Manufacturing Line | Manufacturing | Circuit Wafer + Composite Housing | Automated fabrication — control logic housed in a structural bay |
| Research Lab | Research | Circuit Wafer + Optical Array | Data processing plus scanning optics — feeds `ResearchSystem`'s reverse-engineering jobs |
| Engineering Bay | Engineering | Conductive Coil + Circuit Wafer | Powered workbench tooling plus control logic — feeds `EngineerSystem`'s merge |
| Construction Yard | Construction | Circuit Wafer + Alloy Plate | Placement/targeting logic for `buildRange` plus structural scaffolding — gates `B` and Build orders (§12.26) |

**No faction variants** — same rule as Crew below: identity lives in what a faction builds and
fights with, not in the workbench it repairs at.

#### The Crew Roster — general only, Living and Artificial 📋

*Settled 2026-08-13. `ModuleKind::Crew` is one kind carrying multiple rollable stats (above, "One
`ModuleKind::Crew`, and what it does is what it rolled") — a single module can specialize in one
role or split its budget across several. This roster authors one single-stat baseline per role, the
same function the General row serves in every other roster: the flat thing every specialized roll
is judged against. **Each role now comes in the two flavours settled above** — Living (credits, at
a station, recurring salary, higher ceiling) and Artificial (`Circuit Wafer` + credits, at a
Manufacturing facility, no upkeep, lower-but-consistent ceiling) — fourteen entries, not seven.*

**Only roles with a real consumer get a module** (§2.4's rule, applied to crew directly, above):
`operation`, `command`, Sensors, and Repair are ✅ buildable now; Facility Manager, Template
Designer, and Trade Master are agreed and specified enough to author. **Damage Control and
Navigator are deliberately excluded** — both are still ❌ blocked on a mechanic that doesn't exist
yet (in-flight repair; a `WarpSystem` skill hook), and authoring a module for a stat nothing reads
would violate the exact rule that got those two withheld in the first place.

| Crew | Role | Type | Acquired | Bias |
|---|---|---|---|---|
| Pilot | `operation` | Living | Credits, at a station | Higher skill ceiling, draws a recurring salary |
| Pilot Drone | `operation` | Artificial | Circuit Wafer + credits, Manufacturing facility | Lower ceiling, perfectly consistent, no upkeep |
| Commander | `command` | Living | Credits, at a station | Fleet dispatch and standing orders — `CommanderSystem`, command authority (§4.0) |
| Tactical Core | `command` | Artificial | Circuit Wafer + credits, Manufacturing facility | Same function, built rather than hired — favoured by AI Concordance, resented by Kore (§5.6) |
| Sensor Officer | Sensors | Living | Credits, at a station | Reads the sensor array itself, not just a console — boosts `SensorRange` |
| Sensor Drone | Sensors | Artificial | Circuit Wafer + Optical Array + credits, Manufacturing facility | Second material — the drone carries its own optical interface, not just a console |
| Repair Officer | Repair | Living | Credits, at a station | Boosts the Repair facility's `ratePerSecond` (§12.30.4) |
| Repair Bot | Repair | Artificial | Circuit Wafer + credits, Manufacturing facility | Never calls in sick, never gets better either |
| Facility Manager | Facility manager | Living | Credits, at a station | Boosts a facility's automation/speed, never its output quality — grade still caps the ceiling |
| Automation Core | Facility manager | Artificial | Circuit Wafer + credits, Manufacturing facility | The natural endpoint of "automation" as a word, not just a stat |
| Template Designer | Template Designer | Living | Credits, at a station | Originates new ship arrangements for a faction — the human counterpart to `CustomizeMenu` |
| Design AI | Template Designer | Artificial | Circuit Wafer + credits, Manufacturing facility | The literal algorithmic counterpart to `CustomizeMenu` — an AI designing ships |
| Trade Master | Trade Master | Living | Credits, at a station | Negotiates `TemplateMarketSystem` sales/royalties, separate from fleet command |
| Negotiation Core | Trade Master | Artificial | Circuit Wafer + credits, Manufacturing facility | Consistent terms, no relationship-building — a harder sell to some counterparties, narratively open |

#### The Sensor Roster — general only 📋

*Settled 2026-08-13. `ModuleKind::Sensor` is still 📋 planned in code — `RigFactory` emplaces
`SensorRange` hardcoded at `2000.0f` on every rig root (above, "The three kinds that close existing
gaps"), a producer with nothing authored behind it. This roster also gives `Transparent Composite`
its first real consumer — its stated purpose, "cockpit canopies, **sensor domes**, viewports"
(§2.10), had zero recipes drawing on it until now.*

| Family | Implies | Recipe |
|---|---|---|
| **Passive** | A housing dome, plus electronics that process radio returns. Nothing else | `Transparent Composite` + `Circuit Wafer` |
| **Active** | The same dome, but reading light directly instead of processing radio returns — an optical array in place of the signal processor | `Transparent Composite` + `Optical Array` |

**No faction variants** — same rule as Facility and Crew: this pool is one stat (`range`), thin by
design, and grows "when there is something to detect against" (§2.11) rather than by faction flavor.

| Sensor | Family | Recipe | Bias |
|---|---|---|---|
| Standard Sensor Dome | Passive | Transparent Composite + Circuit Wafer | The flat baseline — replaces `RigFactory`'s hardcoded 2000 range |
| Standard Sensor Array | Active | Transparent Composite + Optical Array | Longer effective range, higher power draw |

#### The Hyperdrive Roster — general only 📋

*Settled 2026-08-13. `ModuleKind::Hyperdrive` is still 📋 planned in code — `WarpSystem` performs
local, system, and galaxy warp today with **no module and no fuel concept at all** (above,
"`Hyperdrive` and fuel"). `Propellant` already feeds "Hyperdrive fuel" per §2.10, but that is the
**consumable** `fuelPerJump` burns each jump, not the drive's own construction recipe — the module
itself is a field generator, and is reciped as one below.*

| Family | Implies | Recipe |
|---|---|---|
| **Standard** | A jump-field generator plus the power routing to drive it. Nothing else | `Field Emitter` + `Conductive Coil` |
| **Long-Range** | The same generator, plus a capacitor that reaches further per jump at the cost of a slower spin-up | `Field Emitter` + `Power Core` |
| **Rapid-Cycle** | A stripped-down generator alone — no routing, no capacitor, just the fastest possible countdown and cooldown | `Field Emitter` alone |

**No faction variants.** `jumpCountdown` is explicitly the tactical vulnerability window (above) —
these three are archetypes of *when* a rig is willing to be caught, not a faction identity axis.

| Hyperdrive | Family | Recipe | Bias |
|---|---|---|---|
| Standard Hyperdrive | Standard | Field Emitter + Conductive Coil | The flat baseline every hyperdrive is judged against |
| Long-Haul Hyperdrive | Long-Range | Field Emitter + Power Core | Longer `jumpRange`, slower `jumpCountdown` |
| Interdiction-Runner | Rapid-Cycle | Field Emitter | Fastest `jumpCountdown` and `cooldown`, shortest range — built to escape, not to explore |

#### The Comms Roster — general only 📋

*Settled 2026-08-13. `ModuleKind::Comms` is still 📋 planned in code — `CommsSystem` gates hailing
on `SensorRange` today purely because it was the only range stat that existed when it was written
(above, "`Comms` — the kind that closes two gaps at once"); `commsRange` is the fix, and it also
gates command reach (§4.0/§4.3), which had no module behind it at all.*

| Family | Implies | Recipe |
|---|---|---|
| **Standard** | An antenna coil plus the signal-processing logic to drive it. Nothing else | `Conductive Coil` + `Circuit Wafer` |
| **Long-Range** | The same array, plus an amplifier for extended reach | `Conductive Coil` + `Circuit Wafer` + `Power Core` |

**No faction variants** — one stat (`commsRange`), aggregation `max`, the same thin-pool shape as
Sensor.

| Comms | Family | Recipe | Bias |
|---|---|---|---|
| Standard Comms Array | Standard | Conductive Coil + Circuit Wafer | The flat baseline — replaces `CommsSystem`'s current `SensorRange` conflation |
| Long-Range Comms Relay | Long-Range | Conductive Coil + Circuit Wafer + Power Core | Amplified reach, for fleet command across a whole system |


### 2.12 The Shell Roster 📋

*Added 2026-08-11, alongside the Preset Ship Roster (§2.13). Where §2.11 rosters what mounts inside a
hardpoint, this rosters the hardpoint housings themselves — the other half of Law 4's Shell →
Component → Module split.*

**Shells carry structure, nothing else.** `ShellDef` stays exactly what it is today: `hull`, `mass`,
`radius`, `moduleSlots`, `spriteLayer`. No shell grants a stat bonus, a damage-type bias, or a special
ability — that is what the module mounted inside it is for. Faction identity in a shell roster comes
entirely from **how the hull/mass/radius trio is balanced** (Aegis trades mass for hull; a faction
built for speed would trade the reverse) and from sprite/flavor, never from a hidden performance stat.

**Shells are not gated by hull class.** The rig → hardpoints → modules structure is identical whether
the rig ends up a fighter, a cruiser, or a station — a fighter and a dreadnought both start from a
`Chassis`-kind shell and grow by attaching more hardpoints, not by picking from a different shell
taxonomy. A roster's 2–5 variants per kind naturally span a small→large range (so there is something
to build a corvette out of as well as a fighter), but nothing in the schema *enforces* which variant
goes with which hull class — that judgment stays with whoever authors a ship (§2.13).

**Every kind gets a full per-faction roster, unlike §2.11's modules.** The module rosters keep
Facility/Sensor/Hyperdrive/Comms/Crew general-only because those modules carry real per-stat tuning
that is expensive to hand-balance ten times over. A shell only carries four numbers and a sprite, so
that cost does not exist — every kind below gets the same per-faction treatment Weapon or Engine get.

**`ShellKind` grows from 7 to 13.** The six new values give every §2.11 module kind a housing of its
own, the same 1:1 relationship `Weapon`/`Shield`/`Engine` already have with their module kinds — a
uniform `IsMountable()` table rather than several kinds sharing a generic housing with finer-grained
rules layered on top.

| `ShellKind` | Houses | Status |
|---|---|---|
| `Chassis` | The rig's root; accepts `Armor` modules directly (as `aegis_vanguard`'s `core` mount already does) | ✅ exists |
| `Armor` | `Armor` modules — pure hull with no function of its own | ✅ exists |
| `PowerCell` | `PowerCell` modules | ✅ exists |
| `Engine` | `Engine` modules | ✅ exists |
| `Weapon` | `Weapon` modules | ✅ exists |
| `Shield` | `ShieldGenerator` modules | ✅ exists |
| `Facility` | `Facility` modules (Repair/Manufacturing/Research/Docking/Engineering, §2.11) | ✅ exists |
| `Sensor` | `Sensor` modules | 🆕 new |
| `Hyperdrive` | `Hyperdrive` modules | 🆕 new |
| `Comms` | `Comms` modules | 🆕 new |
| `CargoBay` | `Cargo Bay` modules | 🆕 new |
| `Crew` | `Crew` modules | 🆕 new |
| `FireControl` | `FireControl` modules | 🆕 new |

#### General — the faction-neutral baseline

*Every faction roster below is a variation on this set. `shells.json`'s current seven generic shells
fold into this table as its `Standard`-tier row — several numbers correct a known defect on the way
in: `shell_fighter_chassis`'s radius is 22 today, and §3.5 already names 12 as the corrected value for
a fighter-scale chassis.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Light Frame | 80 | 25 | 10 | 1 | Small and cheap — a shuttle or interceptor root |
| Standard Frame | 150 | 45 | 12 | 1 | Corrects `shell_fighter_chassis`'s current radius-22 defect (§3.5) |
| Bulwark Frame | 900 | 220 | 60 | 2 | Corvette/cruiser-scale root; a second slot admits a backup armor plate |

| Shell (Armor) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Plate Segment | 60 | 15 | 6 | 1 | Baseline hull-only housing |
| Reinforced Segment | 110 | 30 | 7 | 1 | More hull for more mass — a straight tradeoff, no hidden bonus |

| Shell (PowerCell) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Power Bay | 50 | 15 | 8 | 1 | `shells.json`'s existing `shell_power_bay`, unchanged |
| Reactor Bay | 90 | 28 | 11 | 1 | Larger housing for a capital-scale power cell |

| Shell (Engine) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Thruster Mount | 45 | 12 | 9 | 1 | `shells.json`'s existing `shell_thruster_mount`, unchanged |
| Drive Bay | 140 | 40 | 18 | 1 | Capital-scale mount |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Wing Hardpoint | 35 | 8 | 7 | 1 | `shells.json`'s existing `shell_wing_hardpoint`, unchanged |
| Turret Ring | 200 | 55 | 20 | 1 | Independently-targeting capital battery housing (§2.7) |

| Shell (Shield) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Shield Emitter Housing | 40 | 10 | 8 | 1 | `shells.json`'s existing `shell_shield_emitter`, unchanged |
| Array Housing | 85 | 22 | 13 | 1 | Capital-scale, feeds a Bubble/Conformal emitter (§3.1) |

| Shell (Facility) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Facility Bay | 80 | 20 | 14 | 1 | `shells.json`'s existing `shell_facility_bay`, unchanged |

| Shell (Sensor) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Sensor Mast | 30 | 8 | 6 | 1 | Thin-pool housing, matches Sensor's single-stat module design (§2.11) |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Drive Core Housing | 70 | 35 | 12 | 1 | Houses the Hyperdrive module and its fuel gate (§2.11) |

| Shell (Comms) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Comms Mast | 30 | 8 | 6 | 1 | Same thin-pool shape as Sensor |

| Shell (CargoBay) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Hold Frame | 60 | 15 | 12 | 1 | Houses a Cargo Bay module; capacity lives on the module, not the shell |

| Shell (Crew) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Crew Berth | 40 | 20 | 8 | 1 | Houses one `Crew` module — officer or complement role, whichever it rolled |

| Shell (FireControl) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Fire Control Housing | 35 | 10 | 6 | 1 | Small — it's a targeting computer, not a weapon |

#### Aegis Directorate — the template faction

*"Imposing, strictly symmetrical, military-grade geometric plating" (`lore.md` §3.1). Decision logic
prioritizes border fortification over offense. The Directorate's shells read that directly: heavier
hull for the same mass class as General, never the lightest or fastest option in any kind.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Sentinel Frame | 100 | 30 | 10 | 1 | Interceptor-scale, still hull-heavy for its size |
| Directorate Frame | 190 | 50 | 12 | 1 | Standard-issue hull — +27% hull over General's Standard Frame at the same mass |
| Fortress Frame | 1,100 | 240 | 62 | 2 | Cruiser/capital root; the heaviest Chassis in any faction's roster |

| Shell (Armor) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Directorate Plate | 75 | 16 | 6 | 1 | More hull than General's Plate Segment for the same mass |
| Bulkhead Segment | 145 | 32 | 7 | 1 | Signature piece — the densest armor-per-mass in the game |

| Shell (PowerCell) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Secured Power Bay | 65 | 16 | 8 | 1 | Armored housing around a standard cell |
| Secured Reactor Bay | 110 | 29 | 11 | 1 | Capital-scale, same hardening bias |

| Shell (Engine) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Drilled Thruster Mount | 55 | 13 | 9 | 1 | Slightly heavier than General — nothing on a Directorate hull is unarmored |
| Convoy Drive Bay | 160 | 42 | 18 | 1 | Capital-scale |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Standard-Issue Hardpoint | 45 | 9 | 7 | 1 | Baseline, armored above General's equivalent |
| Bastion Turret Ring | 230 | 58 | 20 | 1 | Faction signature — highest-hull turret ring in the roster |

| Shell (Shield) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Secured Emitter Housing | 50 | 11 | 8 | 1 | |
| Secured Array Housing | 100 | 24 | 13 | 1 | |

| Shell (Facility) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Directorate Facility Bay | 100 | 22 | 14 | 1 | Hardened — Aegis stations are built to take a hit and keep functioning |

| Shell (Sensor) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Watch Mast | 35 | 9 | 6 | 1 | "Security patrols" (`lore.md`) — nothing exotic, just armored |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Secured Drive Core | 85 | 37 | 12 | 1 | |

| Shell (Comms) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Command Mast | 35 | 9 | 6 | 1 | |

| Shell (CargoBay) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Secured Hold Frame | 75 | 16 | 12 | 1 | |

| Shell (Crew) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Barracks Berth | 55 | 21 | 8 | 1 | |

| Shell (FireControl) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Directorate Fire Control | 45 | 11 | 6 | 1 | |

**The pattern held across all nine remaining factions, completed 2026-08-11.** Every kind keeps
General's `radius` and `moduleSlots` (radius is collision/targeting footprint, not a place to encode
faction identity — §3.5 owns that axis, and only Voidwalkers breaks this rule, deliberately); only
`hull` and `mass` move, as a fixed percentage of General's baseline applied uniformly across every
kind, using the same archetype descriptors §2.11's Weapon Roster already established (so a faction
reads the same whether you're looking at its guns or its girders). Named variants and the signature
Chassis/Weapon rows carry real flavor text; the remaining rows are the same delta applied mechanically
— reviewed for correctness, not narrated twice.

#### Meridian Star Corps — cost-efficient, mass-produced, mercenary-adjacent

*Hull ×0.95, Mass ×0.80 vs. General. "Sleek, luxurious, aerodynamic curves cast from single molds"
(`lore.md`) reads as engineering efficiency, not toughness — Meridian hulls are never the strongest in
a fight, only the cheapest to field in numbers.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Courier Frame | 76 | 20 | 10 | 1 | Cheapest Chassis in any roster |
| Corporate Frame | 143 | 36 | 12 | 1 | The mass-produced standard — 20% lighter than General for 5% less hull |
| Flagship Frame | 855 | 176 | 60 | 2 | Still the lightest capital-scale Chassis of any faction |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Composite Plate | 57 | 12 | 6 | 1 |
| Reinforced Composite | 105 | 24 | 7 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Efficiency Bay | 48 | 12 | 8 | 1 |
| Efficiency Reactor | 86 | 22 | 11 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Cruise Thruster | 43 | 10 | 9 | 1 |
| Cruise Drive Bay | 133 | 32 | 18 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Standard Hardpoint | 33 | 6 | 7 | 1 | |
| Corporate Turret Ring | 190 | 44 | 20 | 1 | Signature — cheapest capital battery of any faction |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Standard Emitter | 38 | 8 | 8 | 1 |
| Standard Array | 81 | 18 | 13 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Corporate Facility Bay | 76 | 16 | 14 | 1 |

| Shell (Sensor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Market Mast | 29 | 6 | 6 | 1 |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Efficiency Drive Core | 67 | 28 | 12 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Broker Mast | 29 | 6 | 6 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Freight Frame | 57 | 12 | 12 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Crew Cabin | 38 | 16 | 8 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Standard Fire Control | 33 | 8 | 6 | 1 |

#### Kore Industries — repurposed mining tools, rugged, blunt

*Hull ×1.35, Mass ×1.45 vs. General. "Brutal, heavy industrial... exposed internal machinery" —
Kore trades efficiency for raw endurance harder than any faction except the Reapers, and unlike the
Reapers it's an honest tradeoff: heavier hull for genuinely heavier mass, nothing hidden.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Rig Frame | 108 | 36 | 10 | 1 | |
| Foundry Frame | 203 | 65 | 12 | 1 | |
| Bedrock Frame | 1,215 | 319 | 60 | 2 | Heaviest capital-scale Chassis outside the Reapers |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Salvage Plate | 81 | 22 | 6 | 1 |
| Hull Slab | 149 | 44 | 7 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Drill Power Bay | 68 | 22 | 8 | 1 |
| Drill Reactor Bay | 122 | 41 | 11 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Piston Thruster Mount | 61 | 17 | 9 | 1 |
| Piston Drive Bay | 189 | 58 | 18 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Repurposed Hardpoint | 47 | 12 | 7 | 1 | A mining laser mount pressed into gunnery |
| Foundry Turret Ring | 270 | 80 | 20 | 1 | Signature — heaviest turret ring of any faction |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Hardened Emitter Housing | 54 | 15 | 8 | 1 |
| Hardened Array Housing | 115 | 32 | 13 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Foundry Facility Bay | 108 | 29 | 14 | 1 |

| Shell (Sensor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Watch Rig | 41 | 12 | 6 | 1 |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Hauler Drive Core | 95 | 51 | 12 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Relay Rig | 41 | 12 | 6 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Bulk Hold Frame | 81 | 22 | 12 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Bunk Frame | 54 | 29 | 8 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Repurposed Fire Control | 47 | 15 | 6 | 1 |

#### The Forgotten — jury-rigged, salvaged, high-variance

*No fixed multiplier — the whole point is that there isn't one. Each kind gets one clearly** worse**
variant (Scrap, ~-20%/-20%) and one clearly **better** variant (Patchwork, ~+15%/+30% — heavier than
the gain, since jury-rigged means bulky, not efficient), matching the Weapon Roster's existing
"whatever scrap filled the recipe slots — unpredictable" bias. The Forgotten never build a Standard
tier; General's own numbers stand in for "what they'd build if they had the parts, which they don't."*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Scrap Frame | 64 | 20 | 10 | 1 | Worse than General's Light Frame in every stat |
| Cobbled Frame | 150 | 45 | 12 | 1 | Identical to General's Standard — "whatever they found that day" |
| Patchwork Frame | 1,035 | 286 | 60 | 2 | Better hull than General's Bulwark, at real mass cost |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Scrap Plate | 48 | 12 | 6 | 1 |
| Patchwork Segment | 127 | 39 | 7 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Scrap Power Bay | 40 | 12 | 8 | 1 |
| Patchwork Reactor | 104 | 36 | 11 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Scrap Thruster | 36 | 10 | 9 | 1 |
| Patchwork Drive Bay | 161 | 52 | 18 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Scrap Hardpoint | 28 | 6 | 7 | 1 | |
| Patchwork Turret Ring | 230 | 72 | 20 | 1 | Signature — bulkier than Kore's, less efficient, still lands |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Scrap Emitter | 32 | 8 | 8 | 1 |
| Patchwork Array | 98 | 29 | 13 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Patchwork Facility Bay | 92 | 26 | 14 | 1 | A working facility on a scrap fleet is notable — no Scrap-tier version |

| Shell (Sensor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Scrap Mast | 24 | 6 | 6 | 1 |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Patchwork Drive Core | 81 | 46 | 12 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Scrap Mast | 24 | 6 | 6 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Patchwork Hold Frame | 69 | 20 | 12 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Scrap Berth | 32 | 16 | 8 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Patchwork Fire Control | 40 | 13 | 6 | 1 |

#### AI Concordance — precise, optimized, post-biological

*Hull ×1.05, Mass ×0.75 vs. General — the best hull-per-mass ratio of any faction. "Biomorphic curves,
hyper-efficient" reads as engineering with nothing wasted, the inverse of Kore's honest bulk.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Node Frame | 84 | 19 | 10 | 1 | |
| Lattice Frame | 158 | 34 | 12 | 1 | Best hull-per-mass Chassis in the game |
| Array Frame | 945 | 165 | 60 | 2 | |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Optimized Plate | 63 | 11 | 6 | 1 |
| Optimized Segment | 116 | 23 | 7 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Efficient Power Node | 53 | 11 | 8 | 1 |
| Efficient Reactor Node | 95 | 21 | 11 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Calculated Thruster | 47 | 9 | 9 | 1 |
| Calculated Drive Node | 147 | 30 | 18 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Precision Hardpoint | 37 | 6 | 7 | 1 | |
| Array Turret Ring | 210 | 41 | 20 | 1 | Signature — lightest capital battery for its hull of any faction |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Optimized Emitter | 42 | 8 | 8 | 1 |
| Optimized Array | 89 | 17 | 13 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Node Facility Bay | 84 | 15 | 14 | 1 |

| Shell (Sensor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Analytical Mast | 32 | 6 | 6 | 1 |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Optimized Drive Core | 74 | 26 | 12 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Network Mast | 32 | 6 | 6 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Node Hold Frame | 63 | 11 | 12 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Housing Node | 42 | 15 | 8 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Calculated Fire Control | 37 | 8 | 6 | 1 |

#### Pyre Ascendancy — fire/plasma, aggressive, high-risk

*Hull ×0.70, Mass ×0.70 vs. General — the lightest, frailest shells of any faction. "High aggression,"
"high-risk" (`lore.md`, §2.11's Weapon Roster) is a glass-cannon identity carried into the hull itself,
not just the guns: Pyre wins by burning fast, not by tanking.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Zealot Frame | 56 | 18 | 10 | 1 | Frailest Chassis in the game at Light scale |
| Ascendant Frame | 105 | 32 | 12 | 1 | |
| Reliquary Frame | 630 | 154 | 60 | 2 | Lightest capital-scale Chassis of any faction |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Sacred Plate | 42 | 11 | 6 | 1 |
| Sacred Segment | 77 | 21 | 7 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Flame Power Bay | 35 | 11 | 8 | 1 |
| Flame Reactor Bay | 63 | 20 | 11 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Zealous Thruster | 32 | 8 | 9 | 1 |
| Crusade Drive Bay | 98 | 28 | 18 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Consecrated Hardpoint | 25 | 6 | 7 | 1 | Frailest weapon housing of any faction |
| Reliquary Turret Ring | 140 | 39 | 20 | 1 | Signature — trades survivability for the roster's highest burst damage (§2.11) |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Consecrated Emitter | 28 | 7 | 8 | 1 |
| Consecrated Array | 60 | 15 | 13 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Reliquary Facility Bay | 56 | 14 | 14 | 1 |

| Shell (Sensor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Vigil Mast | 21 | 6 | 6 | 1 |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Crusade Drive Core | 49 | 25 | 12 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Vigil Mast | 21 | 6 | 6 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Reliquary Hold Frame | 42 | 11 | 12 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Devotee Berth | 28 | 14 | 8 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Consecrated Fire Control | 25 | 7 | 6 | 1 |

#### Voidwalkers — exotic, anomaly-touched, cryptic

*Hull ×0.90, Mass ×0.85, **Radius ×0.75** vs. General — the one roster where radius moves, per §2.12's
own note above. "Segmented hulls... non-Euclidean geometric angles" (`lore.md`) is a shape claim: a
Voidwalker hardpoint occupies less space for its stats than anyone else's, not just less mass.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Rift Frame | 72 | 21 | 8 | 1 | Smallest-footprint Chassis in the game |
| Segmented Frame | 135 | 38 | 9 | 1 | |
| Anomaly Frame | 810 | 187 | 45 | 2 | Capital-scale, at a fighter-adjacent radius fraction |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Ribbon Plate | 54 | 13 | 5 | 1 |
| Ribbon Segment | 99 | 26 | 5 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Rift Power Bay | 45 | 13 | 6 | 1 |
| Rift Reactor Bay | 81 | 24 | 8 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Ribbon Thruster | 41 | 10 | 7 | 1 |
| Ribbon Drive Bay | 126 | 34 | 14 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Angular Hardpoint | 32 | 7 | 5 | 1 | Smallest weapon housing footprint of any faction |
| Anomaly Turret Ring | 180 | 47 | 15 | 1 | Signature — a capital battery with a corvette's radius |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Ethereal Emitter | 36 | 9 | 6 | 1 |
| Ethereal Array | 77 | 19 | 10 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Anomaly Facility Bay | 72 | 17 | 11 | 1 |

| Shell (Sensor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Distortion Mast | 27 | 7 | 5 | 1 |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Rift Drive Core | 63 | 30 | 9 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Echo Mast | 27 | 7 | 5 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Rift Hold Frame | 54 | 13 | 9 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Segmented Berth | 36 | 17 | 6 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Angular Fire Control | 32 | 9 | 5 | 1 |

#### Zenith Collective — precision instruments, sensor-integrated

*Hull ×0.90, Mass ×0.80 vs. General. "Smooth, reflective surfaces faceted like a cut gem" reads as
instrument-grade construction — light and precise like AI Concordance, but without Concordance's
hull premium; Zenith spends its efficiency on sensor/knowledge systems, not survivability.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Facet Frame | 72 | 20 | 10 | 1 | |
| Lattice Frame | 135 | 36 | 12 | 1 | |
| Archive Frame | 810 | 176 | 60 | 2 | |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Cut Plate | 54 | 12 | 6 | 1 |
| Cut Segment | 99 | 24 | 7 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Instrument Power Bay | 45 | 12 | 8 | 1 |
| Instrument Reactor Bay | 81 | 22 | 11 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Precision Thruster | 41 | 10 | 9 | 1 |
| Precision Drive Bay | 126 | 32 | 18 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Study Hardpoint | 32 | 6 | 7 | 1 | |
| Archive Turret Ring | 180 | 44 | 20 | 1 | Signature — shares its sensor suite's tech per §2.11's Weapon Roster |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Faceted Emitter | 36 | 8 | 8 | 1 |
| Faceted Array | 77 | 18 | 13 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Archive Facility Bay | 72 | 16 | 14 | 1 |

| Shell (Sensor) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Discovery Mast | 27 | 6 | 6 | 1 | The one kind Zenith would over-invest in if shells carried performance stats — they don't, so this stays General-shaped |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Instrument Drive Core | 63 | 28 | 12 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Catalogue Mast | 27 | 6 | 6 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Archive Hold Frame | 54 | 12 | 12 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Scholar Berth | 36 | 16 | 8 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Study Fire Control | 32 | 8 | 6 | 1 |

#### Edenian Pact — bio-integrated, defensive-leaning, corrosive

*Hull ×1.15, Mass ×0.90 vs. General — good hull for good mass, the opposite mechanism from Aegis's
brute armor. "Segmented carapace armor" (`lore.md`) is defense grown, not bolted on; Edenian shells
are the only roster besides AI Concordance with a hull premium that doesn't cost mass 1:1.*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Sprout Frame | 92 | 23 | 10 | 1 | |
| Carapace Frame | 173 | 41 | 12 | 1 | |
| Grove Frame | 1,035 | 198 | 60 | 2 | |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Carapace Plate | 69 | 14 | 6 | 1 |
| Carapace Segment | 127 | 27 | 7 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Symbiont Power Bay | 58 | 14 | 8 | 1 |
| Symbiont Reactor Bay | 104 | 25 | 11 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Living Thruster | 52 | 11 | 9 | 1 |
| Living Drive Bay | 161 | 36 | 18 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Bio Hardpoint | 40 | 7 | 7 | 1 | |
| Grove Turret Ring | 230 | 50 | 20 | 1 | Signature — high hull, moderate mass, matches "defensive-leaning" |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Living Emitter | 46 | 9 | 8 | 1 |
| Living Array | 98 | 20 | 13 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Grove Facility Bay | 92 | 18 | 14 | 1 |

| Shell (Sensor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Root Mast | 35 | 7 | 6 | 1 |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Symbiont Drive Core | 81 | 32 | 12 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Vine Mast | 35 | 7 | 6 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Carapace Hold Frame | 69 | 14 | 12 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Symbiont Berth | 46 | 18 | 8 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Bio Fire Control | 40 | 9 | 6 | 1 |

#### The Reapers — brutal, mass-destruction, entropic

*Hull ×1.40, Mass ×1.30 vs. General — the heaviest hull of any faction, heavier even than Kore, though
Kore still edges them on mass-for-mass toughness (Kore's hull/mass ratio is slightly better; the
Reapers are simply bigger everywhere). "Matte, skeletal bone-like surfaces" — overwhelming scale
rather than efficient engineering, matching a faction that "does not engage in standard diplomacy."*

| Shell (Chassis) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Bone Frame | 112 | 33 | 10 | 1 | |
| Ossuary Frame | 210 | 59 | 12 | 1 | |
| Charnel Frame | 1,260 | 286 | 60 | 2 | Highest raw hull of any capital-scale Chassis |

| Shell (Armor) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Bone Plate | 84 | 20 | 6 | 1 |
| Bone Segment | 154 | 39 | 7 | 1 |

| Shell (PowerCell) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Marrow Power Bay | 70 | 20 | 8 | 1 |
| Marrow Reactor Bay | 126 | 36 | 11 | 1 |

| Shell (Engine) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Sinew Thruster | 63 | 16 | 9 | 1 |
| Sinew Drive Bay | 196 | 52 | 18 | 1 |

| Shell (Weapon) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Predatory Hardpoint | 49 | 10 | 7 | 1 | |
| Charnel Turret Ring | 280 | 72 | 20 | 1 | Signature — highest-hull turret ring in the game |

| Shell (Shield) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Warding Emitter | 56 | 13 | 8 | 1 |
| Warding Array | 119 | 29 | 13 | 1 |

| Shell (Facility) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Charnel Facility Bay | 112 | 26 | 14 | 1 |

| Shell (Sensor) | Hull | Mass | Radius | Slots | Bias |
|---|---|---|---|---|---|
| Hunting Mast | 42 | 10 | 6 | 1 | Feeds §5.7's structural-density target selection, not a combat stat |

| Shell (Hyperdrive) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Sinew Drive Core | 98 | 46 | 12 | 1 |

| Shell (Comms) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Signal Mast | 42 | 10 | 6 | 1 |

| Shell (CargoBay) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Bone Hold Frame | 84 | 20 | 12 | 1 |

| Shell (Crew) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Thrall Berth | 56 | 26 | 8 | 1 |

| Shell (FireControl) | Hull | Mass | Radius | Slots |
|---|---|---|---|---|
| Predatory Fire Control | 49 | 13 | 6 | 1 |

**Shell Roster complete for all ten factions + General**, all following §2.12's structure exactly:
same `radius`/`moduleSlots` as General throughout (Voidwalkers excepted, by design), only `hull`/`mass`
carrying faction identity, at the percentage deltas named in each faction's intro above.

### 2.13 The Preset Ship Roster 📋

*Added 2026-08-11, built entirely from §2.11's modules and §2.12's shells — no new schema. These are
what `ships.json` grows into: `aegis_vanguard` and `aegis_outpost` already exist and become two of
Aegis's four below.*

**2–5 ships per faction, total — not per hull class.** A small signature fleet, the same shape
`ships.json` already has for Aegis and The Forgotten today, just completed and given the same
treatment across every faction + General.

**Every faction fills the same role-slots, scaled by hardpoint count and by when Facility-kind shells
enter the build:**

| Role slot | Hardpoints | Facility shells? | Chassis tier |
|---|---|---|---|
| **Starter fighter** | 4 – 7 | No | Light/Sentinel Frame |
| **Combatant** | 10 – 20 | No | Standard/Directorate Frame |
| **Capital or station** | 20 – 40 | **Yes** — Docking at minimum | Bulwark/Fortress Frame |

A faction's 2–5 ships pick which of these slots to fill (most factions fill all three plus one
signature wildcard, mirroring the Weapon Roster's 5th-slot pattern); nothing requires filling every
slot exactly once.

#### General — the faction-neutral baseline

| Ship | Role slot | Built from |
|---|---|---|
| Courier | Starter fighter | Light Frame + General Engine/Weapon/PowerCell/Shield shells and modules |
| Trader | Combatant | Standard Frame, CargoBay shell added, general modules throughout |
| Outpost | Capital/station | Bulwark Frame + Facility Bay (Docking), general modules |

#### Aegis Directorate — the template faction

| Ship | Role slot | Mounts (shell → module, from §2.12/§2.11) |
|---|---|---|
| **Vanguard** *(exists — `aegis_vanguard`)* | Starter fighter | Sentinel Frame core + Directorate Plate armor, Secured Power Bay + a power cell module, Drilled Thruster Mount + an engine module, 2× Standard-Issue Hardpoint + an Aegis Weapon Roster entry, Secured Emitter Housing + an Aegis Shield Roster entry |
| **Warden** *(new)* | Combatant | Directorate Frame core, 2× Directorate Plate, Secured Reactor Bay, Convoy Drive Bay, 4× Standard-Issue Hardpoint (mixed Aegis Weapon Roster picks), Secured Array Housing, Watch Mast (Sensor), Command Mast (Comms), 2× Barracks Berth (Crew) |
| **Bastion** *(new)* | Combatant (signature) | As Warden, but one Standard-Issue Hardpoint replaced by **Bastion Turret Ring** carrying the Aegis Weapon Roster's heaviest entry — the faction's showpiece gunship |
| **Outpost** *(exists — `aegis_outpost`)* | Capital/station | Fortress Frame core, Secured Reactor Bay, Directorate Facility Bay + a Docking module, N× Barracks Berth, Secured Hold Frame + a Cargo Bay module |

**Vanguard and Outpost already exist in `ships.json`** and need updating to reference the new
per-faction shells above (their current mounts use General's generic `shell_*` ids) rather than being
rebuilt from scratch. **Warden and Bastion are net-new content.**

#### Meridian Star Corps

| Ship | Role slot | Mounts |
|---|---|---|
| **Runner** | Starter fighter | Courier Frame core + Composite Plate, Efficiency Bay + power cell, Cruise Thruster + engine, 2× Standard Hardpoint + Ledger-Line Cannon, Standard Emitter + shield |
| **Contractor** | Combatant | Corporate Frame core, 2× Composite Plate, Efficiency Reactor, Cruise Drive Bay, 4× Standard Hardpoint (mixed picks incl. Contractor Flak Battery), Standard Array, Market Mast, Broker Mast, 2× Crew Cabin |
| **Liquidator** | Combatant (signature) | As Contractor, one Standard Hardpoint replaced by **Corporate Turret Ring** carrying **Liquidator Beam** — the client-tier flagship weapon |
| **Depot** | Capital/station | Flagship Frame core, Efficiency Reactor, Corporate Facility Bay + Trade module, N× Crew Cabin, Freight Frame ×2 + Cargo Bay modules — the largest cargo footprint of any faction's station |

#### Kore Industries

| Ship | Role slot | Mounts |
|---|---|---|
| **Drillhand** | Starter fighter | Rig Frame core + Salvage Plate, Drill Power Bay + power cell, Piston Thruster Mount + engine, 2× Repurposed Hardpoint + Drillhead Mass Driver, Hardened Emitter Housing + shield |
| **Foreman** | Combatant | Foundry Frame core, 2× Salvage Plate, Drill Reactor Bay, Piston Drive Bay, 4× Repurposed Hardpoint (mixed picks incl. Foreman's Scattergun), Hardened Array Housing, Watch Rig, Relay Rig, 2× Bunk Frame |
| **Bedrock Hauler** | Combatant (signature) | As Foreman, one Repurposed Hardpoint replaced by **Foundry Turret Ring** carrying **Blast-Charge Launcher** — heaviest turret ring of any faction on its heaviest chassis |
| **Refinery** | Capital/station | Bedrock Frame core, Drill Reactor Bay, Foundry Facility Bay + Manufacturing module, N× Bunk Frame, Bulk Hold Frame ×2 + Cargo Bay modules |

#### The Forgotten

| Ship | Role slot | Mounts |
|---|---|---|
| **Scrapper** *(exists — `forgotten_scrapper`)* | Starter fighter | Scrap/Cobbled Frame mix + Scrap Plate, Scrap Power Bay + power cell, Scrap Thruster + engine, 1× Scrap Hardpoint + Scrapgun |
| **Cobble-Wing** *(new)* | Combatant | Cobbled Frame core, mixed Scrap/Patchwork Armor, Patchwork Reactor, Patchwork Drive Bay, 4× mixed Weapon shells (Patchwork Ripper, Jury-Rig Laser, Overcharged Splicer picks), Patchwork Array, Scrap Mast ×2 (Sensor+Comms), 2× Scrap Berth |
| **Salvage King** *(new, signature)* | Combatant (signature) | As Cobble-Wing, one weapon shell replaced by **Patchwork Turret Ring** carrying **Overcharged Splicer** — the faction's rare "everything went right" build |
| **Warren** *(new)* | Capital/station | Patchwork Frame core, Patchwork Reactor, Patchwork Facility Bay + Docking module (the one facility they always get right), N× Scrap Berth, Patchwork Hold Frame + Cargo Bay module |

*`forgotten_scrapper` already exists and needs updating to reference the new Forgotten shells above.*

#### AI Concordance

| Ship | Role slot | Mounts |
|---|---|---|
| **Node Unit** | Starter fighter | Node Frame core + Optimized Plate, Efficient Power Node + power cell, Calculated Thruster + engine, 2× Precision Hardpoint + Calculus Autocannon, Optimized Emitter + shield |
| **Lattice Unit** | Combatant | Lattice Frame core, 2× Optimized Plate, Efficient Reactor Node, Calculated Drive Node, 4× Precision Hardpoint (mixed picks incl. Precision Beam Array), Optimized Array, Analytical Mast, Network Mast, 2× Housing Node |
| **Recursion Unit** | Combatant (signature) | As Lattice Unit, one Precision Hardpoint replaced by **Array Turret Ring** carrying **Entropy-Minimizing Lance** — the faction's highest power-cost signature |
| **Archive Node** | Capital/station | Array Frame core, Efficient Reactor Node, Node Facility Bay + Research module, N× Housing Node, Node Hold Frame + Cargo Bay module |

#### Pyre Ascendancy

| Ship | Role slot | Mounts |
|---|---|---|
| **Cinder** | Starter fighter | Zealot Frame core + Sacred Plate, Flame Power Bay + power cell, Zealous Thruster + engine, 2× Consecrated Hardpoint + Cinder Cannon, Consecrated Emitter + shield |
| **Ascendant** | Combatant | Ascendant Frame core, 2× Sacred Plate, Flame Reactor Bay, Crusade Drive Bay, 4× Consecrated Hardpoint (mixed picks incl. Purification Beam), Consecrated Array, Vigil Mast ×2 (Sensor+Comms), 2× Devotee Berth |
| **Reliquary Blade** | Combatant (signature) | As Ascendant, one Consecrated Hardpoint replaced by **Reliquary Turret Ring** carrying **Sacred Flame Lance** — highest burst damage of any faction's Energy weapon (§2.11), on the frailest capital-scale hull |
| **Sanctum** | Capital/station | Reliquary Frame core, Flame Reactor Bay, Reliquary Facility Bay + Research module (doctrine study), N× Devotee Berth, Reliquary Hold Frame + Cargo Bay module |

#### Voidwalkers

| Ship | Role slot | Mounts |
|---|---|---|
| **Ribbon** | Starter fighter | Rift Frame core + Ribbon Plate, Rift Power Bay + power cell, Ribbon Thruster + engine, 2× Angular Hardpoint + Rift-Edge Driver, Ethereal Emitter + shield |
| **Segment** | Combatant | Segmented Frame core, 2× Ribbon Plate, Rift Reactor Bay, Ribbon Drive Bay, 4× Angular Hardpoint (mixed picks incl. Umbral Beam), Ethereal Array, Distortion Mast, Echo Mast, 2× Segmented Berth |
| **Silent Rift** | Combatant (signature) | As Segment, one Angular Hardpoint replaced by **Anomaly Turret Ring** carrying **Silence Caster** — a capital battery with a corvette's radius (§2.12) |
| **Fleet-City Node** | Capital/station | Anomaly Frame core, Rift Reactor Bay, Anomaly Facility Bay + Docking module, N× Segmented Berth, Rift Hold Frame + Cargo Bay module — the smallest-footprint capital-scale hull of any faction |

#### Zenith Collective

| Ship | Role slot | Mounts |
|---|---|---|
| **Facet** | Starter fighter | Facet Frame core + Cut Plate, Instrument Power Bay + power cell, Precision Thruster + engine, 2× Study Hardpoint + Archivist Autocannon, Faceted Emitter + shield |
| **Lattice Scholar** | Combatant | Lattice Frame core, 2× Cut Plate, Instrument Reactor Bay, Precision Drive Bay, 4× Study Hardpoint (mixed picks incl. Cataloguing Beam Array), Faceted Array, Discovery Mast, Catalogue Mast, 2× Scholar Berth |
| **Discovery** | Combatant (signature) | As Lattice Scholar, one Study Hardpoint replaced by **Archive Turret Ring** carrying **Discovery Lance** — tuned for prolonged engagement over a specimen (§2.11) |
| **Archive** | Capital/station | Archive Frame core, Instrument Reactor Bay, Archive Facility Bay + Research module, N× Scholar Berth, Archive Hold Frame + Cargo Bay module |

#### Edenian Pact

| Ship | Role slot | Mounts |
|---|---|---|
| **Sprout** | Starter fighter | Sprout Frame core + Carapace Plate, Symbiont Power Bay + power cell, Living Thruster + engine, 2× Bio Hardpoint + Thornbranch Driver, Living Emitter + shield |
| **Carapace** | Combatant | Carapace Frame core, 2× Carapace Plate, Symbiont Reactor Bay, Living Drive Bay, 4× Bio Hardpoint (mixed picks incl. Bloomspore Caster), Living Array, Root Mast, Vine Mast, 2× Symbiont Berth |
| **Bloomguard** | Combatant (signature) | As Carapace, one Bio Hardpoint replaced by **Grove Turret Ring** carrying **Blight Lance** — "protecting the garden" (§2.11) |
| **Grove** | Capital/station | Grove Frame core, Symbiont Reactor Bay, Grove Facility Bay + Engineering module (the Pact repairs by growing, not welding), N× Symbiont Berth, Carapace Hold Frame + Cargo Bay module |

#### The Reapers

| Ship | Role slot | Mounts |
|---|---|---|
| **Bone Hunter** | Starter fighter | Bone Frame core + Bone Plate, Marrow Power Bay + power cell, Sinew Thruster + engine, 2× Predatory Hardpoint + Harvest Driver, Warding Emitter + shield |
| **Ossuary** | Combatant | Ossuary Frame core, 2× Bone Plate, Marrow Reactor Bay, Sinew Drive Bay, 4× Predatory Hardpoint (mixed picks incl. Unmaking Beam), Warding Array, Hunting Mast, Signal Mast, 2× Thrall Berth |
| **Charnel Reaper** | Combatant (signature) | As Ossuary, one Predatory Hardpoint replaced by **Charnel Turret Ring** carrying **Entropy Lance** — highest-hull turret ring in the game (§2.12), matching the faction's "structural density" targeting (§5.7) |
| **Charnel Hive** | Capital/station | Charnel Frame core, Marrow Reactor Bay, Charnel Facility Bay (no Docking — the Reapers do not host guests), N× Thrall Berth, Bone Hold Frame + Cargo Bay module |

*Reaper ships are NPC-spawned only (`NpcFactory`/`FactionDecisionEngine`) — §5.7's universal hostility
means there is no diplomatic path to ever dock at or buy from one, so nothing above assumes a player
build path the way the other nine factions' rosters do.*

**Preset Ship Roster complete for all ten factions + General** — 43 ships total (4 × 10 factions + 3
General), following §2.13's shared role-slot structure. `Vanguard`/`Outpost` (Aegis) and
`forgotten_scrapper` (The Forgotten) are the only three that already exist in `ships.json`, and all
three need their mounts updated to the new per-faction shells rather than General's generic ones. The
other 40 are net-new content.

---

## 3. Combat & Localized Damage 📋

Tactical, surgical, and rewarding of both loadout planning and precise execution.

### 3.1 Shield Dynamics & Weapon Typing

Shields are not a universal blanket. They interact with damage *type*:

- **Absorption** — a shield absorbs incoming fire whose type it matches, depleting progressively.
- **Bypass** — mismatched weapon types pass *through* an active shield and inflict immediate
  localized damage on the hardpoints and hull beneath.
- **Recharge & vulnerability** — shields regenerate over time, *unless* the shield generator
  hardpoint itself is destroyed, which permanently disables restoration for that rig.

This is the strongest mechanic in the design, because it makes the interesting decision happen
*before* the fight rather than during it. Reading an enemy's shield type and bringing the mismatched
weapon is a real tactical choice; "more DPS" is not.

#### The roster: two shield types, three weapon types 📋

*Settled 2026-08-08, and the reasoning runs opposite to intuition.*

**Adding shield types makes shields weaker, not richer.** Under the bypass rule above, an attacker
wants a type the defender does *not* have. With two types a random weapon bypasses 50% of the time;
with four, **75%**. A fighter carrying one shield generator would be bypassed three times in four,
and the whole mechanic would quietly become capitals-only, since only they can fit enough generators
to cover a wide spread.

So the two axes are separated:

| Axis | Roster | Rationale |
|---|---|---|
| **Shield-matching type** | **Kinetic · Energy** — two, and it stays two | This is the size of the pre-fight decision. Small keeps shields meaningful at fighter scale |
| **Weapon behaviour** | Unbounded — `penetration` (§3.5), damage-over-time, disable, splash, tracking | Authored stats on `ModuleDef`. No interaction with the shield matrix at all |

Roster variety therefore costs nothing, because it lives on the second axis. **Weapon types are
Kinetic, Energy, and Ion** — three weapons, two shields.

#### Ion — the third weapon type, and the only one that attacks power 📋

*Settled 2026-08-08.*

> **Ion is absorbed by *every* shield type — it never bypasses — and strips shields quickly. Once
> through, it deals no hull damage at all. It suppresses the target's power generation for a
> duration.**

**It is the only weapon that interacts with §2.9.** Every other weapon reduces `Health`; Ion reduces
the power budget, and `PowerSystem`'s existing priority shed does the rest — facilities drop, then
shields, then engines, then weapons, visibly and in a readable order.

**It needs no new machinery.** `PowerSystem` already keeps a shed path for the case where
*generation* drops (a dead power cell), deliberately separate from allocation overcommit. **An ion
hit is exactly a generation drop**, so Ion writes a temporary reduction to `PowerSource` output and
every downstream effect already exists.

**It is the disable weapon §3.2 wants and does not have.** Ion the shields down, kill the cockpit,
take the hull intact — capture (§3.2's uncrewed hull) becomes a plan rather than an accident.

**Never bypassing is what keeps it fair.** With only two shield types, a weapon that bypassed half
the time *and* suppressed power would be strictly dominant. Being absorbed by everything is the
price of its unique effect — and it gives Ion a clean role: it is terrible alone, since an ion-only
vessel kills nothing, and excellent in a mixed group. That pushes varied loadouts and gives §4.3's
squad orders something to coordinate.

⚠️ **Correction, 2026-08-10 (`architecture.md` §15.1 finding 5): "needs no new machinery" is only
half true.** The power-drain half is correct as written — an Ion hit really is exactly a generation
drop, and `PowerSystem`'s existing shed path handles everything downstream. But **"absorbed by every
shield type, never bypasses" has nothing to implement it.** `Shield::absorbs` holds one
`DamageType` — `Kinetic` or `Energy` — and `DamageSystem` treats anything that doesn't match as a
straight bypass. Ion is neither Kinetic nor Energy, so under that rule it would **always** bypass
every shield — the exact opposite of this section's rule. This needed one small addition, specified
below, that does not touch the two-shield-types decision above.

#### The damage-type effect table — how Ion (and anything after it) plugs in without new branches 📋

*Settled 2026-08-10, in response to `architecture.md` §15.1 findings 1 and 5. The goal stated
alongside the request: future weapon types should cost "one enum value and one content row," not a
new `if` in `DamageSystem` — modular, scalable, readable, matching Law 10's content-pipeline
principle applied to damage rules instead of just to stats.*

**This does not reopen "shield-matching types stay at two."** That decision (above) is about how
many types a *shield* can be built to counter, and the balance reasoning for keeping it at two is
unchanged. What follows is about how a *damage type* behaves when it either matches or doesn't —
today that behavior is one hardcoded rule (absorb-if-match, full bypass otherwise) with no way for a
type to say "I'm different." Ion is the first type that needs to say that; it will not be the last.

> **Every `DamageType` has a small, authored effect profile. `Kinetic` and `Energy` both take the
> default profile — absorb-if-matching-shield, full damage otherwise, exactly today's rule. A type
> that needs to behave differently — Ion today — authors an override instead of `DamageSystem`
> growing a branch for it.**

| Field | Default (Kinetic, Energy) | Ion's row |
|---|---|---|
| `alwaysAbsorbedByAnyShield` | false | **true** — never bypasses, regardless of `Shield::absorbs` |
| `bypassStillDrainsShieldCharge` | — (n/a, only meaningful with the above) | **true** — "strips shields quickly" |
| `hullDamageFraction` | 1.0 | **0.0** — deals no hull damage at all once through |
| `powerDrainFraction` | 0.0 | the hit's full value, redirected to `PowerSource` per the mechanism above |

`DamageSystem`'s per-hit path becomes one generic lookup instead of type-named branches: read the
incoming `PendingDamage::type`'s profile, decide absorbed-or-not (matched shield, or
`alwaysAbsorbedByAnyShield`), then split the surviving amount by `hullDamageFraction` /
`powerDrainFraction` — hull damage applies exactly as today, power drain writes into
`PowerSource` exactly as the mechanism above already specifies. **Adding a fourth weapon type that
needs its own special interaction is one more row in this table, not a new code path** — and a
type that behaves like Kinetic or Energy (which most future "weapon behaviour" variety, per the
roster section above, will) needs no row at all.

Ramming (§3.7) needs no row either — it is plain `Kinetic`, and gets the default profile for free.

#### Coverage: Personal, Bubble, Conformal 📋

*Settled 2026-08-08, and it fixes a defect. Verified in code: `IsMountable` permits a shield module
only in a `ShellKind::Shield` housing, and `DamageSystem::ApplyToHealthAndShield` looks up the
`Shield` component **on the hardpoint being damaged**. So a shield generator protects exactly one
hardpoint — the housing it sits in. **Shields currently protect shields and nothing else.***

That leaves §3.1's headline mechanic with no effect on anything a player would notice, gives §3.2's
"stripping or bypassing shields lets fire strike a specific hardpoint" nothing to strip, and leaves
`PowerSystem` with **zero shield references**, so §2.9's shields power category gates nothing.

⚠️ **It is not that shields deplete and then hardpoints become vulnerable — most hardpoints never had
a shield in front of them at all.** A fighter carrying a 500-capacity Kinetic generator:

| Shot | Intended | Actual |
|---|---|---|
| Kinetic → wing gun | Absorbed by the 500 pool | **Full damage.** Shield untouched |
| Kinetic → chassis | Absorbed by the 500 pool | **Full damage.** Shield untouched |
| Kinetic → the shield housing | Absorbed | Absorbed ✅ |

So the generator is functionally **500 extra hull on one hardpoint**, and it is the least important
hardpoint on the vessel. Bringing the *matching* weapon works perfectly well against everything that
matters, which is why §3.1's pre-fight decision currently buys nothing.

**The correct reading is that the code implements one of three modes and the other two do not exist:**

| Mode | Covers | Centred on | Radius |
|---|---|---|---|
| **Personal** | Its own hardpoint only | — | — |
| **Bubble** | Hardpoints within a radius | **The mount** | **Authored** |
| **Conformal** | **Every hardpoint on the rig** | The rig | **None — follows the hull** |

`coverage` is an **identity attribute** (§2.7) — authored per module, never rolled. Conformal is the
premium mode and should be priced and gated as such.

**Implementation is cheaper than it sounds.** Conformal is a *set-membership* question — "is this
hardpoint on the same rig?" — not a geometry one; only its *rendering* is conformal. Bubble is one
distance check. Personal is what exists today.

**A hull's size decides which mode suits it, with no per-class rule.** A conformal field on a 50-unit
fighter is trivial; on a 2,500-unit dreadnought it is enormous and expensive, so capitals distribute
Bubble generators and **choose what to protect** — batteries or engines, bow or stern. Fighters and
capitals defend differently because of what fits, not because anything says "capital" (Law 4).

**Destroying a generator therefore opens a hole in a specific part of the hull**, which is §3.2's
localized damage finally applied to defence rather than only to offence.

#### Shields render on the overlay layer 📋

*Settled 2026-08-08.* §3.5's draw layer 5 already lists "shield shimmer." Drawing the field there,
**semitransparent**, does three jobs at once:

- **Coverage gaps become visible.** You can see which part of an enemy capital is unshielded and aim
  there — §3.2's precision aiming gains a second readable dimension.
- **Damage type becomes readable from the cockpit** if the tint differs by type. §3.1's whole pitch is
  "read the enemy and bring the mismatched weapon," and today there is no way to read it without a
  stats panel. A colour on the hull is the diegetic version.
- **Depletion is legible** if opacity tracks the pool — you see a shield failing before it fails.

Conformal fields draw as an outline offset from the hull silhouette; Bubble fields draw as a dome of
their radius; Personal fields hug their own hardpoint.

#### Recharge archetypes 📋

*Settled 2026-08-08. Both behaviours fall out of fields that already exist — no new mechanics.*

`DamageSystem` sets `rechargeCooldown = rechargeDelaySeconds` on **every** absorbed hit, so the delay
decides whether recharge happens at all:

| Archetype | delay | rate | capacity | Strong against | Weak against |
|---|---|---|---|---|---|
| **Regenerative** | 0 | low | low | Chip damage, long attrition | Alpha strikes |
| **Capacitor** | long | high | high | Alpha strikes, burst trades | Sustained fire |

Two genuinely different defensive philosophies out of three numbers already in the schema.

**This also retires a concern raised the same day** — that on a capital under multi-source fire the
delay never expires and `rechargePerSecond` becomes dead content. That is not a bug, it is **what a
Capacitor shield is**, and a hull expecting sustained fire should be running Regenerative. The stat is
regime-specific, and choosing the regime is the decision.

*A third archetype would need new behaviour:* a **Burst** shield refilling in one lump after the
delay rather than per second. Distinct feel, but a new recharge mode rather than a new number.

#### The shield stat pool 📋

| Stat | Dir | Weight | Notes |
|---|:---:|---:|---|
| `capacity` | ↑ | **2.0** | The only stat that always pays. **No overkill applies** — a shield is a pool, not a hardpoint — so it is cleanly linear, unlike weapon damage |
| `coverageRadius` | ↑ | **1.8** | Bubble only. Decides how many hardpoints benefit at all; somewhat threshold-like |
| `rechargeDelaySeconds` | ↓ | **1.6** | The archetype dial. Gates whether recharge exists, which is why it outweighs the rate |
| `boostMultiplier` | ↑ | **1.5** | §2.9's power-level effect — **required, not optional**; see below |
| `rechargePerSecond` | ↑ | **1.4** | Linear, but only in the regime where recharge runs |
| `bleedThrough` | ↓ | **1.2** | Fraction passing to the hull even on a match |
| `ionResistance` | — | **grade** | Reduces Ion's strip rate. **Moved off the pool 2026-08-08** — seven entries broke §2.7's 4–6 legibility budget, and this reads as build quality rather than a design choice. It scales with grade like `mass` and `powerDraw` (§2.11) |
| `absorbs`, `coverage`, polarizing | — | **excluded** | Identity (§2.7) |

Bubble generators reach six rollable entries and Conformal five — both inside §2.7's 4–6 legibility
band, once `ionResistance` moves to the grade ladder and `powerDraw` stays off every pool (§2.11).

**`boostMultiplier` is not a new idea, it is a missing one.** §2.9 already states that "the draw and
effect multipliers at each level are module attributes," shields are one of its four categories, and
`PowerSystem` contains no shield references at all. This is the piece that lets shields participate in
power allocation. A large multiplier is a panic button; a small one is steady.

**`bleedThrough` and `ionResistance` are both one line in `DamageSystem`.** Bleed-through gives "hard"
versus "soft" shields and suits a design whose §3.4 insists nothing is ever actually safe. Ion
resistance is the counterplay to a weapon deliberately made strong (§3.1) — absorbed by everything,
strips fast, suppresses power.

#### Reflect, and why it works here specifically 📋

**A reflected shot is a real projectile travelling back down its own firing line**, not abstract
damage-back. This design suits it unusually well: projectiles are physical entities that damage
whatever they pass through (§3.2), so a reflection can hit the shooter, the shooter's wingman, or a
neutral in the way — and it rewards attacking from angles a reflection will not return along.

It is also **cheap**: `ProjectileSystem` already owns the entity, so reflection is flipping its
velocity and reassigning its source rather than spawning anything.

⚠️ **Keep the fraction small and the property high-grade only.** A high reflect fraction against a
high-fire-rate weapon reads as unfair rather than clever.

#### Polarizing shields — very advanced 📋

A shield that **retunes which damage type it absorbs at runtime**: the defensive twin of §2.9's power
reallocation. You read the incoming type and adapt, at a moment of vulnerability — the switch empties
the pool or drops it offline.

**Deliberately gated as advanced**, present only on high-grade defs. `DamageSystem` already reads
`absorbs`, so mechanically it is an intent plus a cooldown; the reason to restrict it is balance, not
cost.

#### Shields are projectile-only, and that gives ramming an identity 📋

*Settled 2026-08-08.* A shield stops projectiles. **It does not stop hulls.**

> **Ramming bypasses shields entirely.**

That is the payoff: mass and momentum matter beyond flavour, and ramming becomes *the* anti-shield
tactic, available to anyone willing to trade hull for it (§3.7).

**Standoff distance is dropped.** It is only meaningful under a physical-barrier model, and that model
would hand every shielded vessel free anti-ram defence — quietly neutering a mechanic that already
works — while also entangling `CollisionSystem`, docking, and friend/foe rules. **Physical shields are
logged as a deliberate future feature**, with a real anti-ram identity, rather than arriving as a stat.


### 3.2 Localized Hardpoint Destruction

Ships and stations are physical collections of hardpoints — **each one its own entity** — not a
single health bar.

**Targeted systems** — stripping or bypassing shields lets fire strike a specific hardpoint.
Destroy a thruster shell and the vessel *slows* — propulsion is the sum of its living engines
(§2.11), so it stalls only when the last one dies. Destroy a weapon battery and that firing arc is gone
permanently.

**Functional degradation** — capitals and stations lose capabilities dynamically as hardpoints are
blown apart: repair bays stop healing, manufacturing bays stop building, shield generators stop
regenerating.

**Uniformity is the point.** A fighter wing, a station battery, and a capital's dorsal turret are
the same kind of thing to the damage system. There is no per-vessel-type special case — see
`architecture.md` Law 4 for why this is stated so emphatically.

**Fighters are included, not exempted** (decided 2026-08-07). A fighter takes localized damage by
component location exactly as a capital or station does. Localized damage is not a capital-scale
feature that fighters approximate with a hull bar — if a hull is too small for its hardpoints to be
individually hit, the hull's scale is wrong, not the mechanic. See §3.5.

#### Who aims, and at what 📋

*Decided 2026-08-07. This section previously said only that fire "strikes a specific hardpoint"
without saying who chooses it, and the codebase resolved that ambiguity by choosing for everybody.*

**The player aims manually. There is no target lock.** The player's cursor is the aim point;
turrets slew toward it within their own firing arcs and fire when they bear. Projectiles are
physical — they damage whatever they actually pass through, including a hull the player never
"selected" and including friendly or neutral vessels. **The player is never prevented from shooting
at something**, and never restricted to one designated enemy.

The consequence, and it is intended: *which hardpoint you destroy is a question of marksmanship.*
§3.1's shield-type decision is made before the fight; this is the decision made during it. A
subtarget-cycling UI would return the same information at no risk and is explicitly rejected.

**NPCs aim at a random living hardpoint** of the rig they are engaging, re-rolled only when that
hardpoint dies. Random is the baseline, not the ceiling:

> **Targeting priority is a function of the pilot's or commander's skill — never of their faction
> archetype or role.** A veteran pilot works an enemy's engines; a conscript sprays. The archetype
> weighting in §6.2 governs what a *faction* decides to do strategically; it must never reach down
> into which hardpoint an individual gun is pointed at.

This is consistent with §6.3 rather than an exception to it: §6.3 permits difficulty to be
expressed through "loadout quality, numbers, and **tactical decisions**," and target selection is a
tactical decision. It is not a hidden multiplier, and the player is subject to the same physics —
an unskilled player also sprays.

#### The cockpit is its own shell, at every scale 📋

*Settled 2026-08-07, after first landing the other way. An earlier draft let §3.5's separation
minimum decide per hull — integrated cockpit on fighters, separate bridge on capitals. **Uniformity
won instead:** a fighter's cockpit and a capital's bridge are the same kind of thing and are authored
the same way, per Law 4.*

**Every crewed hull has a discrete crew shell.** Cockpit on a fighter, bridge on a capital or
station. It is aimable, destructible, and separately mounted at every scale — no hull integrates its
crew into the chassis.

> ⚠️ **This forced the scale decision (§3.5), it did not sit beside it.** The old 36-unit fighter
> could not host a chassis *and* a discrete cockpit at any workable separation minimum. Choosing
> uniformity here is why **fighters grew to 50 units** — §3.5 now settles that, and every existing
> blueprint is re-authored against it.

The gain is worth the cost: "shoot the pilot" becomes a real shot on a fighter rather than a
capital-only tactic, disabling stays distinct from killing at every scale, and there is no
per-vessel-type branch anywhere in the content or the code.

#### The uncrewed hull 📋

> **A rig whose crew module is destroyed is not a wreck. It is an intact, powerless, un-flown ship.**

This is the payoff of crew-modules-plus-localized-damage (§2.7), and it falls out of mechanics
already decided rather than needing new ones:

- The hull stops steering and stops firing — nothing is left to issue `ThrustInput` or decide to
  shoot. It keeps its velocity and drifts.
- It remains a physical, collidable, targetable object. It can still be shot apart.
- **It can be captured.** An intact hull with a dead crew is the most valuable thing on a
  battlefield, and taking it is a deliberate act — disable rather than destroy, then claim it.
- It gives The Forgotten (*Opportunistic Survival*, §6.2) something specific to circle, alongside
  the death wrecks §3.3 Tier 2 already hands them.

**Disabling is now tactically distinct from killing**, which is the real gain: "shoot the cockpit" and
"shoot everything" stop being the same plan, and §3.2's promise that precision is rewarded acquires a
second, larger payoff beyond disabling a firing arc.

❓ *Open: how capture actually works — fly-to-and-hold, a boarding action, a module installed on the
capturing ship? And whether an AI faction can capture the player's uncrewed hull the same way (§6.3
says it should).*

❓ *Open: whether a player-piloted vessel losing its crew shell is fatal to the player.* §3.3 says the
player dies with the vessel they pilot — but an uncrewed hull is by definition not destroyed. The
consistent reading is that the player **is** the crew of the ship they are flying, so losing that
shell is a Tier 1 death and the hull is left for someone else to take. That wants confirming.

#### Structural integrity — a ship dies before its last hardpoint does 📋

*Settled 2026-08-09. **This revises the destruction rule in built code.** `DamageSystem` currently
destroys a rig only when `HasLivingHardpoint` is false — every hardpoint must reach zero.*

**Why that fails.** Killing `aegis_vanguard` means zeroing **six separate pools** (325 hull), and the
last is typically a **35-hull wing** you must hunt down on a ship that is visibly wreckage. A capital
under §3.5 carries up to **fifty** hardpoints; requiring all fifty is not a mechanic, it is a chore.
And it is simply not how ships die.

> **Structural integrity is the sum of living hardpoint health over total hardpoint health. Below a
> threshold the hull gives way: every surviving hardpoint is destroyed and the rig with them.**

**Derived, never stored.** Nothing debits an integrity pool; it is recomputed from what the
hardpoints hold. `CockpitHud::AggregateHullFraction` already computes exactly this for the hull bar,
so this gives an existing function a second reader rather than adding a system. A *stored* pool would
become a second competing truth and hardpoints would drift toward decoration — which is the
single-health-bar model this section opens by rejecting.

**This is not the protected core §3.2 rejects.** That rejection was of an *artificial weak point you
must kill*. A threshold is the opposite: no single point, and no disassembly either.

##### One hit lands on one hardpoint

> **A projectile resolves to the *most specific* hardpoint whose shape it crosses. Damage is never
> shared, split, or double-counted.**

Hitting a wing does not touch the chassis. Hitting the chassis does not touch anything else. Only the
**aggregate** moves, because it is a sum rather than a pool.

**The chassis is the structural backstop.** `ShellKind::Chassis` is already *"the rig's root"* and
already the largest hull value on every authored ship. Most-specific-wins means a shot at a turret
hits the turret, and a shot at bare plating hits the structure actually beneath it — at the place it
was aimed, with no fallback to a "nearest hardpoint" that might be hundreds of units away on a
capital.

⚠️ This also replaces `ProjectileSystem::FindHit`'s current **first-in-iteration-order** hit
selection, which `architecture.md` §9.1 and §12.16 both already list as requiring a nearest-hit
tie-break. Most-specific-wins is a better rule than nearest and closes the same defect.

##### Structural coverage must be complete

> **The union of structural hardpoints — chassis plus armour — must cover the hull envelope. No bare
> sections, enforced by `Validation` rather than patched at runtime.**

`ShellKind::Armor` is already defined as *"pure hull, no function"* — that is a **hull segment**. It
tiles the envelope, and functional mounts attach to segments rather than all hanging off the chassis:

```
chassis  (spine — kill it and everything goes)
├── armour: port flank ── turret, engine
├── armour: starboard flank ── turret
└── armour: dorsal ── sensor array
```

Blow open the port flank and the port turret and engine go with it **while the starboard side keeps
fighting** — §12.22's structural cascade doing real work. The model scales without a per-class rule:
on a fighter the chassis *is* most of the hull (today's `shell_fighter_chassis` is r22 under a r27
envelope); on a capital the chassis is the spine and armour carries the surface.

##### The threshold, and how it reads

**~30% integrity remaining**, tunable in the 25–35% band and — like the quantity curve (§2.10) — a
value awaiting `tools/economy_sim` rather than a settled one.

Two death paths, and **both end at a genuine raw zero**:

| Path | Damage on `aegis_vanguard` | What happens |
|---|---:|---|
| **Attrition** | 227.5 spread | Integrity hits 30% → **structural failure** destroys every survivor → raw 0 |
| **Structural kill** | 120 into the chassis | Chassis dies → cascade destroys its children → raw 0 |

**The displayed bar is normalised so it reaches zero at death**: `shown = (raw − threshold) / (1 −
threshold)`. Raw 100% shows 100%; raw 30% shows 0%. No ship ever explodes at "30% health", and no
corpse is left holding live hardpoints.

##### Shooting the middle is the fastest kill, and that is correct

Chassis-focus kills `aegis_vanguard` in **120 damage** against **227.5** for attrition — 1.9× faster.
`architecture.md` §12.22 treats this as a hazard to be tuned away, requiring *"chassis hull must
dominate peripheral hull, or localized damage becomes decorative."* **That constraint eats itself** —
chassis-focus is slower only if the chassis exceeds ~70% of total hull, at which point peripherals
genuinely are rounding errors.

**The framing is what was wrong. Localized damage is not a way to kill faster; it is a way to achieve
things killing does not:**

| Target | Buys |
|---|---|
| **Chassis** | The fastest kill, and nothing else |
| Engines | It cannot flee — a capture precondition |
| Weapons | It cannot hurt you |
| Crew shell | Disabled and boardable — the prize intact |
| Shield generator | Everything else lands |

**Capture play must deliberately avoid the chassis**, since killing it destroys what you came for. So
the choice is never "fast kill versus slow kill" — it is *"kill it, or do something to it."* A fighter
dying to a burst through the middle is the honest outcome when killing is all you wanted.

#### Capture — a state reached several ways 📋

*Settled 2026-08-09, replacing §9's "Ion is the intended route in." **Ion is one tool, not the key.***

> **A hull is capturable when its shields are down, it cannot flee, and it cannot resist — and its
> integrity is still above the structural threshold.**

| Condition | Satisfied by |
|---|---|
| **Shields down** | Any damage type |
| **Cannot flee** | Engines destroyed · hyperdrive destroyed · **held by a tractor** |
| **Cannot resist** | Crew shell destroyed · power suppressed (Ion) |

The threshold does the balancing: **every condition must be met without dropping integrity below the
kill line.** Restraint is the price of a prize, and there are now several ways to pay it rather than
one mandatory weapon.

**Two new modules carry this**, and both are hardpoints — so both can be shot off:

- **Troop bay** — a `capacity` stat; boarding strength is capacity × its §2.7 quality roll. Carrying
  marines costs cargo or guns, and an enemy can destroy your bay to stop you boarding. *A bare
  "passenger count" on the hull was considered and rejected: it would be a second crew system with
  none of the grade, quality, mounting or localized-damage machinery a module gets for free.*
- **Tractor beam** — pull force contested against the target's `BodyMass` × `Propulsion::thrustNewtons`.
  **All three already exist**, so the contest resolves in the physics integrator that is already
  running. It is the **non-destructive** route to "cannot flee": every other path means shooting the
  engines off, which damages the prize.

**Boarding requires no docking bay — you breach anywhere.** Requiring a bay would make fighters
uncapturable, and it muddles §4.1's clean idea of the bay as *your garage*. The real cost is surviving
long enough to get adjacent to something still shooting.

✅ **Ownership transfer — settled 2026-08-11: boarding-in-place completes it.** Once a hull satisfies
all three capturability conditions, a Troop Bay-carrying vessel that gets adjacent and **holds
position there for a duration** (scaled by troop bay capacity vs. the target's hull class) flips the
target's `FactionRef` when the hold completes — the same exposure-for-duration trade every other
"stand still and take the risk" action in this design already uses, and it is why the target has to
already be defenseless (§6.3's symmetry requirement is satisfied for free: an AI faction runs the
identical check against the player's own uncrewed hull).

**Capturing a hull converts its crew, commander included — which is also what network raiding turns
out to be.** A sub-commander is a `Crew` module occupying a slot on the vessel exactly like any other
crew (§4.5); when the hull's `FactionRef` flips, every crew module aboard flips with it, the same way
a station's facilities come under new management when the station itself changes hands. No separate
"steal the knowledge" mechanic is needed: capturing a commander's vessel converts the commander, and
their `KnowledgeNetwork` (§2.5) — which is anchored to that specific commander entity, not to any
station — simply now belongs to whoever holds the vessel. **This is bribery's wholesale cousin**:
§2.7's crew bribery converts one crew module at a time for credits; capture converts an entire crew at
once for combat risk. A faction's own general network (not tied to any one commander) has no such
anchor and stays permanently un-raidable by design — see §2.5.

⚠️ **The crew shell does not exist in code.** `ModuleKind` is
`{Weapon, ShieldGenerator, PowerCell, Engine, Armor, Facility}`, `ShellKind` has no crew value, and
there are **zero occurrences of "crew" anywhere in `src/`**. So §2.7's crew modules, this section's
disable-by-crew-kill, §3.4's "the player dies with their shell," and §12.27's command gating are all
unimplemented. Recorded in `architecture.md` §13.

### 3.3 The Cost of Failure

If the ship the player is piloting — or commanding from the Bridge — is destroyed, **the player
dies.** That vessel and all its equipped modules are permanently lost.

Failure has **three tiers**, and they are deliberately very far apart in severity. Ship loss is
routine. Hard Game Over requires an entire infrastructure to collapse at once.

#### Tier 1 — Ship loss (routine)

**Knowledge survives; matter does not.** Per §2.5's rule, the player's knowledge network is
untouched by death — every reverse-engineering unlock, every saved Template, every discovered system
is still there. What is lost is physical: the vessel, its equipped modules, and everything in its
cargo hold.

**Respawn & backup** — the player respawns at an allied station and may immediately transition into
a stashed backup ship or an existing fleet vessel, backed by stored wealth and the designs their
network still holds.

**The Starter Ship safety net** — with no stored ships remaining, the player may take a basic,
low-stat Starter Ship and claw back up.

**Faction reputation is unaffected by dying.** Standing is a record of what the player *did*, and
being shot down is not a diplomatic act. Reputation moves only through the §5.3 writers.

#### Tier 2 — The recovery run 📋

**Death leaves a wreck.** Cargo and equipped modules are not deleted at the moment of death — they
drop at the death location as a **derelict wreck**, recoverable by flying back and salvaging it
within a time limit. Miss the window and the wreck expires; the contents are gone for good.

This needs no new system. `LootSystem` already owns drops, material salvage, derelict wrecks, and
pickup radius — a player death wreck is one of those with an expiry timer and the player's cargo
manifest attached.

**The wreck is not reserved.** Anyone can loot it first — NPCs included. This turns the recovery run
into a genuine race rather than a formality, and it gives The Forgotten (behavioral driver:
*Opportunistic Survival*; task weighting: *wreck salvage, ambush, vanishing when outgunned*) a
concrete reason to be circling a battlefield. Dying deep in Forgotten space should feel materially
worse than dying next to an allied station.

✅ **Settled 2026-08-11.** The window is **short and measured in in-game (simulated) time, not
wall-clock** — the game has no pause per §3.4, so the two only diverge if the player alt-tabs away —
and the wreck **is marked on the navigation map (§8)** for the duration of the window, giving the
player an exact heading rather than forcing them to navigate from memory. Marking it costs nothing
new: `LootSystem`'s derelict-wreck entity is already a `DerelictWreck`, and §8.2's icon model already
draws map markers by kind.

#### Tier 3 — Hard Game Over 📋

**The player's faction is eliminated by the same rule as any other faction.** This is not a special
case bolted onto the player — it is §5.1's Three Pillars applied to them. A Hard Game Over is what
it looks like when the player's side loses all three at once:

| Pillar | Lost when |
|---|---|
| Command Structure | No surviving station or capital carrying a command module |
| Recognized Leadership Entity | The player is dead **and** every AI sub-commander (§4.5) is destroyed |
| Economic Footprint | No production, no holdings, no territory |

**Everything player-owned is wiped — including knowledge.** Networks die with their hosts (§2.5).
With no stations and no sub-commanders left to hold them, the player's unlocks and Templates go with
the infrastructure. There is no meta-progression layer underneath.

*Rationale, and it is a change from earlier drafts.* Those preserved research across a Hard Game Over
on the reasoning that otherwise players would avoid the Macro loop and never risk their assets. The
Three Pillars answer that better: reaching zero is not one lost fight, it is a total cascade — every
station, every commander, and the player. **The safety margin comes from redundancy, which is earned
in the fiction, rather than from permanence granted outside it.** A player who wants to be safe
builds a second station and appoints a second commander; that is a gameplay decision with a cost,
and it is a far more interesting one than a guaranteed blueprint list.

**The galaxy persists, and the player restarts inside it.** When an AI faction collapses, §5.1 says
its surviving ships scatter into rogue scavenger groups and its territory becomes unclaimed. The
same applies here. The player restarts as an **independent rogue operator** (§5.10) in the same
galaxy — which is the identical entry state as a brand-new game, so this is one code path, not two.
Their former territory is unclaimed wreckage, and rogue scavengers are flying the designs they used
to build.

| Wiped | Preserved |
|---|---|
| Credits and stored materials | Galaxy state — territory, borders, faction standings |
| All ships, stations, and fleet assets | Designs sold to factions (§2.6) — still in service, still manufactured |
| All knowledge networks: unlocks, Templates, sensor intel | The consequences of everything the player did |
| Faction reputation (reset to baseline) | |
| Active contracts | |

#### The Hard Game Over screen offers both exits 📋

*Leaning 2026-08-07, pending final confirmation. Earlier drafts left this open on the grounds that
freely loadable saves make Hard Game Over "a death screen with extra steps."*

**The Hard Game Over screen presents two choices, and neither is hidden:**

| Choice | Result |
|---|---|
| **Load last save** | The run continues from the last save. The galaxy reverts with it. |
| **Continue in this galaxy** | The player restarts as an independent rogue operator (§5.10) in the *same* galaxy — their former territory unclaimed, rogue scavengers flying their old designs |

**Yes, this makes Hard Game Over optional. That is the correct trade.** Offering the reload does not
make the second option worthless — it makes taking it a *choice the player makes*, which is a
stronger position for the design than a harshness the player never consented to. A sandbox that
forces permadeath on a player twenty hours into a run mostly teaches them to back up their save
directory.

**The design pays no cost for this**, because §3.3 already establishes that restarting as a rogue
operator "is the identical entry state as a brand-new game, so this is one code path, not two." Both
buttons lead somewhere that already has to exist.

**What this does mean is that the Three Pillars must not be balanced as if they were a death
sentence.** Their real job is to make faction collapse legible and consequential for the *AI*
factions — a ten-faction galaxy becoming six through play (§5.1) — and the player being subject to
the same rule is what makes that rule honest rather than a special case. The stakes are systemic,
not punitive.

#### The save model 📋

*Settled 2026-08-08. This was §9's highest-leverage open item.*

> **Free/manual saves, plus a coarse periodic autosave as insurance.
> Autosave never fires on death, or on the approach to one.**

**The question was never whether Hard Game Over survives a reload** — §3.3 already concedes the
reload and offers it as a button. **It is whether Tier 2 survives one.** The recovery run is fully
specified and partly built (`core/galaxy/WreckRecord`, `LootSystem`'s death-wreck path,
`architecture.md` §12.5's dual-form resolution), and if death is trivially undoable nobody ever flies
one. It becomes engineering already paid for and never used.

**Free saves keep it alive, precisely because saves are badly timed.** Reloading costs everything
since the last save; the wreck costs a flight. Often the flight is cheaper — a player who has not
saved since before a lucrative haul will choose the run every time. That is a real decision, and it
is a better one than ironman produces.

**The autosave rule is what the whole thing hangs on.** An autosave triggered at or near death
rewinds the player to thirty seconds before the fight and the recovery run is dead content again.
Cadence must be coarse and event-based — on dock, on warp, on a multi-minute timer — and death must
never be a trigger. This is a design constraint, not an implementation detail, and it belongs in the
issue that builds saving rather than being left to whoever picks it up.

### 3.4 No Pause, No Safe Zones 📋

**The simulation never stops while the player is alive.** Opening the navigation map (§8), the
Engineering view, station services, or the Bridge interface does **not** pause the game. Ships keep
flying, weapons keep firing, and the player's vessel remains a physical, targetable object the entire
time.

#### The one exception, and the rule that makes it one 📋

*Settled 2026-08-09. §3.6 has always bound a **"pause menu. Singleplayer only"** to `Esc` while
citing this section — which reads as a contradiction until the actual rule is stated. Architecture
home: `architecture.md` §12.29.*

> **This section forbids pausing on any surface that carries information or decisions.
> The system menu — Resume, Save, Load, Settings, Quit — carries neither, and that is what makes it
> legal. It is the game's *only* pause.**

The distinction is the same one the list above already draws. The navigation map, Engineering,
station services and the Bridge all hand the player **tactical value**: routes, the threat picture,
repairs, orders. Freezing time while reading them is a free advantage, which is exactly what this
section exists to deny. The system menu offers no such thing — you cannot repair, retarget,
reallocate power, or issue an order from it.

⚠️ **This constrains what that menu may ever contain.** Settings means audio, graphics, and controls
— presentation, not ship configuration. **The power-priority list (§2.9) and the weapon-group editor
(§3.6) are explicitly excluded** (confirmed 2026-08-09): both carry in-fight value, and §2.9 already
names their home as the avionics surface, which does not pause. The exception is granted to a menu
that is deliberately barren, and it lasts only as long as the menu stays that way.

**Multiplayer is unaffected**, and not by special-casing: a client-side freeze has no meaning under
Law 9's authority model, so in a session the menu opens and the simulation keeps running. It stays
playable precisely because the menu confers nothing.

There are no safe zones. Docking at a friendly station is protection by *circumstance* — the
station's guns and its owner's disposition — never by rule. A station that is losing a fight is not
a refuge, and §3.3 still applies to a player sitting inside one.

#### What docking actually protects against 📋

*Settled 2026-08-07. The code and this section appeared to contradict each other —
`DockingSystem` removes `Targetable` on dock, which reads as invulnerability. The resolution is that
they are describing different threats.*

> **A docked vessel cannot be shot. A docked vessel dies with its host.**
> Nothing is invulnerable; what differs is *how* a thing is vulnerable.

| While docked | Exposed to |
|---|---|
| Direct fire, ramming, targeting | **No** — the vessel is not a target |
| Destruction of the docking facility | **Yes — total.** Every vessel inside is destroyed with it |

This applies identically to the player and to NPCs (Law 4, §6.3). Docking during a losing fight is
not an escape; it converts a risk you can dodge into one you cannot, and hands your survival to a
structure someone else is shooting at. §3.4's "protection by circumstance, never by rule" is exactly
this — the circumstance is whether the host holds.

**It also makes the recovery run (§3.3 Tier 2) reachable from a dock death**: a station's
destruction should leave the wrecks of what was inside it, not silently delete them.

> ⚠️ **Confirmed unimplemented 2026-08-10 (`architecture.md` §15.1 findings 2–3): neither half of
> this subsection's headline rule exists in code.** A docked vessel can currently still be hit
> directly (only auto-lock targeting excludes it), and destroying a docking facility does not
> destroy what's docked to it at all — the docked rig persists, `Docked` to a now-dead entity.
>
> **The fix, and the mechanism for "leave the wrecks":** excluding `Docked` from hit-testing
> (projectile and collision candidate views, alongside the existing `Targetable` removal) closes
> the first half. For the second half — **don't invent a new destruction path.** When a rig with
> occupied docking bays dies, route every rig `Docked` to it through the *same* `DeathWreck`
> creation `LootSystem` already uses for an ordinary combat kill. This is not a new mechanic wearing
> a different trigger; it is "this rig died," caused by "its host died" instead of "its hull hit
> zero." The demote/promote/salvage-window lifetime that pipeline already has is what makes a dock
> death sometimes salvageable and sometimes not — a natural consequence of whether anyone reaches
> the wreck field before it expires, not a dice roll bolted onto the moment of destruction.
>
> **The station's own `CargoHold`, if it has one, spills the same way** — its contents become
> `MaterialDrop`/`LootDrop` entities at the wreck site via `LootSystem`'s existing drop-spawn
> convention, rather than vanishing with the station. Whether *all* of it survives the blast or only
> a fraction is a tuning question (a partial-survival fraction is the more interesting answer, but
> the exact number belongs to a balancing pass, not this decision) — the mechanism is what's settled
> here, not the percentage.
>
> Architecture: `architecture.md` §12.34.

#### Where the player is, always 📋

*Settled 2026-08-08. This closes an ambiguity that ran through §3.2, §3.3, and §4.5 — the documents
variously had the player as a pilot, as a walking person on a bridge, and as a docked state flag.*

> **The player is always associated with exactly one shell.
> Flying, that is their cockpit. Aboard, it is the shell they are currently in.
> If that shell dies, the player dies.**

Docking places the player **in the docking bay**, alongside the vessel they arrived in (§4.5). From
there they move to a facility — engineering, manufacturing, research, the bridge — by selecting it.
**Movement is instant**, because the time cost §3.4 cares about is the interaction itself, not the
walk. While they are in that facility, they *are* in that hardpoint.

Four things follow, and all four are wanted:

- **It answers §3.2's open question about the crew shell with no special case.** One predicate covers
  flying and docked alike: lose the shell you occupy, lose the player.
- **It sharpens what §3.4 already says.** A docked vessel dies with its host; the *player* dies with
  their **current facility**, which can be destroyed while the station lives. "They blew the
  engineering bay while you were mid-merge" is a far better death than "the station fell."
- **Vessel and pilot become independently losable.** Your fighter sits in the bay; if the bay is
  destroyed you lose the vessel wherever you are, and if your facility is destroyed you are gone but
  the vessel is not.
- **It makes the facility menus and §4's component-driven UI the same feature.** Moving between
  facilities *is* the navigation, each menu is gated on its own hardpoint being alive, and §4's
  "destroying a hardpoint removes its tab mid-session" falls out rather than being implemented.

> ⚠️ **Every facility menu must display its own hardpoint's health.** Without it, dying in a menu
> reads as an arbitrary gotcha rather than as the tension it is meant to be. The player has to be
> able to see it coming, and the component-driven pattern §4 already specifies is where it goes.

This is what gives the engineering and command layers real weight: time spent in a menu is time the
galaxy spends without the player watching it, and choosing *when* to open one is a tactical decision.

##### Nothing in the code carries this yet 📋

*Verified 2026-08-08.* This subsection is the most-cited model in either document and **no component
implements it.** `PlayerControlled` is a bare tag on a rig root; there is no field anywhere naming
the shell the player occupies, so the predicate "lose the shell you occupy, lose the player" has
nothing to evaluate. What exists is one half of the fiction: `BridgeView` already gates on `Docked`
and lists the host station's *living* facility hardpoints as tabs — the tab list this section
describes, computed correctly, with no notion of the player standing in one of them.

`architecture.md` §12.24 specifies the missing piece as **`PlayerLocation { entt::entity shell; }`**
and makes selecting a tab the act of moving. Two things this section promises fall out of it rather
than needing to be written: the death predicate becomes uniform across flying and docked, and
"they blew the engineering bay while you were mid-merge" becomes `DamageSystem` destroying a
hardpoint that happens to be the one `PlayerLocation` names.

> ✅ **Sharpened 2026-08-10 — `architecture.md` §12.30.1.** `PlayerLocation` is not a component
> *alongside* `PlayerControlled`; it is the **only** one written, and `PlayerControlled` is derived
> from it — the rig root whose hardpoint you occupy. Two components naming where the player is would
> let this section's death predicate and the camera disagree, and the player would die while the
> screen showed a healthy hull. `Identity.h` already says the tag *"moves rather than duplicating"*;
> it has always been derived data and nothing has ever derived it.
>
> **Three consequences this section should own.** While standing in a facility you *are* aboard the
> station, so the hull bar measures the **station's** integrity — correct, since you die with your
> facility, but it must be labelled or it silently changes subject. Switching to another of your ships
> is the *same* single write, not a new mechanism. And **`R` while docked is board-then-launch**, two
> writes behind one key, since the hull you occupy is not the one that undocks.

⚠️ **Two gaps behind it, both content rather than design.** `data/base_game/modules.json` authors
exactly one facility (`docking_bay_i`), so a correct implementation still surfaces a one-tab bridge
until Storage, Engineering, and Repair facilities are authored — and `WorldGen` spawns no stations
at all, so there is currently nothing to dock at in a generated system.

### 3.5 Object Scale & Hardpoint Placement 📋

*Settled 2026-08-08. Scale was previously an art decision with no design consequence. §3.2's manual
aim makes it a mechanical one: if two hardpoints cannot be told apart on screen, they cannot be aimed
at separately, and localized damage silently degrades into a hull bar.*

#### Hardpoints vary in size, and art matches collision

**A shell's size is authored per type, in JSON, and its hit radius matches its drawn size.** A wing
gun is small; a chassis is large; a station's main battery is very large. Nothing is uniform.

*A uniform-size model was specified and then withdrawn on 2026-08-08.* It was elegant — one constant,
hull size derived from hardpoint count — but it forced the chassis into a special case (large art,
tiny hit circle), made hardpoints on a 2,500-unit hull only ~2 px when the ship was framed, and
pushed large hulls into the hundreds of hardpoints. Matching size to art fixes all three at once, and
it is what `ShellDef.radius` already does: wing 7, power bay 8, thruster 9, facility bay 14, chassis
22, station core 45.

**The rule that follows is simple and is the whole reason size matters.** Projectiles are
dimensionless; the hit test asks whether a projectile's path passes within a hardpoint's radius. So
two hardpoints are individually aimable exactly when their hit circles are disjoint:

> **distance between centres ≥ r(A) + r(B)**

**And placement needs an upper bound too**, or a hardpoint satisfies "connected" while floating in
space beside the hull:

> **r(A) + r(B) ≤ distance ≤ (A's extent + B's extent)**

The lower bound keeps parts separately aimable; the upper keeps them visually attached. Validation
rule 7 already checks graph connectivity; neither bound is checked today.

**And the hull itself needs an envelope** (settled 2026-08-08), because neither bound stops
*chaining*: rule 11 lets a mount attach to another mount, so a fighter can legally grow a
forty-hardpoint tentacle with every pairwise check passing.

> **Rule 12 — hull envelope:** every mount's centre plus its radius falls within `hullRadius` of the
> chassis, where **`hullRadius` is authored on the chassis shell.**

*This inverts how this section previously read.* An earlier draft treated hull radius **R** as an
abstract quantity you derived the chassis from (`chassis = 0.5R`). It is the other way round:
`hullRadius` is the one authored number that says how big a vessel is, and everything else — chassis
size, peripheral count, mass, manufacturing cost — follows from it.

**Placement is otherwise free.** A hardpoint may sit anywhere on a hull provided it is attached to
the rig graph and satisfies all three bounds. Players are not restricted to authored slots.

#### Hardpoint count is emergent, not authored 📋

*Settled 2026-08-08. An authored `maxMounts` cap was considered and rejected — rules 10 and 12
already produce the count, and a second cap would be a hidden vessel-class concept in a design that
deliberately has none (Law 4).*

Rule 11 puts a peripheral's centre at `c + p` from the chassis centre; rule 10 requires adjacent
centres to be at least `2p` apart. So the centres lie on a ring of circumference `2π(c + p)`, each
consuming `2p` of arc:

> **peripherals per ring ≈ π(c + p) / p**
> `c` = chassis radius · `p` = peripheral radius · another ring fits at `c + 3p` if `c + 4p ≤ hullRadius`

This reproduces the scale table below from pure geometry:

| Hull | `c` | `p` | Rings | Count | Table says |
|---|---:|---:|:---:|---:|---|
| Fighter, `hullRadius` 25 | 12 | 6 | 1 | **9** | "~9 at p = 0.25 R" ✓ |
| Fighter, `hullRadius` 25 | 12 | 2.5 | 1 | **18** | "~18 at p = 0.1 R" ✓ |
| Station, `hullRadius` 1250 | 625 | 150 | 2 | 16 + 22 = **38** | "20 – 50" ✓ |

**Smaller peripherals mean more of them; the envelope decides how many rings.** That is the design
lever, chosen per hull, and it is why the scale table below is *descriptive* — a consequence of three
authored numbers rather than a target to hit.

**Three existing rules bound it from the other directions**, so no further cap is needed:

| Constraint | Caps |
|---|---|
| Rule 4 — total mass within the chassis threshold | How many you can afford to **carry** |
| Rule 3 — net power ≥ 0 | How many you can **run** — more modules need more power cells, which need mounts and mass |
| Rules 10 + 12 | How many physically **fit** |

**On modding and player Templates, this is self-correcting.** Someone authoring `hullRadius: 1250`
on a thing named "fighter" has not exploited anything in normal play: **players cannot author
shells at all** (§2.2), so a Template is assembled only from what `shells.json` already contains. A
modder who edits `hullRadius` is doing the same thing as a modder who edits `damage`, and is
unconstrainable either way. The authoring-accident case is what §2.2's optional rule 13 mass-sanity
band exists for.

**Manufacturing cost scales with total mass** (§2.8), which closes the loop: a bigger envelope is a
bigger hull is a heavier hull is a more expensive hull.

#### The chassis is the hull

**A chassis's radius is ~50% of its hull's `hullRadius`** — an authoring convention, not a derivation
(see rule 12 above) — and its sprite carries the ship's silhouette.
Everything else rings it and protrudes past its edge — which is what a ship *looks* like: a body with
wings, engines, and turrets attached.

It also makes shooting intuitive with no explanation needed: **hit the middle and you hit structure;
hit the wing and you kill the gun.**

The geometry that follows, for a hull of radius **R**:

| Quantity | Value | On a 50-unit fighter (R = 25) |
|---|---|---|
| Chassis radius | 0.5 R | 12 |
| Peripheral radius | ≤ 0.25 R | ≤ 6 |
| Peripheral centre distance | 0.5 R + p | 18 |
| Outer extent | 0.5 R + 2p | 24 — inside the hull ✅ |
| Peripherals that fit on one ring | ~9 at p = 0.25 R, ~13 at p = 0.15 R, ~18 at p = 0.1 R | 9–18 |

Smaller peripherals mean more of them — a direct design lever on how finely a hull subdivides under
localized damage, chosen per hull rather than forced by a constant.

*This is exactly why `aegis_vanguard` fails today:* a chassis radius of 22 on a 36-unit hull swallows
the entire ship, leaving nowhere legal for anything else. The fix is a smaller chassis radius and a
bigger hull — both of which the table below supplies.

#### The scale table 📋

| Object | Units | Hardpoints | Note |
|---|---|---|---|
| Shuttle / courier | 25 – 40 | 4 – 6 | |
| **Fighter** | **50** | 7 – 19 | Chassis r ≈ 12, peripherals r ≈ 5 – 6 |
| Transport | 100 | 10 – 20 | |
| Corvette / gunship | 150 – 400 | 15 – 30 | |
| Cruiser | 500 – 1,000 | 20 – 40 | |
| **Dreadnought / station** | **2,500** (cap) | 20 – 50 | Chassis r ≈ 625; batteries r ≈ 100 – 200 |
| Asteroid | 20 – 200 | 1 – 8 | |
| **Planet** | **20,000 – 100,000** | — | Not fightable |
| **Star** | **40,000 – 350,000** | — | By luminosity class |
| **System radius** | **400,000 – 2,000,000** | — | By luminosity class |

**Fighter to dreadnought is 50×**, and a dreadnought's batteries stay visible and aimable at
whole-ship framing: a 150-radius turret on a 2,500-unit hull is ~120 px when the ship is framed at
1,000 px. Capitals can be fought either whole or section by section, which is a camera choice rather
than a constraint.

#### Why 2,500 caps a fightable hull

**Operating one.** A player flying a dreadnought must zoom out to see it and its surroundings. That
is fine, because **operating a capital is positioning, power allocation, and target designation, not
marksmanship** (§4.0) — its turrets are crewed and target independently (§2.7), so the player flies
while gunners shoot. It does mean an *uncrewed* capital is nearly useless, which is exactly the
pressure that makes capital crew slots matter.

**Reading one.** Beyond ~2,500 units a hull stops being comprehensible on screen at any useful zoom.
Larger structures are scenery or set-pieces, not things fought hardpoint by hardpoint.

**A target schematic panel is still wanted**, though no longer strictly mandatory: a wireframe of the
current target showing live hardpoint status is how a player reads what they have already stripped
off a capital, especially when engaging one from outside visual range (§ weapon ranges below).

#### System radius scales with star class 📋

Bigger stars get bigger systems, keeping the star-to-system ratio near 1:20 and giving luminosity
class a consequence the player *feels* while crossing a system rather than only sees in the lighting.

| Class | Star diameter | System radius |
|---|---|---|
| Dwarf | 40,000 – 80,000 | 400,000 – 700,000 |
| Main sequence | 80,000 – 150,000 | 700,000 – 1,200,000 |
| Giant | 150,000 – 250,000 | 1,200,000 – 1,600,000 |
| Supergiant | 250,000 – 350,000 | 1,600,000 – 2,000,000 |

A ratio much below 1:10 crowds every orbit into one shell — a 500,000-diameter star in a
1,000,000-radius system leaves barely 4:1 between innermost and outermost orbit, which is why the
star band is capped where it is.

**System radius is seed-derived and must never be stored** (§7.2) — it follows from star class, which
follows from the seed. Caching it into a record invites the two to drift.

##### The 2,000,000 ceiling exists for a specific reason

`float` precision is relative, so at large coordinates small increments round away entirely:

| System radius | ULP at edge | Movement slower than this is **lost** |
|---|---|---|
| 1,000,000 | 0.0625 | ~1.9 units/sec |
| **2,000,000** | **0.125** | **~3.8 units/sec** |
| 4,000,000 | 0.25 | ~7.5 units/sec |

`position += velocity * dt` does not change the value when the increment falls below half a ULP. At a
4,000,000 radius anything drifting under ~7.5 units/sec **freezes** — which would hit drifting
uncrewed hulls (§3.2), debris, and fine docking adjustments. Capping at 2,000,000 keeps the threshold
low enough that only barely-moving things are affected, and still leaves a 5× spread between the
smallest and largest systems.

*This is deterministic precision loss, so it does not desync multiplayer (Law 2 is safe) — a gameplay
artefact, not a correctness one.*

**Three consequences of variable system size, worth deciding rather than discovering:**

- **Travel time varies with radius.** Either warp velocity scales with system size, or warp is a jump
  to a target rather than continuous travel. A constant warp speed makes supergiant systems tedious.
- **Content density.** If orbit *count* stays fixed while radius grows, big systems are proportionally
  emptier. Either scale body count too, or accept vastness as the character of giant systems — but
  choose it.
- **Level 3's zoom-out limit becomes per-system** (fit this system) while its zoom-in limit stays
  fixed (ship detail). The zoom *range* varies; the scale within it does not (§8.1).

#### Penetration is a weapon property, not a global rule 📋

A projectile stops at the **first** hardpoint it reaches. Universal pass-through would swing a
weapon's output several-fold on geometry alone, which is unbalanceable, and it deletes the choice of
*which* hardpoint to hit.

**`penetration` is an authored weapon stat** (Law 10): most weapons stop at the first hardpoint, while
a railgun or lance passes through N, or until its damage budget is spent. Raking a dreadnought down
its long axis becomes a real shot to be rewarded for, on the weapons built for it.

It costs almost nothing — resolving hits *in order along the path* is already required to fix the
tie-break defect (`architecture.md` §12.14 item 17), and penetration is then "take the first N."

#### Weapon range follows hull class

| Class | Range | Relationship |
|---|---|---|
| Fighter | 800 – 1,000 | Both combatants on screen; a brawl |
| Capital | 3,000 – 6,000 | Must exceed own hull length so a bow turret covers the stern |

Capitals therefore fight beyond visual range while fighters fight inside it — a real class distinction
rather than the same fight at two sizes.

Projectile speed ~1,200 – 1,800 units/sec, giving a per-tick step of 20 – 30 units. **That exceeds the
diameter of every small hardpoint in the game**, so the swept-segment collision test
(`architecture.md` §12.16) is mandatory; a point test would tunnel straight through.

#### Rendering: the five draw layers 📋

> **Vocabulary.** These are **draw layers** — render order *within* one rig. They are not
> §3.7's **altitude bands**, which are world-space height affecting collision *between* vessels,
> and not `architecture.md` §12.28's **world draw order**, which places non-rig bodies behind
> rigs. §3.7 records why the earlier "Z-layer / Z-level" naming was abandoned: the two names
> differed by three characters and were confused. Three concepts, three names, no overlap.

Shells declare a draw order so top-down hulls read with depth:

| Layer | Contents |
|:---:|---|
| 1 | **Ventral** — engine housings, drop tanks, under-mounts |
| 2 | **Hull** — the chassis |
| 3 | **Hull detail** — panelling, markings, faction livery |
| 4 | **Dorsal** — turrets, canopy, sensor domes |
| 5 | **Overlay** — thruster glow, shield shimmer, damage decals |

#### Collision shape, and drawing what you test 📋

*Settled 2026-08-09, while working through §3.2's structural integrity model.*

> **`ShellDef` gains an optional baked collision polygon, defaulting to a circle of `radius`.**

**Deriving the shape from the art is a load-time bake, not a frame cost** — trace the sprite's alpha
perimeter once, reduce to a convex polygon, store it. Runtime then tests segment-vs-polygon at roughly
4× a circle test at 8–16 edges, against a budget that already requires §9.1's rig-level rejection to be
feasible at all. Once a projectile only tests the few hardpoints on one rig, polygons are affordable.
**`../StarReach2` already proved this**, deriving pixel-perimeter convex hulls for fighters and
capitals with a broad/narrow phase split. Convexity loses concavities, which is near-free per
*hardpoint* — individual mounts are small and mostly convex.

Defining the field now means **no data-model churn when art lands**: circles today, polygons later.

⚠️ **Until then, draw exactly what you test.** `WorldRenderer` currently draws a rig root as a triangle
sized by `CollisionRadius` while `ProjectileSystem` tests circles — so the triangle's flanks cut inside
the tested shape and shots pass through drawn hull. **Render the hit shape itself**, and show heading
with a nose marker rather than by shaping the hull.

*This withdraws a proposal made earlier the same day* that a rig root draw as a nose-forward triangle
when `Propulsion` is non-zero and a disc when it is not. The intent was good — *shoot the engines out
and it visibly becomes a hulk* — but it achieved it by making the drawn shape disagree with the tested
one, which is the defect being fixed. **A heading marker that disappears at zero propulsion says the
same thing without lying about the hitbox.**

⚠️ **`ShellDef` needs a second field for this.** `spriteLayer` today is a *string asset key* — it says
which image, not what order. A draw-layer value (1–5) is a separate concept and currently does not exist
anywhere in code or docs. One string doing two jobs is how the two drift apart. Default the layer per
shell type, with an optional per-mount override in the blueprint for hulls that want an engine
mounted ventrally rather than dorsally.

**Within a layer, sort by y-position** (settled 2026-08-08) — **local** y, measured against the hull,
not world y.

This matters more than it sounds. In a true top-down view nothing is genuinely nearer the camera, so
the sort is a *tiebreak*, and its job is to be **stable**. Sorting on world y would re-order a hull's
own parts as it turned, so a turret drawn over its neighbour would pop behind it when the ship rotated
180°. Local y is fixed relative to the hull, so the stacking never changes.

*A free consequence:* because local y never changes, **the order can be computed once when a rig is
built and stored**, rather than re-sorted every frame. Sorting between separate rigs can stay
arbitrary — two ships overlapping at the same altitude have no correct answer anyway.

**Turret sprites rotate** (settled 2026-08-08). A turret's drawn rotation is its mount's world
rotation **plus `FiringArc::currentOffset`**, so a gun visibly tracks what it is aiming at rather than
firing sideways out of a fixed barrel.

- **No new field is needed to know which shells rotate.** A hardpoint carrying `FiringArc` rotates;
  everything else simply inherits the hull's rotation. Engines, plating, and power bays stay put.
- **Turret art must pivot about its mount point**, so the sprite's centre of rotation is its base, and
  the barrel points along the sprite's forward axis.
- **Collision is unaffected** — a hit circle is rotation-invariant.
- **A long barrel will sweep visually over neighbouring hardpoints**, which looks right and is
  harmless. But it means rule 11's attachment bound should measure a turret's **base** extent, not its
  barrel length, or long-barrelled guns will fail a check they are not actually violating.

#### Texture resolution per object class 📋

**The rule: an asset wants roughly the pixels it will ever occupy on screen, and no more.** Maximum
zoom frames a 50-unit fighter across a ~900 px viewport, giving a ceiling of **18 px per world unit**.
Large objects are never viewed at that zoom — a dreadnought is fought at ~2–4 px/unit — so their art
is budgeted against the zoom they are actually seen at.

| Asset | Diameter | Viewed at | Max px | **Ship at** |
|---|---|---|---|---|
| Small hardpoint — wing gun, emitter | 10 – 20 units | 18 px/unit | ~360 | **512²** |
| Medium hardpoint — thruster, bay | 20 – 40 | 18 | ~720 | **1024²** |
| Fighter / shuttle chassis | 25 – 50 | 18 | ~900 | **1024²** |
| Capital turret / battery | 200 – 400 | 4 | ~1,600 | **1024²** |
| Capital chassis (≤ 600 units) | ≤ 600 | 4 | ~2,400 | **2048²** |
| Capital chassis (> 600 units) | up to 2,500 | 2 – 4 | > 5,000 | **segmented** — tiles from a shared library |
| Asteroid | 20 – 200 | 8 – 18 | ~1,600 | **1024²** |
| Planet | 20,000 – 100,000 | fills viewport | ~1,000 | **1024²** |
| Star | 40,000 – 350,000 | fills viewport | ~1,000 | **2048²** |
| Thruster plume | 10 – 30 | 18 | ~540 | **512²** |
| Explosion | 50 – 200 | 8 | ~1,600 | **1024²** |
| Projectile | 2 – 6 | 18 | ~110 | **128²** |
| Map icon | fixed screen size | — | 32 – 64 | **generated at runtime** (§8.2) |
| HUD element | fixed screen size | — | native | **native, never scaled** |

**Only three things justify 2048²**: capital chassis art up to ~600 units, stars, and nothing else.
**Above ~600 units a chassis must be tiled**, drawn from a shared library of hull segments rather than
authored whole — that is a memory decision *and* a natural fit for the modular design everything else
in §2 already uses.

**Budget estimate** at block compression (~1 byte/px) plus mipmaps:

| Category | Count × size | Total |
|---|---|---|
| Small/medium hardpoints | 40 mixed | ~30 MB |
| Fighter and small chassis | 20 × 1024² | ~27 MB |
| Capital chassis segments | 30 × 2048² | ~160 MB |
| Asteroids, planets, stars | ~35 mixed | ~67 MB |
| Effects, projectiles | — | ~10 MB |
| **Total** | | **~290 MB** |

**The single biggest lever is sharing capital chassis segments.** Authoring unique art per capital
design multiplies that category by the number of designs; a shared segment library keeps it flat.

⚠️ **Mipmaps are mandatory, not an optimisation.** Objects are viewed across a very wide zoom range by
design, and an unmipmapped sprite shimmers badly as it shrinks. Budget the ~33% from the start.

**Author at 2048², ship at these sizes.** Downsampling at bake time is free and keeps headroom.
Shipping the authoring resolution is what turns a texture budget into a problem.

> ✅ **The texture atlas un-defers here** (2026-08-08). `architecture.md` §6 is 🧊, and §2.5 says a
> deferral ends when *"a shipped feature demands it."* §9's performance budget is that demand: 5,000
> hardpoint sprites plus 5,000 projectiles is ~10,000 sprites per frame. The quad count is trivial
> for any GPU; **the texture binds are not**, because every shell type carries its own sprite, and
> thousands of state changes per frame will stall long before the simulation does.
>
> **Only the atlas un-defers.** UUID/hashed asset ids, audio banks, and hot-reload stay 🧊 — recording
> which piece the trigger actually justifies is what keeps the rest of the deferral honest.

#### Art direction: stylised high resolution 📋

**Flat shading, limited palette, strong silhouettes, hard edges** — not painted realism, and not pixel
art.

**Why not pixel art**, despite real advantages in production cost, memory, and small-size readability:
two properties of this design fight it continuously rather than once.

- **Free rotation.** Hulls turn to arbitrary angles and turrets slew continuously within their arcs.
  Pixel art has no correct answer for a 37° rotation without pre-rendered frames or resampling that
  destroys the grid.
- **A ~100:1 zoom range** inside Levels 3–4 alone. Pixel art wants integer scale factors; ours are
  continuous, and the mipmapping this zoom range makes mandatory is what turns pixel art to mush.

Pixel art remains viable *if* §8's zoom model commits to **discrete integer zoom steps** and accepts
rotational aliasing as part of the look — a decision about the zoom model, not an art preference, and
it would also require redoing the chrome-and-glass HUD to match.

**It does not change the resolutions above, but it changes what they buy.** You are paying for **edge
fidelity, not interior detail**: flat interiors survive downscaling almost losslessly, while hard
edges are the first thing to break when a sprite is upscaled.

**Two consequences worth planning for:**

- **Damage states and faction/tier colour should be palette shifts and overlay decals, not separate
  textures.** Flat limited-palette art tints cleanly from a greyscale master, which cuts asset
  *count* — the real budget, since resolution is settled.
- **Test block compression early.** BC7 banding and ringing are *more* visible on flat areas adjacent
  to hard edges than on noisy detail. This style is harder to compress cleanly than painted art,
  which is the opposite of the usual intuition.

#### ❓ Open: what a destroyed hardpoint looks like

`WorldRenderer` notes that there is no wreck art and that drawing a dead hardpoint identically to a
live one is wrong. With localized damage as a headline mechanic (§3.2), **a player must be able to
read a hull's state at a glance** — which of an enemy's turrets are gone, whether its engine is dead.

Three options:

| Option | Cost |
|---|---|
| A scorched **variant sprite** per shell | Doubles the shell art library |
| **Drop the sprite**, leaving a visible gap in the hull | Free, but a hull becomes holes rather than damage |
| A **damage decal on layer 5** | One overlay reused across every shell |

**Recommendation: the layer-5 overlay.** The flat limited-palette art style (below) makes a single
scorch/breach decal read correctly over any shell, so it costs one asset rather than one per shell —
and the five-layer stack already exists to carry it.

#### Enforcement, not documentation

Both placement bounds become **blueprint validation rules** (§2.3), checked by `Validation.h` on every
authored *and* player-created blueprint and run in CI against `data/base_game/`:

- **Rule 10 — separation:** no two mounts closer than `r(A) + r(B)`.
- **Rule 11 — attachment:** every mount within `A extent + B extent` of what it attaches to.

A hull that violates either fails to load with a specific error, the same way the other nine rules
already work. A scale spec that lives only in this document will be violated by the third ship
somebody authors.

### 3.6 The Input Map 📋

*Consolidated 2026-08-08. Several sections independently claimed keys; this is the single place they
are reconciled. **All bindings are rebindable in the Settings menu** — these are defaults.*

| Input | Action | Source |
|---|---|---|
*Rebuilt 2026-08-09. The previous table predates the command pass (§4.0/§4.3) and contradicted §2.9
on power. Every row below is current.*

##### Flight

| Input | Action | Source |
|---|---|---|
| **W** | Thrust forward | `ThrustInput.forward` |
| **S** | Reverse thrust — and therefore the brake | `ThrustInput.forward` negative |
| **A / D** | Turn left / right | `ThrustInput.turn` |
| **Q / E** | Strafe left / right | `ThrustInput.strafe` |
| **Ctrl** *(hold)* | **Afterburner** — engines Boosted while held | §2.9 |

**All three axes of `ThrustInput` now have a producer**, which is the point: the component has
carried `forward`, `strafe`, and `turn` since it was written and nothing has ever set any of them
for the player (`architecture.md` §12.24).

##### Combat

| Input | Action | Source |
|---|---|---|
| **Mouse position** | **Aim point.** The cursor *is* where weapons point — no target lock | §3.2 |
| **Left mouse** | Fire every **enabled** weapon group — *and place, in build mode* | §3.2 |
| **1 – 0** | Toggle weapon group 1–10 on/off | below |
| **F / G / H / J** | Power: weapons · shields · **engines** · facilities. **Tap** = Boosted, **hold** = Offline | §2.9 |

##### Command

| Input | Action | Source |
|---|---|---|
| **Right mouse** | **Select** a unit · **drag** = box select · **double-click** = every unit of that `BlueprintId` on screen · **click in world** = issue the armed order | §4.3 |
| **Shift** | **Add** — to the selection, or **append** the order to the queue instead of replacing it | §4.3 |
| **Z / X / C / V** | Arm an order: **Move · Attack · Defend · Build** | §4.3 |
| **N** | Arm **Stop** — halt and clear the unit's queue | §4.3 |
| **T** | Cycle stance: Hostile → Defensive → Peaceful | §4.3 |

> **Order verbs extend rightward along the bottom row.** `Z X C V` are the four, and a fifth verb
> takes the next free key to the right — `B` is build mode, so `Stop` lands on `N`, and a sixth would
> take `M`. Recording the *rule* rather than only the keys, so the next verb has an obvious home
> instead of a fresh argument.

##### Modes and everything else

| Input | Action | Source |
|---|---|---|
| **B** | Build mode — only with a living `FacilityKind::Construction` hardpoint aboard | §2.11 |
| **R** | Dock / undock | built (`AvionicsMenu`) — **moved off `E`, see below** |
| **Tab** | *Reserved* — tactical map overlay, where off-screen units are selected (§4.3) | §8.1 |
| **Scroll** | Zoom — and zooming out far enough *is* the navigation map | §8 |
| **Esc** | **A ladder, innermost first:** cancel build placement → clear selection → open the system menu → close it. Pauses in singleplayer only, and only for that menu (§3.4, `architecture.md` §12.29) | §3.4, §4.3 |

> ⚠️ **`R` for dock is a change to shipped code.** `AvionicsMenu.cpp` declares
> `constexpr int kDockKey = KEY_E` — the **only** gameplay input that exists in the repository today.
> `E` is now strafe-right, so this binding must move in the same commit that adds flight controls, or
> docking and strafing fight over one key.

#### Weapon groups 📋

*Settled 2026-08-09.* **Ten toggleable groups, on `1`–`0`.** Left-click fires every enabled group;
a disabled group holds fire.

**Why ten assignable groups rather than three automatic ones.** Grouping by §3.1's damage type was
proposed and rejected: damage type has exactly three values, but §3.1 puts **weapon behaviour on an
explicitly unbounded axis** — *"penetration, damage-over-time, disable, splash, tracking."* A missile
rack and an autocannon can both be Kinetic and want entirely different trigger discipline, and damage
type cannot separate them. Ten groups covers the unbounded axis; three would have covered only the
capped one.

| | |
|---|---|
| **Membership** | A group index on each weapon hardpoint. Assigned by the player out of combat, in the fit screen |
| **Default** | **Each distinct weapon `ModuleId` takes the next free group**, so a newly built ship arrives sensibly pre-grouped with no player action. A hull with two cannon types and a missile rack boots with three live groups |
| **Enabled state** | Per rig, not per group definition — a bitmask on the rig root, so it is session state and costs nothing to save |
| **Destroyed hardpoints** | Simply stop contributing. A group whose every hardpoint is dead is dead, and no bookkeeping is required (§2.11's aggregation rule) |

**What this buys, and it is not just convenience.** §3.1 makes Ion *suppress power* rather than
damage hull, and §9's capture route is "strip shields, suppress power, kill the crew shell, take the
hull." That is **impossible today** — with one rig-wide fire command your kinetics destroy the prize
while your ion is disabling it. Weapon groups are what make the capture route reachable at all.

It also gives finer power control than §2.9's category switch: silencing one group frees budget
without taking *all* weapons Offline.

#### The rules behind the layout

1. **The left hand never leaves the movement cluster; the right never leaves the mouse.** The mouse
   is aiming continuously (§3.2), so it can never travel to a menu.
2. **One action per order.** §4.4's no-pause rule means nothing here is a menu — the only modifier is
   `Shift`, and it means the same thing everywhere.
3. **`Shift` means *add*, always.** Add to the selection, or add to the order queue. One mental
   model, and it is the convention every RTS player already has.

> ⚠️ **Rule 1 is a target, not a guarantee, and this layout bends it.** `H`, `J`, and `6`–`0` sit
> outside comfortable left-hand reach. That is a deliberate trade: power allocation and weapon groups
> want *dedicated, always-available* keys more than they want *ideal* ones, and both are toggles
> rather than continuous inputs, so a brief reach costs a moment rather than a manoeuvre. **The
> previous version of this section claimed every action key sat in the left-hand cluster; that is no
> longer true and pretending otherwise would hide a real ergonomic cost.** All bindings are
> rebindable, which is the mitigation.

**The afterburner moved from `Shift` to `Ctrl`** so `Shift` could take its conventional "add"
meaning. §2.9's argument for the afterburner — that it is the clearest demonstration of power
allocation and should be the first thing the player meets — does not depend on which key it sits on.
`Ctrl` held is a momentary boost; tapping `H` is the sustained toggle. Both reach Boosted, and having
both is deliberate rather than a collision.

**The navigation map has no dedicated key** — it is the far end of the zoom range, not a screen
(§8.1). Reaching it costs the same zoom-out that makes you blind to your surroundings, which is the
tension §3.4 wants. `Tab`'s reserved tactical overlay is a *different* surface: a HUD minimap for
selecting units you cannot see, not the full map.

❓ *Open: whether Offline should require a deliberate input rather than a hold, since cutting engines
dead by mis-click while being chased is not a decision anyone means to make.*

❓ *Open: controller support.* Neither document has ever addressed it, and this layout is genuinely
hostile to a gamepad — a continuous cursor aim point plus box selection plus fourteen discrete
actions. Recorded now because it is cheap to note and expensive to discover after the UI is built.


### 3.7 Collision, Ramming & Structural Destruction 📋

*Settled 2026-08-08. Much of this is already built; what follows records what exists, what changes,
and two proposals that were considered and declined.*

#### Ram damage already scales with mass and speed ✅

`CollisionSystem::ApplyRamDamage`:

```
baseDamage = kRamDamagePerSpeed * speedChange
heavyShare = lighterMass / (massA + massB)      // the heavier side's share
lightShare = 1 - heavyShare                     // the lighter side's share
```

A dreadnought of mass 100 ramming a fighter of mass 10 takes **9%** of the damage and deals the
fighter **91%**. Damage lands on the **nearest living hardpoint to the contact point on each side**,
so ramming already localizes (§3.2). The heavier rig is not displaced at all; the lighter absorbs the
whole position correction. `RamCooldown` stops two overlapping hulls exchanging damage every tick.

⚠️ **One gap: total damage does not scale with absolute mass.** `kRamDamagePerSpeed` is a flat
constant, so two dreadnoughts colliding at 10 units/sec produce the same base damage as two fighters
at 10 units/sec — only the split differs. Kinetic energy goes as ½mv², so a heavier collision should
hurt more in absolute terms. Scaling `baseDamage` by reduced mass fixes it, and §2.10's derived masses
make those numbers real rather than arbitrary.

#### Collision resolves per hardpoint, not against a convex hull 📋

*Settled 2026-08-08.* `CollisionSystem`'s narrow phase currently builds a **convex hull from the rig's
hardpoints** (`BuildWorldHull`, ported from StarReach2's `CollisionHull.cpp`), which makes every rig a
solid blob. Replacing it with **per-hardpoint circle tests** produces a property nothing else could:

> **Destroying hardpoints opens holes you can fly through. Damage changes a vessel's physical shape.**

- A fully-armed capital is physically impenetrable; a stripped one is full of gaps. That is §3.2's
  localized damage extended into geometry.
- A well-built hull has **contiguous rings by construction** (rule 11 requires attachment), so gaps
  appear exactly where a capital is *weakly defended* — earned, not arbitrary.
- Cost is bounded: a fighter's 7 hardpoints against a capital's 50 is 350 circle tests per pair, after
  the existing broad phase. The real cost is **replacing ported, debugged code**.
- Two fighters still collide normally — a fighter is mostly chassis, so circles give the same answer a
  hull would.

**And it combines with two decisions already made.** Nearest-hit-along-the-segment (`architecture.md`
§12.14 item 17) means a shot at a capital's centre resolves on whatever it crosses *first*, so killing
the core means **stripping the ring or threading a gap between two peripherals**. §3.5's ring geometry
becomes tactically meaningful rather than only visual.

⚠️ **Worth confirming:** whether `BuildWorldHull` currently excludes `Destroyed` hardpoints. If not, a
stripped capital still collides at full size under any model.

✅ **Confirmed 2026-08-10 by `architecture.md` §15.4** — it already does. A stripped capital does not
collide at full size.

#### Ramming is Kinetic damage, and shields apply to it exactly like a shot 📋

*Settled 2026-08-10, correcting a live bug `architecture.md` §15.1 finding 1 found: ram damage is
already tagged `DamageType::Kinetic`, but `DamageSystem` doesn't yet apply the shield-absorption
rule to it, so today a Kinetic-shielded target's shield does nothing against a ram — full damage
lands regardless.*

> **A ram is a Kinetic hit with no weapon behind it. It is not a fourth thing.**

Route it through the exact same `DamageSystem` path a projectile uses, no special case:

- **A Kinetic shield absorbs its share of ram damage**, exactly as it would absorb a Kinetic shot —
  depleting the pool, resetting `rechargeCooldown`, same as any other absorbed hit.
- **An Energy shield does nothing against a ram** — full Kinetic damage reaches the hull, same as it
  would against a Kinetic gun.
- **Each side's own shield governs only what happens to that side.** Two ships colliding are two
  independent absorption checks against the *same* collision event — an Energy-shielded ship ramming
  a Kinetic-shielded one takes full damage while dealing damage the other's shield eats. The whole
  Kinetic/Energy × Kinetic/Energy matrix falls out of this for free; nothing new needs authoring
  per pairing.

**This changes what ramming is *for*, and the replacement is sharper than "unblockable."** The
original framing (§3.1: shields are projectile-only, so ramming bypasses them) traded away in favor
of consistency: ramming needs no weapon hardpoint at all, its mass/speed-scaled damage (above) is
unchanged and still hits both parties, and it specifically punishes an all-Energy loadout — a real
build weakness rather than a mechanic that made every shield irrelevant to a desperate or
weapons-stripped ship. **Shields no longer have a hole in them; ramming has an identity that isn't
"the thing shields cannot stop."**

No new component. This is the same `DamageTypeEffect` lookup §3.1's Ion section below now
specifies, applied to `Kinetic`'s (unremarkable, default) row — see that section for the mechanism.

#### Destruction cascades along the rig graph 📋

*Settled 2026-08-08. This replaces built behaviour.* `DamageSystem` today kills a rig only when
`HasLivingHardpoint` returns false — **every** hardpoint must die — and `ShellKind::Chassis` is
special-cased **nowhere**, so blowing the chassis off a fighter leaves it flying on its wings.

> **Destroying a shell destroys everything attached to it.**

`StructuralAttachment` already exists in `shared/components/Rig.h` and `RefactorSystem` already
reasons about it ("a hardpoint another hardpoint's attachment points at cannot be deleted — would
orphan its children"). Everything hangs off the chassis directly or transitively, so **chassis death
is rig death, with no special case anywhere.**

What this buys:

- **"Hit the middle and you hit structure" (§3.5) becomes literally true**, rather than a description
  of art.
- **Partial destruction gains real teeth** — kill a wing root and the guns beyond it go with it,
  without touching the hull.
- **It is not the protected core §3.2 rejects.** The chassis is the *most* exposed thing on a hull, at
  50% of hull radius and dead centre. It is critical, not invulnerable.

⚠️ **This makes one content relationship load-bearing: chassis hull must dominate peripheral hull by a
wide margin.** Otherwise "shoot the middle" is simply the fastest kill and localized damage becomes
decorative. The natural version — hull scaling with size, so a 625-radius station core vastly outlasts
a 150-radius battery — supplies it, but it needs stating so nobody authors a fragile chassis.

**Orphaned children are destroyed and drop salvage** through `LootSystem`'s existing death-wreck path.
Leaving them as drifting debris would be lovely and is a separate feature.

#### Two proposals considered and declined 📋

**A rig-wide hull pool for shots that hit the vessel but no hardpoint. ❌ Declined.**

The problem it addresses is smaller than it appears: §3.5 puts the chassis at **50% of hull radius,
dead centre**, so the middle of every vessel is solid. Gaps exist only at the periphery, between
ring-mounted parts — which is exactly where missing *should* mean missing. The costs are real:

- §3.2 opens by defining vessels as *"physical collections of hardpoints… **not a single health
  bar**."* A rig-wide pool is that health bar.
- It weakens precision aiming, which §3.2 makes the core in-fight decision.
- It contradicts per-hardpoint collision — the same gaps would mean one thing for flying and another
  for shooting.
- `architecture.md` §12.14 item 17 already settled one projectile = one hardpoint.

If capital fights read as whiffy in play, the cheaper fixes are more chassis coverage or larger
peripheral radii — content dials, not mechanics.

**A `shipType` enum to decide what may overfly what. ❌ Prohibited.**

Law 4's uniformity is the most protected decision in the architecture: §3.2 states there is "no
per-vessel-type special case," `architecture.md` §12.14 removed the last one, and §3.5's scale system
exists precisely so that "fighter" and "capital" are **emergent from `hullRadius` rather than tagged.**
A `shipType` would be the first thing in the codebase to reintroduce vessel classes, and everything
downstream would begin branching on it. **If a size distinction is ever needed, derive it from a radius
or mass ratio — a comparison, never a tag.**

#### Altitude bands — designed, and deferred 🧊

*Recorded in full 2026-08-08 rather than left as "maybe," so that whoever picks this up gets the
version that works instead of re-deriving the one that breaks.*

**Vocabulary, and keep these apart:** §3.5's five-stack is a **draw layer** — render order *within* a
rig. An **altitude band** is world-space height affecting collision between vessels. The earlier
"Z-layer / Z-level" naming differed by three characters and would have been confused; these do not.

**The model that works:**

| Rule | |
|---|---|
| **Scope** | Bands affect **only** vessel-vessel collision and which hull draws on top. Projectiles, shields, weapons, and sensors ignore them entirely |
| **Occupancy scales with hull size** | Fighter 1 band · corvette 2 · capital and station **all 3** |
| **Readability is mandatory** | A **drop shadow offset by band**. Sprite scale is spoken for by §3.5's hull sizes and colour by rarity, faction, and status |
| **Changing band is a commitment** | A transition window during which you cannot fire or change again |
| **Binding** | `Space` climb / `Ctrl` dive — free in §3.6 and outside the crowded WASD cluster |

**Size-scaled occupancy is what makes it coherent.** A capital blocks every band, so §3.7's earned
holes survive intact — you still cannot go over a dreadnought, only through what you have stripped.
And with altitude readable, ramming is not neutered but **contested**: feint, match, commit.

**Why it is deferred rather than built:**

- **Unreadable altitude would be a dice roll, not attentiveness.** `architecture.md` §12.14 item 17
  rejected exactly this shape — *"precision aiming cannot be the core combat decision on top of a
  tie-break the player cannot see or predict."* The shadow requirement above is not optional.
- **It is the only feature that adds a positional dimension**, and every spatial system afterward
  inherits it — `DockingSystem`'s proximity, `PartySystem`'s formations, `SpawnSystem`'s placement,
  §4.3's squad orders. Each must respect bands or ignore them, and any that ignores them
  inconsistently is a defect nobody notices for months.
- **It solves a problem nobody has observed.** Nothing here has been flown; `architecture.md` §10 notes
  the vertical slice's behavioural claims are unit-tested and *"not yet confirmed in an actual play
  session."* Collision pinball in a furball is plausible, not measured.

**Revisit trigger:** when escort-fighter behaviour, docking interactions, and formation-keeping are
specified in detail — those are the systems that would have to know about bands, so that is the
conversation where the cost is actually visible. If built, the shadow cue and NPC band usage belong in
the **same** issue: a mechanic the AI cannot use violates §6.3, and one the player cannot see violates
§12.14 item 17.


---

### 3.8 Environmental Hazards 📋

*Settled 2026-08-09. Prompted by `architecture.md` §13's wiring audit, which found that the sun
already out-accelerates a fighter ten to one and that nothing happens at the bottom of the fall.
The mechanic below was largely already built; only the consequence was missing. Architecture home:
`architecture.md` §12.28.*

**A star is terrain, not a wall.** It has four boundaries, and each one is legible from the one
before it:

| Distance | What it means |
|---:|---|
| **2,200** | **Gravity begins.** You feel the pull and start correcting for it |
| **≈1,500** | **The point of no return.** Gravity out-accelerates a fighter's engines. Emergent from the numbers, not authored |
| **1,200** | **The corona.** Hull begins to burn, scaling with depth |
| **350** | **The surface.** Seconds to live |

**The point of no return is not a line the game draws.** It is the distance at which the star's
pull exceeds a hull's thrust, so it differs per ship: a heavy freighter is committed sooner than an
interceptor, and stripping a hull's engines (§3.2) moves it outward. Nothing displays it. The
warning is the ship handling worse and worse as you descend, which is the same information without
a number.

#### Burning, not a kill line

A star damages by **accruing hull damage that scales with depth**, not by killing at a radius.
Four things follow, and the last two are the reason:

- **Death needs no special rule.** A hull whose hardpoints all burn away is destroyed the same way
  a hull shot apart is (§3.2). There is no "killed by star" state.
- **Damage is localized like everything else.** The corona is a volume and hardpoints are
  physically placed (§3.5), so **a capital half inside it burns only on the side that is in.** A
  large hull can dip a wing where a fighter would be swallowed.
- **Shields matter, and the interaction was not designed.** Corona damage is Energy (§3.1), so an
  Energy-absorbing shield lets you dive deeper and hold longer. That falls out of the existing
  damage roster rather than being a rule about stars.
- **Grazing the star is real play.** Diving to shake a pursuer, or forcing a heavier hull to break
  off a chase it cannot follow you into, are both live tactics — and they are only tactics because
  the star hurts continuously rather than killing instantly.

*A lethal radius was considered and rejected.* It needs a destruction rule of its own, it makes the
star a wall rather than a place, and crossing an invisible line is the "gotcha the game sprang"
that §2.4 explicitly rules out elsewhere. A hull bar visibly falling is a warning; a threshold is
not.

**Symmetric for NPCs and for the world** (§6.3). Nothing about this checks who owns the hull, so an
AI ship chased into a corona burns on the same terms — and a hostile that follows you in is making
the same mistake you would be.

#### Planets are background

**Everything flies over a planet.** They orbit, they are drawn behind every ship in the system, and
they have no collision, no gravity of their own, and no hazard. A planet is scenery and a
navigational landmark, not an obstacle.

*This is a decision, not an omission.* Contact with a celestial body is a real future feature —
§3.7 already has the vocabulary for the damage side of it — but it needs line-of-sight, sheltering,
and an occlusion model to mean anything. **Revisit trigger:** the first time a body is expected to
block a shot or hide a ship. Until then, "you can hide behind a planet" is not a promise the game
makes.

#### The asteroid belt orbits, and it sits outside the danger

Asteroids **orbit the star** rather than drifting freely. They never fall in, never drain toward the
centre, and their positions are a deterministic function of elapsed time — the same treatment
planets get (§7.1).

The belt is placed **entirely outside both the corona and the point of no return**, close enough
that mining its inner edge still means fighting the star's pull but never so close that a laden
miner cannot climb back out. *An earlier band placed half the belt inside the point of no return,
which would have made the inner belt a one-way trip the moment mining became reachable.*

#### Nebulae — hazardous territory, not a hazardous errand 📋

*Settled 2026-08-09. The second hazard, and it arrives as **content for `HazardSystem`** rather than
as a system of its own — which is the thing the corona was built general for.*

> **A nebula is a property of a whole system, not a region within one.** Some systems are nebular.
> Everything in them — asteroids, gas giants, planets, stations, fleets — sits inside the hazard.

**Resources inside are ordinary.** Nebular systems hold the same asteroids and gas giants as anywhere
else, gathered the same two ways (§2.10). What is scarce is not the material; it is **the ability to
be there at all.**

##### Hazard armour is a threshold, and it is a territory gate

> **If a hull's aggregate `Inert` (§2.10) meets the nebula's severity, it takes zero damage. Below
> that, it burns.**

**Binary, not scaled**, and deliberately so. A threshold makes hazard armour a capability you either
have or lack — a clean strategic fact — where a sliding scale would turn every station inside a nebula
into permanent chip-damage bookkeeping, converting a strategic advantage into a maintenance chore.

This is what makes `Inert` a **strategic** attribute rather than only a tactical one, and it is the
sharpest asymmetry in the game's territorial layer:

- **A faction with good hazard armour can hold systems nobody else can enter.** Not "holds them more
  easily" — *can enter at all.* That is a genuine reason to invest in a material axis, and §5.1's
  Three Pillars and §6's expansion facets both want an asymmetry of exactly this kind.
- **A nebula is a sanctuary.** Basing a fleet or a station inside one is safe from anyone who cannot
  follow you in, which makes it worth taking and worth defending.
- **It is asymmetric warfare in both directions** — a weaker faction with the right hulls can hold
  ground against a stronger one that lacks them.

##### Sensors are degraded, never denied

A nebula **reduces** sensor strength rather than blocking detection outright. Classic terrain, and it
lands directly on the signature/detection model agreed in §8.3: a fleet inside a nebula is harder to
see, not invisible, so scouting it is expensive rather than impossible.

##### It is semi-transparent, and that is a rendering commitment

**You can see into a nebula.** It renders as a hazy, semi-transparent field — things inside are
visible but obscured, which matches the sensor rule rather than fighting it: *degraded, not denied*,
consistently in both the fiction and the UI.

> 🎯 **This is the case `architecture.md` §12.28 predicted by name.** That section fixed the world
> draw order and stated: *"Nothing today wants a world body in front of a rig. If something ever does
> — **a nebula the player flies into**, a corona bloom — it is a new pass, not a reordered
> enumerator."* A nebula haze draws **over** everything inside it, so it is a `DrawWorld` pass after
> `DrawProjectiles`, **not** a `BodyKind` value. The rule was written before the case arrived; follow
> it.

#### One shape, more hazards later

Radiation belts and minefields are the same mechanic again — *"a volume that damages what is inside
it"* — with different numbers and a different damage type. Neither is specified here and neither is
promised. **The star was built as an instance of a general thing rather than as a special case, and
the nebula above is the proof that it worked**: an entire strategic layer arrived as content for a
built system.


---

### 3.9 The Status Display 📋

*Settled 2026-08-09. This is the display **two settled sections have been asking for and neither
specified**: §3.5's "the current target showing live hardpoint status is how a player reads what they
have already stripped," and §3.1's "coverage gaps become visible — you can see which part of an enemy
capital is unshielded and aim there." One object serves both.*

> **A schematic of a rig: the hull outline, a circle per hardpoint, and a loop per shield. Colour
> carries condition, shape carries identity, and the loop's shape carries coverage.**

It is the same object for the player's own ship, for the current target, and — degraded — for a map
marker. Not three designs.

#### Colour is condition. Shape is identity. Everywhere.

*This is a change to built code.* `WorldRenderer` currently tints hardpoints by `ShellKind` — engine
orange, weapon red, shield sky, facility violet — which spends colour on *what a thing is*. Integrity
needs that channel more.

| Encoded by | Carries |
|---|---|
| **Colour** | Integrity — a continuous green → yellow → orange → red gradient |
| **Glyph** | Hardpoint kind, drawn inside the circle |
| **Outline shape** | Shield coverage |
| **Dash density** | Shield charge |

Glyphs start as **monogram placeholders**, the same stand-in the build and buy menus already use
because no per-item art exists. Real iconography replaces them when the asset pipeline un-defers (§6).

#### The palette, and why kinetic is purple

**Integrity owns green through red**, so every other meaning must stay clear of that range or become
ambiguous with a damaged hull.

| | Colour |
|---|---|
| Integrity | green → yellow → orange → red |
| **Energy** shield and projectile | **Blue** |
| **Kinetic** shield and projectile | **Purple** |
| **Ion** projectile *(no shield absorbs it)* | **Electric white-blue** |

*Purple was chosen over the intuitive yellow specifically because yellow sits inside the integrity
gradient.* A yellow kinetic shield beside an amber damaged hull is unreadable, and that failure is
worse than any gain from a conventional colour.

**One colour per damage type, used in all three places** — projectile tracer, in-world shield shimmer
(§3.1's draw layer 5), and this display. The whole pitch of §3.1 is *"read the enemy and bring the
mismatched weapon"*, and that requires the shot and the shield it bounces off to be visibly the same
family.

🐛 **Ion and Kinetic are currently identical.** `WorldRenderer::ColorForProjectile` is
`Energy ? SKYBLUE : YELLOW`, so two of §3.1's three weapon types render the same. Ion must be the
*most* distinguishable, since it is absorbed by neither shield — mistaking it for energy means
misreading whether your shields are doing anything. Separating it by **luminance** rather than hue is
what keeps it legible against the blue field it is punching through.

#### Shields: an outline encloses what it covers

**A shield is a line, not a fill.** That single choice removes the occlusion problem entirely — an
outline hides nothing beneath it, so transparency stops having to carry any meaning.

§3.1's three coverage modes become three loop shapes with no special-casing:

| Mode | Loop |
|---|---|
| **Personal** | Around its own hardpoint's circle |
| **Bubble** | Around the group of hardpoints it reaches |
| **Conformal** | Around the hull outline |

An enemy capital therefore shows one loop around its bow batteries and **nothing** around its stern,
and the bare circles are where you shoot. That is §3.1's headline mechanic becoming readable for the
first time.

##### Charge is dash density, never arc length

⚠️ **A depleting arc was proposed and rejected.** Where the loop's shape carries coverage — Bubble and
Conformal, two of the three modes — a gap in the arc reads as *"this section is unshielded"* rather
than *"the shield is at 70%."* Coverage and charge would be competing for the same geometry.

> **The loop is always complete. Charge is how *solid* it is:** solid → dashed with widening gaps →
> sparse dots → absent.

The full circumference is present at every level, so a bubble at 10% visibly wraps the same hardpoints
it wrapped at 100%. The metaphor is right — a failing field breaks up rather than retracting — and it
fixes the readability floor that also ruled out **alpha**: a sparse dotted line is unmistakably a
line, where 5% alpha is indistinguishable from nothing at exactly the moment it matters most.

**One encoding for all three modes**, no branch on which one it is. Two shields covering the same set
are two nested loops dashing independently, and neither can be misread as a coverage gap.

*Exact numbers live in the readout, not the loop. The loop answers "is it holding?" at a glance.*

#### Detail level is chosen by fit, not by context

A 2,500-unit capital's 20-unit turret is 0.8% of its hull radius — in a 240px panel, a 2px dot. So the
display cannot simply scale a ship to fit.

> **Element size is fixed at whatever is legible. Detail adapts.** When hardpoints drawn at minimum
> legible size would **overlap**, the display collapses to the next level of the structural tree.

**`StructuralAttachment` is already that tree** — chassis → armour segments → functional mounts —
built for the damage model above, and it doubles as the LOD hierarchy for free:

| Fit | Draws | Each coloured by |
|---|---|---|
| Roomy — a fighter's six mounts | Every hardpoint | Its own integrity |
| Tight — a capital's fifty | **Chassis + armour segments** | Aggregate of that segment's children |
| Marker | Hull outline only | Whole-rig integrity |

A damaged capital reads as *"the port flank is red"* rather than as fifty dots — and at a glance that
is **more** useful than the detail, because it is the decision-relevant fact. Every level stays
spatially truthful, so *"shoot the port flank"* survives the collapse. **An abstract grid layout would
have destroyed exactly the property §3.1 wanted the display for.**

**The game chooses the level; the player does not.** The detailed view is not a docked-only or
paused-only mode — it appears in the main game view whenever there is room for it.

*Manual zoom and pan were proposed and superseded by fit-based selection.* §4.4's *"no pause, and it
constrains the UI"* rules out sustained manipulation during combat, and automatic selection removes
the need for it. **A single click to expand a section stays legal** — §4.4 constrains sustained
attention, not all input.

#### Why this keeps earning its place

The structural tree now answers four unrelated questions — cascade destruction, damage locality, hull
coverage validation, and display LOD. **When one piece of data keeps answering questions it was not
designed for, it is usually the right piece of data.**


---

### 3.10 The Flight HUD 📋

*Settled 2026-08-09. §9 called UI/UX "the largest missing document"; this is the first screen of it.
Everything here is read **while being shot at** — §4.4's "no pause, and it constrains the UI" is the
governing rule, and anything demanding sustained attention is disqualified.*

**What exists today: one hull bar.** `CockpitHud::Draw` renders a single aggregate rectangle. Ten of
the eleven surfaces below have no representation at all.

#### Layout

Screen is 1600×900. **The middle stays flyable** — §3.5 requires a dreadnought pilot to zoom out to
see their own hull, so the viewport is contested by the world itself.

| Region | Carries |
|---|---|
| **Bottom band, ~200px** | Three clusters with world visible between them — **not** a solid strip |
| ├ *left* | Speed · fuel · power categories (`F`/`G`/`H`/`J` with boost state) · weapon groups |
| ├ *centre* | **Your status projection** (§3.9). Its diameter sets the band height |
| └ *right* | The module-gated button bar · comms ticker |
| **Top centre** | **Target status projection** (§3.9), sensor-gated |
| **Screen edges** | Hazard tint · sensor contacts · directional damage indicators. **Zero layout cost** |
| **Overlays, on demand** | Orders · build queue · **inventory · loadout** · navigation map |

*The band height follows the status projection rather than a chosen fraction. A solid full-width
strip was considered and rejected — 1600×200 of continuous chrome, where three clusters give the same
information and let the world through.*

#### The HUD is emergent from your loadout

> **A HUD surface is available exactly when a living module provides it** — the same rule §12.24 step
> 5a applies to docked menus, and the same rule `Facility.h` already states for Bridge tabs.

| Surface | Requires |
|---|---|
| Target detail · contacts | **Sensor** module |
| Comms log · hailing | **Comms** module (§12.27) |
| Orders · selection | **Crew** module with a non-zero `command` roll (§12.27) |
| Build placement | **Construction** facility (§12.26) |
| Fuel · jump readiness | **Hyperdrive** (§2.11) |

**This makes the HUD a fitting decision** — you choose what you can *see* by what you bolt on, which
is §2.2's constraints puzzle extended to information. A stripped interceptor flies nearly blind; a
command ship sees everything; **no rule anywhere mentions a vessel class** (Law 4).

It also makes §12.27's symmetry visible. That section already has destroying a hostile's comms degrade
their coordination — now the same happens to you, **and you watch it happen.**

##### Degrade, never remove

⚠️ **A control that vanishes mid-fight creates "was that there a second ago?"** — and §8.3's rule
applies directly: *absence must never look like emptiness.*

- **Button slots are fixed.** A dead module disables its slot and states why (`SENSORS OFFLINE`); the
  bar never resizes as hardpoints die.
- **An open overlay closes with a notification**, never silently.

##### Capability comes from the rolled stat, not a level field

§2.7 already rolls every capability stat from a quality band, so sensor *strength* is continuous and
free. **Do not add a level integer.** `FacilityRef::level` exists but is scoped to Engineering merge
scaling — and `architecture.md` §13.3 K found it is **never parsed from JSON and never copied at
attach time**, so it is broken as well as wrong for this.

#### The target projection

**Populated by hovering the cursor over an object, and sticky until replaced** — it changes only when
you hover something else or the current subject dies.

> ⚠️ **This is not the target lock §3.2 rejects.** It selects *information*, not aim: shots still
> follow the cursor (§12.24's `AimPoint`). §3.2's objection to subtarget cycling was that it *"would
> return the same information at no risk"* — hover-selection costs exactly what shooting costs, and is
> **strictly more expensive than cycling**, since you can inspect only one hull at a time and only by
> aiming at it.

**What it shows is sensor-gated**, which merges cleanly with §3.9's fit-based LOD into one rule:

> **Detail shown = the lesser of what fits and what you can sense.**

A distant hull with poor sensors is an outline and an integrity colour; close with good sensors, every
hardpoint and shield loop. **Sensor quality comes from Optical and Semiconductive elements (§2.10)**,
so the elements a hull is built from decide how much its pilot can learn about an enemy.

#### Sensor contacts — edge indicators, and there is no "radar"

*A circular radar panel was considered and rejected.* In a top-down view a mini-map is a **scaled copy
of the viewport**, so its only unique value is off-screen contacts — which edge indicators give
directly, without the cognitive step of mapping a blip onto the world that §4.4 forbids in combat.
§8.1's zoom continuum already answers *"show me the wider picture."*

> **Sensor contacts are one data source with two presentations** — edge indicators when you cannot
> look away, the navigation map when you can. No `RadarSystem`, no second detection model.

The split is real, not redundant: `SensorRange` reaches 2000 units while the viewport reaches roughly
800 to each side, so **the 800–2000 band is detected-but-off-screen** — a large ring that is precisely
what edge indicators exist for.

##### Edge indicators are a sensor-module capability

> **You can always see out of the window. Seeing past it is equipment.**

On-screen contacts need no module — they are simply visible. **Off-screen awareness is the sensor
module's entire product**, so a hull without one has *no edge indicators at all* and knows only what is
in frame.

That makes **shooting out an enemy's sensor a real tactic**: it does not blind them, it blinds them to
everything they are not already looking at — and §12.27's symmetry means the same is done to you.

**The rolled stat (§2.7) scales two things**, not one:

| Sensor quality raises | Effect |
|---|---|
| **Range** | How far the contact ring reaches — replacing `RigFactory`'s hardcoded `SensorRange` of 2000 (`architecture.md` §13) |
| **Resolution** | How far up the ladder a contact resolves: bare marker → class → faction → relation |

So a cheap sensor gives unresolved markers at short range and a good one gives classified contacts at
long range, from the same elements (Optical, Semiconductive) that decide how much the target
projection can tell you. **One stat, consistent everywhere it appears.**

| Channel | Carries |
|---|---|
| **Shape** | Class — light triangle, heavier chevron, square station |
| **Glyph** | Faction insignia; monogram placeholder until art exists (§8.2) |
| **Colour** | **Relation only** — hostile / neutral / friendly |
| **Numeral** | Range, **only on contacts above a threat threshold** |

**Colour is not faction.** §3.9 spends colour on condition and damage type; faction would be a third
meaning with no hue space left, and §5.2's registry holds far more factions than distinguishable
colours anyway. Red on a contact and red on a hull both mean *bad for you*, so the semantic stays
consistent instead of collided.

**Range is a numeral, not arrow size** — size already carries class — and it is shown sparingly. A
screen edge ringed with numbers reads worse than one showing three.

##### The unresolved contact is the important one

Sensor gating means many contacts resolve as *something is there* with no class, faction or relation.
§8.3 names this the hardest job in the UI, so it needs a deliberate form: **a bare marker, no glyph, no
relation colour, visibly incomplete rather than merely plain.** That shape is what tells the player to
go and look.

⚠️ Three-state relation is not computable today — §13 found `TargetingSystem` treats *any* different
faction as hostile and never consults `DiplomacyMatrix`. Contacts ship as hostile/unknown until
§12.24 step 6 supplies `ctx.diplomacy`.

#### Hazards use the screen edge, not a panel

Corona and nebula proximity (§3.8) must be unmissable **without looking down at the band** — which is
the one thing a bottom-strip element cannot do.

**Tint the screen edge**: amber vignette in a corona, purple-grey haze in a nebula. Costs no layout,
cannot be missed, and in the nebula case the warning *is* the effect — §3.8 already renders nebulae as
semi-transparent haze.

**Pairing: tint alerts, indicator explains.** The edge says *something is happening*; a small readout
in the band says what and how much. Degraded sensors get the same treatment, and must, or a target
showing three hardpoints is ambiguous between *it has three* and *I can only see three*.

#### Overlays, and why opening one has to cost something

Orders, build queue, inventory, loadout and the navigation map open over the viewport, from the button
bar or their §3.6 key.

- **Semi-transparent and offset from centre** — you can still fly.
- **They do not pause** (§3.4). The system menu remains the only pause (§12.29).
- **Opening one is a real tactical cost**, and that is deliberate. If reading were free, §3.2's "no
  risk" objection returns through a different door. This is the same bargain docking already makes.

> ✅ **Two of these five already exist as code, misfiled as docked menus** (settled 2026-08-10,
> `architecture.md` §12.30). **`StorageMenu` is the inventory overlay and `ModulesMenu` is the loadout
> overlay** — neither is facility-gated and neither belongs in the docked-menu router. `architecture.md`
> §12.24 step 5a had gated `StorageMenu` on a Storage facility; that row is superseded, because §2.7
> makes **live refit unrestricted** and a station gate on reading your own manifest would put it out of
> reach in exactly the fight it was legalised for.
>
> **They are not trivial for having moved.** They need the same widget layer, the same row model, and
> the same input plumbing as the docked screens, and they ship in that batch — keyed on a button here
> rather than on a tab. What they shed is the facility gate and §3.4's per-screen hardpoint readout.
>
> 🐛 **Specified 2026-08-10 in `architecture.md` §12.30.7, and the loadout half was worse than
> "not trivial."** `modules_menu::EquippableMounts` lists every hardpoint without an `EquippedModule`
> tag as an empty slot — and that tag is only applied to *runtime* mounts, never to a ship's own
> blueprint loadout. **So on a freshly spawned vessel every occupied hardpoint reads as free**, and the
> list of things you can unmount is empty. Fitting a module there overwrites the original's live
> components; unmounting afterwards **destroys** the original, and scrapping the hardpoint
> **duplicates** it. Both are one click from a surface in this batch, so the overlay must not be wired
> before `architecture.md` §13.4 decision 2 lands.
>
> ✅ **The inventory overlay gains one verb: jettison.** With cargo capacity enforced on every write,
> unmount, scrap and deconstruct all *fill* the hold and only selling at a Trade station drains it — so
> a full hold locks refit out entirely. Jettison closes that, and gives the loose-drop components their
> first producer in the codebase.
>
> **Both open while docked as well as in flight**, over the full-screen docked frame — an overlay is
> defined by being *over* something, not by what it is over. The loadout overlay is the only refit
> surface there is, so a station is exactly where it must work.

#### Every surface has a home

| Surface | Where |
|---|---|
| Integrity · shields | Centre projection |
| Power categories · weapon groups · fuel · speed | Left cluster |
| Target status | Top centre |
| Sensor contacts | Screen edge |
| Hazard warning | Screen edge tint |
| Comms log | Right cluster ticker |
| Selection · order queue | Overlay |


---

## 4. The Bridge & Fleet Command 📋

A control scheme that layers an RTS command surface **on top of** flight, rather than switching
between the two.

**Command is equipment, not a place** *(revised 2026-08-09 — see §4.0)*. Commanding requires a
`Crew` module that rolled into `command` for authority, and a `Comms` module for reach, both on the
rig you are commanding from. Fly a fighter carrying them and you command from the cockpit; fly one
without and you cannot command at all, wherever you are standing.

**Physical docking still matters, for a different reason.** Boarding a capital or station and taking
its bridge is how you come to operate *that hull* — and a capital carries better crew and comms than
a fighter can, so reach and authority scale with the vessel you are aboard. The transition remains
diegetic; what it no longer is, is the only path to giving an order.

**Component-driven menus** — the Bridge UI generates from physical modules. A Manufacturing
hardpoint enables ship construction; absence of a Repair hardpoint disables healing for docked
vessels. Destroying a hardpoint removes its tab mid-session.

**RTS directives & AI autonomy** — the player issues Move, Attack, Defend, and Build against
selected units. Uncommanded AI defaults to self-preservation, patrols, and role-based tasks.

**Command inherits the death rule** — §3.3 applies to the vessel being commanded from. Taking the
Bridge of a capital does not make the player safe; it makes them a larger target.

### 4.0 Operating and commanding are simultaneous 📋

> ⚠️ **Reversed 2026-08-09. This section previously specified Operator and Commander as two
> exclusive player modes, settled 2026-08-08.** That is withdrawn in full. The player now flies,
> shoots, and commands at the same time, with no mode switch anywhere. The superseded model and why
> it was abandoned are recorded at the end of this subsection, because the reasoning it was built on
> is still worth knowing.

> **The player is always operating the hull they occupy. Commanding is an overlay on top of that,
> never an alternative to it.**

This holds identically at both scales, which is the point:

| Where the player is | They operate | They command |
|---|---|---|
| **Their own fighter** | Fly and aim it manually (§3.2) | Any unit in range, if the fighter carries `Comms` + a commanding `Crew` module |
| **A boarded capital's bridge** | Fly and aim *the capital* | Any unit in range, at the capital's better reach and authority |

Boarding does not change the *kind* of thing the player is doing — §3.4 already settled that the
player is always associated with exactly one shell, and taking a bridge simply makes that shell a
capital's bridge instead of a fighter's cockpit. Their fighter waits in the docking bay (§4.1),
cannot be shot, and dies with the host.

**"Assume command" is not a new mechanism.** §3.4 specifies that docking places the player in the
bay, from which they move to a facility — *"engineering, manufacturing, research, the bridge"* — by
selecting it, and that while they are there they **are** in that hardpoint. Taking the bridge is that
selection, through the same docked-facility router every other station service uses
(`architecture.md` §12.24). The death rule comes along for free: blow up the bridge and the player
dies, even if the capital survives.

#### What gates commanding now 📋

*Settled 2026-08-09.* Two modules, on **the rig the player is commanding from** — not on the player,
and not on the units being commanded:

| Module | Supplies | Without it |
|---|---|---|
| **A `Crew` module with a non-zero `command` roll** | **Authority** — the ability to issue orders at all, how many units may be held selected, and how deep an order queue may go | No commanding, at any range |
| **`Comms`** | **Reach** — the radius within which units are selectable and commandable | Commanding is limited to nothing useful |

**Both are required.** A rig with comms but no commanding crew cannot command; one with a commanding
crew and no comms has authority it cannot project. That makes the pairing a real fit cost rather than
a checkbox, and it is symmetric for NPCs (§6.3) — which is what makes an enemy commander's comms
module a worthwhile thing to shoot.

**A single `Crew` module covers flying *and* commanding** (§2.7, consolidated the same day), so a
fighter's one cockpit slot is enough for both. What it cannot do is be good at both: the roll's budget
is shared, so a fighter-commander that rolled heavily into `command` flies at close to baseline. That
is the trade, and it is why an NPC fighter-commander is a real but costly thing to field — it is also
one lucky cockpit shot from decapitation, since crew and shell die together (§2.7).

#### What the `operation` rating is for, restated 📋

Losing the two-mode model removes the justification §4.0 previously gave for a piloting rating, so it
needs restating rather than quietly inheriting the old one.

**A crew module's `operation` rating does nothing in a hull the player is personally operating.**
§2.7 already said this directly — *"the pilot rating does nothing on a ship the player is personally
flying"* — because §3.2 makes the player's aim manual, so their marksmanship *is* their piloting
skill. Under the old model the rating earned its slot by flying the hull during a mode switch; with
no mode switch, that job is gone.

What remains is larger, not smaller: **`operation` matters on every hull the player owns but is not
sitting in**, which §2.7 already calls "most of a developed fleet." And it is what makes the roll a
real decision for the player's own cockpit: they want a crew module that spent its budget on
`command`, because the `operation` half would be dead weight in a hull they are flying themselves.

**The two-berth bridge survives**, for a better reason than §4.0 previously gave. One crew module is
enough to fly a capital *and* command from it, so the second berth is never mandatory — but two
specialists beat one generalist, since each function takes the best rating available for it (§2.7).
And the moment the player leaves that capital, both jobs fall to the crew aboard, which is when a
well-staffed bridge pays for itself.

#### The superseded two-mode model, and why it went 📋

*Recorded rather than deleted, because it was a good design and the reason it lost is instructive.*

The model held that a player aboard a bridge was in exactly one of Operator or Commander at a time.
Because there is no pause (§3.4), whichever job you were not doing was not being done — so a
fully-staffed bridge bought *freedom to switch*, an empty one meant every switch abandoned a post,
and crew modules became the core fit decision of a player-commanded capital. It was the clearest
answer the design had to what crew modules are *for*.

**It was abandoned because the mode switch itself was the least interesting part of it.** Once
commanding is reduced to selecting a unit and pressing an order key (§4.3), it costs a fraction of a
second and no cursor time — so a mode built to make that cost *legible* was charging the player for
something that is no longer expensive. Keeping it would have meant preserving an artificial cost to
justify a fit decision, when §2.7's own account of `Operator` (above) justifies the same fit decision
without it.

**What was genuinely lost:** commanding no longer competes with flying for the player's attention, so
a player with the right two modules is strictly more capable than one without, rather than facing a
moment-to-moment tradeoff. The compensating cost is the fit cost — two modules, two slots, mass, and
a cockpit berth that could have held an `Operator` on any hull the player is not flying.

### 4.1 Boarding, the bay, and the ship you arrived in 📋

*Settled 2026-08-08 — this resolves the open question of whether the player's fighter persists.*

**It persists, in the bay.** The player flies to a capital or station, docks in its docking bay, and
walks to the bridge. The fighter sits in that bay for the whole visit: it cannot be shot (§3.4), and
it dies only if the host does.

**The bay is the player's garage.** On leaving the bridge, the player may re-board the vessel they
arrived in *or* take any other vessel they own that is sitting in that bay — including one bought or
manufactured during the visit.

This makes §3.3's "stashed backup ship" concrete rather than aspirational: the stash is a real place,
with a real location, that an enemy can destroy. It also gives capitals and stations a logistical
purpose beyond their guns.

**Bay capacity is a real limit**, and a natural tier-scaled property of the docking-bay shell (§2.2):
a Common bay berths one or two vessels, a Mythic one a squadron. Vessels cannot be summoned from
elsewhere — swapping is limited to what is physically parked here, which keeps fleet logistics a
thing the player must actually solve.

### 4.2 Two scales of command, one interface 📋

*Raised 2026-08-08. The design has been carrying two different command vocabularies without noticing:
`features.md` §4 says Move / Attack / Defend / Build, while `CommanderOrders` in code says Dispatch /
Retreat / Defend.*

**They are not a contradiction. They are the same command surface at two simulation tiers (§1.1):**

| | **Tactical** | **Strategic** |
|---|---|---|
| Scope | The resident system | Everywhere else |
| Tier | 1 | 2–3 |
| Orders | **Move · Attack · Defend · Build** | **Dispatch · Retreat · Defend** |
| Targets | Entities you can point at | `core/galaxy/` records |
| Unit of command | A **party** (§4.3) | A fleet as an abstract record |
| Backing | Real positions, real steering | Origin, destination, ETA |

Tactical orders can name a position and a target because both exist. Strategic orders cannot — Tier 3
has no registry, no steering, and no projectiles — so they resolve as scheduled arrivals and outcome
rolls. `CommanderOrders`' existing three are the **strategic** set, which is correct for a
sub-commander running a fleet in a system the player is not in. The tactical four need a separate,
per-party home.

> ⚠️ **Two amendments, 2026-08-09.** First, **tactical command no longer requires a bridge** — it is
> gated on carrying `Commander` + `Comms` modules (§4.0), so a fighter so equipped commands in its
> resident system. Second, `CommanderOrders`' three values are better read as **stances plus an AI
> override** than as strategic orders (§4.3); the strategic *order* set — dispatch a fleet, set a
> system's build queue — is still unspecified and still waits on galactic coordinates
> (`architecture.md` §12.17), since "move that fleet there" has no *there* until systems have
> positions.
>
> ❓ **Whether the strategic layer stays bridge-gated is deliberately still open.** Tactical command
> travelling with the player does not settle it, and it is the one gating question left in §4.

> **The Navigation Map (§8) is this interface — they are one feature, not two.** §8.1 already says
> Zoom Level 3 shows "planets, belts, stations, and **real-time fleet assets** — the RTS command
> surface (§4)." Zoom Levels 1–2 are where strategic orders are issued. Building the Bridge's
> command layer *is* building the navigation map, and the two sections should be specified and
> implemented together rather than as separate features that later have to be reconciled.

### 4.3 The unit of command is a party 📋

*Settled 2026-08-08.* Tactical orders address a **party** — `PartySystem` already provides
formation-keeping and shared retaliation, so a fleet is a party with a leader rather than a second
grouping concept invented alongside it.

⚠️ **Parties are registry-local**, by their own definition. That is correct for tactical command and
insufficient for strategic: a fleet in another system has no registry to be a party in. **Strategic
fleets therefore need a `core/galaxy/` record**, and the promotion/demotion path between the two is
the same one §1.1 already specifies for everything else crossing a tier boundary.

#### Commanding from a fighter: full selection, and the input scheme that allows it 📋

> ⚠️ **Reversed 2026-08-09.** This subsection previously granted a pilot only three *cursor-free*
> verbs (Attack my target · Form on me · Retreat) on the grounds that the cursor cannot mean two
> things at once, and reserved full RTS command for a bridge. **Full selection now works from a
> fighter.** The old objection was correct about the cursor and wrong about the conclusion: the fix
> is not to remove selection, it is to stop routing the *middle* of the interaction through the
> cursor at all.

**The constraint is real and unchanged.** §3.2 makes the cursor a *continuous* aim point — turrets
slew toward it every frame — so any interaction that parks the cursor on a menu stops the player
aiming. §3.4 forbids pausing, and §4.4 requires order verbs reachable "in roughly one action each."

**The resolution: select and issue with the free mouse button, choose the verb on the keyboard.**

| Step | Input | Cursor cost |
|---|---|---|
| Select a unit | **right-click** it | one instant |
| Box-select | **right-click-drag** | one drag |
| Add to the selection | **`Shift`** + either of the above | none — a modifier |
| Select all of a kind on screen | **double-right-click** a unit — matches by `BlueprintId` | one instant |
| Read the available orders | HUD panel, *displayed not navigated* | **none** |
| Arm an order | **`Z`/`X`/`C`/`V`/`N`** (§3.6) | **none** |
| Issue it | **right-click** in the viewport or on the tactical map | one instant |
| **Append** rather than replace the queue | **`Shift`** + right-click | none — the same modifier |
| Clear selection | **`Esc`** | none |

**Left click keeps firing throughout, and the aim point is never surrendered** — only interrupted for
the instants of two right-clicks. That is what makes commanding-while-dogfighting real rather than
nominal, and it satisfies §4.4's one-action rule better than a popup menu would.

> **`Shift` means *add*, in both senses.** Add a unit to the selection, add an order to the queue —
> one modifier, one mental model, and the convention every RTS player already has. *This replaced a
> click-replaces / drag-adds gesture split that existed only because `Shift` was the afterburner; the
> afterburner moved to `Ctrl` on 2026-08-09 (§3.6) and the workaround was deleted.*

**Weapons are never disabled *by commanding*.** An earlier proposal to free the cursor by switching
weapons off was rejected for the same reason §4.3 originally gave: §3.4 says there are no safe zones.
Giving an order never takes the guns away. *(Per-group weapon toggles on `1`–`0` are a separate,
deliberate combat control — §3.6.)*

> **`BlueprintId` is what "same type" means.** Law 4 removed vessel classes deliberately — a fighter,
> a capital, and a station are all rigs — so blueprint identity is the only honest definition of
> "select all of these." Consequence, accepted deliberately: two ships from the same Template match,
> and a custom variant does not, however similar it looks. That is *"select all my Vanguards,"* which
> is the useful reading.

##### Orders, stance, and the queue 📋

*Settled 2026-08-09.* Three separate things, and conflating any two of them causes trouble:

| | What it is | Set by |
|---|---|---|
| **The order queue** | An ordered list of tasks a unit works through, front to back | Player, right-click |
| **Stance** | How the unit behaves toward hostiles *while* doing whatever it is doing | Player toggle |
| **AI default** | What it does with an empty queue | Nothing — it is the fallback |

**Orders queue rather than replace.** A constructor told to move, then build, then defend the player
does all three in that order. A build is **one entry** in that queue and occupies the unit for its
duration — the constructor travels, builds, and only then moves on. That is what makes a shipyard a
committed asset rather than a button, and what gives escorting one to the frontier any weight.

**Stance is orthogonal to the queue**, and it is the third axis the design was missing:

| Stance | Behaviour |
|---|---|
| **Hostile** *(default)* | Engages any hostile it gets the opportunity to engage, while carrying out its orders |
| **Defensive** | Engages only after being fired upon |
| **Peaceful** | Never returns fire. Takes damage and keeps going — which is what makes running a blockade a real option |

> ⚠️ **This reveals that `CommanderOrders { Dispatch, Retreat, Defend }` was never an order list.**
> `Defend` is a posture, `Retreat` is a posture, and `Dispatch` means literally "engaged, no
> override." It is a *stance* enum that was named for orders. §4.2's strategic/tactical split
> (below) still holds, but the strategic set should be re-read as stances plus an AI override, not
> as peers of Move/Attack/Defend/Build.

**`Retreat` becomes the one AI behaviour allowed to interrupt a player order.** `CommanderSystem`
already escalates a badly damaged commander to Retreat unprompted. Player orders otherwise take
absolute precedence, and a unit with an empty queue falls back to default AI — so the arbitration
rule is: *player order wins; Retreat interrupts; empty queue means AI.*

**An impossible order is dropped and the queue advances.** Attack target destroyed, build site
occupied — the unit does not halt and wait. **Local orders die when a unit leaves the system**, which
falls out of Law 2 for free: the order lives on a registry-local component, and warping out destroys
the entity that held it. Strategic orders live in `core/galaxy/` records instead, which is exactly
why the two tiers need separate homes (§4.2).

##### What orders a unit can accept is emergent 📋

*Settled 2026-08-09, and it is Law 4 applied to the command surface.*

> **A unit's available orders are a function of its living hardpoints, never of its type.**

| Order | Requires |
|---|---|
| **Move** | Non-zero `Propulsion` — i.e. at least one living engine hardpoint |
| **Attack** | At least one living weapon hardpoint |
| **Build** | A living `FacilityKind::Construction` hardpoint |
| **Defend** | Propulsion to escort an entity; nothing to hold a position |

**A station with engines can move**, which is the point — there is no static/mobile vessel class, only
rigs with or without propulsion. *This requires deleting the `mobile` flag's role in movement; see
`architecture.md` §12.25, where the code currently prevents it outright.*

**And destroying a unit's engines removes Move from its order list mid-session** — the same principle
§4 already applies to bridge tabs, arriving a second time without being implemented twice.

**A mixed selection offers the intersection.** Select a freighter, a fighter, and a station together
and you may issue only what all three can perform. That falls out of the rule above rather than
needing a compatibility table.

##### The player's own rig is not commandable 📋

The player cannot order the hull they are operating — they are already flying it. But **other units
can be ordered to defend it**, which is how "form up on me" works: a Defend order pointed at the
player's rig makes the ordering unit a party member escorting them.

> **This is what `PartySystem` has always been missing.** It provides formation-keeping and shared
> retaliation against `PartyLeader`/`PartyMember`, and **nothing anywhere creates either component.**
> A Defend order aimed at a friendly entity *is* a party membership with a formation offset, so a
> built and permanently inert system switches on through an order type the design wanted regardless.
> The order's executor distributes offsets among the units defending the same target.

#### Strategic resolution: values and rolls, not simulated battles 📋

*Settled 2026-08-08.* Outside the resident system, outcomes are decided by comparing **aggregate
fleet values** and rolling against them — never by running conditions and behaviours of the kind
Tier 1 supports.

**This is what the design already committed to**, and it is worth pointing at rather than
re-deciding: §1.1's LOD table gives Tier 2 "fleet-strength attrition rolls" and Tier 3 "outcome
resolved as a single event," and §6.4 already states two probability thresholds in exactly this shape
("Material Security below 30% → +45% chance to launch a resource-raiding fleet"; "border skirmish
rolled every macro tick, moderated by relative fleet strength").

**The machinery for it is also already built and unused.** `core/ai/FactionDecisionEngine` is a pure
evaluator whose `EvaluateRaidDispatch` takes the roll *as a parameter supplied by the caller* — the
exact shape this model wants. It has no caller. The strategic layer is less "design a simulation"
than "wire up the one that exists."

⚠️ **The rolls must be deterministic** — derived from a hash of `(entity or record id, tick)`, the
idiom `MiningSystem` and `CommsSystem` already use, never RNG state. Law 2's fast-forward requires a
skipped macro tick to resolve identically whether it was played through or banked, and a stateful
generator breaks that silently.

#### Military Weight 📋

*Settled 2026-08-08.* The aggregate a fleet is compared on. §1.1 and §8.1 both name it; neither
defined it.

**Two factors, multiplied — not one flat weighted sum:**

> **Military Weight = f( offence , survivability )**
>
> *offence* — damage output, range, penetration
> *survivability* — hull, shields, **and mobility**

*This revises an earlier draft that made mobility a small fraction of a single sum.* Mobility is not
a minor contributor to combat power — a manoeuvrable vessel is genuinely hard to kill, and at Tier 1
that is exactly how fights resolve. **If mobility wins fights when the player is watching, it must
win them when the player is not**, or the same fleet performs differently depending on where the
player happens to be standing — which breaks the whole promise of the LOD model.

**The multiplication is what keeps it honest.** A fast unarmed scout has real survivability and
near-zero offence, so its weight is near zero however nimble it is. A heavy gunship with no engines
is the mirror case. A fleet must be able to *both* hurt something and survive doing it, which is the
actual dynamic a flat sum cannot express.

Accumulate into a **`double`**.

**Tier needs no special case.** A Mythic-fitted ship has better underlying stats, so summing stats
captures its quality automatically — there is no separate tier multiplier to author or keep in sync.

**On precision and overflow, with the actual numbers.** Both concerns are real at different scales:

| Galaxy size | Rough total | `int32` (max 2.1 e9) | `double` |
|---|---|:---:|:---:|
| 500 systems × 50 ships × 5,000 | 1.25 e8 | ✅ fine | ✅ |
| 100,000 systems | 2.5 e10 | ❌ **overflows** | ✅ |
| 1,000,000 systems | 2.5 e11 | ❌ **overflows** | ✅ |

So at the galaxy sizes this design contemplates, **`int32` genuinely does overflow** and `double` is
the correct accumulator. Precision is not stressed: a 2.5 e11 total built from 5 e3 addends spans
about eight orders of magnitude, well inside `double`'s fifteen-to-sixteen significant digits.

**But do not pre-scale each ship down to a small fraction before summing.** That is the instinctive
fix for overflow and it introduces the opposite failure — small addends vanishing into a large
running total. Keep per-ship contributions at natural magnitude, accumulate in `double`, scale only
for display.

⚠️ **At a million systems the arithmetic is not the problem — the iteration is.** Any operation that
touches every system on a macro tick is the real scaling wall, and it is exactly what `features.md`
§9's missing performance budget exists to bound. Faction-wide totals should be maintained
incrementally at the per-system level rather than re-summed from a million records each tick.

#### Military Weight is a demotion artifact, not a cache 📋

Caching it with invalidation on "new ship, new facility, new resource" is the obvious design and
the wrong one: every write site that forgets to invalidate produces a stale number that the strategic
simulation then makes decisions from — a bug that is nearly invisible, because the galaxy simply
behaves slightly wrongly forever.

**The tier model already provides the answer.** At Tier 3 there are no entities to sum; a fleet *is*
a `core/galaxy/` record. So Military Weight is **computed once at demotion and stored in the record**,
which is exactly when the data is in hand and exactly when it must stop depending on entities. At
Tier 1 it is derived live from the resident registry, where it is cheap and always correct.

That is not a cache with invalidation rules — it is the same promotion/demotion path §1.1 already
specifies for everything else crossing a tier boundary, applied to one more value. *If* live Tier 1
derivation ever measures as too slow, the codebase's own rule applies: a cache lives as a component
on a singleton entity, never as a private field.

### 4.4 No pause, and it constrains the UI 📋

*Reaffirmed 2026-08-08.* §3.4 applies to the Bridge in full: the galaxy runs while orders are issued,
and the hull the player is commanding from is a live, targetable object the entire time. Realtime is
also what keeps multiplayer tractable — a pausing RTS layer in a shared session is not a feature that
can be added later.

**The design consequence is that orders must be fast to issue.** Every second spent in the command
surface is a second not spent flying, and on an understaffed bridge it is a second nobody is flying
at all. This rules out deep menu trees, multi-step targeting flows, and anything requiring precision
clicking under fire. Whatever §8's Level 3 ends up looking like, its order verbs need to be reachable
in roughly one action each.

### 4.5 AI Sub-Commanders 📋

*Renumbered 2026-08-08 — this section and §4.1 (Boarding) were both numbered §4.1, so every
cross-reference to "§4.1" resolved ambiguously, including four in `architecture.md`. Boarding keeps
§4.1 because it holds that position; this section becomes §4.5 and §4 now reads 4.0 → 4.5 in order.*

The player does not personally pilot everything they own. **Sub-commanders are AI officers appointed
to command a capital ship or station on the player's behalf**, running its fleet under standing
orders while the player is elsewhere in the galaxy.

**They are entities, not menu entries.** A sub-commander occupies a specific vessel, is bound by the
same mechanical rules as any other AI (§6.3), and can be killed — destroying the capital they command
destroys them.

**Each holds its own knowledge network** (§2.5). A sub-commander knows the designs it was given, and
those may be a strict subset of what the player knows. A commander running a forward manufacturing
station can only build what its own network holds, which makes *distributing* designs across your
commanders a real logistical decision rather than an automatic one.

**They are the redundancy that Hard Game Over measures.** §3.3's Recognized Leadership Entity pillar
survives as long as one sub-commander does. A player with commanders scattered across three systems
is genuinely hard to eliminate; a player who does everything personally is one bad fight from the
Tier 3 outcome. This is the intended tradeoff — appointing a commander costs resources and gives away
direct control, and buys survival of the run.

✅ **Settled 2026-08-11: a sub-commander is any `Crew` module with a non-zero `command` roll, assigned
to an unpiloted capital or station's Bridge and marked autonomous.** No separate acquisition track —
hire Living or manufacture Artificial (§2.7) exactly like any other crew, then assign. **Their rolled
`operation`/`command` stats (§2.7's quality-band/budget machinery) *are* their competence**; no
separate personality system is needed, and none is built.

**Loyalty is stable, not a hidden roll — it only moves via crew bribery (§2.7).** A sub-commander does
not spontaneously defect on a timer or a relation threshold; a rival faction (or the player, against a
rival's commander) has to actually attempt and win a bribe against that specific crew module, the same
mechanism that can turn *any* crew module, not only commanders. This is also what makes bribery an
"affect a faction from the inside" tool as originally proposed: turning a low-ranked crew module is a
minor nuisance, turning a `command`-heavy one *is* turning a sub-commander, and both are the identical
roll — there is no separate "coup" mechanic layered on top.

---

## 5. The Living Universe & Factions 📋

The galaxy runs independently through the Tier 2/3 background simulation (§1.1). Factions have
**global market awareness but localized physical inventories** — they know the price of ore
everywhere and can only spend the ore they physically hold, which is what makes blockades a
strategy rather than an inconvenience.

### 5.0 What a faction actually owns 📋

*Settled 2026-08-08. `core/economy/FactionEconomy` today holds an abstract stock figure —
`Deposit`, `Spend`, `TotalProduction` — with no notion of **what** is being stocked. That was
survivable while nothing could be manufactured. Once §2.10 gives Elements and Materials real identity,
and §6.3 requires AI factions to use the same recipes the player does, a faction that manufactures a
design has to hold the actual inputs.*

> **Everything purchasable or tradable is modelled: materials, Materials, modules, shells, chassis, and
> whole vessels. Stock is held per *station*, not per faction and not per system.**

That is what makes §5's opening promise real rather than a slogan — *"global market awareness but
localized physical inventories"* — and it is what gives blockades teeth: a faction knows the price of
iridium everywhere and can only spend the iridium it physically holds, in the system it holds it in.

#### Making it efficient 📋

Three rules, and the first is the one that decides whether this scales:

**1. The station is the container, and the built code already assumes it.**
`StationServicesSystem` trades against *"the station's own stock"* today (`architecture.md` §12.10),
so per-station is what Tier 1 already does. A system-level model would give us two granularities for
one concept — the same drift `FacilityStats::level` and the rarity ladder just produced.

**It also buys a consequence a system-level model silently loses: destroy a station and its
stockpile goes with it.** Under a per-system ledger, blowing up a faction's forward depot costs them
nothing, which is absurd, and it would quietly undercut §6.1's Material Security facet and the entire
case for blockades.

> ✅ **And it settles what depositing into a station means** (2026-08-10, `architecture.md` §12.30.3).
> If stock is held per station and dies with it, the station's hold is **one owner's inventory**, not
> a warehouse with tenants:
>
> > **A transfer within one owner is free, and is called deposit. A transfer across an ownership
> > boundary costs credits, and is called trade.**
>
> So deposit and withdraw are offered only at a station whose owner is you, and **stocking your own
> station's hold is stocking your own shop** — the first concrete economic action on §5.10's
> player-as-faction path, and what makes a player-built trading post a thing that can run dry. At
> anyone else's station there is no locker; you sell. **A per-visitor rented locker was rejected**:
> it would need a hold keyed by owner (a second granularity for the concept this section just
> settled), and the rule above would have to either except it or destroy the player's goods with a
> station they do not control.

**Per-system totals are a derived sum, computed on query and never stored** — the same discipline
this section applies to price, and §3.5 applies to system radius.

**2. Sparse, never dense.** Stock is keyed on *(faction, station)* → a small sorted vector of
`(ItemId, quantity)`. Ten factions × ~200 systems held × 1–5 stations × 10–30 live item types is
roughly 150k–300k entries worst case — a few megabytes. A dense table would be ten factions × every
station × several hundred ids and almost entirely zeros. **A small sorted vector beats a hash map
below ~32 entries**, which is where nearly every ledger will sit.

**Stock is dual-form**, the same shape as wrecks (§3.3) and research jobs (§2.4): a component on the
station rig while its system is resident, a `core/galaxy/` record when the system demotes. **The
record holds per-station ledgers, not a system sum** — storing the sum and redistributing on
promotion is lossy in a way players notice immediately, when 500 iridium left at a forward base comes
back smeared across three stations.

**3. One write path, and faction-wide totals are maintained inside it.** §6.1's four facets, Military
Weight (§4.3), and §5.1's Three Pillars all want faction-wide aggregates, and re-summing them on the
macro tick is exactly the iteration wall §9.1 warns about. But a cache with invalidation rules is
worse — every write site that forgets to invalidate produces a number the strategic simulation then
makes decisions from. **The resolution is the same one §4.3 uses for Military Weight:** there is a
single `Deposit`/`Withdraw` API, it maintains the totals as it writes, and there is no second path to
forget. You cannot write stock without updating the total, because the write *is* the update.

**4. Price is a function, never state.** Base price derives from the recipe (§2.10) and is computed
once at content load; the local supply-and-demand modifier is computed on query from that one ledger.
**Nothing about price is stored or ticked**, so a galaxy of any size costs nothing to price. This is
§3.5's rule for system radius applied again — a derived value that gets cached is a derived value
that will drift.

#### Internal logistics — shipping before raiding 📋

*Settled 2026-08-08.* A faction must be able to move goods between its own stations to meet a local
shortfall. The naive implementation scans every ledger for imbalances on the macro tick, which is
exactly the iteration wall §9.1 warns about. The cheap implementation is **event-driven off
shortfalls**:

1. Each macro tick, a faction inspects only its **jobs blocked on missing inputs**. That list is
   short by construction — most jobs are not blocked.
2. For each shortfall, find the nearest own station holding a surplus of what is missing.
3. Dispatch a transfer as an **in-transit fleet record** — origin, destination, ETA, manifest. Law 2
   in `architecture.md` already models in-transit fleets exactly this way, so this introduces no new
   type.
4. Cap concurrent transfers per faction.

Cost scales with **blocked jobs**, never with holdings.

> **This is also a refinement §6.1 needs.** Low Material Security currently jumps straight to
> "aggressive expansion or desperate raids," which skips the obvious step and makes factions read as
> unreasonable. **A faction should try shipping before it tries raiding** — and raid only when it has
> nothing of its own to ship.

And it makes interdiction concrete: a logistics convoy is a thing that can actually be intercepted,
which is what §5's opening promise about blockades has been describing all along.

#### What this makes possible 📋

- **Trade becomes physical.** Moving goods between systems is a fleet with a manifest, which Law 2
  already models as a galaxy-level record with an origin, destination, and ETA. Blockading is
  intercepting those records, not adjusting a number.
- **Manufacturing can fail for want of inputs.** A faction that holds the design (§2.5) but not the
  iridium must trade for it or take a system that has some — which is §6.1's Material Security facet
  acquiring an actual cause rather than a percentage.
- **§6.1's Market Dominance facet gets something real to read**, since prices now exist per system
  and derive from holdings.

⚠️ **This is a change to a built system**, so `SaveFile` gains an item-aware stock section with a
matching `SaveMigrator` step, and `FactionEconomy`'s existing three-function API widens.

### 5.1 Faction Survival & Elimination — The Three Pillars 📋

Factions are stationary empire-builders or nomadic survivors — The Forgotten scavengers, the
Voidwalkers migrating away from conflict. Both kinds live or die by the same test.

**A faction remains active as long as it holds at least one of three pillars:**

| Pillar | Held while the faction has… |
|---|---|
| **Command Structure** | At least one station or capital carrying a command module |
| **Recognized Leadership Entity** | At least one surviving leader — a faction head, or an AI sub-commander (§4.5) |
| **Economic Footprint** | Any active production, holdings, or claimed territory |

*Earlier drafts tested only "its final vessel and command module," which was both narrower and
harder to evaluate — a nomadic faction with no stations at all read as permanently one hit from
death, and the Voidwalkers are supposed to survive exactly that way. Three independent pillars let
nomads persist on leadership and footprint, and let a shattered industrial power persist on a single
surviving command station. Any one is enough.*

**Losing all three is collapse, and collapse is irreversible:**

- The faction is permanently eliminated from the galaxy. A ten-faction galaxy can become a
  six-faction galaxy through play.
- **Surviving ships scatter into rogue scavenger groups** — they do not vanish. They become
  unaligned hostiles picking over what is left, which keeps a dead faction's former space dangerous
  and populated rather than empty.
- **Territory becomes unclaimed**, and neighboring factions' Material Security and Doctrine facets
  (§6.1) will notice the vacuum on the next macro tick. Collapse triggers expansion.

**This rule is not player-exempt.** The player's faction is tested identically — that is what a Hard
Game Over *is* (§3.3). One predicate, one implementation, applied to all eleven possible factions.

### 5.2 The Canonical Faction Registry

Ten factions. `lore.md` §3 holds the full profile for each — origin, aesthetic, palette, comms
archetype, behavioral driver, and decision logic.

| Registry key | Canonical name | Behavioral driver |
|---|---|---|
| `aegis_directorate` | Aegis Directorate | Order and Containment |
| `meridian_star_corps` | Meridian Star Corps | Monopoly and Q3 Projections |
| `kore_industries` | Kore Industries | Autonomy and Hard Utility |
| `the_forgotten` | The Forgotten | Opportunistic Survival |
| `ai_concordance` | AI Concordance | Algorithmic Perfection |
| `pyre_ascendancy` | Pyre Ascendancy | Divine Purification |
| `voidwalkers` | Voidwalkers | Transcendent Isolation |
| `zenith_collective` | Zenith Collective | Preservation of Knowledge |
| `edenian_pact` | Edenian Pact | Ecological Expansion |
| `reapers` | Reapers | Systemic Unmaking |

*Naming note: prior versions used "Concordance" here and "AI Concordance" in `lore.md`. The
canonical name is **AI Concordance**, key `ai_concordance`.*

### 5.3 The Relation Model

**Relations are symmetric.** `relation[A][B] == relation[B][A]`, always. The matrix is stored as a
10 × 10 grid of signed integers but only the upper triangle is authored; `SetRelation` writes both
cells or it is a bug.

**Why symmetry is the correct model here.** A one-sided hostility produces incoherent combat: one
faction's ships open fire while the other only ever *responds*, which reads as broken AI rather
than as nuanced politics. The aggressor's fleets hunt; the victim's fleets mill around waiting to
be shot at. Worse, it makes the macro simulation unpredictable in an unfair way — a faction can be
losing a war it never registered as a war, because the Doctrine facet on its side never saw a
hostile neighbor.

Symmetry also gives the player one legible rule: **if they shoot at you, you shoot at them.** Every
faction's threat picture is the same picture. Asymmetric flavor — one side resents the relationship
more than the other — belongs in `lore.md` rationale and in comms dialogue, where it costs nothing,
not in the number that decides who fires.

**Balance constraint.** Every faction has exactly **1 ally and 3 rivals** at world generation. This
is a deliberate design property carried over from earlier drafts: it guarantees no faction starts
friendless or universally besieged, and it makes the starting galaxy legible to a new player. The
graph below satisfies it exactly — 5 mutual ally pairs and 15 mutual rivalry edges, every node
degree-3.

Relations drift from these baselines through play via the writers in §5.3. Symmetry is preserved on
every write.

| Band | Range | Behavior |
|---|---|---|
| **Allied** | +50 … +100 | Mutual defense, shared docking, trade discounts |
| **Friendly** | +15 … +49 | Docking permitted, contracts offered |
| **Neutral** | −14 … +14 | No reaction, standard prices |
| **Distrustful** | −15 … −49 | Docking refused, scanned and shadowed |
| **Hostile** | −50 … −84 | Fired on in claimed territory |
| **War** | −85 … −100 | Fired on anywhere, raids actively dispatched |

> 🐛 **Three of those six rows are unimplementable against the built code** (verified 2026-08-10,
> `architecture.md` §12.30.3). `DockingSystem::FindEligibleBay` accepts a bay only when the station's
> `FactionRef` **equals** the seeking rig's — its own comment says *"a different, same-faction rig."*
> An equality test has two outcomes; this table has six, and §5.10 starts the player as *"an
> independent rogue operator — not a faction, not a member of one,"* for whom equality admits exactly
> the stations they built themselves. **The gate becomes a band lookup, refusing at Distrustful and
> below** — a reader of `DiplomacyMatrix`, which has none in gameplay today. It is the same predicate
> `TargetingSystem` needs (`architecture.md` §13.3 N) asked in the other direction — *may I dock*
> versus *may I shoot* — and both should land in one pass.
>
> **"Trade discounts" and "standard prices" have the same gap**: nothing in the codebase derives a
> price at all, so the reputation modifier ships as identity until the matrix has a reader. Say so in
> the header, or a Market that never discounts reads as a tuning failure rather than a missing
> pointer.
>
> ✅ **Specified 2026-08-10 in `architecture.md` §12.32.** The equality-vs-band-table gap went one
> layer deeper than this callout knew: `Relation` (`core/diplomacy/Relation.h`) was itself a
> three-state enum, so even a correct band-lookup gate would have had nothing to test against. §12.32
> widens `Relation` to the real six states above, seeds `DiplomacyMatrix` from this table, and gives
> the Reapers' `War`-tier row (§5.7) a value that actually exists now instead of collapsing into
> `Hostile`. The docking gate itself and `TargetingSystem`'s reader are still §13.5 group 3's work —
> this closes the type gap underneath both, not the wiring.

**Player inheritance** — aligning with a faction inherits that faction's *outgoing* relations as the
player's baseline. Branching off to found a rogue faction retains those baselines as a starting
point, which then drift independently.

**📋 Who writes to this matrix.** Defining the readers is not enough. In StarReach2 the dynamic
relation matrix was fully implemented — `Get` and `Set` both present — and **`Set` was never called
from gameplay.** It was read-only decoration for the entire life of the project. The systems
permitted to write are therefore named here:

| Writer | Trigger | Effect |
|---|---|---|
| `DamageSystem` | Player or faction fires on a faction asset | Sharp negative to the victim's outgoing relation |
| `ContractSystem` | Contract completed / failed | Positive / negative to the issuer |
| `FactionDecisionSystem` | Macro-tick border skirmish, treaty, betrayal | Bilateral shift |
| `FactionEconomySystem` | Trade volume, blockade, embargo | Slow drift |
| `CommsSystem` | Successful diplomacy, tribute, threat | Bounded nudge |
| `DiscoverySystem` | Trespass in claimed space | Small negative |

If a system is not in this table, it does not write relations.

### 5.4 The Baseline Matrix

The authoritative starting relations. Every row has exactly one ally and three rivals, and every
entry appears in both directions.

| Faction | Ally | Rivals |
|---|---|---|
| Aegis Directorate | Meridian Star Corps | The Forgotten · Pyre Ascendancy · Reapers |
| Meridian Star Corps | Aegis Directorate | Kore Industries · Voidwalkers · Edenian Pact |
| Kore Industries | The Forgotten | Meridian Star Corps · Edenian Pact · AI Concordance |
| The Forgotten | Kore Industries | Aegis Directorate · Pyre Ascendancy · Reapers |
| AI Concordance | Zenith Collective | Voidwalkers · Kore Industries · Reapers |
| Pyre Ascendancy | Reapers | Aegis Directorate · The Forgotten · Zenith Collective |
| Voidwalkers | Edenian Pact | Meridian Star Corps · Zenith Collective · AI Concordance |
| Zenith Collective | AI Concordance | Pyre Ascendancy · Voidwalkers · Edenian Pact |
| Edenian Pact | Voidwalkers | Kore Industries · Meridian Star Corps · Zenith Collective |
| Reapers | Pyre Ascendancy | Aegis Directorate · The Forgotten · AI Concordance |

**Invariants a unit test must enforce** — these are cheap to check and they are exactly the class of
error that produced the inconsistencies in earlier drafts:

1. Symmetry: for all A, B — `relation[A][B] == relation[B][A]`.
2. Degree: every faction has exactly 1 ally and 3 rivals at world generation.
3. Disjointness: no pair is both allied and rival.
4. Closure: every named faction resolves to a registry key in §5.2.

### 5.5 The Five Alliances

Every one of these was already mutual in earlier drafts, and together they form a perfect matching —
all ten factions paired, none left out.

| | | Basis |
|---|---|---|
| Aegis Directorate | ↔ Meridian Star Corps | Corporate trade feeds the military machine |
| Kore Industries | ↔ The Forgotten | Shared lower-class roots; convenient black-market trade |
| AI Concordance | ↔ Zenith Collective | Fellow intellects, shared archival interest |
| Voidwalkers | ↔ Edenian Pact | Symbiotic harmony |
| Pyre Ascendancy | ↔ Reapers | See §5.7 — the one alliance neither party understands the same way |

### 5.6 The Fifteen Rivalries

| | | Basis | Source |
|---|---|---|:---:|
| Aegis Directorate | ↔ The Forgotten | "Lawless filth" vs. the boot that abandoned them | lore |
| Aegis Directorate | ↔ Pyre Ascendancy | Unpredictable fanatics vs. the secular order to be purified | lore |
| Aegis Directorate | ↔ Reapers | Existential threat to galactic stability | lore |
| Meridian Star Corps | ↔ Kore Industries | Cartels vs. unions and independent miners | lore |
| Meridian Star Corps | ↔ Voidwalkers | Unpredictable variables outside market control | lore |
| Meridian Star Corps | ↔ Edenian Pact | Strip-mining and pollution vs. ecological blockade | lore |
| Kore Industries | ↔ Edenian Pact | Heavy industry vs. protected biospheres | lore |
| Kore Industries | ↔ AI Concordance | **Labor vs. automation.** Kore sees an intelligence that makes workers obsolete; the Concordance sees hand-built hydraulics as gross inefficiency. | new |
| The Forgotten | ↔ Pyre Ascendancy | Nihilistic scavengers vs. zealots who would burn them clean | lore |
| The Forgotten | ↔ Reapers | Scavengers picking over what the plague leaves | lore |
| AI Concordance | ↔ Voidwalkers | Optimized scientific grids across sites the Voidwalkers hold inviolable | inferred |
| AI Concordance | ↔ Reapers | Statistical chaos incarnate | lore |
| Zenith Collective | ↔ Pyre Ascendancy | Preservation of history vs. purification by fire | lore |
| Zenith Collective | ↔ Voidwalkers | Zenith aggressively secures pre-collapse ruins; Voidwalkers react violently to anyone exploiting or sealing anomalies | inferred |
| Zenith Collective | ↔ Edenian Pact | **Specimens vs. sanctity.** Zenith catalogs living worlds as data; the Edenians hold that life is not sample material. | new |

**Source column:** *lore* = both factions' rationale in `lore.md` supports it. *inferred* = drawn
from stated behavioral drivers without being named explicitly. *new* = introduced here to close the
symmetric graph; see §5.8.

### 5.7 The Reapers

The Reapers keep their table row, because the balance constraint applies to them too — but the row
means something different from every other row.

**Their three listed rivals are priority targets, not the limit of their hostility.** The Reapers
have no diplomacy to conduct, so their behavior is governed by a rule that sits alongside the
matrix:

- **Target selection is driven by structural density, not politics.** They hunt complex systems,
  high-density infrastructure, and active energy grids, and bypass isolated or derelict sectors
  entirely. A quiet frontier system is safer than a fortified capital.
- **Every faction not otherwise listed sits at Hostile with the Reapers, mutually.** Aegis, The
  Forgotten, and AI Concordance sit at **War** — they are the three whose territory is densest with
  exactly what the Reapers hunt, so they absorb the most pressure.

This reconciles a contradiction in earlier drafts, which listed three Reaper rivalries in the table
while the lore described universal hostility. Both are now true: the table names who they hunt
*first*, and the rule makes them hostile to everyone regardless.

**The Pyre alliance.** Pyre Ascendancy reads the Rift-Born as a divine force of judgment and does
not fire on them. The Reapers, for their part, simply pass Pyre over — Pyre's fleets are ritual,
sparse, and structurally low-density, precisely the profile Reaper targeting ignores. Pyre
interprets being spared as a blessing. Mechanically the relation is a mutual alliance and needs no
special case; narratively, only one party thinks it is one. **The Reapers have not agreed to
anything.**

### 5.8 Changes From Earlier Drafts

Documented so the reasoning is auditable, and so anything here can be vetoed without archaeology.

**Dropped (7).** Every one was a one-sided entry with no reciprocal: Kore→Pyre, Zenith→Reapers,
Concordance→Edenian, Reapers→Voidwalkers, Edenian→Pyre, Edenian→Reapers, Meridian→Reapers. Several
were flavorful, but each created exactly the aggressor/non-responder incoherence symmetry exists to
prevent. Where the flavor is worth keeping — Kore's resentment of Pyre, the Voidwalkers' unnerving
calm around the Rift-Born — it belongs in comms dialogue and `lore.md` rationale, not in the number
that decides who fires.

**Added (4).** Two are drawn directly from `lore.md` and simply weren't in the table:
Meridian ↔ Edenian Pact (Edenian's profile names Meridian explicitly) and AI Concordance ↔ Reapers
(Concordance's profile names the Reapers explicitly). Two are new and are the only genuinely
invented relations in the matrix: **Kore ↔ AI Concordance** (labor vs. automation) and
**Zenith ↔ Edenian Pact** (specimens vs. sanctity). Both were added to close the graph, and both
have a thematic basis worth keeping regardless — but they are the two entries to challenge first if
the matrix feels wrong in play.

**Preserved (11).** Every rivalry that was already mutual in earlier drafts survives unchanged, as
do all five alliances.

**No `lore.md` changes are required.** The four additions and seven removals are all consistent with
the faction profiles as written — the profiles were never the problem, the table was. If the two
*new* rivalries are kept, adding a clause to each faction's Relationship Rationale in `lore.md` §3
would make the bible self-consistent, and that is the only lore edit worth making.

> ⚠️ **Verified 2026-08-10 — this section's "documented so anything here can be vetoed without
> archaeology" claim, and "the profiles were never the problem," are incomplete.** Cross-checking all
> ten `lore.md` §3 Relationship Rationale lines against the current 15-pair table finds hostility
> `lore.md` still states that is neither in the table nor in the seven tracked drops above:
>
> - **AI Concordance ↔ Pyre Ascendancy — the one symmetric case.** AI Concordance's own rationale:
>   *"Opposed to volatile factions like Pyre Ascendancy and The Reapers."* Pyre's own rationale:
>   *"At war with corporate and scientific factions (Meridian, Zenith, **Concordance**)."* Both sides
>   name each other — unlike every dropped pair above, which were one-sided by definition. This is not
>   flavor left in a single profile; it is two profiles independently agreeing, and it is simply
>   absent from both the table and the drop list.
> - **The Forgotten → Meridian Star Corps.** *"At war with everyone else, especially Aegis and
>   **Meridian**."* One-sided — Meridian's own rationale names only Kore and Voidwalkers.
> - **Pyre Ascendancy → Meridian Star Corps** (same sentence as the Concordance case above).
>   One-sided, same reason.
> - **Voidwalkers → Aegis Directorate.** *"Distrustful of industrial empires (Meridian,
>   **Aegis**)."* One-sided — Aegis's own rationale names Reapers, The Forgotten, and Pyre, not
>   Voidwalkers. Weakest of the four: §5.3 names **Distrustful** as its own band, milder than a
>   starting **Rival**, so this may be intentional — a relationship worth a lighter starting value
>   rather than evidence of a missed rivalry.
>
> **This is a decision, not a mechanical fix**, for the same reason `architecture.md` §13.4 keeps its
> design calls separate from its bug list: the balance constraint (§5.3 — exactly 1 ally and 3 rivals
> per faction, already true of all ten today) means adding any pair above requires removing another to
> compensate. The available options are the same shape as the seven already-dropped pairs: disclose
> these as `lore.md`-only flavor the way Kore→Pyre already is, or rebalance the graph to include one at
> another pair's expense. **Recommendation: add Concordance↔Pyre to the dropped list explicitly** — it
> is the one case with two-sided lore support, which every currently-listed rivalry has and every
> currently-dropped entry lacks — **and leave the three one-sided cases undisclosed**, the way
> Kore→Pyre already is, since flagging every one-sided adjective across ten profiles would make this
> list longer than the table it explains.
>
> ✅ **Resolved 2026-08-10.** `lore.md` §3 was swept faction by faction: every Relationship Rationale
> line now names exactly that faction's real ally and three rivals from §5.4, drawing its reasoning
> from this section's own Basis column rather than inventing new justification. **Concordance↔Pyre
> was dropped from both sides**, per the recommendation above — neither profile claims it any longer.
> The two one-sided flavors this document had already named as worth keeping — Kore's resentment of
> Pyre, and the Voidwalkers' calm around the Reapers — were relocated into each faction's Decision
> Logic rather than deleted, so they read as color and can no longer be mistaken for a rivalry claim
> the way they were before this pass. **`Dropped (7)` above is now fully retired from every
> Relationship Rationale line** — none of the seven, nor Meridian↔The Forgotten, Meridian↔Pyre, or
> Aegis↔Voidwalkers (the three additional one-sided cases this cross-check surfaced), remain stated
> as an active hostility in `lore.md` §3.

### 5.9 Unlisted Pairings

Everything not named above starts **Neutral** and drifts through play via the writers in §5.3.

### 5.10 The Player As A Faction 📋

**The player starts as an independent rogue operator** — not a faction, not a member of one, and not
represented in the §5.4 baseline matrix. They are a single pilot with no territory, no production,
and no standing beyond what they earn. This is the entry state for a new game *and* for a restart
after Hard Game Over (§3.3); there is only one.

**Installing a Command Module is the transition.** Placing a functioning command module on a station
or capital the player owns is the moment they become a **recognized faction** — mechanically, not
ceremonially. It is a single unambiguous trigger the player performs deliberately, and it is the
same component the AI factions are tested on in §5.1.

Crossing that line changes what the galaxy does with them:

| As a rogue operator | As a recognized faction |
|---|---|
| Tracked only by individual **reputation** with each faction | Holds an entry in the §5.3 **relation matrix** |
| Cannot claim territory | Can claim and hold territory; borders are evaluated on the macro tick |
| No Tier 3 presence — not simulated as a power | Evaluated by rival `FactionDecisionSystem` facets (§6.1) as a neighbor, a threat, or an opportunity |
| Not eliminable — death is Tier 1/2 (§3.3) | Subject to the Three Pillars (§5.1); collapse is a Hard Game Over |
| Cannot be allied with, embargoed, or invaded | Can be — including having its territory targeted by a resource raid |

**The trade is deliberate: recognition buys reach and costs safety.** A rogue operator is beneath
notice and correspondingly hard to destroy permanently. A recognized faction can hold space, run an
economy, and appear on other factions' threat pictures — and can be eliminated from the galaxy for
good. Players should feel the weight of installing that module.

**Player inheritance on alignment** (§5.3) applies at the moment of recognition: a player who has
been operating under a faction's flag inherits that faction's outgoing relations as their starting
row, then drifts independently. A player with no alignment enters the matrix at Neutral across the
board.

#### The other branch: climbing a faction instead of founding one 📋 ❓

*Scoped 2026-08-08. Not built, not fully specified — recorded so it is not invented ad hoc later.*

Founding a faction is one of two paths, and the other is to **join an existing one and rise through
its ranks.** This is attractive because it gives §5.3's reputation a *destination*: today reputation
ends at band effects — docking, prices, contracts — and rank would make it a progression track with
something at the top.

**Rank grants are faction-flavoured and deterministic, never random.** A Pyre rank grants what a
Meridian rank does not, per §6.2's archetypes, so *which* faction you climb is a real decision rather
than a reskin. Random grants would also be the only random progression in a design where everything
else is earned or built. The grants themselves are existing machinery: access to the faction's
knowledge network (§2.5), command of its fleets, territory, and standing.

**Rank is not commander grade** (§6.5). Grade is the module you carry; rank is standing within a
faction. Headship requires both.

**Defection is where it gets interesting, and it needs no new mechanism.** Leaving a faction copies
what you learned into your new network, **frozen at the moment you leave** — which is precisely the
operation §8.3 already specifies for fog inheritance, and which `KnowledgeStore::Copy` already
implements. Walking out of Zenith with their blueprints is a complete betrayal mechanic built from
one existing call.

❓ **Still open:** what joining does to assets the player already owns, whether membership and
founding are mutually exclusive (they probably are), and the reputation cost of defecting. This wants
its own pass — it touches §2.5, §5.3, §6.5, and asset ownership at once.

---

## 6. Simulation Decision Engine 📋

Full behavioral profiles are in `lore.md` §5. Summarized here for the systems that implement it —
`FactionDecisionSystem` and `FactionEconomySystem`, both Tier 3.

### 6.1 The Four Operational Facets

Each macro tick, a faction evaluates its systems across four axes:

| Facet | Question | Low value triggers |
|---|---|---|
| **Material Security** | Stable access to ore, energy cells, components? | Aggressive expansion or desperate raids |
| **Market Dominance** | Are trade routes profitable? Being undercut? | Embargoes, blockades, corporate takeovers |
| **Ideological Doctrine** | Neighbors as territory, heretics, or specimens? | Military campaigns or isolationist retreat |
| **Tech Superiority** | Holding advanced blueprints, or falling behind? | Exploration fleets, research, espionage |

### 6.2 Task Weighting by Archetype

| Archetype | Factions | Heavy weight on |
|---|---|---|
| Corporate / Industrial | Meridian, Kore | Resource gathering, trade route protection, blockade running |
| Military / Zealot | Aegis, Pyre | Patrols, system invasion, purging hostiles |
| Scientific | Zenith, AI Concordance | Ruin securing, research projects, exploration contracts |
| Anomalous | Voidwalkers, Reapers | Anomaly hunting, migration, unprovoked raids |
| Opportunist | The Forgotten | Wreck salvage, ambush, vanishing when outgunned |
| Ecological | Edenian Pact | Slow deliberate expansion, blockading polluters |

### 6.3 Uniform Mechanical Rules 📋

**AI vessels are bound by exactly the same mechanics as the player.** This is a hard design rule, not
an aspiration, and it applies at every level from a lone NPC fighter to a faction's flagship:

- **Power priority lists.** An AI ship runs the same `PowerSystem` budget the player does. It sheds
  load when its generation drops, and it gates weapons and shields against available power on the
  same terms.
- **Transient ability spikes.** AI ships dynamically reallocate power mid-combat — dumping shield
  regeneration into a weapon burst, or cutting engines to hold a shield up — using the same
  reallocation the player has access to.
- **Mass and the constraints puzzle.** An AI vessel built from a Template obeys §2.2's mass/power
  tradeoffs. There is no AI-only stat budget and no hidden multiplier.
- **Localized damage.** AI rigs lose specific hardpoints and degrade functionally (§3.2). A crippled
  NPC is crippled in a way the player can read on its silhouette.

**Why this is stated as a rule.** The moment AI ships get a private set of mechanics, two things
break at once: the player can no longer reason about an enemy by reading its loadout, and every
balance change has to be made twice. It also makes the §2.6 Template sale meaningless — a faction
manufacturing your design must fly it the way you would, or seeing your silhouette in their fleet
teaches the player nothing.

Difficulty and faction competence are expressed through **loadout quality, numbers, and tactical
decisions** — never through mechanical exemption.

### 6.4 Probability Thresholds

- Material Security below 30% → **+45%** chance to launch a resource-raiding fleet.
- Two rival factions sharing a border with hostile Doctrine → border skirmish rolled every macro
  tick, moderated by relative fleet strength.

❓ *These two are the only tuned numbers in the design. The remaining weights, drift rates, and
thresholds need a balancing pass — which is what the headless `tools/economy_sim` is **planned** for
(🧊 `architecture.md` §3; `tools/` holds only `ci/` today, so this answer is not yet obtainable)
(`architecture.md` §3). Building that tool early is cheap, because `sr_core` links no renderer.*

### 6.5 Boss encounters are commanded fleets 📋

*Settled 2026-08-08. "Bosses" appeared nowhere in either document, and the obvious implementation —
unique named NPCs with hand-authored drop tables — would have been a whole content pipeline with its
own spawn rules, uniqueness semantics, and respawn policy.*

> **A boss encounter is any enemy fleet whose leader carries a `Crew` module with a non-zero
> `command` roll. There is no boss type, no boss flag, and no unique-NPC system.**

*Restated 2026-08-09 for the crew consolidation (§2.7): what used to be "a `Commander` module" is now
"a crew module that rolled into `command`." Nothing else in this subsection changes — if anything it
gets better, because how *much* of a boss something is now varies continuously with the roll rather
than being binary on a module kind.*

Everything a boss fight needs already exists and falls out of decisions made elsewhere:

| Property | Comes from |
|---|---|
| **It fights differently** | A `command` rating runs standing orders and fleet dispatch (§4.5), so a commanded fleet manoeuvres as a group where an uncommanded one does not |
| **It is worth something** | The crew module is **matter** (§2.5) and mounts in a crew shell — kill the bridge or cockpit and it drops like any other module (§2.7) |
| **The reward scales** | The crew module's own grade, on the same seven-tier ladder as everything else — **and its roll**, so a captured admiral may be a specialist worth more than its tier suggests |
| **It matters strategically** | §5.1's Recognized Leadership Entity pillar. Killing a faction's commanders is direct progress toward collapse, not just loot |
| **It is findable** | Commanders are placed by the faction simulation, not by an encounter table — so where the bosses are is a consequence of who is expanding |

> **A fighter can now be a boss** (§4.0), since a cockpit may hold a commanding crew module. That is a
> *good* addition rather than a dilution: a fleet led from a fighter manoeuvres as a group like any
> other, and it is far more fragile — one cockpit hit decapitates it. A faction fielding
> fighter-commanders is one taking a real risk, which is exactly the sort of thing §6.2's archetypes
> should express.

**This is also the achievement half of §2.10's gate.** High-grade *finished items* come from rare
locations, quests, and boss kills — and "boss kill" now has a concrete meaning: strip a commanded
capital's bridge and take what was mounted in it, including, if you are lucky, an officer better than
anything you could manufacture.

**And it sharpens decapitation** (`architecture.md` §12.16 item 22). The crew module sits on the crew
hardpoint, so **destroy the bridge or cockpit, lose the commander, keep the ship** — which means the
boss's reward and the boss's defeat are the *same shot*, and taking the hull intact afterwards
(§3.2's uncrewed hull) is a second, larger prize.

#### The faction head is the highest `command` rating 📋

*Settled 2026-08-08; restated 2026-08-09 for the crew consolidation.* A faction's head is whichever
of its commanders carries the highest **effective `command` rating** — grade *and* roll together,
since a Rare that spent everything on command may out-command an Epic that split its budget. Ties
break arbitrarily, and when a head dies the next-highest succeeds automatically.

Two properties follow, and both are wanted:

- **The head is derived, never stored.** It is a query over the faction's commander roster, so there
  is no "head" field to go stale when one dies — the same discipline this design applies to price
  (§5.0), system radius (§3.5), and Military Weight (§4.3).
- **Killing the head is a setback, not a kill.** Succession is automatic, so §5.1's Leadership pillar
  fails only when *every* commander is gone — which is what §5.1 already says. The boss fight stays
  repeatable and consequential without being a win button.

**And it makes rank and grade two different things**, which §5.10 depends on: command rating is
mechanical (the module you are carrying and what it rolled), rank is social (standing within a
faction). Headship needs both — membership *and* the top rating among that faction's own commanders —
or a player could loot a Mythic crew module from a boss and become head of a faction they have never
met.


---

## 7. Procedural Generation & Universe Structure 📋

### 7.1 Hierarchical Position-Based Seeding

**The universe is derived, not stored.** Every celestial body, belt, and resource deposit is produced
by a cascade of deterministic seeds, each derived from the one above it plus the object's own
coordinates:

```
Global Universe Seed  +  Galaxy Coordinates          ->  Galaxy Seed
Galaxy Seed           +  Solar System Coordinates    ->  Solar System Seed
Solar System Seed     +  Orbit / Belt Coordinates    ->  Celestial / Resource Seed
```

Each level is a pure function of its parent seed and a position. The consequences are what make this
worth doing:

- **A galaxy costs one integer.** The Global Universe Seed plus a coordinate is enough to regenerate
  any system in the galaxy identically, on demand, on any machine.
- **Generation is order-independent.** Visiting system C first produces exactly the same system C as
  visiting A, B, then C. Nothing accumulates state during exploration.
- **It scales to Tier 3 instantiation for free** (§1.1). Warping into a never-visited system does not
  need a stored record to exist — the seed produces the system, and the `core/galaxy/` record layers
  faction ownership on top.

### 7.2 What Seeds Do Not Cover

**Seeding replaces storage for *authored-by-nature* content only.** Anything the simulation or the
player changed must still persist, because it is not a function of position:

| Derived from seed (never stored) | Persisted in `core/galaxy/` (must be stored) |
|---|---|
| Star class, luminosity, position | Which faction claims the system |
| Planet count, size, orbital elements | Stations built, destroyed, or captured |
| Asteroid belt layout, initial deposits | Depleted deposits and mining progress |
| Anomaly and derelict placement | Whether a derelict has already been looted |
| Baseline resource richness | Faction stock levels, production, fleet presence |

*This is the correction to the "zero disk storage" framing of earlier drafts. Seeds eliminate storage
for the **initial** state of the universe, which is most of its bulk. They eliminate none of the
storage for what happens to it — and §1.1's demotion path explicitly collapses a departed system into
a stored record. Both are true and they do not conflict: the seed supplies the stage, the record
supplies the play.*

**The boundary rule:** *if a player or a faction could have changed it, it is not seed-derived.* A
system that tries to regenerate mutable state from a seed will silently undo player actions on every
revisit.

### 7.3 On-Demand Streaming

**Full ECS entities are instantiated only where they are needed** — when the player approaches a
system, or when a background sector needs active processing. This is §1.1's LOD model expressed as a
memory strategy rather than a tick-rate one, and Law 2 in `architecture.md` (one registry per star
system) is what makes it cheap: streaming a system in or out is constructing or destroying one
registry, not filtering a global entity store.

Distant unrendered sectors run **lightweight abstract loops** instead — aggregate Military Weight,
economic yield, and outcome rolls, per §1.1's Tier 3 column. No entities, no physics, no projectiles.

---

## 8. The Navigation Map 📋

A single seamless zoomable map from galactic scale down into live flight, replacing separate map
screens.

> **Vocabulary warning.** This section's **Zoom Levels** are *not* §1.1's simulation **Tiers**. They
> are independent axes and they do not line up: Zoom Level 3 shows a solar system's contents, while
> simulation Tier 3 is the *unrendered galactic background*. Zoom is what the player is looking at;
> Tier is how fast a registry ticks. Never call a zoom level a tier.

### 8.1 The Four Zoom Levels

| Level | Scope | Shows |
|:---:|---|---|
| **1 — Universal / Galaxy** | Macro cluster view | Faction territory, trade vectors, military weight. No individual objects |
| **2 — Solar System** | Star systems as points | System metadata, ownership, known resources, warp routes |
| **3 — Local / Orbital** | Inside one system | Planets, belts, stations, and **real-time fleet assets** — the RTS command surface (§4) |
| **4 — Active Sector** | Live gameplay | Transitions smoothly into the rendered top-down viewport (`SpaceFlight.cpp`) |

**Level 4 is not a map screen.** The zoom-in resolves continuously into actual gameplay rendering,
which is what makes this one interface rather than four. There is no modal transition and — per §3.4
— no pause at any level.

#### Level 1 is not one view — it is a continuous zoom through territory scales 📋

*Settled 2026-08-10, extending §15.1 finding 4's fix (Level 1 was drawing individual system markers,
which this table's own row already forbade) into the fuller design that prompted the fix.* This
mirrors the pattern Level 3→4 already uses — a continuous re-scale within one representation,
not a stack of separate screens — applied to the **outer** end of the map instead of the inner one.
Nothing here moves the Level 2/3 registry boundary (§8.1 below); every scale on this list stays on
the grid-index, schematic side of it.

| Zoomed from → to, within Level 1 | Shows | Aggregates |
|---|---|---|
| **Regional** (bordering Level 2) | Clusters of nearby star systems | Individual systems, grouped by proximity |
| **Galactic** | This galaxy's full territory picture | Regional clusters |
| **Intergalactic** | Groups of galaxies | Whole galaxies |
| **Universal** | Everything | Galaxy groups |

*(§9: decided 2026-08-11 — "groups of galaxies" and "the universe" are literal, reachable places, just
future-expansion scope beyond the one-galaxy base game. This table's shape didn't depend on the
answer either way, only the tiers' long-term meaning did.)*

**Every scale shows territory, never individual objects**, per this section's existing rule — a
regional cluster is still an aggregate blob, just a smaller one than the galactic view. The
`core::diplomacy::Territory` type (`architecture.md` — currently has zero consumers anywhere in
`src/`) is exactly the record this rendering needs and does not yet read.

**Hover and click, gated by sensors.** At every scale above Level 2, hovering a territory blob or a
system/galaxy point surfaces summary info — name, controlling faction, threat picture — **exactly
as far as §8.3's fog-of-war model already allows.** Undiscovered or out-of-sensor-range objects show
as present-but-unknown (§8.3: *"absence must never look like emptiness"*), never invisible and never
fully detailed. This is not a new gating rule; it is §8.3 applied one more level out than it
currently reaches.

**Clicking opens a popup with a warp button; whether the button is enabled is `WarpSystem`'s job,
not the map's.** The popup surfaces whatever hover already showed (name, controlling faction, threat
picture — fog-gated per above), plus a Warp action. The button renders enabled or disabled based on
the same hyperdrive-range gate §2.11 already assigns `WarpSystem` and `architecture.md` §13.1 already
records as unbuilt (*"no fuel, no module, no charge time"*) — the navigation map is a **consumer** of
that gate, not a second place that decides range. Pressing an enabled button is what turns the
eventual `SystemWarpRequest` producer on.

**Warp plays a fade transition at every scale except local.** Confirming a warp within the current
system (an in-registry `WarpRequest` to a planet, station, or point of interest) cuts instantly —
same scene, no loading boundary to hide. Confirming a warp to a different star system or beyond
(a `WarpToSystem` registry swap, or its eventual galaxy-scale equivalent) fades out, then fades in
on arrival — there **is** a loading boundary there (Law 2's clean registry handoff), and the fade is
what keeps it from reading as a hitch. This is presentation, not new simulation state: the same
distinction already exists in code as "local warp" vs. "system warp," this just assigns each one its
correct transition.

**Icon collapse follows the same zoom-out logic at both existing boundaries, not just the new
outer one.** Level 3 already shows individual objects (ships, planets, the sun); crossing out to
Level 2 already collapses a system to a single point per system (§8.2's existing culling rule). This
section's outer tiers extend the identical pattern one more step each: crossing from Regional into
Galactic collapses a cluster of systems into a single star-cluster icon; crossing from Galactic into
Intergalactic collapses a galaxy's full territory picture into a single galaxy icon. Nothing new is
being invented at the boundary — it's the same "aggregate below, point above" rule §8.2 already
states, applied consistently at every scale rather than only at the one it was first written for.

**What this reuses, and what it's new work for:** `Territory` gets its first real reader; §8.3's fog
model extends outward with no new rule; `WarpSystem`'s range gate becomes something the UI can
finally depend on once it exists. The genuinely new piece is the aggregation itself — building a
territory blob from the systems/galaxies inside it — which has no existing analog.

Architecture: `architecture.md` §12.35.

#### The zoom levels split where the data model splits 📋

*Settled 2026-08-08. This resolves a worry that procedurally-sized systems would, on zoom-in, sprawl
across the space a neighbouring system occupied at galaxy scale.*

**They cannot, because above a system there is no space to sprawl into.**

| Levels | Backed by | Coordinates | Rendering |
|---|---|---|---|
| **1–2** Galaxy, neighbourhood | `core/galaxy/` records | **Integer grid indices** | **Schematic** — systems are points on a lattice |
| **3–4** Orbital, active sector | The resident `entt::registry` | **`float` world units, local to the system origin** | **True scale** — continuous zoom to cockpit |

The boundary between Level 2 and Level 3 is **exactly Law 2's registry boundary**. Above it, systems
have grid positions and no extent; below it, one system owns a real coordinate space measured from
its own origin. A system's physical size therefore has no relationship to its spacing on the galaxy
lattice, and cannot overlap a neighbour however large it is.

**So the 2 → 3 transition is a change of representation, not a continuous zoom through one space.**
Discrete levels with an animated transition (agreed above) is not merely the simpler implementation —
it is the one that matches the data.

**Within Levels 3–4, scale is fixed and true.** A given number of world units always occupies the
same number of pixels at a given zoom, so a gas giant reads as a gas giant and a moon as a moon. The
alternative — scaling each system to fit the viewport — would destroy all comparative size
information, which is the one thing §3.5 spent its effort making meaningful. A large system is
panned and zoomed within Level 3, never shrunk to fit.

#### Procedural systems need a stated size envelope 📋

Star class already varies a system's extent, and that is desirable — a supergiant's system *should*
be larger. But generation must be bounded at both ends:

- **Floor:** large enough to space planets and belts at readable distances.
- **Ceiling:** **2,000,000 world units of usable radius**, scaling with star radius (§3.5). Beyond
  that, `float` precision degrades until slow-moving objects freeze — see §3.5 for the arithmetic.
  *An earlier draft of this bullet said ~1e6; §3.5's exact binade analysis settles it at 2e6 and that
  number is canonical wherever the two disagree.*

The seed cascade must produce radii inside that envelope for **every** star class it can generate, or
a rare supergiant becomes an unplayable system that only appears for some players.

### 8.2 Icon Rendering & Culling

**Icons are generated programmatically, not authored.** Map markers are vector shapes or runtime
template bakes cached into small `RenderTexture2D`s — no hand-drawn icon per ship class, no atlas to
maintain, and correct scaling at every zoom level. `IconRenderer` (`modes/space/render/`) already
does exactly this for the HUD reticle and is the natural home.

**Ship and fleet icons are strictly scoped to Zoom Level 3.** Zooming out past the solar system
boundary culls them entirely.

This is both a performance rule and a legibility rule, and the legibility half matters more. A galaxy
view speckled with thousands of individual ship markers communicates nothing. At Levels 1–2 the
player should see **territory, pressure, and movement of weight** — not units. Fleets aggregate into
military-weight indicators; individual vessels simply do not exist at that scale.

### 8.3 Fog of War 📋

*Settled 2026-08-08.* Level 3 shows **only what the viewer has sensor coverage of**. Fog is real, and
the RTS command surface is deliberately partially blind.

**Fog is per faction.** A faction's members share what any one of them has found — which is already
exactly how `core/galaxy/Discovery.h` models it.

**Founding a faction inherits the fog of the faction you left, frozen at the moment you leave.** A
copy, not a link: from that instant the two diverge, and your former parent learns nothing more from
you nor you from them. This is the same operation §2.5 already provides for copying knowledge between
networks, applied to sensor intel.

**A rogue operator (§5.10) is not a faction** and carries their own personal fog until they found one
(§5.10's command-module trigger), at which point their personal picture seeds the new faction's.

**What this commits the design to, and it should be entered deliberately:**

- **Sensor coverage becomes strategic infrastructure.** Sensor modules, picket ships, and stations
  are radar you build, position, and defend. Losing a picket blinds a border.
- **You command against a picture that may be wrong.** An enemy fleet can enter a system unobserved
  and your orders will be issued against stale truth. That is excellent tension and an unusual
  property for an RTS surface — which means the UI's hardest job is rendering *"you do not know"*
  distinctly from *"there is nothing there."* Absence must never look like emptiness.
- **Sub-commanders can know things the player does not.** Each holds its own network (§2.5), so the
  map shows what **the viewing entity** knows, not a global truth. A commander three systems away
  may be looking at a threat the player cannot see.

✅ **Two homes existed for this fact. Knowledge networks win** (settled 2026-08-08).
`core/galaxy/DiscoveryState` stores discovery per faction, while `KnowledgeNetwork` has a
`DiscoveredSystem` entry kind and §2.5's table lists "discovered systems and sensor intel" as
network-stored. `DiscoverySystem` writes the former; the design specifies the latter.

**This is not a preference — `DiscoveryState` cannot implement this section at all.** It is keyed by
faction, and the last bullet above requires *"the map shows what **the viewing entity** knows"*: a
sub-commander three systems away looking at a threat the player cannot see. A faction-keyed store has
no way to express two members of one faction knowing different things. Networks are per-owner
(`Player`, `Commander`, `Faction`), so per-sub-commander divergence, a rogue operator's personal fog,
and inheritance-as-frozen-copy (`KnowledgeStore::Copy`, already built) all fall out of one mechanism.

**What it costs:** `DiscoverySystem` (built and tested) rewrites to write networks and its tests
change; `SaveFile` drops its `DiscoveryState` section with a matching `SaveMigrator` step. One
convenient side effect — `SystemContext::discovery` disappears entirely, since `knowledge` covers it.

#### The signature / detection model — agreed in principle 📋

*Agreed 2026-08-09 during the materials pass, **not yet specified.** Recorded here so it is a known
commitment rather than a gap rediscovered later; the full treatment is a §12 entry nobody has
written.*

`architecture.md` §11.9 has long carried this as a blocking prerequisite in its own words:
*"Sensors carry only a range; there is nothing to detect against. **This is a system, not a
module.**"* Today `SensorRange` is a single float, **hardcoded to 2000** in `RigFactory`, and
everything inside it is seen equally.

> **Replace "how far I can see" with "how far I can see *you*."** A rig carries a **sensor
> strength**; every rig carries a **signature**. Detection compares the two across distance.

**Signature is derived, never authored** — from hull mass, current power draw, and the materials the
hull is built from (§2.10). It is not a stat someone types.

Five things already in this document need it, which is why it is a commitment rather than an idea:

| Wants it | Currently |
|---|---|
| **This section's per-viewer fog** | Needs coverage that varies by *who is looking* |
| **§2.9's "run silent"** | Its Offline level literally promises this — **there is nothing to be silent from** |
| **§2.11's ECM and cloak** | Homeless modules, blocked on exactly this |
| **`TargetingSystem`** | Already reads `SensorRange` — a degenerate one-float version of it |
| **§2.10's Optical and Semiconductive attributes** | Their depth comes from sensor strength |

**A free consequence worth having:** a heavy, power-hungry hull is genuinely easier to see than a
light composite one running dark, so **element choice becomes a stealth decision** and not only a
combat-stat one.

#### Sensor sharing is a comms link, not telepathy 📋

*Settled 2026-08-09. This section already treats coverage as shared infrastructure at the strategic
scale — "sensor modules, picket ships, and stations are radar you build, position, and defend;
**losing a picket blinds a border**." This is the same idea at the **tactical** scale, and it needs
the same mechanism rather than a new one.*

> **You see what your comms-linked allies see.**

**The `Comms` module is the gate.** §12.27 already moves `CommsSystem`'s hail check onto a dedicated
`commsRange` and justifies the module on two consumers — hailing and command reach. **Sensor datalink
is the third, on the identical check.** No new component, no new range stat, no new system.

Three consequences, none of which costs new machinery:

- **Destroying comms is a far bigger prize.** §12.27 already has it degrade fleet coordination; it now
  also **collapses the shared sensor picture**. Same hardpoint, same check, **both sides** (§6.3).
- **A scout becomes a real fleet role.** One hull with strong Optical and Semiconductive elements
  (§2.10) feeds an entire formation — and losing it is a sudden, legible loss rather than one ship
  fewer.
- **It is multiplayer-safe with no special case.** Linked players share; unlinked players do not. Law
  9's authority model never has to know the difference.

##### It sharpens this section's tension rather than dissolving it

The obvious worry is that sharing makes the player never blind, against §8.3's stated goal that *"you
command against a picture that may be wrong."* It does not: **you see only where you have assets.**
Datalink gives you your fleet's coverage, never omniscience — and it makes the failure mode dramatic.
Kill the picket, or merely its comms hardpoint, and the enemy's picture goes dark **mid-engagement**.
That is a better version of this section's tension than never having had the coverage at all.

##### Relayed contacts must not look like direct ones

When a linked ally dies, everything it was feeding goes stale in the same instant — and the player
must already have known which contacts those were. This is *"absence must never look like emptiness"*
in a second form.

⚠️ **Dash density is already spent on shield charge** (§3.9), so second-hand contacts need a different
channel — hollow versus filled, or a small link glyph. Decide it when the contact iconography is
drawn (§3.10).

##### ❓ Open: links outside a fleet

Fleet members link automatically. Whether **hailing a friendly or neutral can establish a temporary
link** — sharing sensors as a favour, a trade good, or a treaty term — is undecided and appealing. It
needs a relation model, and `SystemContext::diplomacy` is `nullptr` until `architecture.md` §12.24
step 6. **Deferred with a clear trigger rather than left as an oversight.**

---

## 9. Open Design Questions ❓

Genuinely undecided items, listed so they are not mistaken for oversights.

**Performance budget — ✅ settled 2026-08-08.** It was the longest-standing "needed soonest" item;
see §9.1 below. The remaining pacing questions in this section are economic, not technical.

**Economy and progression pacing — ⚠️ partly settled 2026-08-09, and the rest is now measurable
rather than undecided.** The **currency scale is set**: one unit of any element is 1 credit, and every
other price in the game derives from that single number (§2.10). **Prices are outputs, not targets** —
nobody decides what a fighter costs; the recipe does. What remains is not a design hole but an
unbuilt measurement: `tools/economy_sim` runs the derivation and prints the curve, and it does not
exist. Time-to-milestone targets — first custom Template, first capital, first owned system — stay
open, and should be *read off* the derived curve rather than declared in advance.

**The save model — ✅ settled 2026-08-08.** Free/manual saves plus a coarse periodic autosave that
**never fires on death**. See §3.3. The threatened mechanic turned out to be Tier 2's recovery run,
not Tier 3.

**UI/UX specification — ✅ settled 2026-08-10, superseding the two entries below.** §3.9 (status
display) and §3.10 (flight HUD) landed 2026-08-09 and cover the flight surface. The docked menu set
— once this section's open item — is now fully specified by `architecture.md` §12.30: the router
(§12.24 step 5), the shared widget layer, and all six docked screens (Bay, Market/Storage, Repair,
Engineering, Research, Manufacturing) individually, down to per-screen gating and request shape.
`StorageMenu`/`ModulesMenu` were reclassified as flight-HUD overlays rather than docked screens in
the same pass (§12.30 §1). **What remains is not specification, it's construction** — `architecture.md`
§13.5 groups 4a/4b track building the nine still-unreachable menus against this spec, which is a task-
list item, not a documentation gap.

**UI/UX specification — the original entry, retained for its reasoning.** The Bridge, component-driven menus, HUD
theme, Engineering view, and station services are referenced throughout both documents and specified
nowhere as screens or flows. Much of this game is menus. StarReach2 accumulated roughly 5,000 lines
across a dozen menu files with no unifying spec, which is why they are the messiest port target in
`architecture.md` §9. *§8 specifies the navigation map, `architecture.md` §12.9–§12.12 gave Template
creation, station services, cargo/hardpoint equip, and construction/refit/grafting the same treatment,
and §12.30 (above) closed the remaining gap — the full docked-screen inventory, including the
`EngineerMenu` question, which resolved to Engineering being a real docked screen rather than a new
mechanic.*

> ⚠️ **The menus exist and none of them handles input** (verified 2026-08-08, still true as of
> §12.30's audit 2026-08-10). All nine are a pure `Build*Request()` plus a stateless `Draw()`, with
> no selection state, no open/closed state, and no producer placing the request they build. That gap
> is now a **construction** task — `architecture.md` §13.5 groups 4a (shared widgets) and 4b (router
> + screens) — not a specification one; a screens-and-flows document written before §12.24's router
> landed would have specified interactions nothing could perform, but §12.30 is written against the
> router's actual shape.

**Damage type roster — ✅ settled 2026-08-08.** Two shield types (Kinetic, Energy) and three weapon
types (Kinetic, Energy, Ion), with weapon *behaviour* on a separate unbounded axis. See §3.1.

**Altitude bands — 🧊 deferred, fully designed.** Vessel-vessel collision and draw order only,
with occupancy scaling by hull size and a mandatory shadow cue. See §3.7. **Revisit trigger:** the
escort-fighter, docking, and formation-keeping pass, since those are the systems that would inherit it.

**Is the setting one galaxy or a literal multiverse? — ✅ decided 2026-08-11: both, on a timeline.**
The ten-faction setting, the entire base game, and everything else in this document take place in
**one galaxy**, matching `lore.md`'s existing Diaspora backstory exactly (*"transports the player to
a random location in the same galaxy 177 years later"* — no rewrite needed). **A literal multiverse is
real and eventually reachable**, but it is future expansion scope, not base-game scope: other galaxies
genuinely exist, the player can eventually reach them, and §8.1's "Intergalactic"/"Universal" zoom
tiers are **literal**, not relabeled — they are simply further out than anything the base game's
content populates. Nothing about §8.1's aggregation, sensor-gating, or warp-range selection changes
either way, which is why this could stay unresolved as long as it did: the mechanism was never
blocked on the answer, only the tiers' long-term meaning was. `lore.md` needs no change now — it
already only describes the one galaxy the base game covers — but **will need a hook seeding the wider
multiverse's existence** whenever that expansion is actually scoped, rather than inventing the hook
today ahead of any content that uses it (§2.5's "not required by anything above" pattern).

**Physical (hull-blocking) shields — 🧊 deferred.** §3.1 keeps shields projectile-only so that
ramming bypasses them. A physical model is a real future feature with an anti-ram identity, but it
would neuter ramming and entangle collision, docking, and friend/foe rules.

**Fighter persistence in Bridge mode — ✅ settled 2026-08-08.** It persists in the bay, cannot be
shot, and dies only with its host. See §4.1.

**Elements, Materials, and the recipe base — ✅ settled 2026-08-08, substantially revised 2026-08-09,
roster validated and family count grown 2026-08-12.** The 2026-08-08 version had fourteen tiered
materials and seven manufactured families. **Now:** a **41-element** roster (target was ~50; eleven
of twenty candidates survived hand-run `element_check` validation, and the section authorizes
stopping short) on real densities with **no rarity tiers at all**, eight attributes per element,
**eleven** Material families (`Refractory Plate`, `Transparent Composite`, and `Radiation Shielding`
added 2026-08-12) weighted by attribute *role* rather than by named element and carrying **no
faction specificity at all** — exclusivity lives at the module/shell/vessel design level instead —
and both mass and base price deriving from the recipe. See §2.10. **What remains is authoring plus
one tool** — `elements.json` and `materials.json` do not exist, and `tools/element_check` is what
would re-verify the final roster in CI rather than by hand.

**Recovery-run parameters — ✅ decided 2026-08-11, in full at §3.3 Tier 2.** Short in-game
(simulated-time, not wall-clock) window, wreck marked on the navigation map for its duration. See
§3.3 for the reasoning.

**Sub-commander recruitment and loyalty — ✅ decided 2026-08-11, in full at §4.5.** Any `Crew` module
with a non-zero `command` roll, assigned to a Bridge and marked autonomous — no separate acquisition
track. Competence is the module's rolled stats, not a personality system. Loyalty only moves via
crew bribery (§2.7), not a spontaneous defection roll.

**Network raiding — ✅ decided 2026-08-11, in full at §2.5.** Only sub-commander networks are
raidable, and it costs no new mechanic — capturing (not destroying) a commander's vessel converts
their crew, network included, the same way §3.2's capture already reassigns a hull's crew wholesale.
A faction's general network has no single anchor and stays permanently un-raidable.

**Royalty scale and posthumous payment — ⚠️ partly settled, and this listing was itself stale.**
§2.6 already states outright: *"royalties do not survive the seller's death"* — no
inheritance/estate mechanic exists or is planned, consistent with the game's general stance that
loss has teeth. §9 hadn't been swept after §2.6 answered it, the same drift class §14.1/§14.6 catch
elsewhere. The **per-unit rate** is genuinely still open — it's a tuning number in the same category
as the quantity-per-grade multiplier, and §2.6 itself already defers it to `tools/economy_sim`
rather than a guess.

**Level 3 fog of war — ✅ settled 2026-08-08.** Sensor coverage only, per viewing entity, stored in
knowledge networks. See §8.3.

**The word "craft" — ✅ settled 2026-08-08, then ❌ retired entirely 2026-08-09.** The 2026-08-08
ruling made a *craft* a crafted intermediate and a vessel a *vessel*, which fixed the collision but
left a jargon noun. **The supply chain is now `Element → Material → Module`** (§2): the periodic table
at the bottom, manufactured intermediates in the middle, and *craft* used for nothing. A proposed
fourth `Compound` tier was rejected in the same pass. This adds `elements.json`, `elements.json`, and
an `ItemId` to a content set that has none of them.

**The rarity ladder — ✅ settled 2026-08-07/08.** Seven tiers, one ladder for shells and modules,
with a quality band supplying every capability stat and a distributed budget deciding how a roll is
spent. See §2.7. Still absent from the codebase; it is construction work, not a design question.

**Capture.** §3.2 — the uncrewed hull is now a real state, and nothing says how a hull changes
owner. Fly-to-and-hold, a boarding action, a module installed on the capturing vessel? And can an AI
faction capture the player's uncrewed hull on the same terms (§6.3 says it must)? *Disabling is
worth shipping before this is answered — a hull that goes dead and adrift is already a complete
mechanic without an ownership transfer behind it.* **Ion (§3.1) is now the intended route in:** strip
shields, suppress power, kill the crew shell, take the hull.

**Does the player die with their crew shell? — ✅ settled 2026-08-08.** Yes, and it is not a special
case: the player is always associated with exactly one shell, whether flying or aboard a station, and
dies with it. See §3.4.

**Object scale numbers — ✅ settled 2026-08-08.** Validation rules 10, 11, and 12 (separation,
attachment, hull envelope), with hardpoint count emergent from `hullRadius`, chassis radius, and
peripheral size. See §2.3 and §3.5.

**The quantity-per-grade multiplier — ✅ settled 2026-08-11 by `tools/economy_sim`.** Revised from ~3×
to ~2× on 2026-08-09 by argument (§2.10), then to **~1×** on 2026-08-11 by actually running the tool:
at 2×, the three-knob compound reaches a 28,672× Common→Mythic module cost against a ~5× combat-value
gain; at 1×, breadth and the input-grade chain alone already deliver a real 14× climb, which satisfies
§2.4's constraint without rebuilding the drop-rate's scarcity on the cost side. See §2.10 for the full
table across candidate values.

**Manufacturing and progression pacing — ✅ the time curve was already settled, and this listing was
itself stale.** §2.8's own "The time curve" subsection (*"Settled 2026-08-08"*) gives an exact table:
base 5s (Material) / 10s (Module), doubling per grade, 60-fold across the ladder — `tools/economy_sim`
confirms it exactly (`ManufacturingTimeSeconds`, tested against Mythic's 5m20s/10m40s figures). §9
never removed the question after §2.8 answered it, the same drift class the deconstruction-yield entry
below already caught. **`tools/economy_sim` now exists** (`tools/economy_sim/`, tests in
`tests/unit/EconomyModelTests.cpp`) and is what closed the genuinely open pacing question — the
quantity-per-grade multiplier, above — not this one.

**Deconstruction yield — ✅ was already settled 2026-08-08, and this §9 listing was itself the stale
artifact.** §2.4's own text at "Facility grade drives all three" says outright: *"Settled 2026-08-08,
replacing the open question of whether deconstruction yield was flat or scaled"* — followed by a full
facility-grade → yield-% table (Common 20–45% up through Mythic 80–100%). §9 never removed the
question after §2.4 answered it, the same class of drift §14.1/§14.6 already caught elsewhere in
`architecture.md`. Re-confirmed 2026-08-11: facility-level-scaled is the right call, and it's the one
already built into the design.

### 9.1 The Performance Budget 📋

*Settled 2026-08-08. This was the longest-standing "needed soonest" item — `architecture.md`
prescribed LOD tiers, spatial grids, and pooling against no stated target, so nothing could be called
essential or premature.*

**Reference hardware:** a mid-range desktop — 4-core CPU, GTX 1060-class GPU, 16 GB. **Target:** 60 FPS.

| Quantity | Target | Basis |
|---|---:|---|
| Vessels per active system (Tier 1) | **100** | A large fleet engagement at §3.5's 800–1,000 fighter range |
| Hardpoints per vessel | up to **50** | §3.5's scale table; emergent from `hullRadius` (§3.5) |
| Hardpoint entities, Tier 1 | **~5,000** | Law 4 already blesses this range explicitly |
| Total entities, Tier 1 | **~11,000** | Rigs + hardpoints + projectiles + asteroids/loot. **Hardpoints *are* entities** — a separate low "object" cap is a category error |
| Projectiles in flight | **~5,000 peak** | ~40% of hardpoints are weapons, ~1 shot/s, ~2 s lifetime |
| Neighbour systems at Tier 2 | **8, hard cap** | see below |
| Galaxy extent | **unbounded** | see below |
| `core/galaxy/` records in a playthrough | **~2,000** | The number that actually matters |

**Tier 2 needs a hard cap, not "within 2 warp jumps."** §1.1 defines Tier 2 by jump distance, which
makes its cost depend on topology degree: at an average degree of 4, two jumps is 1 + 4 + 12 = **17
systems** at 5 Hz, or `17 × 5/60 ≈ 1.4×` the entire Tier 1 budget — Tier 2 would silently more than
double the simulation cost, and be unbounded in a dense cluster. **Capping at the 8 nearest** makes
it ~0.7× and topology-independent.

**Galaxy size is not bounded by performance, and this is better news than it looks.** Seed-derived
systems cost nothing to *exist* (§7.1), and `core/galaxy/` records exist only for systems something
has actually touched. So extent is bounded only by the coordinate type — an `int32` grid is
±2.1 × 10⁹ per axis. §4.3's "million-system iteration wall" is not a size problem at all; it is a
problem with scanning coordinates instead of records.

> **The macro tick iterates existing `core/galaxy/` records, never the coordinate space.**

With that held, a billion-system galaxy costs exactly what a two-thousand-system one costs, because a
playthrough only ever touches a couple of thousand. **The preferred target is realism — millions to
billions of systems — and it is reachable from day one** provided nobody writes a scan. The ~2,000
figure above is a working-set estimate, not a cap.

The one thing that must stay genuinely local at that scale is **faction territory and expansion**: a
faction evaluating adjacency across a billion systems needs bounded borders, which §5.1's pillars and
§6's facets imply but never state.

#### What the budget decides 📋

| Finding | Consequence |
|---|---|
| Naive `FindHit` is 5,000 × 5,000 = **25 M segment tests per tick** (1.5 **billion**/s) | Infeasible. `architecture.md` §12.16's rig-level rejection and batched inner loop move from optional cleanup to **required**, scheduled with the nearest-hit tie-break fix — three changes, one file, one issue |
| With rig-level rejection: ~33 M tests/s, and the 100 rig bounds sit in L1 | Comfortable. **The spatial grid stays out of scope**, along with uniform-radius as an optimisation |
| ~10,000 sprites/frame, one texture per shell type | **The texture atlas un-defers** (§3.5). Quads are free; thousands of texture binds are not |
| ~5,000 projectile create/destroys per second | The one place `architecture.md` §5's 🧊 memory pooling might genuinely trigger. **Watch, do not build** — §5 says profile first, and this is the reason to profile |
| Objects outside the camera are simulated but not drawn | **Cull by camera AABB before drawing, and substitute a runtime-generated icon** (§8.2's `IconRenderer`) below a few pixels. A rendering rule, not an optimisation — it is also what makes §8.1's zoom-out into the navigation map a continuous degradation rather than a mode switch |

*Resolved since earlier drafts:* **Template economics** (§2.6 — player chooses lump sum or
royalties, negotiated against faction disposition) and **research permanence** (§2.5 — knowledge
lives in networks, which die with their hosts).

*Resolved 2026-08-07:* **Aiming model** (§3.2 — player aims manually via cursor, no target lock;
NPCs roll a random hardpoint), **targeting priority's driver** (§3.2 — pilot/commander skill, never
faction role), **fighter localized damage** (§3.2 — uniform with capitals, no exemption),
**research vs. engineering vs. deconstruction** (§2.4 — three distinct mechanics sharing only a
facility gate), **engineering merge scaling** (§2.4 — facility level, not engineer skill; ratifies
what is already built), **where skill lives** (§2.7 — a crew module that mounts into a shell, not a
hidden stat or a separate entity type), **the mass/power model** (§2.2 — shells add mass, modules add
mass and draw power, both recomputed on every change; a shell cannot be removed while occupied),
**docking vulnerability** (§3.4 — a docked vessel cannot be shot but dies with its host),
**manufacturing's home** (§2.8 — `ConstructionSystem` keeps vessels; modules go to a new system,
because they produce inventory rather than entities), **the tier count** (§2 — shell and component
are one thing; there are two tiers, not three, and no `ComponentDef` is needed), **crew power draw**
(§2.7 — zero, as a value rather than a `PowerSystem` exception; life support **cut** entirely, so
crew and shell always die together), **cockpit placement** (§3.2 — a discrete crew shell at *every*
scale, which is what forced fighters to grow), and **the uncrewed hull** (§3.2 — crew death disables
rather than destroys).

*Resolved 2026-08-08:* **the full scale system** (§3.5 — per-shell hardpoint sizes with collision
matching art, chassis at 50% of hull radius, 50-unit fighters, a 2,500-unit cap on fightable hulls,
system radius scaling with star class to a 2,000,000 ceiling), **hardpoint placement** (§3.5 — free
placement bounded below by `r(A)+r(B)` and above by visual attachment), **rendering layers** (§3.5 —
five draw layers, needing a new `ShellDef` field), **art direction** (§3.5 — stylised high resolution,
not pixel art), **texture resolution per class** (§3.5 — capital chassis above ~600 units segmented
from a shared library), **Military Weight** (§4.3 — offence × survivability,
accumulated in a `double`, computed at demotion rather than cached), **upkeep** (§2.7 — 🧊 deferred;
repair, refuel and assembly already provide the sink), and **fog of war** (§8.3 — per faction,
inherited as a frozen copy when founding a new one).

*Resolved 2026-08-08, second pass:* **the word "craft"** (§2 — a crafted intermediate item; a vessel
is a *vessel*), **the quality band** (§2.7 — a grade is a multiplier band, every instance rolls a
point in it, adjacent bands overlap), **budget distribution** (§2.7 — a grade also caps how many
stats the roll may spread across; more stats, less potent each), **merge bounds** (§2.4 — clamped to
the band, refused at the ceiling, gain measured against headroom), **hardpoint count** (§3.5 —
emergent from `hullRadius`, chassis radius and peripheral size; validation rules 10–12; no
`maxMounts`), **shell mass** (§2.2 — proposed as derived from radius, which is what makes the hull
envelope self-correcting), **the save model** (§3.3 — free saves plus a coarse autosave that never
fires on death, because Tier 2's recovery run is what a death-triggered autosave would kill), **the
player's location** (§3.4 — always exactly one shell, flying or aboard; dies with it), **crew roles**
(§2.7 — sensors and repair have live consumers today, damage control and navigation do not), **the
damage roster** (§3.1 — two shield types, three weapon types, Ion suppressing power rather than
dealing hull damage), **manufacturing's shape** (§2.8 — a queued job with re-rolled quality, gated by
a grade-*N*−1 input chain), and **the performance budget** (§9.1 — including that galaxy extent is
unbounded provided the macro tick iterates records rather than coordinates).

*Downgraded 2026-08-07:* **whether the player may reload instead of accepting Hard Game Over**
(§3.3) — leaning yes, offered explicitly on the Hard Game Over screen alongside continuing in the
same galaxy. The save model underneath it is still open.

**Economy content scale — the one number nobody has estimated.** §5.0 settles *how* per-system,
per-item faction stock is stored and kept efficient. What is unestimated is how many distinct item
ids the content set will actually reach once modules, shells, and vessels are authored across seven
grades — which is the input to whether the sparse ledger stays comfortably small or wants a second
look. Cheap to measure once `elements.json` and `materials.json` exist; do not guess it now.

**Faction heads.** §6.5 settles that a boss encounter is any commanded fleet, which needs no new
system. Whether a *faction head* — §5.1's other Leadership pillar holder — is simply a commander of
unusually high grade, or something with its own rules, is undecided. The former needs nothing; the
latter is a feature.

*Resolved 2026-08-09 — the command pass:* **operating and commanding are simultaneous** (§4.0 —
the two-mode model settled 2026-08-08 is **reversed**; the player flies, shoots, and commands with
no mode switch), **command is equipment** (§4.0 — a `Comms` module for reach and a commanding `Crew`
module for authority, both on the rig you occupy; a fighter so fitted commands from its cockpit),
**crew consolidate to one kind** (§2.7 — `ModuleKind::Crew` replaces `Operator`/`Commander`, with
`operation` and `command` as rollable stats, so the ace pilot and the fleet admiral are two rolls of
one module and neither is strictly better), **full selection from a fighter** (§4.3 — §4.3's
"cursor-free verbs only," settled 2026-08-08, is **reversed**: right-click selects and issues,
keys arm the verb, and left-click never stops firing), **order queues and stance are separate axes**
(§4.3 — queues hold tasks, stance holds Hostile/Defensive/Peaceful, and `CommanderOrders` turns out
to have been a stance enum all along), **order availability is emergent** (§4.3 — derived from
living hardpoints, which is why a station with engines can move and why the `mobile` flag's movement
gate is deleted), **live refit is unrestricted** (§2.7 — legal any time, priced by §3.4's no-pause
rather than by a station gate), and **construction is gated on `FacilityKind::Construction`**
(§2.11 — carrying `buildRange`, measured from the builder).

*Resolved 2026-08-09 — the wiring-audit pass (`architecture.md` §13):* **stars are hazards, not
walls** (§3.8 — a corona that burns with depth rather than a lethal radius, so death needs no new
rule, Energy shields let you dive deeper, and grazing a star to shake a pursuer is real play),
**the point of no return is emergent** (§3.8 — gravity already out-accelerates a fighter ten to one
at ≈1,500 units; nothing draws the line and it differs per hull, so shooting out an engine moves
it), **planets are non-colliding background** (§3.8 — scenery and landmarks; occlusion and
sheltering are a deliberate later feature with a stated revisit trigger), and **the asteroid belt
orbits and moves outward** (§3.8 — asteroids on the same deterministic orbit treatment planets get,
banded entirely outside the corona and the point of no return, correcting a placement that would
have made the inner belt a one-way trip once mining became reachable).

*Resolved 2026-08-09 — the materials pass:* **the supply chain is `Element → Material → Module`**
(§2 — three tiers; a fourth `Compound` tier rejected because it re-asks the Material tier's own
question and each extra hop averages attribute contributions toward the mean), **no element is rarer
than another** (§2.10 — the Common/Uncommon/Rare/Anomalous bands are deleted, resolving a
contradiction the section had with its own opening sentence; scarcity is now purely a property of
territory and is therefore *perceived*, differing between two factions looking at one galaxy),
**elements carry an eight-attribute vector rather than a quality score** (§2.10 — a single "how good"
number would restore the rarity ladder through the back door; every attribute has a named consumer,
and two proposed additions were rejected for lacking one), **attributes propagate deterministically
while quality rolls once** (§2.10 — what you built it from is a choice, how well it came out is a
roll), **recipes demand roles, never named elements** (§2.10 — substitution means a missing element
costs quality rather than access, which is what makes a hyperdrive softlock impossible), **every
system carries at least one element per role** (§2.10 — the seeding invariant that makes "cannot be
hard-stuck" testable rather than hoped-for), **volatiles are ordinary Elements at stored-liquid
density** (§2.10 — withdrawing a separate resource class proposed earlier the same day), and
**gas giants and nebulae become gathering sites** (§2.10 — which is what finally gives planets an
economic reason to exist after `architecture.md` §12.28 left them as scenery).

**Strategic command's gating — the one question this pass left open.** Tactical command now travels
with the player. Whether the *strategic* layer — cross-system dispatch, fleet movement, per-system
build queues — stays reachable only from a bridge is undecided, and it blocks nothing: strategic
command additionally waits on galactic coordinates (`architecture.md` §12.17) before "send that fleet
there" has a *there*. See §4.2.

**Mod load order.** `data/mods/` exists in the directory plan. Override vs. merge semantics, load
order, and what is moddable at all are unspecified (🧊 deferred, but worth deciding before the
registry format calcifies).
