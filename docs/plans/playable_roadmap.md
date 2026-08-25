# Star Reach: The Playable Roadmap

**Compiled:** 2026-08-11 · **Sources:** `docs/features.md` (design), `docs/architecture.md`
(engineering, §12 homes, §13 wiring audit, §15 behavior audit), verified against `src/` on the same
date.

---

## What this file is

`architecture.md` §13.5 is a **task list grouped by what unblocks what**. This file is a **sequenced
roadmap grouped by what the player can do at the end of each phase**, and it is a superset: it also
covers the `features.md` design sections that §13.5 deliberately scopes out (items, manufacturing,
skill, crew, the bridge, the coarse loop, persistence, content authoring, tooling).

**Where the two disagree on detail, `architecture.md` wins.** Every task below cites the section
that owns its specification; this file never restates a spec, it schedules one.

### The starting position, stated once

Pressing **Start Game** produces an empty world at the origin with no player in it
(`architecture.md` §0). Thirty systems tick over an empty registry. Eight of thirty are wired end to
end. Nine menu files handle no input and are referenced by nothing but their own tests.
`data/base_game/` holds three JSON files. 424 tests pass — they exercise systems in isolation, which
is exactly why the wiring gaps survived.

**The work is overwhelmingly wiring and content, not new systems.** Of the 113 tasks below, ten
introduce a new system or store.

### How to use it

- Each task is sized to be **one GitHub issue and one PR**, and follows this repo's
  `Home:` / `Types:` / `Systems:` / `Content:` / `Tests:` issue shape (`CLAUDE.md`,
  `architecture.md` §11.3).
- **`Depends on:` names tasks in this file.** Before starting, confirm the dependency has *merged to
  `main`* — `architecture.md` §11.9's rule, and it applies here unchanged.
- **⚡ = startable today**, nothing blocking — **and it means it**: 21 of the 113 tasks carry it, and
  no task carrying it declares a dependency (audited 2026-08-11, when six did). Enough parallel work
  is always available, and the mark can be trusted when picking the next thing up.
- Branch prefix follows the issue label: `feature/`, `fix/`, `docs/`, `chore/`.
- Phases are **judgement gates, not sprints.** A phase is done when its exit criteria can be
  demonstrated in a running build, not when its tasks are merged.

### Filing these as issues

For whoever converts this file into a GitHub tracker:

1. **Title:** `[P4-01] The router` — the task id in brackets, then the task title verbatim. The id is
   the stable key; it is what every `Depends on:` in this file resolves against.
2. **Body:** copy the task's bullets **unchanged**, then add the two lines this file cannot supply:
   `Depends on: #NN` (the issue numbers of the tasks named in `Depends on:`) and
   `Spec: docs/architecture.md §X` repeated from the `Docs:` line. **Do not paraphrase the cited
   spec into the issue** — the issue points at the spec, it does not replace it, and the specs are
   revised more often than this file is.
3. **Maintain the id → issue-number map** — the table at the bottom of this file, pre-populated with
   all 113 ids and awaiting only the numbers. Fill in each row as you file, and tick **Merged** only
   when the PR lands on `main`. Without it, §11.9's rule (*confirm the dependency has actually merged
   to `main`*) has nothing to check against, and dependencies degrade into prose.
4. **Label** from the task's `Label:` line, which also picks the branch prefix (`feature/`, `fix/`,
   `docs/`, `chore/`).
5. **Do not file ❓ tasks as build issues.** File them as design issues — see Appendix D.

#### Three one-time steps before issue #1

*Verified against the repo 2026-08-11. Do these in order; each blocks the next.*

**① This file must be on `main` first.** Every issue body will carry `Spec: docs/architecture.md §X`
and every `Depends on:` resolves against ids that live here. File an issue while this file is on a
branch and it points at nothing a reader can open — and the id → issue map below, which §11.9's
*confirm-it-merged* rule has nothing to check without, is an **untracked** file one `git clean` from
gone.

**② The four labels — ✅ created 2026-08-11, nothing to do.** They did not exist when this file was
written: `Trosper3/Star-Reach` carried only GitHub's stock set (`bug`, `documentation`,
`enhancement`, `duplicate`, `good first issue`, `help wanted`, `invalid`, `question`, `wontfix`), so
step 4 above **silently no-opped and the branch-prefix rule had no input.** `feature`, `fix`, `docs`
and `chore` are now on the repo. The commands are kept only so the set can be rebuilt or mirrored:

```bash
gh label create feature --repo Trosper3/Star-Reach --color 0E8A16 \
  --description "New capability — branch prefix feature/"
gh label create fix     --repo Trosper3/Star-Reach --color D73A4A \
  --description "Defect repair — branch prefix fix/"
gh label create docs    --repo Trosper3/Star-Reach --color 0075CA \
  --description "Documentation — branch prefix docs/"
gh label create chore   --repo Trosper3/Star-Reach --color FBCA04 \
  --description "Tooling, CI, content checks — branch prefix chore/"
```

⚠️ **Do not delete the stock labels they overlap** (`bug`, `documentation`, `enhancement`). Deleting
a label strips it from every issue already carrying it, and the history predates this file.

**③ File in phase order, in batches — not all 113 at once.** The order in this file *is* dependency
order, so `Depends on: #NN` can always name an issue that already exists, with the three exceptions
in the table below.

| Batch | Tasks | Why stop there |
|---|:---:|---|
| **First** | **Phase 0 + Phase 1** | **24 issues** — everything needed to reach M1, containing most of the ⚡ set, and a tracker a person can still read end to end |
| Then | One phase at a time | Phase 0 will teach you things that revise later task text. Filing 113 up front freezes wording this file's own note says gets revised more often than the roadmap does |

**The three forward dependencies**, which name a task filed in a *later* batch. File the issue with
the dependency written as the **id**, then backfill the `#NN` when the target exists:

| Task | Names | Backfill when filing |
|---|---|---|
| **P7-05** | `P9-01` | Phase 9 |
| **P9-07** | `P10-01` | Phase 10 |
| **P10-04** | `P11-04` | Phase 11 |

*(P11-01 also names `T-02`, which sits in the Tooling & QA set rather than a numbered phase — file
the T-tasks alongside the first batch if you want it resolved in order; T-01, T-03 and T-04 are ⚡
and depend on nothing.)*

### Before implementing any task

**Read the cited section in full.** Task bodies here are summaries sized to be scannable; the
`Docs:` line names the authority. Where this file and `architecture.md` disagree, `architecture.md`
wins, and the disagreement is a bug in this file worth reporting.

**Read `architecture.md` §12.8 once, before the first task.** It is *"The Constraints That Apply To
All Of It"* — the rules no individual task cites because they govern every task, which is exactly
how a section like it gets skipped.

Then follow `CLAUDE.md`'s workflow unchanged — fresh branch off `main`, confirm dependencies merged,
build and run `sr_tests` in one tree, hand off without committing.

### Status key

| Mark | Meaning |
|:---:|---|
| ⚡ | Startable today — no unmerged dependency |
| 🔗 | Blocked by a task in this file |
| ❓ | Needs a design decision before it can be built (see Appendix B) |
| 🧊 | Out of scope — listed in Appendix C so nobody re-derives it |

---

## The milestone ladder

| # | Milestone | Phases | The player can… |
|:---:|---|:---:|---|
| **M1** | **It is a game** | 0–1 | Start, fly a visible world, shoot, be shot, dock, quit to the menu, start again |
| **M2** | **Combat has teeth** | 2 | Lose hardpoints, read what is hurting them, die and recover — against an enemy that manoeuvres, retreats, and leaves salvage |
| **M3** | **The meso loop** | 3–4 | Dock anywhere, store, repair, engineer, research, refit under fire — **buying and selling is M5**, with the prices it needs |
| **M4** | **The game explains itself** | 5 | Read the status display, HUD, comms and navigation map without a wiki |
| **M5** | **The economy runs** | 6–7 | Mine → manufacture → design → sell a Template and collect royalties |
| **M6** | **Command** | 8 | Order units, run a party, operate from a bridge, employ sub-commanders |
| **M7** | **A living galaxy** | 9 | Watch factions fight, expand, collapse; warp on fuel; own territory |
| **M8** | **Shippable** | 10–11 | Save, load, and play against a full content roster |

**M1 is the only milestone with a hard ordering claim.** Everything from M2 onward has parallel
tracks; the phase numbers are a recommended order, not a lock.

---

## Phase table

| Phase | Goal | Exit criteria | Tasks |
|:---:|---|---|:---:|
| **0** | Headless foundations | The world is drawable and shootable; the widget layer exists; the audit's one-line defects are gone, **and the schedule that runs every system is under test**. All verifiable by `sr_tests` with no window | 16 |
| **1** | The micro loop | Start Game → a visible system with a player, a station, NPCs → fly, shoot, dock, respawn, quit to menu, start again cleanly | 8 |
| **2** | Combat that reads true | Damage types, shields, structural failure, crew death and capture behave as `features.md` §3 specifies — **and the opposition fights back, dies, and leaves something behind** | 12 |
| **3** | Faction state | Relations exist at runtime, are seeded, are read by combat/docking/pricing, and are written by gameplay | 5 |
| **4** | The docked screens | Six of §12.30's seven screens reachable through the router — **Market is the seventh and ships in P6-08**, because a price needs `Pricing.h`; both flight overlays work | 14 |
| **5** | Legibility | Status display, flight HUD, comms, navigation map, tutorial, signature/detection | 7 |
| **6** | Items and economy | Elements → Materials → Modules manufacture and price themselves | 13 |
| **7** | Knowledge and templates | Reverse-engineer, draft, pitch, collect royalties | 5 |
| **8** | Command | Local command, construction, parties, sub-commanders, the bridge | 6 |
| **9** | The living galaxy | Coarse tick, faction AI with a caller, seeding, territory, fuel-gated warp | 9 |
| **10** | Persistence | `RigState`, world save/load, autosave, a main menu with a save picker | 6 |
| **11** | Content breadth | Full element/material/module/shell/vessel/faction rosters, **plus audio and the asset pipeline** | 9 |
| — | **Tooling & QA** | Runs alongside every phase | 5 |

---

# Phase 0 — Headless foundations

*Everything here is verifiable by `sr_tests.exe` with no window and no player. It exists so that
Phase 1 produces something judgeable, and so seven screens are not written against a widget layer
that does not exist. `architecture.md` §12.28 and §13.5 group 4a both argue this explicitly.*

**Exit criteria:** a populated `SystemWorld` renders every body it contains and every body can be
damaged; `shared/ui/` has one row widget instead of four; §13's one-line findings are closed; the
schedule that invokes every system is itself under test (P0-16); the test suite still passes.

---

### P0-01 · World bodies, hittability and the star hazard ⚡
**Docs:** `architecture.md` §12.28 (full spec) · §13.3 A, B · §13.5 group 1
**Depends on:** —
**Label:** `feature`

- **Home:** `shared/components/Physics.h`, `modes/space/factories/WorldGen.cpp`,
  `modes/space/render/WorldRenderer.cpp`, `modes/space/systems/{OrbitSystem,ProjectileSystem}.cpp`,
  new `modes/space/systems/HazardSystem.{h,cpp}`.
- **Types:** `enum class BodyKind { Star, Planet, Wreck, Drop, Asteroid, Anomaly }` — **add
  `Anomaly` now**, even though nothing spawns one until P5-06: `features.md` §1.2's prologue turns on
  one, `lore.md` §2 makes anomalies a recurring source of rare tech and the Voidwalkers' migration
  target, and the renderer's `switch` is deliberately exhaustive with no `default`, so adding the
  enumerator later means revisiting every site instead of one;
  `WorldBody { float radius; BodyKind kind; }`; `Corona { float range; float damagePerSecond; }`.
- **Systems:**
  - `ProjectileSystem::FindHit`'s view narrows to `HitRadius, WorldTransform` — `ParentRig` moves
    into the loop body, plus a `Destroyed` skip. This alone makes asteroids shootable.
  - `WorldGen` gives the sun `WorldBody`+`Corona`, planets `WorldBody`, and asteroids
    `WorldBody`+`HitRadius`+**`OrbitBody`** — asteroids *lose* `Velocity` (they must not enter the
    physics view; §12.28's own correction explains why the belt would drain into the star).
  - Asteroid belt moves out to 1,800–2,800.
  - `OrbitSystem` writes `PreviousTransform` before moving a body.
  - `WorldRenderer` gains `DrawWorldBodies`, first pass, sorted by `BodyKind`, interpolating only
    when `PreviousTransform` is present. `DrawWorld`'s body becomes the single statement of draw
    order.
  - New `HazardSystem`: view `Health, WorldTransform`, `exclude<Destroyed>`; queues corona damage
    with **`PendingDamage::source = entt::null`** (a real source makes every escort attack the sun
    via `PartySystem::FindAttacker`). Schedule after `HierarchySystem`, before
    `CollisionSystem`/`ProjectileSystem`; add both constraints to `SystemSchedule.cpp`'s comment
    block.
  - `MiningSystem`/`LootSystem` drops and wrecks gain `WorldBody`.
  - Rig roots draw as a nose-forward triangle when `Propulsion` is non-zero and a disc when it is
    not — so a station stops rendering as an arrowhead and a de-engined hull reads as a hulk.
- **Tests:** an asteroid takes projectile damage and is tagged `Destroyed`; `MiningSystem` then
  produces a drop; a body inside the corona loses health with a null damage source; a body at
  `Corona::range` takes zero; every `BodyKind` is drawn (exhaustive `switch`, no `default`).

> **This is the single highest-leverage task in the file.** Four dead subsystems — mining, the
> tutorial's asteroid step, material loot, `LootSystem`'s material path — come back on one `emplace`
> and one narrowed view.

---

### P0-02 · The shared widget layer ⚡
**Docs:** `architecture.md` §12.30 "The shared widget layer" · §13.5 group 4a
**Depends on:** — **Label:** `feature`

- **Home:** `shared/ui/` (`sr_shared_ui` becomes `STATIC`).
- **Types:** `UiInput`, `Row` (incl. `Row::glyph` and `Row::fill`), and five widgets: `PanelFrame`,
  `ListView`, `Button`, `TabStrip`, `Gauge`. Immediate-mode, stateless, pure functions over POD.
- **Systems:** none. **Delete `InventoryGrid`**; fold the four hand-rolled `kRowHeight = 20.0f` row
  loops (`StationServicesMenu`, `RefactorMenu`, `EngineerMenu`, `InventoryGrid`) into `ListView`.
- **Tests:** `ListView` hit-test maps cursor → row index and to none outside the content rect;
  scroll offset clamps at both ends and does not move when all rows fit; `PanelFrame`'s content rect
  is inset and never inverted at small sizes; an empty list renders an empty **row**, never a blank
  panel.

> Landing this before Phase 4 is what stops seven screens being written in parallel against no
> shared widget — which is how `InventoryGrid` acquired two verbatim clones before any consumer
> could run.

---

### P0-03 · Galaxy topology ⚡
**Docs:** `architecture.md` §12.17 · §12.15 🐛
**Depends on:** — **Label:** `feature`

- **Home:** `core/galaxy/Topology.{h,cpp}` — `sr_core`, no raylib, no registry.
- **Types:** `SystemCoord` (int32 per axis), `SystemId` **derived from the coordinate**,
  `SystemRecord` (coord, discovered name, faction claim, §7.2's persisted per-system state),
  `Topology` (coord ↔ id, neighbours, warp adjacency).
- **Systems:** none — a store, like `KnowledgeNetwork`.
- **Tests:** coord ↔ id round-trips; the same coordinate yields the same id **on every platform**,
  asserted against hardcoded expected values; neighbour queries are symmetric; a system with no
  record still resolves to a seed.
- **Note:** the macro tick must iterate existing `SystemRecord`s, **never the coordinate space** —
  one wrong loop caps galaxy size permanently and presents only as "the game got slow."

> Unblocks the most of anything startable: nav-map levels 1–2, strategic fleets, per-system fog,
> territory adjacency, and the `std::hash` warp-seed desync.

---

### P0-04 · The schedule and `Docked` correctness sweep ⚡
**Docs:** §13.3 G, H · §12.34 (exclusion half) · §15.1 finding 2 · §13.5 group 2
**Depends on:** — **Label:** `fix`

- **Home:** `modes/space/systems/{SystemSchedule,NpcAiSystem,WeaponSystem,ProjectileSystem,CollisionSystem}.cpp`.
- **Systems:**
  - Move `HierarchySystem` **after** `PhysicsSystem` and before `DockingSystem`. Every hit test in
    the game currently resolves against hardpoint positions one tick stale.
  - `NpcAiSystem` and `WeaponSystem` `exclude<Docked>` — a docked NPC currently thrusts out of the
    bay and keeps firing.
  - `ProjectileSystem::FindHit` and `CollisionSystem`'s ramming-candidate view `exclude<Docked>` —
    closing the half of §3.4's "a docked vessel is not a target" that removing `Targetable` doesn't
    cover.
  - Update `SystemSchedule.cpp`'s ordering comment block in the same commit (§2.4).
- **Tests:** a hardpoint's `WorldTransform` matches its root's *current*-tick pose; a docked NPC's
  throttle stays zero and it emits no `FireIntent`; a docked rig survives a direct projectile hit
  and a ram aimed at it.

---

### P0-05 · The hardpoint lifecycle defect pack ⚡
**Docs:** §13.3 D, E, F, V · §12.30.5 · §12.30.7 · §15.1 findings 8, 9 · §13.5 group 2
**Depends on:** — **Label:** `fix`

- **Home:** `shared/components/`, `shared/rig/ModuleAttachment.cpp`,
  `modes/space/systems/{ModuleEquipSystem,WeaponSystem,RefactorSystem,StationServicesSystem}.cpp`.
- **Types:** `MountTraverse { float radians; }`, written by `RigFactory` at spawn.
- **Systems:**
  - `ModuleEquipSystem` passes the real traverse instead of the hardcoded `0.0f` — **a live-refitted
    weapon currently gets a zero-width arc and never fires.**
  - `WeaponSystem` reads `PowerShed` (it has zero readers) and `SpawnProjectiles` uses
    `FiringArc::currentOffset` for the shot bearing (it is simulated and discarded).
  - `RefactorSystem` refuses to delete the **last** hardpoint, refuses a hardpoint that still holds
    modules (rather than refunding them, per §2.2's settled reversal), returns **nothing** when
    scrapping a `Destroyed` hardpoint, and stops routing its room check through
    `CargoHoldHasRoomFor`'s count-not-mass bug.
  - **One `Destroyed` sweep across every hardpoint writer** (§12.30.7): repair must not heal a
    destroyed hardpoint, scrap must not refund its modules, mount must not fit a module to it.
    `DockingSystem` and `DamageSystem` are the only two writers that test the tag today.
- **Tests:** a runtime-mounted weapon fires within its authored arc and not outside it; a shed
  hardpoint does not fire; deleting the last hardpoint is refused and the rig survives; a destroyed
  hardpoint cannot be repaired, refunded or mounted.

---

### P0-06 · `MountedModules` is the single mount record ⚡
**Docs:** §13.3 C · §13.4 decision 2 (accepted) · §12.30.7
**Depends on:** — **Label:** `fix`

- **Home:** `shared/components/`, `shared/rig/ModuleAttachment.cpp`,
  `modes/space/systems/{ModuleEquipSystem,RefactorSystem}.cpp`, `modes/space/ui/ModulesMenu.cpp`.
- **Types:** **delete `EquippedModule`**; `MountedModules` is the sole record of a mount's contents.
- **Systems:** `ModuleEquipSystem`'s occupancy check, mount and unmount all read/write
  `MountedModules`; `ModulesMenu::EquippableMounts` uses the same predicate.
- **Tests:** every blueprint-mounted hardpoint on a fresh ship reads as **occupied**; mounting into
  an occupied mount is refused; unmounting returns the module that was actually there; scrapping
  refunds exactly once.

> **Priority fix, not cleanup.** Today `EquippableMounts` offers every occupied mount on a fresh
> ship as an empty slot; mounting there silently destroys the original and scrapping duplicates it.
> Both are one click away on a surface Phase 4 ships. **§12.31's `RigState` cannot be written
> against two answers**, so this is load-bearing for saves too.

---

### P0-07 · Delete the `mobile` movement gate ⚡
**Docs:** `architecture.md` §12.25 · §13.3 J
**Depends on:** — **Label:** `fix`

- **Home:** `modes/space/factories/RigFactory.cpp`, `shared/blueprints/`.
- **Systems:** always emplace `Propulsion` **and `LinearDamping`**, on every root; capability becomes
  emergent from living engine hardpoints rather than a blueprint flag.
- **Tests:** a `mobile: false` blueprint with engine shells produces non-zero `Propulsion` and moves;
  a station inside `kSunGravityRange` does not accelerate without bound (today it has `BodyMass` +
  `Velocity` and no damping, so gravity flings it).
- **Note:** prerequisite for P1-05 placing a station anywhere near the star, and for §12.27's Move
  order availability.

---

### P0-08 · Rig aggregation: `RecomputeRigTotals` and the Sum/Max rule ⚡
**Docs:** `architecture.md` §12.23 "The aggregation rule" · §12.16 item 8
**Depends on:** — **Label:** `feature`

- **Home:** `shared/rig/ModuleAttachment.{h,cpp}`, `modes/space/systems/DamageSystem.cpp`.
- **Types:** each rig-level attribute declares **Sum** or **Max**: `thrust`/`turnTorque`/
  `fuelCapacity` sum; **`maxSpeed`, `sensorRange`, `jumpRange` max**.
- **Systems:** `RecomputeRigTotals` runs on mount, unmount **and every hardpoint death** —
  `Propulsion` is currently zeroed only when the *last* engine dies, and `BodyMass` is never
  recomputed at all. `DamageSystem` runs last in the schedule, so the ordering already works.
- **Tests:** propulsion falls proportionally as engines die and reaches zero only at the last one;
  two engines double acceleration and do not raise top speed; mounting an armour module raises
  `BodyMass` and the rig accelerates measurably slower.

> Required by live refit (§2.7), by P0-07, and by the loadout overlay — *a swap that does not change
> how the hull flies is the mechanic broken.*

---

### P0-09 · Four new `ModuleKind`s for systems that have no module ⚡
**Docs:** `architecture.md` §12.23 "New kinds"
**Depends on:** — **Label:** `feature`

- **Home:** `shared/blueprints/Taxonomy.h`, `shared/rig/ModuleAttachment.cpp`,
  `modes/space/systems/WeaponSystem.cpp`, `data/base_game/modules.json`.
- **Types:** `ModuleKind::{Sensor, CargoBay, FireControl, Hyperdrive}`.
  - `Sensor` — `SensorRange` is read by `DiscoverySystem` and produced by nothing (hardcoded 2000).
  - `CargoBay` — `slotCount` × `slotCapacity`, total derived, never stored.
  - `FireControl` — drives `FiringArc::turnRatePerSecond`, today hardcoded to `kPi`. **Do not merge
    into `Weapon`**; separate modules are what let a cheap turret with a good gunner match an
    expensive automated one.
  - `Hyperdrive` — `WarpSystem` has no module and no fuel reference at all (gate lands in P9-06).
- **Systems:** `PowerPriorityFor(ModuleKind)` absorbs all four with no fifth category (FireControl →
  Weapon priority; the rest → Facility).
- **Content:** one authored module per kind in `modules.json`.
- **Tests:** a rig with no sensor module has zero sensor range; `turnRatePerSecond` comes from the
  mounted fire control; each new kind lands in the correct power priority band.

---

### P0-10 · `CargoHold` moves onto the bay 🔗
**Docs:** `architecture.md` §12.23 "The hold lives on the bay" · §12.30.3
**Depends on:** P0-09 — **Label:** `feature`

- **Home:** `shared/components/`, new `shared/rig/CargoView.h`, every `CargoHold` reader.
- **Types:** `CargoHold` becomes per-bay `{ stacks, slotCount, slotCapacity }`; `CargoView` is the
  **one write path**; placement is automatic and must not read `Rig::children` order.
- **Systems:** two limits mean two refusals (slot count and mass) — the row model already wanted
  this. `CargoHoldEntryCount`/`CargoHoldHasRoomFor` are re-expressed over `CargoView`; the four
  systems that write a hold unchecked start checking.
- **Tests:** a full hold refuses a deposit and reports which limit refused it; blowing off a cargo
  bay reduces capacity and spills nothing that was never there; placement is deterministic and
  independent of child order.

---

### P0-11 · The repair path defect pack ⚡
**Docs:** `architecture.md` §12.30.4 · §13.3 I · §13.4 decision 1 (accepted)
**Depends on:** — **Label:** `fix`

- **Home:** `modes/space/systems/{DockingSystem,StationServicesSystem}.cpp`.
- **Systems:**
  - **Delete `DockingSystem::HealAndImmobilize`'s heal loop** (15%/s, free, unconditional, no
    facility check) and rename the function. It predates the facility model and removes the credit
    sink §2.7 depends on.
  - **The rate is not deleted with it** — `kDockHealPerSecond` moves to
    `FacilityStats::ratePerSecond`, which is parsed, merged, and read by nothing today, and is
    `features.md` §2.7's Repair crew role's only named consumer.
  - `ProcessRepairRequests` gains a **`FacilityKind::Repair` gate** (it has none — the station
    handle is only used as a docked-ness check) and a **`Destroyed` exclusion** (it currently heals
    a permanently dead hardpoint to full, making §3.9's colour-is-condition schematic draw a
    destroyed mount green).
- **Tests:** docking heals nothing; repair at a station with no Repair facility is refused; a
  destroyed hardpoint is not healed; repair rate scales with the facility's authored rate.
- **Note:** deleting the free heal also **removes NPC repair** (the view is `<Docked, Rig>` with no
  player filter and NPCs carry no `Wallet`). **It re-homes onto `ctx.economy` in P4-04, not in
  P3-01** — §12.30.4's dependency table is explicit that step 6 is needed *"only for the NPC payer"*
  and that the player path needs nothing, so the payer branch lives with the Repair screen that owns
  `StationServicesSystem`'s repair path. `ctx.economy` is already non-null today, so nothing about
  this waits on Phase 3.
- **Accepted regression window:** between this task and P4-04, NPCs do not repair. §12.30.4 calls
  that *"a visible regression rather than a silent one"* and accepts it. Do not re-add a heal
  anywhere to paper over it.

---

### P0-12 · The research chain defect pack ⚡
**Docs:** `architecture.md` §12.30.6 · §13.5 group 2
**Depends on:** — **Label:** `fix`

- **Home:** `modes/space/systems/ResearchSystem.cpp`, `core/galaxy/ResearchRecord.h`,
  `shared/components/`.
- **Systems:**
  - `ResearchSystem::Tick` gains a **`FacilityKind::Research` gate** — blowing the lab off a station
    does not currently stop the jobs in it.
  - **Delete `StationFacility::researchTier`** — a *third* tier system beside `FacilityStats::level`
    and `Grade`, written only by tests.
  - Rename `ResearchJob::cost` / `ResearchRecord::cost` → **`durationSeconds`**; they hold seconds in
    a codebase where every other `cost` is credits.
  - A null `ctx.knowledge` **freezes the job instead of erasing it** — today the guard spends the
    input and produces nothing.
- **Tests:** destroying the lab halts progress; a null knowledge pointer leaves the job resumable
  with its input intact.

---

### P0-13 · The `Element` / `Material` rename — one pass or none ⚡
**Docs:** `features.md` §2 (supply vocabulary) · `architecture.md` §13.5 group 2b · §12.19b
**Depends on:** — **Label:** `chore`

- **Home:** `shared/components/`, `modes/space/systems/{MiningSystem,LootSystem}.cpp`,
  `shared/blueprints/Validation.cpp`.
- **Types:** `Element` is the **raw** tier and `Material` the **manufactured** one — which *flips*
  the meaning of names already in the code. `CargoHold::materials`, `MaterialStack`, `MaterialDrop`,
  `MaterialChance`, `AsteroidComposition::materials` and `MiningSystem`'s spawn path all hold what
  are now **Elements**.
- **Systems:** fold in §13's finding that these are untyped `std::string` ids against no registry
  (`"iron"`, and a `"silica"` that is not an element — silicon is; silica is SiO₂).
- **Content:** none — **no `materials.json` exists yet, which is why this is the cheapest it will
  ever be.**
- **Also:** `Validation.cpp`'s message *"a mobile craft needs an engine"* — `features.md` §2 retired
  *craft*; a vessel is a vessel. The docs were swept 2026-08-09; this string was not, because it is
  code.
- **Tests:** existing mining/loot tests pass under the new names; no identifier in `src/` uses
  *material* in the old sense.

> **One commit or none.** A half-migrated tree where one word means both things is worse than either
> name alone.

---

### P0-14 · Delete `TickCoarse` until it has a driver ⚡
**Docs:** §13.3 L, M · §13.4 decision 3 (accepted) · §2.4
**Depends on:** — **Label:** `chore`

- **Home:** `modes/space/systems/{CommanderSystem,DiscoverySystem,FactionEconomySystem}.cpp`.
- **Systems:** delete the three `TickCoarse` definitions. They are §2.4 dead abstractions — three
  implementations of an interface with no caller, which is the exact "scaffolded, never adopted"
  pattern §0 opens with. **P9-01 reinstates them with the loop.**
- **Tests:** none new; the suite must still pass.

---

### P0-15 · Group 2 leftovers: text, tabs and one-liners 🔗
**Docs:** §13.5 group 2 · §15.1 findings 15, 18, 21, 22, 23, 24 · §13.3 W · §13.4 decision 5
**Depends on:** P0-02 — **Label:** `fix`

- **Home:** `modes/space/ui/{BridgeView,BuildMenu,CustomizeMenu,RefactorMenu,ModulesMenu,StationServicesMenu}.*`,
  `data/base_game/{ships.json,modules.json}`, `shared/blueprints/Validation.cpp`.
- **Systems / UI:**
  - `BridgeView::kAllKinds` gains `Engineering` **plus a `static_assert` on the enumerator count** —
    a hand-maintained parallel list missing a value is why both Engineering menus can never surface.
  - `BuildMenu::CanAfford` reads `CargoHold`, not just `Wallet` (the file's own header already
    claims it does).
  - `CustomizeMenu::Draw` iterates `ValidationResult::errors` and names the failed rule instead of
    rendering the literal string `"INVALID"`.
  - `RefactorMenu::Draw` renders non-deletable hardpoints as **disabled rows** rather than omitting
    them (§8.3: *absence must never look like emptiness*), each with its real name/`ShellRole` glyph
    instead of the literal `"hardpoint"`.
  - `ModulesMenu::Draw` calls `EquippableMounts` as well as `EquippedMounts` — already exported,
    already correct, never called.
  - `StationServicesMenu.h`'s promotion-note citation moves from §12.11 to §12.10.
- **Content:** `traverseRadians` on `forgotten_scrapper`'s `gun_nose` (it omits the field, so the
  ship's only gun essentially never fires); weapon mounts on `aegis_outpost` (the only station in
  the content set cannot defend itself).
- **Validation:** an omitted `traverseRadians` on a weapon-capable shell becomes a **`Validation`
  error**, not a silently unusable gun.
- **Tests:** `AvailableTabs` returns all six kinds when all six live; a weapon mount with no authored
  traverse fails validation naming the file and key.

---

### P0-16 · The schedule is tested 🔗
**Docs:** `architecture.md` §0 · §2.4 · §13 (the audit this closes the recurrence of)
**Depends on:** P0-04 — **Label:** `chore`

> **Why this is a task at all.** `SystemSchedule.cpp` names all 30 systems today and is correct —
> verified 2026-08-11 by diffing `systems/*.h` against the table. **Nothing tests that.** No test
> file references `SystemSchedule` at all: every system test builds its own `SystemContext` and calls
> `xxx_system::Tick` directly, so a test passes identically whether or not the schedule runs it.
> Delete a line from the table and the entire suite still goes green.
>
> That is the legacy failure exactly — §0's headline finding was systems that existed, compiled, were
> unit-tested, and **were never invoked**. It is closed here by construction rather than by vigilance,
> because vigilance is what already failed once.

- **Home:** `tests/unit/SystemScheduleTests.cpp` (**new**), `tests/CMakeLists.txt` (explicit source
  list — add the file *and* reconfigure).
- **Systems:** none. This is a test-only task; it changes no game behaviour.
- **Tests:**
  - **Membership** — every system in the schedule appears exactly once, and the count matches the
    number of `*System.h` under `modes/space/systems/` (excluding `System.h` and `SystemSchedule.h`).
    Prefer asserting against a list the test owns over re-deriving from the filesystem: a test that
    reads the directory can never fail, because it re-derives the very thing it is checking.
  - **Order** — the constraints P0-04 establishes, as relations rather than absolute indices, so an
    insertion elsewhere does not spuriously fail: `HierarchySystem` after `PhysicsSystem` and before
    `DockingSystem`; `WeaponSystem` before `ProjectileSystem` before `CollisionSystem` before
    `DamageSystem`. **This is why the task depends on P0-04** — encoding the order first would pin
    the bug P0-04 exists to fix.
  - **The check must be able to fail:** removing an entry fails membership, and swapping two ordered
    entries fails order. Confirm both by hand once, per P11-01's rule that *a check that has never
    failed is not known to work.*
- **Relationship to T-03.** T-03 is the general CI check for components with no reader or writer;
  this is the specific, cheap unit test for the one wiring surface every system passes through.
  T-03 supersedes nothing here — it runs at a different layer and on a different trigger, and this
  one is ~20 lines available in Phase 0 rather than whenever the tooling batch lands.

---

# Phase 1 — The micro loop

*`architecture.md` §12.24, amended by §13.5 group 1, §12.29 and §12.30.1. **Steps 1–4 are one issue
in §12.24's own framing** — each is a few lines and none is independently verifiable — but they are
split below where a later section imposed a real requirement.*

**Exit criteria — the M1 demo:** Start Game → a system with a sun, planets, an asteroid belt, a
station and NPCs, all visible → the player flies with the camera following → shoots and destroys an
asteroid and an NPC → docks → opens the system menu → quits to the main menu → starts a second game
with exactly one of everything.

---

### P1-01 · `OnEnter`: the world and the player 🔗
**Docs:** §12.24 step 1 · §12.30.1 · §12.29 (re-entrancy) · §13.3 P · §13.5 group 1
**Depends on:** P0-01, P0-07 — **Label:** `feature`

- **Home:** `modes/space/SpaceFlight.cpp` (`OnEnter()`/`OnExit()` are both empty today),
  `shared/components/` (the one new type below).
- **Types:** **`PlayerLocation { entt::entity shell; }` is declared here, not in P4-01.** It exists
  nowhere in `src/` today, and the bullet below cannot be followed without it. P4-01 is where it
  becomes *authoritative* — the router writes it per-hardpoint and `PlayerControlled` becomes derived
  from it — but the type and its first write site are this task's, or this task has nothing to
  emplace.
- **Systems:**
  - Extract the shared body of `WarpToSystem` — `world_gen::PopulateSystem` → `rig_factory::Spawn` →
    player marking — rather than writing it twice. **Use the same `std::hash` placeholder seed
    `WarpToSystem` uses** (one bug to fix in P0-03's follow-up, not two).
  - The player rig gets **`CargoHold` + `Wallet`** at spawn (four systems bail on a null hold today;
    only `WarpToSystem` emplaces them) and **`ActorRef`** (P1-02).
  - **Emplace `PlayerLocation`, not `PlayerControlled`** — §12.30.1 makes location the source of
    truth and the tag derived. `PlayerControlled` has one write site today against thirty readers;
    writing it the other way now is cheap and expensive later.
  - **Write it re-entrant:** `OnExit()` releases the world (`world_ = SystemWorld{}` plus
    `intents_.Clear()`); `OnEnter()` resets `world_` and `clock_` before populating. Without this,
    starting a second game populates on top of the first — two suns, two players, and the first one
    still ticking underneath.
- **Tests:** entering produces exactly one sun, one player, one station and the expected NPC count;
  **`OnEnter` twice in a row leaves exactly one of each**; the player has a hold and a wallet.

---

### P1-02 · Player input through the intent queue 🔗
**Docs:** §12.24 step 2 (full spec, including the A-vs-B argument) · §3.6
**Depends on:** P1-01 — **Label:** `feature`

- **Home:** new `modes/space/systems/PlayerInputSystem.{h,cpp}` and
  `modes/space/ui/FlightControls.{h,cpp}`; two lines in `CMakeLists.txt`'s `sr_space` list.
- **Types:** `ActorRef { core::ActorId id; }` in `shared/components/Identity.h`. **`ActorId` moves
  down to `shared/blueprints/Ids.h`** in the same commit — `shared/` may not include `core/`
  (§2.3), and the component is illegal otherwise.
- **Systems:**
  - `FlightControls::Poll(IntentQueue&, ActorId)` reads raylib and pushes at most one
    `SetThrottleIntent` and one `FireWeaponsIntent`, called from `SpaceFlight::Update` **before**
    `clock_.Advance`.
  - `PlayerInputSystem` drains them, resolves `ActorId` against the `ActorRef` view, writes
    `ThrustInput` / emplaces `FireIntent`. **It must not poll raylib** — a system takes a bare
    `SystemContext`, and the headless harness depends on that staying true. An unresolvable
    `ActorId` is ignored, never asserted.
  - Schedule immediately after `HierarchySystem`, **before `PhysicsSystem`, `WeaponSystem` and
    `DockingSystem`** — the last constraint is the one that would silently break (a docked player
    would fly away).
  - **The dock key moves off `E`** (§12.24 step 2's sub-item).
- **Tests:** a held key produces motion at any frame rate; an intent naming a dead actor is dropped;
  a docked player cannot thrust; `FireIntent` reaches `WeaponSystem` in the same tick it was pushed.

> This makes `core::IntentQueue` — Law 9's only concrete expression, with zero producers today —
> real, on the highest-frequency case in the game.

---

### P1-03 · Weapon groups, and no automatic target lock 🔗
**Docs:** §12.24 step 2's 🐛 and "Weapon groups gate firing" · §3.2 · §3.6 · §13.3 Q
**Depends on:** P1-02 — **Label:** `feature`

- **Home:** `modes/space/systems/TargetingSystem.cpp`, `modes/space/render/IconRenderer.cpp`,
  `modes/space/ui/FlightControls.cpp`, `shared/components/`.
- **Types:** `AimPoint` (the cursor's world position) and the weapon-group assignment `features.md`
  §3.6 binds.
- **Systems:** `TargetingSystem` **excludes `PlayerControlled`** — §3.2 forbids the player an
  automatic lock. Firing resolves against `AimPoint`; weapon groups gate which mounts respond.
  `IconRenderer::DrawTargetReticle` is **deliberately repurposed to the cursor aim point or
  deleted** — not left to silently no-op.
- **Tests:** no `Target` is ever emplaced on the player; a shot goes where the cursor is, not where
  the nearest enemy is; a group-2 key fires only group-2 mounts.

---

### P1-04 · Camera, and the two render defects 🔗
**Docs:** §12.24 step 3 · §13.3 AB · §0
**Depends on:** P1-01 — **Label:** `fix`

- **Home:** `modes/space/SpaceFlight.cpp`, `modes/space/render/WorldRenderer.cpp`.
- **Systems:** assign `cameraTarget_` from the player's interpolated transform (it is
  default-constructed and never written; the view is nailed to the origin). **Delete the duplicate
  `render::DrawWorld` call** at `SpaceFlight.cpp:145` — it runs twice per frame with identical
  arguments. Size the drawn silhouette from the **tested** shape rather than `CollisionRadius`, so
  shots stop passing through the drawn nose and flanks.
- **Tests:** the camera target tracks the player across a warp and a respawn; `DrawWorld` is called
  once per frame.

---

### P1-05 · A station to dock at 🔗
**Docs:** §12.24 step 4 · §13.3 O, R · §12.30.6 (`NetworkOwner`)
**Depends on:** P0-07, P0-10, P1-01 — **Label:** `feature`

- **Home:** `modes/space/factories/{WorldGen,StationFactory}.cpp`.
- **Systems:** `WorldGen` spawns a station (it spawns none today; `station_factory::Spawn` is
  reachable only from `ConstructionSystem`, which has no producers). `StationFactory` stops being a
  six-line pass-through and emplaces the **five components with zero producers anywhere**:
  `StationFacility`, `CargoHold` (with a **non-zero capacity** — an unbounded hold makes
  `LocalPrice` meaningless), `SpawnAnchor`, `NetworkOwner`, and a `Wallet`.
- **Content:** `aegis_outpost` gains the facility hardpoints Phase 4 needs (see P4-01's content
  bullet); weapon mounts land in P0-15.
- **Tests:** a generated world contains a dockable station of the player's faction; the station's
  hold accepts and refuses correctly; `SpawnSystem` finds an anchor.

---

### P1-06 · Death, respawn and the cull 🔗
**Docs:** `features.md` §3.3 (cost of failure) · §13.3 R · §12.5 (recovery run)
**Depends on:** P1-05 — **Label:** `feature`

- **Home:** `modes/space/systems/SpawnSystem.cpp`, `modes/space/systems/LootSystem.cpp`,
  `core/galaxy/WreckRecord.{h,cpp}` (built, uncalled).
- **Systems:** `RespawnPending` gains a producer (player death); `SpawnSystem` resolves it against
  the station's `SpawnAnchor`; `CullFarRigs` starts running (the 20,000-unit registry bound §1.1
  relies on does not exist at runtime today). Player death promotes the hull into a
  `WreckRecord`/`DeathWreck` per §3.3 Tier 2, marked on the navigation map for its window.
- **Tests:** dying respawns the player at the anchor with the documented losses; the wreck exists and
  is recoverable inside the window and gone after it; a rig 25,000 units out is culled.

---

### P1-07 · The system menu and quit to main menu 🔗
**Docs:** `architecture.md` §12.29 (full spec) · §3.4 · §3.6 · §13.3 Y
**Depends on:** P1-01 — **Label:** `feature`

- **Home:** new `modes/space/ui/SystemMenu.{h,cpp}`; `SpaceFlight::ShouldReturnToMenu()`;
  `main.cpp`'s mode loop.
- **Types:** open/closed state lives on the **singleton entity** (the `CommsLog` precedent), not a
  `SpaceFlight` member.
- **Systems:**
  - `main.cpp` gains the mirror of the transition it already has — today it is **one-way** and the
    only exit from a running game is closing the window.
  - **It pauses**, and that is legal precisely because the menu confers nothing: Resume and Quit
    only. **The power-priority list and the weapon-group editor are excluded** — both carry in-fight
    value and would void §3.4's exception. *Re-read §12.29 before ever adding an entry.*
  - The **`Esc` ladder**, innermost first: cancel build placement → clear selection → open menu →
    close menu.
  - Quit prompts with **"all progress is lost"** stated plainly — there is no save yet, and a vague
    "Are you sure?" implies the player could have avoided it.
  - **Save / Load / Settings are absent, not disabled** (§2.4: a button that cannot work is a dead
    abstraction). They arrive in P10-03.
- **Tests:** `ShouldReturnToMenu()` is a latch that does not re-fire; the `Esc` ladder resolves
  innermost-first; **pausing does not bank real time** in `FixedTimestep`'s accumulator, so
  unpausing does not fast-forward the world through however long the player sat in the menu.

---

### P1-08 · M1 verification pass 🔗
**Depends on:** P1-01 … P1-07 — **Label:** `chore`

Not a code task. Play the M1 demo end to end, record what is wrong, and file what it finds. Phase 1
is the first point in this project's history where playing it is possible, and §13's whole thesis is
that a system nobody can reach accumulates defects that look correct in tests.

---

# Phase 2 — Combat that reads true

*`features.md` §3. Most of this is startable today — it is sequenced after Phase 1 because it is
judged by eye, not because it is blocked.*

*Three tasks were added here on 2026-08-11 (P2-09, P2-10, P2-11) after a coverage sweep found that
the phase modelled **being hit** completely and **fighting** not at all: nothing converted a kill
into a reward, the opposition was a 67-line stub, and no task anywhere repopulated a system. All
three sat in the middle of the core loop rather than at the edges — see Appendix D's note, which
predicted the opposite.*

**Exit criteria:** three damage types that look and behave differently; shields that absorb by type
and are bypassed by ramming; hardpoints that fail structurally and cascade; a crew shell whose death
kills the player; a hull that can be disabled and taken; **an enemy that patrols, closes, breaks off
when it is losing, and leaves a wreck worth salvaging; and a faction that still holds the system
because its commander is alive.**

---

### P2-01 · The damage-type effect table, and Ion ⚡
**Docs:** `architecture.md` §12.33 · `features.md` §3.1, §3.7 · §15.1 findings 1, 5 · §13.3 AA
**Depends on:** — **Label:** `feature`

- **Home:** `shared/components/Damage.h`, `modes/space/systems/DamageSystem.cpp`,
  `modes/space/systems/CollisionSystem.cpp`, `modes/space/render/WorldRenderer.cpp`.
- **Types:** `DamageType` gains **`Ion`**; a `DamageTypeEffect` table drives absorb/hull/power split.
- **Systems:** `DamageSystem` gets a generic path instead of exact-type-match absorption. **Ramming
  bypasses shields** — today it is tagged `Kinetic` and absorbed exactly like gunfire, the opposite
  of the documented mechanic and of ramming's whole tactical identity. Projectile colours follow
  §3.9's palette (Energy blue, Kinetic purple, Ion electric white-blue) — today Ion and Kinetic
  render identically, which is the worst possible pair to conflate since Ion is absorbed by neither
  shield type.
- **Tests:** a charged Kinetic shield absorbs a kinetic *shot* and not a *ram*; Ion passes both
  shield types and suppresses power; each type renders its own colour in projectile, shield shimmer
  and status display alike.

---

### P2-12 · Directional ECM — click-and-hold jamming 🔗
**Docs:** `features.md` §8.3 "Sensor ghosts and ECM — two roles, passive area and directional" · §3.6
**Depends on:** P2-07, P5-07 — **Label:** `feature`

*Revised 2026-08-24 — the original version of this task rode the Ion weapon's damage-type machinery.
That was wrong: jamming does not damage anything, so it does not belong on a `DamageType`. It is now
`ModuleKind::ECM`'s Directional role, mechanically identical to P2-07's Tractor Beam — aim, hold, no
projectile, no hit roll.*

- **Home:** `modes/space/systems/{TargetingSystem,CommsSystem}.cpp`, `core/knowledge/KnowledgeNetwork.*`
  (P5-07 supplies the fabricated/degraded-entry capability this reads), new input handling for `U`
  (§3.6).
- **Types:** no new `DamageType`. A held-effect state on the carrier (which target is locked, held
  since when), mirroring however P2-07 tracks an active Tractor Beam contest.
- **Systems:** while `U` is held with a valid target under the cursor and the carrier's ECM hardpoint
  is alive and powered, each tick: writes a ghost or degrades a real entry in the target's
  `KnowledgeNetwork` (P5-07's write path), and suppresses the target's effective `commsRange` for the
  duration — cutting command authority, hailing, and sensor datalink to comms-linked allies
  identically to a destroyed `Comms` hardpoint, without the hardpoint dying. The hold breaks on
  release, on the hardpoint dying, or on the target leaving range/line-of-sight — same shape as
  Tractor Beam, no new rule. **Runs simultaneously with weapon fire and with Tractor Beam** (§3.6): all
  three read the same cursor-designated target.
- **Content:** one authored `ModuleKind::ECM` def (Directional role) is enough for this task; a
  Passive Area def ships alongside it from P5-07.
- **Tests:** a held target's `commsRange` reads suppressed and a jammed ship cannot hail while held,
  restoring the instant `U` is released; a jammed fleet member stops both contributing to and
  receiving its formation's shared sensor coverage for the hold's duration; holding `K`, `U`, and
  left-click on the same target at once succeeds at all three; jamming never reduces `Health` or
  triggers a structural-integrity check.

---

### P2-09 · What a kill leaves behind — the wreck path and the reaper 🔗
**Docs:** `features.md` §2.7 "Drop rates" · §3.3 · §2.10 "Gathering" · `architecture.md` §12.5
**Depends on:** P0-01 — **Label:** `feature`

> **Added 2026-08-11. Nothing in the codebase converts a kill into a reward, and no task did
> either.** `LootDrop` and `DerelictWreck` have **zero `emplace` sites in `src/`**; `DamageSystem`'s
> root-death branch tags `Destroyed` and stops; `LootSystem`'s header says outright *"this system
> never creates an entity from nothing — there is no `LootFactory` yet."* The only `DeathWreck`
> producer is `PromoteWreckRecord`, restoring a **saved** wreck. So §2.7's seven-tier drop ladder
> has no consumer, and the macro loop's first step — salvage — never fires.

- **Home:** `modes/space/systems/LootSystem.cpp` (the producer it has never had),
  `modes/space/systems/SystemSchedule.cpp` (the ordering comment, §2.4). **No new file, and no
  `LootFactory`** — see the layering note below.
- ⚠️ **Layering — resolve this in the task, do not route around it.** `modes/space/systems/` **may
  not include `factories/`** (§2.3), and the codebase's *one* narrow exemption belongs to
  `ConstructionSystem`, granted because *"construction IS assembly."* A wreck is **not** assembly:
  it is a single entity carrying POD components, with no hierarchy and no blueprint→entity
  conversion — exactly what `MiningSystem` already spawns inline today
  (`MiningSystem.cpp:39`, `registry.emplace<MaterialDrop>`) with no factory and no exemption. **So
  this spawns inline in `LootSystem`, adds no second exemption, and Law 5 is untouched** — Law 5
  guards `RigFactory`'s role as the one blueprint→entity converter, not every `emplace`.
  **`LootSystem.h`'s header comment is wrong and is corrected in the same commit** (§11.7 — a rule
  that is wrong gets changed with its reasoning written down, not worked around): it claims *"this
  system never creates an entity from nothing… there is no `LootFactory` yet,"* which reads as a
  deferral when the real answer is that a drop never needed one.
- **Systems:**
  - **A kill spills and leaves a hull.** `LootSystem` gains a `view<Destroyed, Rig>` pass: the
    victim's `CargoHold` becomes pickups at the death site, and the hull becomes a `DeathWreck`
    carrying what was mounted on it. It runs **after `DamageSystem`**, which is already last in the
    schedule and is what tags `Destroyed` — so the pass sees every death from that tick, whether the
    cause was gunfire, a ram, or P2-02's cascade, and there is one death path rather than three.
    Add the constraint to `SystemSchedule.cpp`'s comment block in the same commit (§2.4).
  - **Mounted modules are never handed over intact.** They come out of the wreck through P6-11's
    **harvesting** verb — the third of that task's three sites — partial and damaged, at a yield the
    beam decides. **This is what keeps capture worth doing** (P2-07): stripping shields, suppressing
    power and taking the hull whole is strictly better than shooting it apart, and it should read
    that way at the moment the player first compares them.
  - **The husk is reaped.** A `Destroyed` rig currently stays in the registry **forever** — the
    renderer excludes it so it vanishes visually, while 28 of 31 systems keep iterating it, and
    `CullFarRigs` filters on distance only. Once the wreck is written, the same pass destroys the
    hull entity and its hardpoints, reusing the deferred `toDestroy` vector `LootSystem` already
    uses so nothing is destroyed mid-iteration. This is the same "one wrong loop presents only as
    *the game got slow*" failure P0-03's note warns about.
- **Deliberately partial:** the wreck holds the victim's cargo and a module manifest; **turning that
  manifest into items needs P6-03's `ItemStack` and P6-11's beam.** Ship the producer and the reaper
  now — they are what every later reward path attaches to.
- **Tests:** a combat kill produces exactly one `DeathWreck` and spills the victim's hold as
  pickups; the destroyed root and every hardpoint of it are **gone from the registry** on the
  following tick; a rig killed with an empty hold still leaves a wreck; the wreck expires on its
  `lifetimeSeconds` and a `DerelictWreck` does not.

---

### P2-02 · Docked cascade destruction 🔗
**Docs:** `architecture.md` §12.34 · `features.md` §3.4 · §15.1 finding 3
**Depends on:** P0-04 (exclusion half), P2-09 — **Label:** `feature`

- **Home:** `modes/space/systems/DamageSystem.cpp`.
- **Systems:** when a rig is tagged `Destroyed`, every rig with `Docked.station == thisEntity` routes
  through **the same** `DeathWreck` path P2-09 builds for a combat kill — the identical function, a
  different cause. A dying rig's `CargoHold` spills as pickups at the wreck site. Runs in
  the same tick, immediately after the existing rig-death branch, before anything treats the orphaned
  `Docked.station` as valid.
  > ⚠️ **Corrected 2026-08-11.** This task previously read *"the same `DeathWreck` path `LootSystem`
  > already runs for a combat kill."* **There is no such path** — see P2-09, which now builds it and
  > which this task depends on.
- **Tests:** multiple docked rigs on one dying host all resolve; a host with an empty bay dies
  cleanly; cargo appears as pickups.

---

### P2-03 · Shields, narrow-phase collision and the structural cascade ⚡
**Docs:** `architecture.md` §12.22 · `features.md` §3.1, §3.2, §3.7
**Depends on:** — **Label:** `feature`

- **Home:** `modes/space/systems/{DamageSystem,CollisionSystem}.cpp`, `shared/blueprints/ShellDef.h`.
- **Systems:** shield coverage modes (a shield currently protects only its own housing);
  `CollisionSystem`'s narrow phase becomes per-hardpoint circles rather than a convex hull;
  destruction **cascades along `StructuralAttachment`**; `ApplyRamDamage` scales with absolute mass;
  `ShellDef.acceptsKinds` replaces the hardcoded mountability table.
- **Tests:** a Personal shield covers its housing and a Zone shield covers its neighbours; severing a
  structural parent destroys its children; ramming a station in a fighter is asymmetric in the
  fighter's disfavour.

---

### P2-04 · Structural integrity and honest hit resolution 🔗
**Docs:** `features.md` §3.2, §3.5, §3.9 · §13.5 group 2d · §12.16 items 17, 19 · §9.1
**Depends on:** P2-03 — **Label:** `feature`

- **Home:** `modes/space/systems/ProjectileSystem.cpp`, `modes/space/ui/CockpitHud.cpp`,
  `shared/blueprints/Validation.cpp`, `modes/space/render/WorldRenderer.cpp`.
- **Systems:** structural integrity as a derived aggregate with failure at ~30% and a normalised
  display that reaches a true zero; **most-specific-wins hit resolution** in `FindHit` replacing
  first-in-iteration order; `Validation` rule 12 (chassis plus armour must cover the hull envelope);
  draw the tested shape until real art exists.
- **Tests:** two overlapping hardpoints resolve to the more specific one deterministically; a hull at
  29% integrity fails structurally; a blueprint with an uncovered envelope is rejected.

---

### P2-05 · Object scale, `hullRadius` and draw layers 🔗
**Docs:** `features.md` §3.5 · `architecture.md` §12.16 item 19 · §15.1 finding 26
**Depends on:** P2-04 — **Label:** `feature`

- **Home:** `shared/blueprints/ShellDef.h`, `shared/blueprints/Validation.cpp`,
  `modes/space/render/WorldRenderer.cpp`.
- **Types:** `hullRadius` (**no such concept exists anywhere in the codebase today**), a draw-layer
  field on `ShellDef`, and the optional baked collision polygon.
- **Systems:** validation rules 10 and 11 (separation, attachment); hardpoint count emergent from
  `hullRadius`, chassis radius and peripheral size; `DrawHardpoints` sorts by §3.5's five intra-rig
  layers, tie-broken by local y.
- **Tests:** the §3.5 scale table's fighter/capital cases produce the documented hardpoint counts; a
  ventral module never draws over a dorsal one.

---

### P2-06 · The crew shell 🔗
**Docs:** §13.3 Z · `features.md` §2.7, §3.2, §3.4 · §12.16 items 10, 15, 16
**Depends on:** P0-08, P0-09, P2-03 — **Label:** `feature`

- **Home:** `shared/blueprints/Taxonomy.h`, `shared/rig/ModuleAttachment.cpp`,
  `modes/space/systems/{DamageSystem,NpcAiSystem}.cpp`, `data/base_game/{modules.json,shells.json}`.
- **Types:** `ModuleKind::Crew`, with **all four rollable stats §2.7 says are buildable today** —
  `operation`, `command`, **`sensors`** and **`repair`**. There are zero occurrences of "crew"
  anywhere in `src/` today, and four settled designs depend on it.
  - The last two were previously unscheduled and **both already have live consumers**: `sensors`
    feeds `SensorRange`, which `DiscoverySystem` reads and P0-09's Sensor module produces; `repair`
    feeds the Repair facility's `ratePerSecond`, which P0-11 calls *"§2.7's Repair crew role's only
    named consumer."* §2.7's other two roles — damage control and navigator — **stay out**: each
    needs a mechanic that does not exist, and §2.4 forbids authoring a number nothing reads.
- **Content:** **crew slots are an authored `ShellDef` field** — §2.7's *"one authored field carries
  all of it"* — so a shell decides how much crew it can carry. Author it here; P11-04 fills the
  roster with it.
- **Systems:** destroying the crew shell **disables** a hull rather than destroying it; `NpcAiSystem`
  gains a crew check so an uncrewed hull stops flying and firing; the player is always associated
  with exactly one shell and **dies with it**, whether flying or aboard a station.
  - **Officers provide percentages, modules provide base stats** (§2.7) — a crew stat is a
    **multiplier applied after** P0-08's Sum/Max pass, never a term inside it. That is a **third
    aggregation rule** and it belongs beside the other two in `RecomputeRigTotals`, documented
    there, or the next reader will find two rules in one place and a third somewhere else.
- **Content:** crew modules — §12.16 item 10 notes these need content before code — plus the
  `ShellDef` crew-slot field above.
- **Tests:** an uncrewed hull goes dead and adrift but is not `Destroyed`; killing the player's crew
  shell ends the run whether the player is flying or docked.

---

### P2-07 · Capture — boarding in place 🔗
**Docs:** `features.md` §3.2 · §13.5 "Blocked, and worth knowing why" · §6.3
**Depends on:** P2-06 — **Label:** `feature`

- **Home:** `modes/space/systems/DockingSystem.cpp` or a boarding path beside it; `FactionRef`
  reassignment; new `Troop Bay` and `Tractor Beam` hardpoint modules (`data/base_game/modules.json`).
- **Systems:** a hull is capturable when shields are down (any damage type), it cannot flee (engines
  destroyed, hyperdrive destroyed, or **held by a Tractor Beam**), and it cannot resist (crew shell
  destroyed or power suppressed — Ion is **one tool among several** here, not the intended route, per
  `features.md` §3.2's 2026-08-09 correction). Ownership transfer is boarding-in-place: a Troop
  Bay-carrying vessel holds position adjacent for a duration (scaled by capacity vs. hull class) to
  flip `FactionRef`. **Tractor Beam is the non-destructive "cannot flee" path** — pull force contested
  against `BodyMass` × `Propulsion::thrustNewtons`, resolved every tick in the physics integrator
  while the player holds `K` (§3.6) — every other path to "cannot flee" means shooting the engines
  off, which damages the prize. §6.3 requires the same terms for an AI faction capturing the player's
  uncrewed hull.
- **Tests:** a crewed hull cannot be captured; a captured hull's crew, network included, transfers
  wholesale (this is also §2.5's network-raiding mechanic, at no extra cost); an AI faction can
  capture the player's uncrewed hull; releasing `K` before the hold completes drops the tractor and
  the target regains its own thrust immediately.

---

### P2-08 · Power allocation — the half the player commands 🔗
**Docs:** `features.md` §2.9 · §3.6 · §13.3 X · §12.16 item 18
**Depends on:** P1-02 — **Label:** `feature`

- **Home:** `modes/space/systems/PowerSystem.cpp`, `modes/space/ui/AvionicsMenu.cpp`,
  `shared/blueprints/ModuleDef.h`, `data/base_game/modules.json`.
- **Types:** four levels per **category** (Offline / Reduced / Normal / Boosted); **draw and effect
  multipliers authored per module** (Law 10 — `ModuleDef` has no such field and `modules.json`
  authors none).
- **Systems:** the four category keys `F`/`G`/`H`/`J`; **boost refuses without headroom** rather than
  browning out the ship; `Ctrl`-held engine afterburner; a player-configurable priority list living
  on the **avionics surface, which does not pause** (§12.29 excludes it from the system menu by
  design — configuring under fire is the intended cost).
- **Tests:** boosting weapons without headroom is refused and says so; setting engines Offline stops
  thrust and frees exactly its authored draw; the priority list changes which hardpoint sheds first.

> §13.3 X: today `PowerSystem` implements roughly a third of §2.9 — the part that reacts to damage —
> and none of the part the player commands, which is the entire point of the section as counterplay
> to hardpoint fragility.

---

### P2-10 · The opposition state machine 🔗
**Docs:** `architecture.md` §4 (the `NpcAiSystem` inventory row) · §10 item 7 · `features.md` §6.3 ·
§3.2 · §14.1
**Depends on:** P0-04, P2-01 — **Label:** `feature`

> **Added 2026-08-11.** `NpcAiSystem` is **67 lines of approach-and-fire**, and its own header says
> so: *"The full Patrol/Chase/Attack/Flee/Escort state machine from the section 4 system inventory
> is out of scope here."* §4's inventory row nevertheless lists that state machine as **✅** — a
> stale row of exactly the class §14.1 already found five of. Phase 2 is titled *combat that reads
> true*, and every other task in it models **what happens when you are hit**; this is the only one
> about **how the enemy fights.** Without it the M2 gate judges combat against an opponent that
> cannot retreat, cannot regroup, and cannot be outmanoeuvred.

- **Home:** `modes/space/systems/NpcAiSystem.{h,cpp}`, `shared/components/` (the state, POD).
- **Types:** `AiState { Patrol, Chase, Attack, Flee, Escort }` and its per-rig transition thresholds.
  **POD only** (Law 1) — no behaviour on the component, and no `std::vector` where a child entity
  would do (Law 4).
- **Systems:**
  - The five states, with transitions derived from what the rig already carries — no new sensor
    concept: acquisition from `TargetingSystem`'s `Target`, disengagement from `Health`/rig
    integrity, reach from `SensorRange` (P0-09's module, once it exists; the hardcoded 2000 until
    then).
  - **`Flee` is the load-bearing one.** It is what makes structural damage frightening rather than
    arithmetic, and it is the state P4-04's retreat-to-repair extends into a dock — **P4-04 does not
    invent retreat, it gives `Flee` a destination.**
  - **`Escort` writes no party membership of its own.** A party is created by a Defend order
    (P8-02), which is `PartyLeader`/`PartyMember`'s one producer; until then `Escort` is station-
    keeping on a named entity and nothing more. **Two producers for one relationship is the defect
    this roadmap exists to close** — do not add a second here.
  - **The system still writes only `ThrustInput` and `FireIntent`** and still does not aim
    (`WeaponSystem`'s `FiringArc` owns that), so its contract with the schedule is unchanged: after
    `TargetingSystem`, before `WeaponSystem`, `exclude<Docked>` (P0-04) and `exclude<PlayerControlled>`.
  - **§6.3 forbids an AI-only path** — where a behaviour has a player equivalent, the NPC uses the
    same components the player does. It does not get a private route to a result the player cannot
    reach.
- **Content:** none — thresholds are authored per hull only if §2.11's module stats cannot supply
  them. Prefer derivation (Law 10 keeps tuning in JSON, but a number nothing reads is §2.4's dead
  abstraction).
- **Tests:** a damaged NPC disengages at its threshold and stops firing; a patrolling NPC acquires,
  closes and attacks; an NPC that loses its target returns to patrol rather than freezing; an
  escorting NPC keeps station without acquiring `PartyMember`; a docked NPC does none of it (P0-04);
  **§4's inventory row is updated to match reality in the same commit** (§2.4).

---

### P2-11 · Faction commanders exist, and hold a system 🔗
**Docs:** `features.md` §4.5 · §5.1 · §6.6 · `architecture.md` §12.2 · §12.3 · §12.16 item 22
**Depends on:** P1-05, P2-06, P2-10 — **Label:** `feature`

> **Added 2026-08-11, and it settles how the galaxy stays populated.** Nothing repopulates a system
> today: `WorldGen` spawns 3–5 NPCs once and no task added more. **The answer is not a spawner —
> it is that factions act through commanders**, who gather, build, expand, fight and negotiate
> exactly as the player does. The built code already assumes this and was written waiting for it:
> `EvaluateSurvival`'s `hasLeadership` parameter is documented as *"the faction head or an AI
> sub-commander still alive,"* and `EvaluateColonization` takes its candidate system **from the
> caller**. `Commander` has no producer anywhere. This task is that producer.

- **Home:** `modes/space/factories/WorldGen.cpp` (seeding), `modes/space/systems/CommanderSystem.cpp`
  (Tier 1 behaviour), `shared/components/Commander.h` (existing — no new type).
- **Types:** **none new.** `Commander { KnowledgeNetworkId network; CommanderOrders orders;
  FactionId faction; }` already exists and already lives on the vessel it commands, *"so destroying
  the capital destroys the commander for free, no special-case code needed."* Per §12.16 item 22 it
  belongs on the **bridge hardpoint**.
- **Systems:**
  - **Three commanders per faction at seed**, on capitals or stations in that faction's holdings.
    A commander is a `Crew` module with a non-zero `command` roll mounted to a Bridge (P2-06,
    §4.5) — **not a separate acquisition track**, so seeding uses the same equip path a player would.
  - **Tier 1 only, in the resident system:** order patrols, dispatch defenders at a threat, queue a
    local build. `Commander::orders` finally gets a reader. **Expansion, colonisation and
    cross-system dispatch are Tier 3 and land in P9-05** on the macro tick — the same component,
    a wider scope, never a second implementation.
  - **`hasLeadership` gets its answer**: a faction with at least one living commander holds §5.1's
    leadership pillar. This is what makes the pillar mean something and what makes **decapitation a
    real strategy** — kill a faction's commanders and it stops acting, expanding and rebuilding
    until it recruits another.
  - **Factions recruit replacements** on the macro tick when they hold a bridge, a `Crew` module and
    the stock to fit it (P9-03). So decapitation cripples and destabilises; it finishes only a
    faction already economically broken, which is §5.1's three-pillar logic working as designed
    rather than a special case.
  - **The player is a commander**, not an exception — §12.3's *"the player is not a special case"*
    and §3.3's Hard Game Over being the same predicate on the player's own faction both fall out of
    this for free (P9-08).
- **Tests:** a seeded galaxy has three living commanders per faction; destroying a commander's
  vessel removes the commander and nothing else; a faction with no living commander reports
  `hasLeadership == false`; a commander orders a local patrol and a defender responds; **the player's
  own faction is evaluated by the identical predicate.**

---

# Phase 3 — Faction state

*Five `SystemContext` pointers are `nullptr` at runtime, so five systems silently no-op against
their own null guards. `DiplomacyMatrix` has **zero writers anywhere** — the exact failure
`features.md` §5.3 was written to prevent, reproduced.*

**Exit criteria:** a hostile is hostile because the matrix says so; the matrix is seeded at startup
and written by gameplay; docking, targeting, pricing and the nav map all read it.

---

### P3-01 · The five null pointers, seeded relations, and the Reapers ⚡
**Docs:** §12.24 step 6 · §12.32 · §13.5 group 3 · §14.6 · `features.md` §5.3–§5.7
**Depends on:** — **Label:** `feature`

- **Home:** `modes/space/SpaceFlight.cpp` (populates only `economy` today),
  `core/diplomacy/{Relation.h,DiplomacyMatrix.cpp}`, `core/diplomacy/Reputation.cpp`.
- **Types:** `Relation` widens from three states to the full **six** `features.md` §5.3 specifies —
  which is what lets the Reapers' three priority rivals be stored as `War`, distinct from the other
  five factions' `Hostile`. `Reputation`'s thresholds widen to match.
- **Systems:** populate `discovery`, `knowledge`, `diplomacy`, `reputation`, `craftedModules`.
  `SeedBaselineRelations` and `SeedReaperHostility` run at the **same startup point** — a live
  pointer into an unseeded matrix is the same null-object failure one layer down. `SelectReaperTarget`
  (a max-by-key over caller-supplied structural-density scores) lands here too.
- **Tests:** the five pointers are non-null in a fresh `SpaceFlight`; the baseline matrix matches
  §5.4's table exactly; the Reapers are hostile to every unlisted faction and allied with Pyre;
  `TemplateMarketSystem::PassesGate` can now return true.

> **`TemplateMarketSystem` is a guaranteed no-op until `ctx.diplomacy` is non-null**, regardless of
> any producer work anywhere else.

---

### P3-02 · Combat, docking and the map consult the matrix 🔗
**Docs:** §13.3 N · §15.1 finding 17 · §12.30.3 🐛 · `features.md` §5.3
**Depends on:** P3-01 — **Label:** `fix`

- **Home:** `modes/space/systems/{TargetingSystem,DockingSystem}.cpp`, `modes/space/ui/NavigationMap.cpp`.
- **Systems:** `IsHostile` stops being `!(seeker.id == other.id)` — **faction inequality** — and
  consults `DiplomacyMatrix`. `DockingSystem::FindEligibleBay` stops filtering on `FactionRef`
  equality and applies §5.3's **six-row docking band table**; this is what makes docking at a station
  you do not own possible at all, which the entire Market screen assumes.
  `NavigationMap::VisibleHostileRigs` consults `DiscoveryState`/`DiplomacyMatrix`.
- **Tests:** a neutral faction's rig is not auto-acquired; docking at an allied station succeeds and
  at a hostile one is refused; the map's hostile list respects both fog and relations.

---

### P3-03 · Gameplay writes relations 🔗
**Docs:** §15.1 finding 14 · `features.md` §5.3 · §13.5 group 3
**Depends on:** P3-01 — **Label:** `feature`

- **Home:** `modes/space/systems/{ContractSystem,CommsSystem,FactionEconomySystem}.cpp`.
- **Systems:** the relation-writing triggers §5.3 already names these three for — contract
  complete/fail, successful diplomacy, trade/blockade drift. **§15 found none of the three contains
  any of it**, which is stronger than "blocked on the null pointer": there is no dormant trigger
  waiting behind it.
- **Tests:** completing a contract raises the issuing faction's relation by the documented step;
  failing lowers it; a sustained blockade drifts a pair toward Hostile.

---

### P3-04 · The player's faction moves onto the player record 🔗
**Docs:** §12.30.3 (amending §12.30.1)
**Depends on:** P3-02 — **Label:** `fix`

- **Home:** `modes/space/systems/DiscoverySystem.cpp:13`, `ConstructionSystem.cpp:15`, and the player
  record.
- **Systems:** identity is read off the controlled rig root today, and §12.30.1 makes that root the
  **station** while docked — so both live readers invert the moment the docked router lands. Every
  screen's *"is this station mine?"* test depends on this.
- **Tests:** docking at a foreign station does not change the player's faction; discovery and
  construction attribute to the player, not the host.

---

### P3-05 · Fog of war migrates to knowledge networks 🔗
**Docs:** `features.md` §8.3 · §15.1 finding 12 · §13.5 group 3
**Depends on:** P3-01 — **Label:** `feature`

- **Home:** `modes/space/systems/DiscoverySystem.cpp`, `core/knowledge/KnowledgeNetwork.*`,
  `core/serialization/SaveFile.cpp`.
- **Systems:** `DiscoverySystem` writes `KnowledgeNetwork`, not `DiscoveryState` — §8.3 settled that
  *"knowledge networks win… `DiscoveryState` cannot implement this section at all"* and named the
  cost (a `DiscoverySystem` rewrite, and `SaveFile` dropping its `DiscoveryState` section).
- **Tests:** coverage is per-viewer; two allies linked by comms share coverage; an undiscovered
  system reads as **present-but-unknown**, never absent.

---

# Phase 4 — The docked screens

*`architecture.md` §12.30, **six tabs over one router** — §12.30 counts *seven screens* because
Storage and Market share the Trade tab. `features.md` §9 is explicit that what remains here is
**construction, not specification.***

> **This phase builds six of the seven.** Storage ships here as a warehouse (P4-03); **Market — the
> seventh screen — ships in P6-08**, once `Pricing.h`, `ItemId` and `ctx.diplomacy` exist to price a
> trade. A phase cannot deliver a screen whose only content is a number no system can yet compute.

**Exit criteria:** docking opens a tab strip whose tabs are the station's living facilities;
each tab is a working screen, with the Trade tab showing its Storage half only; both flight overlays
open anywhere; every request a screen builds lands on the docked requester and is consumed by its
system on the following tick.

---

### P4-01 · The router 🔗
**Docs:** §12.24 step 5 · §12.30 · §12.30.1 · §12.30.3 · §14.2
**Depends on:** P0-02, P0-15, P1-05 — **Label:** `feature`

- **Home:** `modes/space/ui/BridgeView.{h,cpp}`, `core/registries/BlueprintJson.cpp`,
  `data/base_game/modules.json`.
- **Types:** `ScreenId`; **`PlayerLocation` becomes authoritative here** — declared and first
  written in P1-01, this is where the router writes it per-hardpoint and `PlayerControlled` becomes
  derived from it rather than written directly; `AvailableTabs` returns **`{ ScreenId screen; entt::entity hardpoint; }`**
  (deduped, first living hardpoint of each kind) instead of a bare `FacilityKind` — the current
  signature discards the entity, so the router cannot set `PlayerLocation` from the tab list it
  draws. `hardpoint` may be `entt::null` for the Storage tab, whose readout then measures the
  station's aggregate integrity.
- **Systems:** the `FacilityKind::Storage` → **`Trade`** enum swap and its six sites (two are silent
  defaults). **`ParseFacilityStats`'s `kind` becomes `Require`, not `OptionalEnum`** — a
  `"facility": {}` block silently becomes the enum's default today, which is finding W's class
  applied to a facility's *identity*. `grade` is parsed and forwarded as `FacilityRef::grade`.
- **Content:** **the standalone blocker §14.2 names** — `modules.json` authors exactly one facility,
  of an unstated kind. Author `Trade`, `Repair`, `Engineering`, `Manufacturing`, `Research` facility
  modules and put them on `aegis_outpost`. Every facility-gated system stays unexercisable until
  this lands, independent of every code fix.
- **Tests:** a facility module authoring no `kind` **fails to load**, naming the file and key;
  selecting a tab sets `PlayerLocation` to that specific hardpoint; a facility module authoring no
  grade defaults nothing silently.

---

### P4-02 · Screen 1 — the Bay 🔗
**Docs:** §12.30.2 · `features.md` §4.1
**Depends on:** P4-01 — **Label:** `feature`

- **Home:** `modes/space/ui/` (new screen file).
- **Systems:** a **bay-scoped roster** — `view<Docked>` filtered on `docked.bay == thisBay`, no new
  component. Verbs: board, launch, park, ask-to-purchase, ask-to-escort, crew-it. Capacity is
  enforced in the search, not at the handoff.
- **Deliberately partial on first ship:** *ask to purchase* needs §12.19's `ItemId` (P6-03);
  *ask to escort* needs §12.27 (P8-02) and a **sticky** refusal roll weighted on
  `ctx.diplomacy`/`ctx.reputation`; *crew it* needs `ModuleKind::Crew` (P2-06); **parked hulls** need
  `RigState` (P10-01). Ship the roster, board and launch; gate the rest behind their dependencies
  rather than faking them.
- **Tests:** a vessel in another bay of the same station is not listed; boarding moves
  `PlayerLocation`; launching into a full bay is refused in the search.

---

### P4-03 · Screen 2 — Storage 🔗
**Docs:** §12.30.3 (Storage half) · §13.3 O
**Depends on:** P0-10, P4-01 — **Label:** `feature`

- **Home:** `modes/space/ui/` (split from `StationServicesMenu`),
  `modes/space/systems/StationServicesSystem.cpp` (two new verbs).
- **Systems:** two holds side by side; the verb is the direction of transfer. Deposit/Withdraw only —
  **the Market half ships in P6-08**, because a price needs `Pricing.h`, `ItemId` and `ctx.diplomacy`.
  A warehouse that does not deal is a complete screen and exercises the row model, the capacity check
  and the transfer path before a price exists.
- **Tests:** a transfer that would exceed either limit is refused and names which; the station's hold
  and the vessel's hold both round-trip.

---

### P4-04 · Screen 3 — Repair 🔗
**Docs:** §12.30.4
**Depends on:** P0-11, P2-10, P4-01 — **Label:** `feature`

- **Home:** `modes/space/ui/` (split from `StationServicesMenu`), `StationServicesSystem`,
  `modes/space/systems/NpcAiSystem.cpp`.
- **Types:** `RepairOrder { entt::entity subject; entt::entity hardpoint; float targetFraction;
  float creditRemainder; }` replaces `RepairRequest`, in `shared/components/StationServices.h`.
  **It is an order, not a request** — §12.30.4 flags this as a deliberate exception to the
  consumed-same-tick idiom every other request follows, and says to name and comment it as such, or
  the next contributor "fixes" it back and silently makes repair instant again.
- **Systems:** a **subject selector** — *"the only two sites that raise a `Health.current` in `src/`
  both require `Docked` on the healed rig, and a station is never `Docked`,"* so every fixed asset in
  the galaxy currently decays monotonically. Continuous billing (the version with no refund logic).
  Price derives from what the hardpoint is made of.
- **NPC repair lands here, both halves** (§12.30.4 "Who pays when the repaired rig is not the
  player's"), closing the regression P0-11 opens:
  - **The payer branch** — `Wallet` on a rig that has one, **`ctx.economy` otherwise**, billed to the
    faction's stock. This gives `FactionEconomy::Spend`'s all-or-nothing contract its first Tier 1
    consumer, and it is what lets §6.1's Material Security facet and §5.1's Three Pillars read
    combat attrition as a cost. §12.30.4's table: step 6 is needed *"only for the NPC payer"*, and
    `ctx.economy` is already non-null, so this blocks on nothing in Phase 3.
  - **The AI producer, which no task owned before** — `DockRequest` has **zero producers anywhere in
    `src/`**, so the payer branch alone would never execute: nothing makes a damaged NPC dock.
    **P2-10's `Flee` state is the retreat; this gives it a destination** — a fleeing NPC picks a
    friendly bay, emits `DockRequest`, and then a `RepairOrder`. Do not re-implement disengagement
    here. **The order component is identical whether a screen or the AI places it** — that is the
    point of it being an order, and it is why this is not a second code path.
- **Deliberately partial:** destroyed rows point at **Rebuild**, which is P4-05.
- **Tests:** repairing a station works — the assertion that fails today for every station in the
  galaxy; billing stops when the player stops; the price tracks the hardpoint's composition; a
  repair spanning hundreds of ticks charges a total, not zero (the rounding trap); an empty `Wallet`
  stops the repair with the hull it paid for; **a damaged NPC retreats, docks and repairs against
  `ctx.economy`, is refused when its faction cannot afford it, and with `ctx.economy == nullptr`
  does not repair and does not repair for free.**

---

### P4-05 · Screen 4 — Engineering 🔗
**Docs:** §12.30.5 · `features.md` §2.4
**Depends on:** P0-05, P0-06, P4-01 — **Label:** `feature`

- **Home:** `modes/space/ui/` (merging `EngineerMenu` + `RefactorMenu` behind their one shared gate),
  `modes/space/systems/{EngineerSystem,RefactorSystem}.cpp`.
- **Systems:** four verbs on two axes — **Merge · Deconstruct · Delete · Rebuild**. **Rebuild is the
  delete inverted** and needs no snapshot: the blueprint still holds the `MountBlueprint`. Duplicate
  facilities are not fungible — `PlayerLocation` naming the hardpoint is what makes every graded
  bench addressable, replacing `DockedEngineeringLevel`'s "first in `Rig::children` order."
- **Deliberately partial:** **Merge is reachable-and-wrong until §12.21's `Quality` exists** —
  `MergeField` is `p + s × (level × 0.1)`, additive on raw stats with no ceiling, while §2.4's
  formula moves a normalised quality against `bandMax`. Ship Delete, Rebuild and Deconstruct; gate
  Merge on P6-02.
- **Tests:** rebuild restores a deleted hardpoint from the blueprint; the bench you selected is the
  bench that is used; a merge at a Grade-3 bench differs from one at a Grade-1 bench.

---

### P4-06 · Screen 6 — Research, and its five missing producers 🔗
**Docs:** §12.30.6 · §12.1
**Depends on:** P0-12, P3-01, P4-01 — **Label:** `feature`

- **Home:** `modes/space/ui/` (new), `modes/space/systems/ResearchSystem.cpp`.
- **Types:** `StartResearchRequest` — **there is no such thing anywhere in `src/` today.**
  `ResearchJob` gains a `MountId facility`.
- **Systems:** **five links, each built and tested, forming a mechanism with no entry point.**
  `ResearchJob`, `StationFacility` and `NetworkOwner` have zero producers; `ctx.knowledge` was null
  (P3-01). Already-known items are refused **before** the click, not after. No credit fee — that is a
  decision. Duration derives, never authored per item. `FacilityStats::capacity` becomes the
  concurrent-slot limit (its second reader, same meaning).
- **Tests:** a sample researched at a living lab grants exactly one unlock into the actor's network;
  a known item cannot be queued; concurrent jobs cap at capacity.

---

### P4-07 · Screen 5 — Manufacturing, Draft half 🔗
**Docs:** §12.30.8 · §12.9
**Depends on:** P4-01, P3-01 — **Label:** `feature`

- **Home:** `modes/space/ui/` (absorbing `CustomizeMenu`), `core/registries/ContentLibrary.cpp`,
  `modes/space/systems/ConstructionSystem.cpp`.
- **Systems:** **Draft alone** — one retained field (`ConsumeSaveTemplateRequests` discards a
  blueprint body the intent already carries, so **a saved Template can never be built**), one overlay
  rename (`RegisterCraftedModule` → `RegisterDraftedTemplate`), and one **knowledge gate on
  `ConstructionSystem`**, which has none at all today. Also move
  `CustomizeMenu::ConsumeSaveTemplateRequests` out of `modes/space/ui/` — it is the only place a UI
  file mutates `core/` state, a system wearing a menu's filename (Law 9 inverted).
- **Deliberately partial:** **the Queue half needs §12.19 in full plus `ManufacturingSystem`**
  (P6-03, P6-06). Shipping Draft alone turns a built, tested, unreachable feature into a working one.
- **Tests:** a saved Template retains its body and can be built; a draft naming an unknown module is
  refused; validation errors name the failed rule.

---

### P4-08 · The two flight overlays 🔗
**Docs:** §12.30.7 · `features.md` §3.10, §2.7
**Depends on:** P0-02, P0-06, P0-08 — **Label:** `feature`

- **Home:** `modes/space/ui/` (`StorageMenu` → **inventory** overlay, `ModulesMenu` → **loadout**
  overlay). These leave the docked-screen set entirely: they are available *everywhere*, not behind a
  facility gate.
- **Systems:** live refit, unrestricted, per §2.7 — **sanctioned combat play**, which is why P0-05's
  `MountTraverse` and P0-08's `RecomputeRigTotals` are hard prerequisites rather than polish.
  **Jettison** lands here, and it gives `LootDrop`/`MaterialDrop` their **first producer**
  (§13.3 T) — without it a full hold has no exit but selling at a Trade station, while unmount, scrap
  and deconstruct all fill it.
- **Tests:** a refit mid-flight changes handling on the next tick; a jettisoned stack appears as a
  pickup; a full hold refuses a refund and says why.

---

### P4-09 · M3 verification pass 🔗
**Depends on:** P4-01 … P4-08 — **Label:** `chore`

Play the meso loop: dock → repair → engineer → research → draft → store → refit → launch. File what
it finds.

---

*Five tasks added 2026-08-23, from a UI design pass over the router and four of the six shipped
screens. All follow-ons to already-merged work, not revisions of it — none blocks P4-09.*

### P4-10 · Router — gate a tab on its screen shipping, not just its hardpoint living 🔗
**Docs:** §12.30 "A tab needs a working screen behind it, not just a living hardpoint"
**Depends on:** P4-01 — **Label:** `fix`

- **Home:** `modes/space/ui/BridgeView.{h,cpp}`.
- **Types:** a small `ScreenId → bool` shipped-table, compile-time, no new component.
- **Systems:** `AvailableTabs` gates each candidate on the shipped-table entry in addition to the
  existing living-hardpoint check. Starts `{Bay, Storage, Repair, Engineering, Research}` true,
  `{Market, Manufacturing}` false; flips one entry per screen as it ships (P6-08 for Market;
  Manufacturing's Queue half between P4-07 and P6-03/P6-06).
- **Tests:** a station with a living `Trade` hardpoint shows no Market tab before the entry flips;
  flipping it is the only change its landing task needs to make the tab reachable.

---

### P4-11 · Storage — sibling-hold selection 🔗
**Docs:** §12.30.3 "Sibling holds — a chosen destination, not an auto-routed one"
**Depends on:** P4-03 — **Label:** `feature`

- **Home:** `modes/space/ui/` (Storage screen), `shared/components/Loot.h`, `shared/rig/CargoView.{h,cpp}`.
- **Types:** `ItemStack` gains a bay/hardpoint identity field.
- **Systems:** a sibling selector over every living `CargoHold` hardpoint (§12.30.2's `TabStrip`
  pattern, generalised), each pill showing that hold's own integrity; `cargo_view::Deposit` gains a
  destination-choosing overload alongside its existing auto-routed one, which stays the default for
  non-screen callers.
- **Tests:** depositing with a hold selected always lands there, not the emptiest one; destroying one
  hold loses only that hold's stacks, verified against a row that names which hold it came from.

---

### P4-12 · Engineering — editing the station's own rig 🔗
**Docs:** §12.30.5 "Editing the station's own rig, when it is yours"
**Depends on:** P4-05 — **Label:** `feature`

- **Home:** `modes/space/ui/` (Engineering screen).
- **Systems:** the same dual-subject shape §12.30.4's Repair screen already ships — a second section,
  present only when the docked station's `FactionRef` matches the player's, editing the station's own
  `RigBlueprint` through the same Delete/Rebuild (and Merge/Deconstruct once their gates pass) verbs.
  No new mechanism — `DockedStation` is already resolved by this screen; this reuses it as a second
  subject the way `OwnedVesselAt` already is the first.
- **Tests:** the station section is absent when the docked station is not the player's; Delete/Rebuild
  on the station's own hardpoints behaves identically to the vessel's.

---

### P4-13 · Research — the Codex 🔗
**Docs:** §12.30.6 "The Codex — browsing what is already unlocked"
**Depends on:** P4-06 — **Label:** `feature`

- **Home:** `modes/space/ui/` (new screen or overlay, reached by a button on the Research screen).
- **Systems:** a read-only browse of the player's `NetworkOwner` unlocks, three sections by item kind
  (Modules, Shells, Materials), each row resolved against `ContentLibrary` for its display fields and
  tagged by faction and grade/tier; faction/grade filter chips and search over the combined set. No
  new component, no new request — a pure read.
- **Deliberately not a tech tree.** `features.md` §9's tech-tree shape is still 📋 unspecified; this
  stays a flat filterable list, not a graph.
- **Tests:** an item present in `NetworkOwner` but not `ContentLibrary` is impossible by construction
  and asserted as such; the filter chips narrow the combined set without touching the underlying data.

---

### P4-14 · NPC logistics — spreading stock across holds ❓ **deferred, not re-filed**
**Docs:** §12.30.3 "Sibling holds," the NPC-parity paragraph
**Depends on:** a real caller of `SpendRequest`/`DepositRequest` (the `FactionDecisionSystem` #31
slot) that manipulates physical `CargoHold`/`ItemStack` state, not just P4-11 — **Label:** n/a, not a
build issue

Filed as #223 (design issue, per this task's own instruction above) and audited 2026-08-25: the
premise does not hold yet, so the section it points at was corrected rather than specified. `core/
economy/FactionEconomy` is one scalar `int` per faction with no per-good/per-station/per-hold
subdivision; `FactionEconomySystem::Tick` has zero producers; `NpcAiSystem` never touches the ledger
or any `CargoHold`; and `core/ai/FactionDecisionEngine` (#31, the actual spender) is Tier 3 and
structurally cannot see registry state — a `CargoHold` included — by design. There is no physical
location for the faction's stock to be "in a hold" in the first place, so nothing per-hold is
buildable. **Do not re-file this as a build issue until both prerequisites above exist** — full
finding in §12.30.3's NPC-parity paragraph. #223 closed with this finding rather than a
`features.md` section.

---

# Phase 5 — Legibility

*The game becomes explicable without reading `features.md`.*

**Exit criteria:** a player can read hull condition, shield state, power, contacts, comms and the
galaxy from the screen alone.

---

### P5-01 · The status display 🔗
**Docs:** `features.md` §3.9 · §13.5 group 2e
**Depends on:** P2-01, P2-03 — **Label:** `feature`

- **Home:** `modes/space/ui/CockpitHud.cpp` and a new status-projection file.
- **Systems:** colour-is-condition / glyph-is-identity; outline-encloses-coverage with **dash-density
  charge**; fit-based LOD driven by `StructuralAttachment`. Needs §3.1's shield coverage modes
  (P2-03) to have anything but Personal to draw. The palette is shared with projectiles and shield
  shimmer — one definition, three readers.
- **Tests:** a destroyed hardpoint never renders as healthy; coverage outline matches the shield's
  actual mode; the projection degrades legibly at small sizes.

---

### P5-02 · The flight HUD 🔗
**Docs:** `features.md` §3.10 · §13.5 group 2e · §8.3
**Depends on:** P5-01 — **Label:** `feature`

- **Home:** `modes/space/ui/CockpitHud.cpp`, `modes/space/ui/` (new HUD surfaces).
- **Systems:** **a HUD surface exists exactly when a living module provides it** — sensor, comms,
  crew-with-command, construction, hyperdrive — with **fixed slots that disable rather than
  disappear**, since a control vanishing mid-fight is §8.3's *"absence must never look like
  emptiness"* in miniature. **There is no radar and no `RadarSystem`**: sensor contacts are one data
  source rendered as screen-edge indicators in combat and as the navigation map out of it.
- **Tests:** destroying the sensor module disables the contact indicators without moving any other
  control; no HUD element changes position when a module dies.

---

### P5-03 · The comms surface 🔗
**Docs:** §13.3 S · §12.27 (`commsRange`)
**Depends on:** P5-02 — **Label:** `feature`

- **Home:** `modes/space/ui/`, `modes/space/systems/CommsSystem.cpp`.
- **Systems:** **`CommsLog` is write-only** — the hail feature is complete from request to formatted
  response string and the response is unreadable. Draw the 8-entry log; give `HailRequest` a real
  producer (a HUD hail action). `CommsSystem`'s range check moves from `SensorRange` to `commsRange`.
- **Tests:** hailing a station in comms range produces a readable response; out of range is refused
  with a message, not silence.

---

### P5-04 · Multi-scale territory navigation 🔗
**Docs:** `architecture.md` §12.35 · `features.md` §8.1, §8.2 · §15.1 findings 4, 19, 20
**Depends on:** P0-03, P3-05 — **Label:** `feature`

- **Home:** `modes/space/ui/NavigationMap.cpp`, `modes/space/render/IconRenderer.cpp`,
  `core/diplomacy/Territory.cpp`.
- **Systems:** every Level-1 sub-scale renders **territory aggregates**, not individual system
  markers — `features.md` §8.1 forbids individual objects there by name, and today's code fakes the
  zoom difference with layout spacing instead of changing what it draws. This gives
  `core::diplomacy::Territory` its **first consumer anywhere in `src/`**. A pure aggregation function
  (systems → regional clusters → galactic territory), not a system.
  `IconRenderer::DrawMapMarker` **gains a camera/zoom parameter** — a real signature change, since
  the current one has nowhere to put a cull or a `BodyKind` dispatch — and markers become cached
  per-kind shapes rather than one immediate-mode circle.
- **Deliberately partial:** click-to-warp needs `WarpSystem`'s range gate (P9-06); hover info needs
  P3-01/P3-05, both now merged.
- **Tests:** a cluster of undiscovered systems renders present-but-unknown; a blob recomposes when a
  member's owner changes; a target outside hyperdrive range renders unavailable.

---

### P5-05 · Icon culling and substitution 🔗
**Docs:** `features.md` §8.2 · §9.1
**Depends on:** P5-04 — **Label:** `feature`

- **Home:** `modes/space/render/IconRenderer.cpp`.
- **Systems:** camera-AABB culling and **icon substitution when a body shrinks below a few pixels** —
  §9.1 makes both a requirement, not an optimisation, and `BodyKind` (P0-01) exists partly to answer
  which icon.
- **Tests:** an off-screen body is not drawn; a body under the pixel threshold draws its kind's icon
  at a fixed size.

---

### P5-06 · Act I — the prologue 🔗
**Docs:** **`features.md` §1.2 "Act I — The Anomaly"** (spec) · `lore.md` §1 · §13.1 (`TutorialSystem` row)
**Depends on:** P0-01, P1-05, P4-08, P5-03 — **Label:** `feature`

- **Home:** `modes/space/systems/TutorialSystem.cpp`, `modes/space/factories/WorldGen.cpp` (the one
  hand-authored system), `modes/space/ui/` (the lesson surface), `modes/main_menu/`.
- **Types:** `Tutorial` gains an **act**; `TutorialStep`'s ported enum is **re-scoped to Act I's four
  lessons** — Move, Target, Fire, Equip. The legacy values (`DestroyAsteroid`, `CollectMaterial`,
  `Sell`, `Warp`) move to Act II's list in P6-13.
- **Systems:**
  - A **fixed authored system** — the only hand-placed one in the game — holding the player, a fleet
    of same-faction NPCs in formation, a fleet-commander rig, and an **anomaly** (`BodyKind::Anomaly`,
    P0-01) with a trigger radius.
  - The commander **hails on arrival** (P5-03's comms surface) and offers instruction. Declining
    changes nothing else.
  - Crossing the anomaly's trigger radius fires **the cataclysm**: the fleet is lost and the player is
    force-warped to a seeded random system. **This is the only scripted event in the game** — say so
    in the code comment, so the second one is a decision rather than a precedent.
  - The main menu's *New Game* offers **Play the prologue** / **Skip to the frontier**.
- **Tests:** each lesson advances on its real trigger and can be abandoned; declining the offer still
  permits reaching the anomaly; the cataclysm fires exactly once; **running the prologue twice in one
  process leaves one fleet and one anomaly** (§12.29's re-entrancy rule); skipping starts at Act II's
  arrival with identical state.

> **The tutorial is the prologue, not an overlay of hint boxes.** `lore.md` line 4 already made this
> canon and it never became mechanics; `features.md` §1.2 is now the spec.

---

### P5-07 · Signature, detection, and Passive Area ECM 🔗
**Docs:** `features.md` §8.3 "The signature / detection model" and "Sensor ghosts and ECM — two
roles, passive area and directional"
**Depends on:** P0-09, P3-05 — **Label:** `feature`

*Directional ECM (click-and-hold, single-target) is P2-12, not this task — it needs P2-07's
Tractor-Beam-shaped contest mechanic and belongs beside it. This task owns the base detection math,
the `KnowledgeNetwork` fabricated/degraded-entry capability both roles read, and Passive Area, the
simpler of the two.*

- **Home:** `modes/space/systems/{TargetingSystem,DiscoverySystem}.cpp`,
  `core/knowledge/KnowledgeNetwork.*`, `data/base_game/modules.json` (one `ModuleKind::ECM` def,
  Passive Area role).
- **Types:** no new stored component for signature — it is derived every read, never authored, per
  this document's derive-never-store default. `KnowledgeNetwork` gains the ability to hold a
  fabricated or confidence-degraded entry alongside a real one (a flag, not a second entry kind) —
  P2-12's Directional role reuses this same capability.
- **Systems:**
  - **Signature** is computed from a rig's mass, current power draw (`PowerSource`/`PowerLoad`), and
    its `§2.10` material composition — unchanged from the 2026-08-09 model. **Sensor strength** is
    computed from a rig's Optical + Semiconductive element contributions (§2.10). `DiscoverySystem`
    compares the two across distance to decide whether a contact is detected, replacing today's flat
    hardcoded `SensorRange == 2000` check.
  - **Passive Area ECM**: always active while the hardpoint is mounted and powered, no player input.
    Every tick, reduces effective sensor strength for every hostile rig within its radius — range
    suppression only, weaker than Directional's full effect list (P2-12), the tradeoff for costing no
    player attention and no dedicated key.
  - **No auto-lock anywhere in this task.** Degraded readings only ever change what
    `NavigationMap`/edge indicators render — never what `TargetingSystem` offers as a lock candidate
    (§3.2).
- **Tests:** a heavy, high-power rig is detected at greater range than a light, power-idle rig with
  identical sensor strength on the observer; toggling a power category to Offline (§2.9) measurably
  lowers a rig's signature; a hostile rig inside a Passive Area ECM radius reads reduced sensor
  strength, and one outside it does not; destroying the ECM hardpoint ends the effect immediately.

---

# Phase 6 — Items and the economy

*`architecture.md` §12.19 is the load-bearing section, and its own scheduling note calls it large.
It touches every holder of a `ModuleId` in the tree, and it is verifiable headless.*

**Exit criteria:** mine an element → manufacture a material → manufacture a module with attributes
derived from what it was made of → sell it at a price nobody authored.

---

### P6-01 · `Grade` in the taxonomy (§12.19a) ⚡
**Docs:** §12.19 "The types" and "Scheduling" · §13.3 K
**Depends on:** — **Label:** `feature`

- **Home:** `shared/blueprints/Taxonomy.h`, `core/registries/BlueprintJson.cpp`,
  `shared/rig/ModuleAttachment.cpp`.
- **Types:** `Grade` — the seven-tier ladder, with `ToString`/`FromString`, and **`FromString` must
  reject rather than default** (Law 10). `FacilityRef::level` → `::grade`. **Delete
  `FacilityStats::level` and `StationFacility::researchTier`** — three tier systems collapse to one.
- **Tests:** an unknown grade string fails to load naming the file and key; a facility's authored
  grade reaches `FacilityRef` (today `AttachModuleComponents` passes the kind alone, so **every merge
  in the game has preserved exactly 10% of the secondary**).

---

### P6-02 · Stat pools and rolled quality (§12.21) 🔗
**Docs:** `architecture.md` §12.21 · `features.md` §2.7
**Depends on:** P6-01 — **Label:** `feature`

- **Home:** `shared/blueprints/` (pools are authored content, Law 10), `core/registries/`.
- **Types:** `StatRef` (kind-scoped), `PoolEntry { StatRef stat; float weight; Direction dir; }` —
  **all three required**, `StatPool` (per-`ModuleKind` defaults, per-`ModuleDef` overridable),
  `Quality` (budget point + chosen distribution, stored per instance).
- **Systems:** **`Direction` is not optional** — `fireIntervalSeconds`, `spreadRadians` and
  `rechargeDelaySeconds` improve by *decreasing*, and a naive band multiplier makes a Mythic module
  strictly worse than a Common one while passing every test that only checks "the number changed."
  **Weight is price per unit of improvement** (`improvement = share / weight`), the single balance
  dial for the whole loot system, and it lives in JSON.
- **Validation:** **CI rejects a pool entry naming an enum or integer field.** `damageType`,
  `Shield::absorbs`, `FacilityKind` and every int including `projectilesPerShot` are identity, not
  quality — rolling 1 → 3 turns a cannon into a shotgun.
- **Tests:** a roll's total value is invariant across which stats it picks; a ↓-direction entry
  improves; per-def overrides beat kind defaults; the same `(item id, tick)` rolls identically on
  every platform.

---

### P6-03 · The item model (§12.19c) 🔗
**Docs:** `architecture.md` §12.19 · `features.md` §2.10
**Depends on:** P0-13, P6-01, P6-02 — **Label:** `feature`

- **Home:** `shared/blueprints/Item.h`, `ElementDef.h`, `MaterialDef.h`; `core/registries/` parsers;
  `data/base_game/{elements.json,materials.json}`.
- **Types:** `ItemKind { Element, Material, Module, Shell, Vessel }`;
  `ItemId { ItemKind kind; std::string id; }` (the kind-agnostic envelope — it does **not** replace
  `ModuleId`/`ShellId`); `ItemRef`, `ItemInstance`, `ItemStack`, `Attributes`; `ElementDef` (id, name,
  **periodic abbreviation**, authored density, eight attributes 0–3, **no price, no rarity band**);
  `MaterialDef` (per-**role** weighting, recipes **generated**, never authored per grade); `Recipe`
  (`n(G)` role slots — **never a list of element ids**; a recipe is a demand, not a bill of
  materials).
- **Content:** the **41-element roster** and **eleven Material families** `features.md` §2.10
  specifies. Mass, attributes and base value all **derive** — nothing is authored per item.
- **Validation:** `Validation` rule 13 (recipe-derived mass within a band of `k·density·radius²`) and
  rule 14 (role coverage — every recipe slot names a role some element scores ≥ 1 in, or *"a player
  can never be hard-stuck"* stops being an enforced property).
- **Tests:** derived mass/attributes/value identical on every platform; a silver Conductive Coil and
  an aluminium one of the same def and grade differ in attributes and mass and share a `recipeValue`;
  a ↓-direction attribute improves with a higher score; doubling every input doubles mass and leaves
  the attribute vector unchanged.

---

### P6-04 · The widening (§12.19d) 🔗
**Docs:** §12.19 "Widened, not new" / "Deleted"
**Depends on:** P6-03 — **Label:** `chore`

- **Home:** every holder of a `ModuleId`, plus `core/serialization/`.
- **Types:** `CargoHold` → one `std::vector<ItemStack>`; `MountedModules::ids` → `::items` of
  `ItemInstance`; `MountBlueprint::shell`/`::modules` → `ItemRef`; `LootDrop`/`MaterialDrop`/
  `DeathWreck` → `ItemStack`; new `ShellInstance` on every hardpoint.
  **Deleted:** `ContentLibrary::RegisterCraftedModule`, `craftedModules_`,
  `SystemContext::craftedModules`, `CargoHold::modules`/`::materials`, `MaterialStack`.
- **Systems:** stacking is **by indistinguishability** — two units stack iff every field of their
  `ItemInstance` is equal, so a rolled module never stacks with another roll of the same def. The
  stack key is not the price key.
- **Tests:** `ItemStack` round-trips a save for all five kinds; a rig carrying a merged module
  restores **without any content-library entry existing for it** (the regression test for the
  crafted-module overlay's retirement).

---

### P6-05 · `core/economy/Pricing.h` 🔗
**Docs:** §12.19 "The corrected signatures" · §13.5 group 2c · `features.md` §2.10
**Depends on:** P6-03 — **Label:** `feature`

- **Home:** `core/economy/Pricing.h` — free functions beside `FactionEconomy`, `sr_core`, no raylib.
- **Types:** `BaseValue(const ItemInstance&, const ContentLibrary&)`;
  `LocalPrice(const ItemInstance&, int quantity, const CargoHold& stock, const ContentLibrary&)`;
  `RepairCostPerHp(const ItemInstance& shell, Grade facilityGrade, const ContentLibrary&)` — the last
  reads §2.10's **Inert** attribute and is its only named reader.
- **Systems:** pure, stored nowhere, ticked never; called by the screen to display and by the system
  to charge (Law 9). `LocalPrice` prices a **quantity** by walking the curve, not by multiplying a
  spot price. **There is no pricing logic anywhere today** — `BuildBuyRequest(module, cost)` takes
  cost as a parameter, so the price is invented by whoever calls it.
- **Tests:** `LocalPrice` moves as the `(ItemId, Grade)` pool draws down and is unaffected by how
  that pool splits across stacks; **buying ten and selling nine back cannot net a profit at any
  quality distribution** — the reroll exploit, asserted directly.

---

### P6-06 · `ManufacturingSystem` 🔗
**Docs:** `architecture.md` §12.18 · `features.md` §2.8
**Depends on:** P6-03, P6-05 — **Label:** `feature`

- **Home:** new `modes/space/systems/ManufacturingSystem.{h,cpp}` + `core/galaxy/ManufacturingRecord`.
  *(Not `ConstructionSystem` — that produces entities and carries the codebase's one narrow
  `check_layers.py` exemption. Not `EngineerSystem` — it does not share the gate.)*
- **Types:** `ManufacturingJob { ItemRef item; MountId facility; float progress; int remaining;
  ItemStack pendingOutput; }` on the station's facility component, a separate queue from research.
- **Systems:** advances against `dt` and `FacilityStats::ratePerSecond`; concurrent slots are
  `FacilityStats::capacity` (**this retroactively gives `ResearchSystem` the slot limit it lacks**).
  Duration is **derived, never authored**: base 5s Material / 10s Module, doubling per grade, facility
  grade dividing by up to ~3.3×. Output lands in `CargoHold` with **quality rolled fresh in its
  grade's band**. Inputs consume **per unit, never per job**. Gate: a living Manufacturing facility
  **plus** a `ctx.knowledge` check no existing system performs.
- **Persistence:** `ManufacturingRecord`, exactly parallel to the built `ResearchRecord` — demotion
  writes it, promotion re-instantiates with elapsed time banked.
- **Tests:** refused without a living facility; refused when the design is absent from the actor's
  network; refused when inputs are unaffordable **before mutating anything**; a demote→promote cycle
  resumes at caught-up progress; two units of one item roll independent qualities.

> **This is what closes the research loop.** `ResearchSystem` grants an unlock into a network today
> and nothing consumes it. It is also where §2.10's attribute propagation is *computed* — the whole
> material-attribute model is invisible in play until this exists.

---

### P6-07 · Manufacturing, Queue half 🔗
**Docs:** §12.30.8
**Depends on:** P4-07, P6-06 — **Label:** `feature`

- **Home:** the Manufacturing screen's second section.
- **Systems:** the queue names its own bench (`MountId`); an unlock is keyed on `(ItemId, Grade)`;
  manufacturing at someone else's factory is free (for §12.30.6's reason); the output lands on the
  **vessel**, not the station. **A Template Chip item is burned by a Queue job** — this is
  `TemplateMarketSystem`'s real producer path, not `CustomizeMenu`.
- **Tests:** a job on a destroyed bench halts; the chip is consumed exactly once; output reaches the
  requesting vessel's hold.

---

### P6-08 · Market — the trading half 🔗
**Docs:** §12.30.3 (Market half) · §12.30 dependency table
**Depends on:** P3-02, P4-03, P6-05 — **Label:** `feature`

- **Home:** the Market/Storage screen, `StationServicesSystem`.
- **Systems:** **`BuyItemRequest`/`SellItemRequest` lose their caller-supplied `cost`/`value`** —
  filling them in is not the fix; a menu that names its own price is a client-authoritative wallet
  write (Law 9). The request states *what and how many*; the system calls the same pure `Pricing.h`
  function the screen called to display. Reputation modifies price per §5.3; the relation band gates
  access.
- **Tests:** the displayed price equals the charged price; a hostile-band station refuses; buying
  drains the station's stock and moves the next unit's price.

---

### P6-09 · Per-item faction stock 🔗
**Docs:** `architecture.md` §12.20 · `features.md` §5.0 · §15.1 finding 11
**Depends on:** P6-03 — **Label:** `feature`

- **Home:** `core/economy/FactionEconomy` (existing), `sr_core`.
- **Types:** widens from one scalar per faction to `(FactionId, StationId) → ItemId, quantity`.
- **Systems:** the current `Deposit`/`Spend` on a bare `int` would give **wrong answers about
  blockades and Material Security even fully wired**, since it has no notion of *what* is held or
  *where*.
- **Tests:** deposit/withdraw round-trip per system; faction totals equal the sum of ledgers;
  `SpendResult` gains a reader (today a failed faction spend is indistinguishable from a successful
  one).

---

### P6-10 · Vessel assembly takes time 🔗
**Docs:** §12.18's ⚠️ · `features.md` §2.8
**Depends on:** P6-06 — **Label:** `feature`

- **Home:** `modes/space/systems/ConstructionSystem.cpp`.
- **Systems:** a **vessel's build time is the sum of its parts' build times × an assembly factor** —
  mass, base price and build time all derive from the recipe by one rule. Vessel assembly is
  instantaneous today; making a capital under construction a raidable window is a feature, but it is
  a behaviour change to a built system.
- **Tests:** a capital's build occupies its facility for the derived duration; destroying the yard
  mid-build halts it.

---

### P6-11 · The gathering beam 🔗
**Docs:** **`features.md` §2.10 "Gathering — one verb, three sites"** (spec) · §13.5 group 2c ·
§15.1 finding 13 · `features.md` §7.2
**Depends on:** P0-01, P0-09, P6-03 — **Label:** `feature`

- **Home:** `shared/blueprints/Taxonomy.h`, `shared/rig/ModuleAttachment.cpp`,
  `modes/space/systems/MiningSystem.cpp`, new `core/galaxy/` depletion record,
  `modes/space/factories/WorldGen.cpp`, `data/base_game/modules.json`.
- **Types:** `ModuleKind::Gathering` with `extractRate` (**Sum**), `extractRange` (**Max**) and
  `yieldBonus` (**Max**) — the §12.23 aggregation rule applies. A planet **`type`** flag, so a gas
  giant is distinguishable before skimming can target one.
- **Systems:** a **held beam** that extracts directly into the hold, replacing "shoot the rock until
  it dies" as the primary path. **Sources deplete rather than die** — the composition drains
  progressively, no `Destroyed` tag and no drop entities in the common case — backed by a depletion
  record shaped like `WreckRecord` so a drained belt **stays** drained across a demote/promote cycle
  (§7.2: *a system that regenerates mutable state from a seed will silently undo player actions*).
  **Weapons stop being mining tools** — a source can still be shot apart, and destroying it destroys
  what was in it: no kill-path drop table, no debris salvage. **This deletes `MiningSystem`'s current
  `Destroyed` path and the tests covering it**, which is the intended change, not collateral.
  One verb serves three sites — mining (asteroids), skimming (gas giants), harvesting (biospheres,
  ruins, wrecks).
- **Tests:** extraction rate scales with mounted beams and reach does not; a drained rock is still
  drained after a warp round-trip; **destroying a source yields nothing at all**; skimming targets
  only gas giants; a beam with no power (§2.9 Offline) extracts nothing.

> Also the mechanism P6-13's tutorial tithe splits at — which is why the tithe is a rate at the point
> of extraction rather than an audit of the player's hold.

---

### P6-13 · Act II — the frontier tutorial and the tithe 🔗
**Docs:** **`features.md` §1.2 "Act II — The Frontier"** (spec) · `lore.md` §1, §4
**Depends on:** P5-06, P6-08, P6-11 — **Label:** `feature`

- **Home:** `modes/space/systems/TutorialSystem.cpp`, `modes/space/systems/MiningSystem.cpp` (the
  split), `modes/space/ui/`.
- **Types:** `Tutorial` gains `titheFraction`; Act II's lesson list — Gather, Dock and trade,
  Customize, Build, Warp.
- **Systems:**
  - A **local station hails** shortly after arrival and offers instruction **at 50% of every Element
    gathered while the tutorial is active**. The rate is stated in the offer — accepting is informed
    or it is a trick.
  - **The tithe is a split at the point of extraction**, not a deduction: while active, the beam
    routes half its yield to the instructing station's hold. Nothing is confiscated later, and walking
    away mid-lesson keeps everything already banked. The player gathers twice as long for what they
    keep.
  - **A lesson whose feature does not exist yet is skipped, never blocked** — this is what lets the
    task ship with Gather and Trade and grow to five as P8-01 (Build) and P9-06 (Warp) land.
  - **The faction registry beat closes Act II:** the station uploads the public registry, the player
    reads all ten profiles and picks a provisional alignment or wipes the transponder to stay
    unaligned.
- **Tests:** the tithe splits exactly 50/50 at extraction and stops the instant the tutorial ends;
  declining costs nothing and the station still trades; a lesson for an unbuilt feature is skipped
  silently; the alignment choice reaches the player record.

> ⚠️ **`lore.md` §4 needs rewriting with this task** — its amnesia framing contradicts a player who
> flew the prologue. `features.md` §1.2 records the resolution: the faction-registry beat survives,
> the amnesia does not.

### P6-12 · Build `tools/element_check` — the four content checks 🔗
**Docs:** §13.5 group 2c · §12.19 "Validation and CI" · `features.md` §2.10
**Depends on:** P6-03 — **Label:** `chore`

> ⚠️ **`tools/element_check/` does not exist.** `tools/` holds `ci/` and `economy_sim/` only, so this
> task **writes the checker**; **T-02 wires it into CI as blocking.** The two were one task until the
> absent directory was noticed — keep them split, because a checker is useful the day it runs by hand
> and must be provably able to fail (below) before it is allowed to gate anyone's PR.

- **Home:** `tools/element_check/` (**new**, beside `tools/economy_sim`), `CMakeLists.txt`.
- **Systems:** four checks — pairwise dominance, Pareto validity, role coverage, density spread —
  over `data/base_game/elements.json`, run **standalone and advisory**; the CI wiring beside the four
  existing structural checks is **T-02**, not this task. **The first time §2.2–2.4's enforcement
  principle reaches content rather than code.** `tools/economy_sim` (built 2026-08-11) graduates from
  modelling the curve's shape parametrically to pricing the actual authored set.
- **Tests:** run against a fixture, the checker **fails** on a planted dominated element and on a
  recipe naming an uncovered role, and **passes** on a clean set — *a check that has never failed is
  not known to work* (P11-01's own criterion, and it depends on this).

---

# Phase 7 — Knowledge, research and templates

**Exit criteria:** salvage → deconstruct → research → unlock → draft a Template → pitch it → watch a
faction build your design and pay you for it. This is `features.md` §1's macro loop, minimum viable.

---

### P7-01 · Knowledge networks reach gameplay 🔗
**Docs:** `architecture.md` §12.1 · `features.md` §2.5
**Depends on:** P3-01, P4-06 — **Label:** `feature`

- **Home:** `core/knowledge/KnowledgeNetwork.*` (built), its producers and readers.
- **Systems:** grant/copy/destroy paths reachable in play; `NetworkOwner` producers (P1-05);
  destroying a commander's network releases rather than deletes. **Network raiding costs no new
  mechanic** — capturing a commander's vessel converts their crew, network included (P2-07). A
  faction's general network has no anchor and is permanently un-raidable.
- **Tests:** grant/copy/destroy round-trips; a save→load→save cycle preserves contents; capturing a
  commander transfers their network.

---

### P7-02 · Reverse engineering, end to end 🔗
**Docs:** `features.md` §2.4 · §12.30.5, §12.30.6 · §12.16 item 21
**Depends on:** P4-05, P4-06, P6-03 — **Label:** `feature`

- **Home:** `EngineerSystem` (deconstruction is a second intent, **not a new file**),
  `ResearchSystem`, the Research screen.
- **Systems:** deconstruction **reads the recipe backwards** (`deconstructsTo` is superseded); yield
  scales with facility grade (Common 20–45% → Mythic 80–100%); the **sample-survival roll is the
  failure mechanic** and **research cannot fail** — do not add a second roll. Researching shells as
  well as modules (§12.16 item 21). An unlock carries a **grade**, which is what decides whether the
  rarity ladder means anything.
- **Tests:** deconstruction never yields more mass than the instance carried (the conservation clamp)
  and never exceeds 100%; a destroyed sample grants nothing; an unlock at Grade 3 does not unlock
  Grade 4.

---

### P7-03 · Template creation 🔗
**Docs:** `architecture.md` §12.9 · `features.md` §2.2, §2.3
**Depends on:** P4-07, P7-01 — **Label:** `feature`

- **Home:** the Manufacturing screen's Draft section.
- **Systems:** a Template **is** a `ShipBlueprint`/`RigBlueprint` — no new type. Draft emits a
  `SaveTemplateRequest` intent carrying the body; the network entry is the saved artifact.
  §2.3's template validation rules gate the save.
- **Tests:** an invalid draft (any of `Validation.h`'s rules) cannot be saved and **reports which
  rule failed**; a valid draft appears in the actor's network and can be manufactured.

---

### P7-04 · Template negotiation, royalties and the market 🔗
**Docs:** `architecture.md` §12.7 · `features.md` §2.6 · §15.1 finding 10
**Depends on:** P3-01, P6-07, P7-03 — **Label:** `feature`

- **Home:** `modes/space/systems/TemplateMarketSystem.cpp`, the Trade facility's sell popup.
- **Systems:** the producer is **selling a Template Chip item at a Trade facility** (corrected
  2026-08-11) — `PitchTemplateIntent` has zero producers today. The negotiation is a three-step roll;
  **the seller proposes a per-unit rate and the buyer accepts or refuses against a disposition-rolled
  0–50% ceiling.** `PassesAccept` needs a real `archetypeFits` computation — it is a pre-set boolean
  nothing ever sets true, a second gap independent of the null pointer. **Royalties do not survive
  the seller's death** — no inheritance mechanic exists or is planned.
- **Tests:** a sold Template survives destruction of the buyer's stations; selling the same Template
  twice behaves per §2.6; a refused pitch costs the chip or does not, per spec; royalties stop at
  death.

---

### P7-05 · Factions manufacture what you sold them 🔗
**Docs:** `features.md` §1 (macro loop), §2.6 · §12.18
**Depends on:** P6-06, P7-04, P9-01 — **Label:** `feature`

- **Home:** `core/ai/FactionDecisionEngine`, `ManufacturingSystem`'s Tier 2/3 path.
- **Systems:** a faction that bought a Template actually builds it, at scale, in systems the player
  can visit and see. **This is the payoff the entire macro loop is written for**, and it is the one
  place the coarse tick (P9-01) is a hard prerequisite rather than a nicety.
- **Tests:** a purchased Template appears in the buyer's production over N macro ticks; royalties
  accrue per unit built.

---

# Phase 8 — Command

**Exit criteria:** select units, issue orders, run a party, build in the field, delegate to a
sub-commander, and operate from a bridge — all without the simulation pausing.

---

### P8-01 · Construction gating and build mode 🔗
**Docs:** `architecture.md` §12.26
**Depends on:** P1-02, P1-07 — **Label:** `feature`

- **Home:** `shared/blueprints/Taxonomy.h` (new enumerator), `shared/rig/ModuleAttachment.cpp`,
  `modes/space/systems/ConstructionSystem.cpp`, `modes/space/ui/BuildMenu.cpp`.
- **Types:** `FacilityKind::Construction` — **not a docked tab**; a command surface opened by `B` and
  issued at a unit.
- **Systems:** `ConstructionSystem` gains the two checks it does not have — the executing rig must
  carry a **living Construction hardpoint**, and a range gate. `BuildMenu` gains a blueprint list and
  the placement-mode hookup (today its `Draw` renders one hardcoded-cost "BUILD" label and never
  calls the request functions the file itself exports). `Esc` cancels placement (P1-07's ladder,
  rung 1).
- **Content:** a constructor bay module on `shell_facility_bay`.
- **Tests:** a build request from a rig with no living Construction hardpoint is refused and **costs
  nothing**; placement out of range is refused; `Esc` cancels cleanly.

---

### P8-02 · The local command system 🔗
**Docs:** `architecture.md` §12.27 (full spec) · `features.md` §4.0, §4.3, §3.6
**Depends on:** P0-07, P0-08, P8-01 — **Label:** `feature`

- **Home:** `shared/components/` (POD), new `modes/space/systems/OrderSystem.{h,cpp}`.
- **Types:** `OrderKind { Move, Attack, Defend, Build }`; `Stance { Hostile, Defensive, Peaceful }`;
  `UnitOrder`, `OrderQueue`, `UnitStance`. **Attack needs no new targeting type** — `Target { rig,
  hardpoint }` already models attack-the-chassis vs. attack-a-hardpoint. Add `IssueOrderIntent` to
  the intent variant (orders are what an *actor* did, so `core::Intent`, not a request component).
- **Systems:** `OrderSystem` reads `OrderQueue`, writes `ThrustInput`/`Target`/**party membership** —
  *a Defend order aimed at a friendly **is** a party membership*, which is the producer
  `PartyLeader`/`PartyMember` have never had. Schedule **immediately after `NpcAiSystem`**.
  Arbitration in one rule: *player order wins; `Retreat` interrupts; an empty queue means AI.*
  Order availability is **derived, never authored** (Move needs `Propulsion`; Build needs a
  Construction hardpoint; a mixed selection offers the intersection). Selection is presentation state
  on the singleton entity, never a component on the selected rigs.
- **Gating:** on the rig the player occupies — a living `Comms` hardpoint **and** a living `Crew`
  module with a non-zero `command` roll. Symmetric for NPCs, so **destroying a hostile's comms
  hardpoint degrades their fleet coordination** at no extra code cost.
- **Tests:** the full list at §12.27 — a Defend order creates the codebase's first live party;
  `Retreat` interrupts and the queue resumes; a Peaceful unit under fire keeps moving and does not
  return fire; the player's own rig never appears in the commandable set.

---

### P8-03 · The comms module and the sensor datalink 🔗
**Docs:** §12.27 "Gating" · `features.md` §8.3
**Depends on:** P0-09, P3-05, P8-02 — **Label:** `feature`

- **Home:** `shared/blueprints/Taxonomy.h`, `modes/space/systems/CommsSystem.cpp`,
  `modes/space/systems/DiscoverySystem.cpp`, `data/base_game/modules.json`.
- **Systems:** one `commsRange` stat with **three consumers** — command authority, hailing, and
  **linked allies sharing sensor coverage**. Killing one comms hardpoint blinds a formation
  mid-engagement *and* desynchronises it, from the same check.
- **Tests:** an ally outside comms range contributes no coverage; destroying comms drops both effects
  in the same tick.

---

### P8-04 · AI sub-commanders 🔗
**Docs:** `architecture.md` §12.2 · `features.md` §4.5
**Depends on:** P2-06, P8-02 — **Label:** `feature`

- **Home:** `shared/components/Commander.h`, `modes/space/systems/CommanderSystem.cpp`.
- **Types:** `Commander { NetworkId network; CommanderOrders orders; FactionId faction; }` — **it
  belongs on the bridge hardpoint** (§12.16 item 22), and it has zero producers today.
  `CommanderOrders { Dispatch, Retreat, Defend }` is a **stance enum, not an order list** — rename or
  fold it into `UnitStance`; do not add tactical verbs to it.
- **Systems:** recruitment is **any `Crew` module with a non-zero `command` roll, mounted to a Bridge
  via the existing equip flow** — no separate acquisition track and no "mark autonomous" toggle
  (autonomy is emergent from not being the player's current shell). Competence is the module's rolled
  stats, not a personality system. **Loyalty moves only via crew bribery**, never a spontaneous
  defection roll. `Commander::orders` finally gets a reader in `OrderSystem`.
- **Tests:** a commander's death releases its network reference without destroying the network;
  orders reach the commanded units; a bribed crew module changes hands.

---

### P8-05 · The bridge — two scales, one interface 🔗
**Docs:** `features.md` §4.0–§4.4 · `architecture.md` §12.16
**Depends on:** P8-02, P8-04 — **Label:** `feature`

- **Home:** `modes/space/ui/BridgeView.*` and the command surfaces.
- **Systems:** **operating and commanding are simultaneous** (§4.0) — left-click keeps firing
  throughout; the aim point is surrendered only for the instants of a right-click. The unit of
  command is a **party** (§4.3). **No pause, and it constrains the UI** (§4.4). Boarding, the bay and
  the ship you arrived in (§4.1) — already covered mechanically by P4-02.
- **Tests:** issuing an order does not interrupt firing; the same surface addresses one unit and a
  party without a mode switch.

---

### P8-06 · The contract board, distress calls and escorts 🔗
**Docs:** **`features.md` §5.11 "Contracts"** (spec) · §5.3 · §15.1 finding 14 · §12.30.2
**Depends on:** P3-03, P4-01, P6-05, P6-08, P8-02 — **Label:** `feature`

- **Home:** the Market screen's **Contracts section** (a contract is a transaction, so it sits beside
  buying and selling rather than taking its own tab), `modes/space/systems/ContractSystem.cpp`,
  `modes/space/systems/DistressSystem.cpp`, the Bay screen's escort row.
- **Types:** an **offer** representation — `Contract.h`'s own comment names its absence: *"there is no
  separate offer representation yet."* No new `ContractType`; **Courier stays deferred** until the item
  model gives it a cargo and a destination.
- **Systems:**
  - Offers are **per station**, generated on a timer and expiring on one; **reputation gates
    visibility** per §5.3's band table, and below Friendly the board is empty **and says so** (§8.3:
    absence must never look like emptiness). One active contract at a time is already locked in code;
    the board greys the rest with the reason.
  - **Rewards derive, they are not authored** — Bounty from the target hull's `BaseValue` × kills ×
    an issuer-to-target relation risk multiplier; Escort from the escorted hull's `BaseValue` ×
    duration × the system's hostile density. Both fall out of `Pricing.h` and the matrix, so neither
    needs a tuning table and both stay correct as content grows.
  - **Completion writes relations** — the §5.3 writer §15 found missing entirely: positive to the
    issuer, and a Bounty additionally negative to the target, which is how taking work picks a side.
  - Distress calls surface on the HUD and map with an answer action; the Bay's *ask to escort* row
    becomes a legitimate `PartySystem` producer. **Do not invent a producer inside a system** — that
    is how `CustomizeMenu::ConsumeSaveTemplateRequests` became a Law 9 violation.
- **Tests:** an offer expires and leaves the board; accepting below the reputation band is refused;
  a completed bounty pays the derived amount and moves both relations; a second acceptance is refused
  while one is active; an escort whose target dies fails immediately.

---

# Phase 9 — The living galaxy

**Exit criteria:** leave a system and come back to find it changed; factions expand, fight and
collapse without the player present; warping costs fuel and has a range.

---

### P9-01 · The coarse tick, and `FactionDecisionEngine` gets a caller 🔗
**Docs:** `features.md` §1.1 · §13.3 L, M · §13.4 decision 3
**Depends on:** P0-03, P0-14 — **Label:** `feature`

- **Home:** `modes/space/systems/SystemSchedule.cpp` (or a new coarse driver), `core/ai/FactionDecisionEngine`.
- **Systems:** Tier 2 (~5 Hz, systems within 2 warp jumps, hard cap **8**) and Tier 3 (one macro tick
  ~30 s, **no registry** — `core/galaxy/` records only). Reinstate the three `TickCoarse` functions
  P0-14 deleted, now with a driver. `FactionDecisionEngine` — a tested library with **no invocation
  path anywhere** — gets its macro tick. **The macro tick iterates existing `SystemRecord`s, never
  the coordinate space.**
- **Tests:** a Tier 3 system banks elapsed ticks and fast-forwards deterministically on arrival —
  never by replaying skipped frames; the same elapsed time produces the same state on every platform;
  eight Tier 2 neighbours is a hard cap.

---

### P9-02 · The four operational facets, and border skirmishes 🔗
**Docs:** `features.md` §6.2, §6.4 · §15.1 findings 6, 7
**Depends on:** P9-01 — **Label:** `feature`

- **Home:** `core/ai/FactionDecisionEngine.cpp`.
- **Systems:** **§6.2's entire archetype-weighting table has no code at all** — three of
  `ComputeFacets`' four facets are permanently `50.0f`, so no faction ever branches on its archetype.
  **§6.4's border-skirmish mechanic has no function to call**, not merely no caller: `RaidDispatchChance`
  exists and matches its threshold, and there is nothing named `Border`/`Skirmish` anywhere in `src/`
  for the section's second tuned constant.
- **Tests:** two archetypes given identical inputs produce different facet weights; two rivals sharing
  a border skirmish at the documented rate.

---

### P9-03 · Faction survival and elimination 🔗
**Docs:** `architecture.md` §12.3 (✅ built) · `features.md` §5.1
**Depends on:** P8-04, P9-01 — **Label:** `feature`

- **Home:** `core/ai/FactionDecisionEngine` (existing entry point).
- **Systems:** the Three Pillars — command structure, leadership, economic footprint — evaluated on
  the macro tick. The logic is built and tested; it has never run.
  - **`hasLeadership` is answered by counting living commanders** (P2-11) — the caller supplies it,
    exactly as `EvaluateSurvival`'s header already anticipates (*"the faction head or an AI
    sub-commander still alive"*). This is the pillar that makes decapitation matter.
  - **Recruitment closes the loop:** a faction below its commander count that holds a bridge, a
    `Crew` module with a `command` roll and the stock to fit it **recruits a replacement** on the
    macro tick, through §4.5's ordinary equip path and no separate acquisition track. So killing a
    faction's commanders cripples it and stops it expanding, and **finishes** only a faction whose
    economy is already gone — §5.1's three-pillar logic working as written.
- **Tests:** each pillar independently sufficient; all three lost ⇒ collapse; collapse scatters ships
  per spec; a decapitated faction with a healthy economy recruits and resumes acting; a decapitated
  faction with no stock does not.

---

### P9-04 · Seeding drives world generation 🔗
**Docs:** `architecture.md` §12.4 · `features.md` §7.1, §7.2
**Depends on:** P0-03 — **Label:** `feature`

- **Home:** `core/galaxy/Seeding.*` (built, unusable), `modes/space/factories/WorldGen.cpp`,
  `SpaceFlight::WarpToSystem`.
- **Systems:** the hierarchical position-based cascade finally has coordinates to derive from
  (P0-03). **Deletes the `std::hash` cross-platform desync** in `WarpToSystem` and P1-01's inherited
  placeholder. §7.2's boundary: mutable state (depletion, ownership, wrecks) lives in records, never
  regenerated from the seed.
- **Tests:** same seed ⇒ same output, asserted against **hardcoded expected values**, not against a
  second run; a visited system's mutable changes survive a round trip.

---

### P9-05 · Territory, adjacency and expansion 🔗
**Docs:** **`features.md` §6.6 "Expansion and Colonization"** (spec) · §5.0, §5.1 · §13.2
(`Territory` row) · §15.1 finding 25
**Depends on:** P2-11, P5-04, P9-01, P9-02 — **Label:** `feature`

- **Home:** `core/diplomacy/Territory.cpp`, `core/ai/FactionDecisionEngine.cpp`,
  `modes/space/systems/CommanderSystem.cpp`.
- **This is how the galaxy repopulates**, and it is the Tier 3 half of P2-11 — **the same
  `Commander` component, a wider scope, not a second implementation.** A cleared system refills
  because a faction's commander decides to expand into it, builds there, and garrisons it. **A
  faction with no living commander does not expand, does not rebuild, and does not reclaim** — which
  is what gives P2-11's decapitation its strategic weight and what stops the galaxy repopulating
  itself out of nowhere.
- **Systems:** territory gains writers as well as readers — claim, contest and lose on the macro
  tick, with adjacency from `Topology`. **`EvaluateColonization` gains a candidate selector**, which
  is the half it has never had (the caller supplies `candidateSystemId` today and nothing calls it)
  — **and the caller is a commander**, which is why the parameter was left open:
  **only systems adjacent to territory already held**, chosen among them by the faction's archetype —
  yield for Corporate/Industrial, a border position for Military/Zealot, an anomaly for Anomalous, a
  ruin for Scientific, a garden world for Ecological. **The Reapers never colonize.** 20% per macro
  tick at threshold, deliberately slower than §6.3's 45% raid chance; surplus must exceed existing
  consumption, and success spends it.
- **Tests:** a faction with no adjacent unclaimed system never expands; two archetypes given the same
  candidate set pick differently; a conquered system changes owner and the map's blob recomposes;
  expansion and rearmament cannot both be funded from one surplus; **a faction with no living
  commander never expands, however much stock it holds**; a cleared system adjacent to a healthy
  faction's territory is repopulated within N macro ticks.

---

### P9-06 · Warp: hyperdrive, fuel, charge and range 🔗
**Docs:** §13.1 (`WarpSystem` row) · §12.23 (`Hyperdrive`) · §12.35 · `features.md` §2.11
**Depends on:** P0-09, P6-03, P9-04 — **Label:** `feature`

- **Home:** `modes/space/systems/WarpSystem.cpp`, `modes/space/ui/NavigationMap.cpp`.
- **Systems:** `WarpSystem` has **no fuel, no module, no charge time** today, and `WarpRequest`/
  `SystemWarpRequest` have no producers. Gate on a living `Hyperdrive` hardpoint; consume fuel (a
  Material, per §2's supply chain); charge over time, interruptibly; range from `jumpRange` (Max
  aggregation, P0-08). Click-to-warp on the nav map reads this gate and does not reimplement it.
- **Tests:** warping without a hyperdrive is refused; without fuel is refused; the charge is
  interrupted by destruction of the drive; an out-of-range target cannot be confirmed.

---

### P9-07 · Promotion and demotion round-trip 🔗
**Docs:** `features.md` §1.1, §7.3 · §12.18, §12.5, §12.30.6
**Depends on:** P6-06, P9-01, P10-01 — **Label:** `feature`

- **Home:** `SpaceFlight::WarpToSystem`, `core/galaxy/*Record`.
- **Systems:** every record type is actually invoked at the boundary —
  `WreckRecord`, `ResearchRecord` (`CollapseResearchJobs`/`PromoteResearchJobs` are **built, correct
  and have no callers**), `ManufacturingRecord`, the depletion record, and `RigState` for parked
  hulls. Today `WarpToSystem` carries `CargoHold`, `Wallet` and `wreckLedger_` and **destroys
  everything else**, including a research job in the departing system.
- **Tests:** a research job survives a warp round-trip at caught-up progress; a parked hull is still
  parked; a demoted system's stations still exist on return.

---

### P9-08 · The player as a faction 🔗
**Docs:** `features.md` §5.10
**Depends on:** P6-09, P9-03, P9-05 — **Label:** `feature`

- **Home:** `core/diplomacy/`, `core/economy/`, the faction registry.
- **Systems:** the player's faction is a real row in the matrix, the ledger and the territory model —
  claimable systems, a treasury, relations others react to, and the Three Pillars applying to the
  player too.
- **Tests:** the player's faction can be attacked, can hold territory, and can collapse by the same
  rules as any other.

---

### P9-09 · Boss encounters — a verification task 🔗
**Docs:** `features.md` §6.5 · §14.6
**Depends on:** P8-04, P9-03 — **Label:** `chore`

Not new code. §6.5 designs bosses as **emergent commanded fleets** — a consequence of §2.5, §2.7,
§4.5 and §5.1 rather than a system. Verify that a high-competence sub-commander with a good Template
and a healthy faction economy actually produces one, and file what is missing. If it does not emerge,
the gap is in one of the four, not in a missing boss system.

---

### P9-10 · Warp-in and no-bay arrival cinematic 🔗
**Docs:** `architecture.md` §12.36 (Scope) · `features.md` §2.11
**Depends on:** P9-06 — **Label:** `feature`

§12.36 settled *where* a player appears (nearest friendly `DockingBay`'s exit point, or a
`SpawnAnchor` ring search with no bay) but explicitly did not build *how it looks* — named there as
"an explicit, unowned gap" rather than left implicit. Today `WarpToSystem` swaps `SystemWorld`s
instantaneously and `SpawnSystem`'s placement is a silent teleport; there is no animation anywhere
in `render/` for either a system-to-system warp-in or a no-bay respawn.

- **Home:** `modes/space/render/`, `modes/space/SpaceFlight.cpp` (`Draw()`), `modes/space/systems/WarpSystem.cpp`.
- **Systems:** a brief, skippable arrival effect (e.g. an expanding ring / flash / velocity-streak
  fade) plays at the resolved placement point on `OnEnter()`, a death respawn, and a system warp —
  the same visual regardless of which of §12.36's two placement tiers resolved it, so a docking-bay
  exit and a no-bay fallback both read as an arrival rather than the ship simply existing on the
  next frame.
- **Tests:** an arrival plays at the resolved spawn/respawn/warp position exactly once per arrival
  and does not block input past its own duration; the ship is not targetable/collidable until it
  completes (no free hit on a still-materializing ship).

---

# Phase 10 — Persistence and the shell

**Exit criteria:** save, quit, load, and be where you were — with the same hull, the same damage,
the same refit, and the same galaxy.

---

### P10-01 · `RigState` — the live-rig snapshot 🔗
**Docs:** `architecture.md` §12.31 · §13.3 AC · §12.30.2
**Depends on:** P0-06, P6-02, P6-04 — **Label:** `feature`

- **Home:** `shared/rig/` + `core/serialization/` (§12.31's layer split).
- **Types:** **`RigState` is a delta against a `BlueprintId`, not a copy of the rig, and it must not
  be a `ShipBlueprint`** — §12.21's per-instance `Quality` cannot survive one, and adding health to
  `MountBlueprint` would have `ManufacturingSystem` building pre-damaged ships. Carries
  `ShellInstance` and `MountedModules::items` per mount.
- **Systems:** **four features wait on this one capability** — parked hulls, warp damage persistence,
  cross-system refit, and the world save. Today `WarpToSystem` re-spawns the player from
  `BlueprintRef`, so **every jump is a free complete repair and rolls back every refit**, and anything
  else parked in the system is destroyed.
- **Tests:** a damaged, refitted rig round-trips through a warp with its damage and its loadout; a
  restored rig re-derives its stat block from `def + attributes + quality`; a merged module restores
  with no content-library entry.

---

### P10-02 · The world save 🔗
**Docs:** **`features.md` §3.3 "What a save contains"** (spec) · §13.3 Y
**Depends on:** P10-01 — **Label:** `feature`

- **Home:** `core/serialization/{SaveFile,SaveMigrator}.cpp`.
- **Systems:** `SaveFile`'s entire API is four blueprint/knowledge functions today. Implement the ten
  sections §3.3 now enumerates — header/seed/elapsed ticks, player, `RigState`, holds, knowledge,
  touched `SystemRecord`s, diplomacy, economy, the four `core/galaxy/` records, parked hulls — and
  bump the migrator.
  **The structural rule that keeps this small: a save is a demotion, not a registry serializer.**
  The resident system is written as a record and re-promoted on load, so loading is warping in and
  there is no second code path. **Nothing derived is saved** — no stat blocks, no aggregate mass, no
  prices; a save that stores derived state disagrees with a content edit forever.
- **Tests:** a full round-trip restores the galaxy the player left; **nothing re-derivable from the
  seed appears in the file** (assert on section membership, not just on the round-trip); an older
  schema migrates; the autosave never fires on the death tick.

---

### P10-03 · Save, Load and Settings in the system menu 🔗
**Docs:** §12.29 "What the menu contains"
**Depends on:** P10-02 — **Label:** `feature`

- **Home:** `modes/space/ui/SystemMenu.cpp`.
- **Systems:** the three entries P1-07 deliberately omitted, now that each can actually work. The
  §3.4 constraint stands: **nothing with in-fight value may be added to this menu** — if Settings
  gains a power-priority or keybinding surface, that surface moves to a non-pausing home.
- **Tests:** saving from the menu and loading from the main menu returns the same world; the quit
  confirmation stops claiming all progress is lost once saving exists.

---

### P10-04 · The main menu grows up 🔗
**Docs:** §14.3 · `MainMenu.h`'s own header comment
**Depends on:** P10-02, P11-04 — **Label:** `feature`

- **Home:** `modes/main_menu/`.
- **Systems:** the three-file stub deliberately keeps only *"a title screen the player can actually
  get past."* Add what now has a supporting system: a **save picker** (P10-02), the **Play the
  prologue / Skip to the frontier** choice (P5-06), and a ship showcase (needs a showcase renderer).
  The multiplayer connect flow stays 🧊.
  **No faction picker here** — `features.md` §1.2 puts alignment at the end of Act II, as a diegetic
  act inside the world. That is a *removal* from this task's original scope, and the reason is that
  ten faction profiles read better when the player has just learned the galaxy has ten of them.
- **Tests:** the save picker lists real saves and loads the selected one; skipping the prologue
  produces the same arrival state as flying it.

---

### P10-05 · Crafted content survives 🔗
**Docs:** §12.31 🐛 · §12.30.5
**Depends on:** P6-04, P10-01 — **Label:** `fix`

- **Home:** `core/registries/ContentLibrary.cpp`, `EngineerSystem`.
- **Systems:** `merged.id` is `primary + "+" + secondary + "@L" + level`, so ids **double in length
  per merge generation** — and that string is the key `craftedModules_` is stored under. Replace with
  a short generated id. §12.19's `ItemInstance` retires the overlay entirely, so this is mostly
  deletion; confirm a restored hull never references a `ModuleId` that no longer resolves.
- **Tests:** a five-generation merged module has a bounded id and restores from a save.

---

### P10-06 · M8 verification pass 🔗
**Depends on:** P10-01 … P10-05 — **Label:** `chore`

Save/load a long session at every phase boundary; confirm nothing silently resets.

---

# Phase 11 — Content breadth

*Authoring, not engineering — **with two exceptions, P11-08 and P11-09**, which are here because
audio and art are what turn a working simulation into something someone would play, and because
both are content-shaped even where they need code. Every other task here is gated on its schema
existing, not on other content. `data/base_game/` holds three files today; `features.md`
§2.10–§2.13 specify the full rosters.*

**These are content issues, and content needs an acceptance criterion the same way code does.** Each
task below carries a **Done when:** line in place of `Tests:` — a roster is finished when a checker
says so, not when it feels complete. Where a task's criterion is *"the CI check passes"*, that check
is built by P6-12/T-02 and the dependency is listed.

---

### P11-01 · The element roster 🔗
**Docs:** `features.md` §2.10 · §14.2 · `architecture.md` §12.19 "Validation and CI"
**Depends on:** P6-03, P6-12, T-02 — **Label:** `feature`

- **Home:** `data/base_game/elements.json` (exists today with a 4-entry starter roster — iron,
  carbon, silica, titanium; the 41-element roster below is not yet authored into it),
  `tools/element_check/`.
- **Content:** **41 elements** — the count §2.10 settles explicitly, not a target to approach — each
  with a real density, its **periodic abbreviation**, and **eight attributes scored 0–3**.
  **No rarity tiers, no authored price, no authored mass**: all three derive (§12.19's three-axis
  rule). Real chemistry supplies the densities; `"silica"` is not an element and does not appear.
- **Done when:** `tools/element_check` passes **blocking** in CI on all four checks — pairwise
  dominance, Pareto validity, role coverage, density spread — and `Validation` rule 14 passes
  against every authored recipe role. Add a planted dominated element to a fixture and confirm the
  checker **fails** on it; a check that has never failed is not known to work.

---

### P11-02 · The material families 🔗
**Docs:** `features.md` §2.10
**Depends on:** P11-01 — **Label:** `feature`

- **Home:** `data/base_game/materials.json` (does not exist today).
- **Content:** **eleven families**, weighted by attribute **role** and never by named element, with
  **no faction specificity** — exclusivity lives at the module/shell/vessel level. Recipes are
  **generated per grade, never authored**, so eleven families cover ~77 materials across the seven
  tiers.
- **Done when:** every family generates a valid recipe at **all seven grades** (§12.19's test list,
  asserted as a loop, not spot-checked); **no material def names an element id anywhere** — the
  recipe-is-a-demand rule, assertable by a grep in the checker; rule 14 role coverage still passes
  with the full element roster loaded.

---

### P11-03 · The module roster 🔗
**Docs:** `features.md` §2.11 (~1,100 lines of specified content) · `architecture.md` §12.21
**Depends on:** P6-02, P11-02 — **Label:** `feature`

- **Home:** `data/base_game/modules.json` (87 lines today), stat pools wherever P6-02 homed them.
- **Content:** the specified roster across **every `ModuleKind`** — including the six this roadmap
  adds (`Sensor`, `CargoBay`, `FireControl`, `Hyperdrive` from P0-09, `Crew` from P2-06, `Gathering`
  from P6-11) — with stat pools per kind and per-def overrides. **ECM and cloak are excluded**; they
  wait on the signature/detection model (Appendix B).
- **Done when:** **every `ModuleKind` enumerator has at least one authored module**, asserted by a
  test that iterates the enum rather than a hand-maintained list — this is P0-15's `kAllKinds` lesson
  applied to content, and the same class of defect; every pool entry passes P6-02's CI rule (no entry
  names an enum or integer field); the whole file loads with `FromString` rejecting nothing.

---

### P11-04 · The shell roster and the faction registry 🔗
**Docs:** `features.md` §2.12, §5.2 · §14.6 · `lore.md`
**Depends on:** P11-02 — **Label:** `feature`

- **Home:** `data/base_game/shells.json` (74 lines today), the faction keys in
  `data/base_game/ships.json`.
- **Content:** **2–5 variants per shell kind, per faction** — §2.12 gives every kind the per-faction
  treatment §2.11's modules deliberately skip, because a shell carries four numbers and a sprite
  rather than tuned stats. Each variant authors `acceptsKinds`, a draw layer, `hullRadius`, and
  optionally a baked collision polygon. The **ten canonical factions** of §5.2 become authored
  content (`ships.json` authors two of the ten keys today) — content-only per Law 10, `lore.md` as
  the source. **No shell grants a stat bonus**; faction identity is the hull/mass/radius balance and
  the sprite, never a hidden performance stat.
- **Done when:** all ten faction keys resolve from content with no hardcoded fallback; every shell
  kind has at least one variant per faction; validation rules 10–12 pass for every authored shell.

---

### P11-05 · The preset ship roster 🔗
**Docs:** `features.md` §2.13
**Depends on:** P11-03, P11-04 — **Label:** `feature`

- **Home:** `data/base_game/ships.json` (three vessels today).
- **Content:** **2–5 ships per faction, total — not per hull class.** Each faction fills §2.13's
  role slots, scaled by hardpoint count: **starter fighter 4–7**, **combatant 10–20**, **capital or
  station 20–40** (Facility-kind shells enter at the third). Most factions fill all three plus one
  signature wildcard. `aegis_vanguard` and `aegis_outpost` already exist and become two of Aegis's.
- **Done when:** every authored ship passes `Validation.h`'s **full** rule set including new rules
  12–14; every ship's hardpoint count lands inside its role slot's band, asserted per ship; **every
  faction has a starter fighter** — P10-04's showcase and the new-game path both index one, and a
  faction missing it fails at the point a player picks it, not at load.

---

### P11-06 · Nebulae, planet types and hazard content 🔗
**Docs:** `features.md` §3.8 · §13.5 group 2c · `architecture.md` §12.28
**Depends on:** P0-01, P6-11 — **Label:** `feature`

- **Home:** `modes/space/factories/WorldGen.cpp`, `modes/space/render/WorldRenderer.cpp`,
  `modes/space/systems/HazardSystem.cpp` (P0-01), `data/base_game/`.
- **Types:** a nebula as a **system-level hazard property**, not a gathering site and **not a
  `BodyKind` value** — a `Corona`-shaped volume `HazardSystem` already handles. An **`Inert`
  threshold** on the rig decides zero-damage vs. burning.
- **Systems:** a **semi-transparent haze pass placed *after* `DrawProjectiles`** — the case §12.28
  predicted by name when it ruled that a body wanting to draw *in front of* rigs is a new pass, not a
  new enumerator. Ordinary asteroids and gas giants sit inside a nebula; what is scarce is the
  ability to be there.
- **Done when / Tests:** a rig whose `Inert` clears the threshold takes zero damage inside and one
  below it burns; the haze draws over rigs and projectiles alike and over nothing in the UI layer;
  a gathering source inside a nebula is still extractable by a rig that can survive there.

---

### P11-08 · Audio — the game makes a sound 🔗
**Docs:** `architecture.md` §3 (`engine/`) · §2.3 (layer rules) · §6 (asset pipeline) · Law 12
**Depends on:** P0-01, P2-01, P2-09 — **Label:** `feature`

> **Added 2026-08-11.** There is **no audio anywhere in this project** — no system, no task, no
> spec. The only mentions across ~18,000 lines of design are *"audio banks 🧊"* in the deferred
> asset pipeline and one Settings row. M8 is labelled **Shippable**, and as scheduled it ships
> silent. This task makes that untrue at the cheapest point: raylib plays a sound from a path today,
> so a first pass needs **no asset pipeline at all**.

- **Home:** new `src/engine/audio/` (`sr_engine`) — **the only layer below `modes/` that may link
  raylib** (§2.3), and both `sr_menu` and `sr_space` already link it, so one device serves both
  modes. Plus `modes/space/systems/` for the one system that decides *when*, and two lines in
  `CMakeLists.txt`'s `sr_engine` list.
- ⚠️ **`engine/` may not include `core/`, `modes/`, `shared/`** (§2.3), so the device API is
  **primitive-typed** — an id and a volume, never a component and never a `SystemContext`. The
  system that knows an explosion happened lives in `modes/space/systems/`; the device that plays a
  sound knows nothing about explosions. Getting this backwards is what makes an audio layer
  impossible to test.
- **Systems:** an `AudioSystem` draining a per-tick cue queue, written by the systems that already
  know what happened — `DamageSystem` (hit, shield absorb, hardpoint destroyed), P2-09 (the kill),
  `WeaponSystem` (the shot), `DockingSystem`, the UI's click. **No event bus** — there is none in
  this codebase, Law 12 governs a mechanism that was never built, and introducing one for audio
  would be §2.4's dead abstraction plus the exact chain-reaction spaghetti Law 12 exists to forbid.
  Cues follow the **same request-component idiom every other producer uses**: written this tick,
  drained and cleared the next.
- **Content:** placeholder sounds, the same way `WorldRenderer` draws placeholder shapes. A
  distinguishable cue per damage type matters more than any of them being good, because §2.1's
  palette already teaches the player that Energy, Kinetic and Ion are different things.
- **Done when / Tests:** the cue queue drains fully every tick and never grows unbounded when no
  device is available; a headless `sr_tests` run **plays nothing and passes** — the audio device is
  never a test dependency; muting is a device-level state, not a per-cue branch.

---

### P11-09 · The asset pipeline, and the end of placeholder shapes 🔗
**Docs:** `architecture.md` §6 · Appendix C · `features.md` §3.5, §3.9
**Depends on:** P2-05, P11-04 — **Label:** `feature`

> **Added 2026-08-11.** Every vessel in the game is a flat-coloured triangle or disc, and stays one
> through all of this roadmap: P2-04 and P2-05 both say *"draw the tested shape until real art
> exists"* as a deliberate position. That is correct as sequencing and wrong as an ending — **§6's
> asset pipeline is the prerequisite nothing was allowed to demand.** These two tasks are now what
> demands it, which is precisely the trigger Appendix C names.

- **Home:** `src/engine/assets/` (existing), `modes/space/render/WorldRenderer.cpp`,
  `modes/space/render/LightingPass.cpp`.
- **Systems:** §6's pipeline to the extent the shipped game needs it and **no further** (§2.5 —
  deferral is explicit, and so is scope): sprite loading and an atlas, ids resolved through
  `ContentLibrary` rather than by path, and `ShellDef`'s `spriteLayer`/draw-layer fields (P2-05,
  P11-04) finally selecting something real. **Hot-reload, UUID asset ids and audio banks stay 🧊**
  until something demands them the way this task demands sprites.
- **Note:** `LightingPass` is already written for this. Its header records that the legacy per-pixel
  shader was deliberately not ported *"because there is no texture/shader asset pipeline"*, and that
  what exists today is legacy's own per-object tint fallback promoted to the only implementation.
  **When sprites land, that decision is worth revisiting on purpose** rather than inheriting the
  fallback by default.
- **Done when / Tests:** a shell renders its authored sprite at its authored draw layer, and a shell
  with no sprite still renders its placeholder shape — **absence must never look like emptiness**
  (§8.3), and a half-authored roster must not produce invisible ships.

---

### P11-07 · Documentation sweep ⚡
**Docs:** this file · `features.md` §9 · `architecture.md` §13.5, Law 4
**Depends on:** — **Label:** `docs`

- **Home:** `docs/features.md`, `docs/architecture.md`, `docs/plans/playable_roadmap.md`.
- **Content —** small, and worth doing once the above lands so the docs stop disagreeing with
  themselves:
  - ~~`features.md` §9's **"Capture"** entry still reads as open; `architecture.md` §13.5 records
    ownership transfer as **settled 2026-08-11 (boarding-in-place)**. One of the two is stale.~~
    **Fixed** — §9 now points to §3.2 and no longer contradicts it.
  - `features.md`'s Status Legend line *"Nothing in this document is ✅ yet"* needs re-reading after
    every phase gate here.
  - `architecture.md` §13.5's group list should gain pointers to this file's phase numbers so the two
    do not drift.
  - ~~Law 4's wording still describes a `Shell → Component → Module` model; `features.md` §2 settled
    **`Shell → Module`, two tiers**, and explicitly says nothing needs to be added to close it.~~
    **Stale bullet, drop it** — Law 4 already reads `Shell → Module` (corrected 2026-08-07, four days
    before this roadmap was compiled). §12.14 item 7 has the record of that fix.
  - `architecture.md` §12.6, §12.8 and §12.12 are cited by **no task in this file**. §12.8 is the
    constraints section every implementer should read and is now referenced from *Before implementing
    any task*; §12.6 and §12.12 look superseded by §12.35 and §12.30.x — mark them so, or cite them.
- **Done when:** no statement in `docs/` contradicts another on capture, on the shell tier model, or
  on which section owns the nav map.

---

# Tooling & QA — runs alongside every phase

*These are issues like any other and carry the same shape. T-01 and T-03 propose new tools; their
paths below are the obvious parallel to `tools/economy_sim` rather than a name any spec fixes.*

### T-01 · The headless combat harness ⚡
**Docs:** `features.md` §2.7 · `architecture.md` §12.24 step 2's note
**Depends on:** — **Label:** `chore`

- **Home:** `tools/combat_sim/` (new, beside `tools/economy_sim`), `CMakeLists.txt`.
- **Systems:** headless — **no window, no raylib, a bare `SystemContext`** — exactly the pattern
  `tools/economy_sim` (built 2026-08-11) demonstrates. It lets balance be **measured rather than
  argued**, and it is the reason P1-02's `PlayerInputSystem` must never poll raylib.
- **Done when / Tests:** a scripted duel between two authored ships runs to a result with no window
  and no display, and the same inputs produce the same result **on every platform**; the harness
  builds in CI on all three matrix legs.

### T-02 · CI gains the content checks 🔗
**Docs:** `architecture.md` §12.19 "Validation and CI" · §12.21
**Depends on:** P6-12 — **Label:** `chore`

- **Home:** `.github/workflows/build.yaml`, `tools/ci/`, `tools/element_check/`.
- **Systems:** `element_check` **blocking** rather than advisory, beside the four structural checks
  already in the *Structural enforcement* job; plus the attribute→stat map rule — reject a pool entry
  naming an enum or integer field. **One rule, two producers** (§12.19 and §12.21), so it is written
  once and called from both.
- **Done when / Tests:** CI **fails** on a planted dominated element, on a recipe naming an uncovered
  role, and on a pool entry naming `damageType` or `projectilesPerShot`; and passes on the clean tree.

### T-03 · A wiring regression check ⚡
**Docs:** `architecture.md` §0 · §2.4 · §13 (the audit this would have automated)
**Depends on:** — **Label:** `chore`

- **Home:** `tools/ci/check_wiring.py` (new, beside the four existing checks),
  `.github/workflows/build.yaml`.
- **Systems:** flag a `shared/components/` type with **no non-test writer** or **no non-test
  reader** — the two defect classes this whole roadmap exists to close, and between them most of
  §13's findings. **An explicit allowlist with a reason per entry**, since a component legitimately
  awaiting its producer is exactly what Phase 0 is full of; an unexplained entry is the thing the
  check exists to prevent.
- **Done when / Tests:** the check flags a planted write-only component; the allowlist is non-empty
  at merge time and every entry names the task that removes it; structural enforcement over
  documentation is §0's whole thesis, so this lands as CI, not as a doc.
- **Note:** **P0-16 is the narrow version of this**, shipped in Phase 0 rather than waiting for the
  tooling batch — it tests the schedule specifically, where this covers every component. Neither
  replaces the other: a system can be correctly scheduled and still read a component nothing writes.

### T-05 · The combat balance pass 🔗
**Docs:** `features.md` §9.1 · Appendix B item 3 · `architecture.md` §6.3's tuning note
**Depends on:** T-01, P2-10 — **Label:** `chore`

- **Home:** `tools/combat_sim/`, `data/base_game/`.
- **Systems:** none — this is a **tuning pass, not code.** T-01 builds the harness and **nothing
  consumes it**; Appendix B item 3 defers *economic* pacing to `economy_sim`'s derived curve and
  says nothing at all about damage, health, speed or time-to-kill. Those numbers currently come from
  whatever was typed first.
- **Done when:** time-to-kill for each of §2.13's three role slots sits in a stated band, measured
  by the harness rather than asserted; a fighter cannot beat a capital head-on and **can** win by
  manoeuvre against P2-10's state machine; the numbers live in JSON (Law 10) so the next pass edits
  content, not code.

### T-04 · Playtest gates ⚡
**Docs:** `architecture.md` §13 (its thesis) · this file's milestone ladder
**Depends on:** — **Label:** `chore`

- **Home:** the GitHub tracker, not the tree.
- **Content:** P1-08, P4-09 and P10-06 already exist. Add one verification task **per milestone**
  thereafter — M2, M5, M6, M7 — each a play-it-and-file-what-breaks chore like the three that exist.
- **Done when:** every milestone in the ladder has exactly one verification task filed against it.
  **§13 exists because nobody could play the game**; the counter-measure is playing it at every gate,
  not auditing harder.

---

# Appendix A — What is already built and just needs a caller

Worth knowing before writing anything new. Each of these is complete, tested, and invoked from
nowhere:

| Unit | What it needs | Task |
|---|---|---|
| `core/serialization/` (4 files) | A caller — and a world-save capability underneath it | P10-02 |
| `core/ai/FactionDecisionEngine` | A macro tick | P9-01 |
| `core/diplomacy/Territory` | Any consumer at all | P5-04 |
| `core/galaxy/Seeding` | Coordinates to derive from | P9-04 |
| `core/galaxy/WreckRecord` | Death and recovery to be reachable | P1-06 |
| `CollapseResearchJobs` / `PromoteResearchJobs` | To be called at the warp boundary | P9-07 |
| `PartySystem` | A producer of `PartyLeader`/`PartyMember` | P8-02 |
| `CommanderSystem` | A producer of `Commander` and a reader of `orders` | **P2-11**, P8-02, P8-04 |
| `LootSystem`'s drop path | Anything at all to create a drop — `LootDrop` and `DerelictWreck` have **zero producers** | P2-09 |
| `EvaluateSurvival`'s `hasLeadership` / `EvaluateColonization`'s candidate | The commander the signatures were left open for | P2-11, P9-03, P9-05 |
| `KnowledgeNetwork` | A non-null `ctx.knowledge` and reachable producers | P3-01, P4-06 |
| Nine menu files | Input, selection state, and a router | P4-01 |
| `SpawnSystem` | `SpawnAnchor` and `RespawnPending` producers | P1-05, P1-06 |
| `FiringArc::currentOffset`, `PowerShed`, `SpendResult` | Readers | P0-05, P6-09 |

---

# Appendix B — Open questions that block work

These need a decision before their dependents can be built. Everything else in both documents is
settled.

| # | Question | Blocks | Where |
|:---:|---|---|---|
| **1** | **The signature / detection model — mostly specified 2026-08-24.** Sensors carry only a range, hardcoded to 2000; the base model (signature from mass/power/materials, sensor strength from Optical+Semiconductive) plus ECM/sensor-ghosts and directional jamming are now spec'd in `features.md` §8.3, tasked as P5-07 and P2-12. **Cloak/stealth (§2.11) and the exact detection-comparison formula remain open** | Cloak/stealth (§2.11), §8.3's per-viewer fog at full fidelity (already built, P3-05) | §11.9's dependency table · P5-07 (#255) · P2-12 (#256) |
| **2** | **Does the strategic layer stay bridge-gated** while tactical command travels with the player? | Nothing today — it blocks the *shape* of P8-05, not its start | §12.27 "What is deliberately not here" |
| **3** | **Time-to-milestone pacing** — first custom Template, first capital, first owned system. Deliberately left to be **read off `tools/economy_sim`'s derived curve**, not declared in advance | Balance passes, not construction | `features.md` §9 |
| **4** | **Whether §9's multiverse expansion ever gets a lore hook.** Decided: base game is one galaxy, multiverse is real future scope. `lore.md` needs no change *now* | Nothing | `features.md` §9 |
| **5** | **Whether a carried shell item can be installed at a mount, swapping in a different already-owned shell.** Raised 2026-08-23. Related to but distinct from the existing shell-grade-upgrade question above it in `features.md` §2.4 — this is a swap, not an upgrade. Needs a compatibility model for which shells fit which mounts (none exists — `MountBlueprint` authors exactly one `ShellId` per mount today) and a new `ItemKind::Shell` before it is buildable | A future Engineering task (would extend P4-12) | `features.md` §2.4 · `architecture.md` §12.30.5 "Shell items — an open question, distinct from the closed one" |

**Not open, despite appearances:** capture/ownership transfer (settled as boarding-in-place
2026-08-11 — `features.md` §9's entry was stale and has been fixed, see P11-07), the save model, the damage-type roster,
deconstruction yield, the rarity ladder, the quantity-per-grade multiplier, recovery-run parameters,
sub-commander recruitment, network raiding, and royalty scale.

---

# Appendix C — Deliberately out of scope

`architecture.md` §2.5 **prohibits starting these** until a shipped feature demands one. Listed so
nobody re-derives them as gaps.

| Item | Status | Revisit trigger |
|---|---|---|
| **`net/` — multiplayer** | 🧊 | Law 9's authority model is honoured throughout regardless, so this stays cheap to add later |
| **Asset pipeline** — atlases, UUID asset ids, audio banks, hot-reload | ⚡ **trigger fired 2026-08-11** | §2.5 prohibits starting it *until a shipped feature demands one*. **P11-09 is that feature**, and it takes only the sprite/atlas half; UUID asset ids, audio banks and hot-reload stay 🧊 until something demands them the same way. P11-08's first audio pass deliberately needs none of it |
| **Memory pooling** | 🧊 | §9.1's entity budgets being missed on reference hardware |
| **Release packaging** | 🧊 | An actual release |
| **Altitude bands** (§3.7) | 🧊 fully designed | The escort-fighter / docking / formation-keeping pass |
| **Physical hull-blocking shields** | 🧊 | Would neuter ramming and entangle collision, docking and friend/foe rules |
| **`UpkeepSystem`** | ❌ cut, do not build | §12.16 item 24 |
| **Two player modes** | ❌ withdrawn 2026-08-09 | §12.16 item 23 |
| **A `RadarSystem`** | ❌ never — contacts are one data source, two renderings | §13.5 group 2e |
| **A `ComponentDef` third tier** | ❌ not a gap — `ShellDef` + `ModuleDef` are the complete set | `features.md` §2 |
| **A fourth `Compound` supply tier** | ❌ rejected 2026-08-09 | `features.md` §2 |

---

# Appendix D — The five design gaps, and how they were closed

*This appendix opened 2026-08-11 as a list of five tasks that **could not be implemented as
written**, because the design they would build existed nowhere. **All five were specified the same
day** and now have real `features.md` sections behind them. The record is kept rather than deleted,
because the shape of the gaps is worth remembering: every one sat at the edge of the design, and none
was in the core loops.*

| # | Gap | Now specified in | What it changed |
|:---:|---|---|---|
| **D-01** | The tutorial | **`features.md` §1.2 The Opening** | Became the **two-act diegetic prologue** `lore.md` line 4 already made canon and nobody had turned into mechanics. Act I teaches Move/Target/Fire/Equip inside the Diaspora itself; Act II charges **50% of what the player gathers**. Split across P5-06 and P6-13, and it removed the faction picker from P10-04 |
| **D-02** | Contracts | **`features.md` §5.11 Contracts** | A board at the Trade facility over the two types already in code, with **derived rewards** rather than an authored table, reputation-gated visibility, and the §5.3 relation writes §15 found missing. Restored to P8-06 |
| **D-03** | What a save contains | **`features.md` §3.3 "What a save contains"** | Ten enumerated sections, and one structural rule that shrank the task: **a save is a demotion, not a registry serializer**, so loading is warping in. P10-02 unblocked |
| **D-04** | The gathering module | **`features.md` §2.10 "Gathering — one verb, three sites"** | Mining stopped being *"shoot the rock until it dies"* and became a **held beam** with three stats, serving mining, skimming and harvesting alike. Sources now **deplete rather than die**. P6-11 rewritten |
| **D-05** | Colonization | **`features.md` §6.6 Expansion and Colonization** | The **candidate selector** `EvaluateColonization` never had — adjacency first, archetype second — plus the thresholds. Gives §6.2's archetype table its first real reader. P9-05 unblocked |

**Two follow-ups this created**, both small and both tracked:

- **`lore.md` §4 contradicts `features.md` §1.2** and must be rewritten with P6-13. Its amnesia
  framing cannot coexist with a player who flew the prologue and watched the collapse; the
  faction-registry beat is the part that survives, and it moves into Act II.
- **`features.md` §9's open-questions list** should drop the tutorial and contract entries it never
  had, and gains nothing — these five were gaps in the document's *coverage*, not entries in its own
  list of known unknowns. That is what made them dangerous: **nothing anywhere said they were
  missing.** They were found by asking whether each roadmap task could actually be handed to someone.

> **The pattern, kept because it will recur:** all five sat at the edges — onboarding, side content,
> a persistence enumeration, one module's numbers, one AI threshold. The core loops are specified to
> an unusual depth. The next set of gaps will be at the edges too, and the way to find them is the way
> these were found: try to write the issue, and notice when you cannot.

---

---

## Counting the work

| Phase | Tasks | New systems/stores | Mostly |
|:---:|:---:|:---:|---|
| 0 | 16 | 2 (`HazardSystem`, widget layer) | Fixes, one-liners, and one test |
| 1 | 8 | 2 (`PlayerInputSystem`, `SystemMenu`) | Wiring built code |
| 2 | 11 | 0 | Changing built systems |
| 3 | 5 | 0 | Pointers and predicates |
| 4 | 14 | 0 | UI over built systems |
| 5 | 6 | 0 | UI |
| 6 | 13 | 2 (`ManufacturingSystem`, `Pricing`) | Types, content, derivation |
| 7 | 5 | 0 | Wiring built systems together |
| 8 | 6 | 1 (`OrderSystem`) | Producers for inert systems |
| 9 | 9 | 1 (the coarse driver) | Invocation paths |
| 10 | 6 | 1 (`RigState`) | Serialization |
| 11 | 9 | 1 (`AudioSystem`) | Authoring, plus audio and assets |
| QA | 5 | 0 | Tools and gates |
| **Total** | **113** | **10** | |

**Ten new systems or stores across the whole roadmap.** Everything else is a caller, a producer, a
predicate, a component on a factory, or a JSON file — which is exactly what §13's audit predicted:
*"the dominant defect is not absence of code."*

---

# The id → issue-number map

**Fill this in as you file, and keep it current.** The task id is the stable key — every
`Depends on:` in this file resolves against it, and `CLAUDE.md` step 2 / `architecture.md` §11.9's
rule (*confirm the dependency has actually merged to `main`*) has nothing to check against without
this table. A closed issue is **not** a merged one; the **Merged** column is the only thing the rule
reads.

- **Issue** — the GitHub issue number, `#NN`.
- **Merged** — `✅` once the PR closing it is merged to `main`, blank otherwise. Nothing else.

| Task | Title | Issue | Merged |
|---|---|:---:|:---:|
| P0-01 | World bodies, hittability and the star hazard | #110 |  |
| P0-02 | The shared widget layer | #111 | ✅ |
| P0-03 | Galaxy topology | #112 |  |
| P0-04 | The schedule and `Docked` correctness sweep | #113 |  |
| P0-05 | The hardpoint lifecycle defect pack | #114 | ✅ |
| P0-06 | `MountedModules` is the single mount record | #115 | ✅ |
| P0-07 | Delete the `mobile` movement gate | #116 |  |
| P0-08 | Rig aggregation: `RecomputeRigTotals` and the Sum/Max rule | #117 | ✅ |
| P0-09 | Four new `ModuleKind`s for systems that have no module | #118 |  |
| P0-10 | `CargoHold` moves onto the bay | #119 | ✅ |
| P0-11 | The repair path defect pack | #120 | ✅ |
| P0-12 | The research chain defect pack | #121 | ✅ |
| P0-13 | The `Element` / `Material` rename — one pass or none | #122 |  |
| P0-14 | Delete `TickCoarse` until it has a driver | #123 |  |
| P0-15 | Group 2 leftovers: text, tabs and one-liners | #124 | ✅ |
| P0-16 | The schedule is tested | #125 |  |
| P1-01 | `OnEnter`: the world and the player | #126 |  |
| P1-02 | Player input through the intent queue | #127 |  |
| P1-03 | Weapon groups, and no automatic target lock | #128 |  |
| P1-04 | Camera, and the two render defects | #129 |  |
| P1-05 | A station to dock at | #130 | ✅ |
| P1-06 | Death, respawn and the cull | #131 |  |
| P1-07 | The system menu and quit to main menu | #132 |  |
| P1-08 | M1 verification pass | #133 |  |
| P2-01 | The damage-type effect table, and Ion | #165 |  |
| P2-09 | What a kill leaves behind — the wreck path and the reaper | #166 |  |
| P2-02 | Docked cascade destruction | #167 |  |
| P2-03 | Shields, narrow-phase collision and the structural cascade | #168 |  |
| P2-04 | Structural integrity and honest hit resolution | #169 |  |
| P2-05 | Object scale, `hullRadius` and draw layers | #170 |  |
| P2-06 | The crew shell | #171 |  |
| P2-12 | Directional ECM — click-and-hold jamming | #256 |  |
| P2-07 | Capture — boarding in place | #172 |  |
| P2-08 | Power allocation — the half the player commands | #173 |  |
| P2-10 | The opposition state machine | #174 | ✅ |
| P2-11 | Faction commanders exist, and hold a system | #175 |  |
| P3-01 | The five null pointers, seeded relations, and the Reapers | #188 | ✅ |
| P3-02 | Combat, docking and the map consult the matrix | #189 | ✅ |
| P3-03 | Gameplay writes relations | #190 | ✅ |
| P3-04 | The player's faction moves onto the player record | #192 | ✅ |
| P3-05 | Fog of war migrates to knowledge networks | #191 | ✅ |
| P4-01 | The router | #199 |  |
| P4-02 | Screen 1 — the Bay | #200 |  |
| P4-03 | Screen 2 — Storage | #201 |  |
| P4-04 | Screen 3 — Repair | #202 |  |
| P4-05 | Screen 4 — Engineering | #203 |  |
| P4-06 | Screen 6 — Research, and its five missing producers | #204 |  |
| P4-07 | Screen 5 — Manufacturing, Draft half | #205 |  |
| P4-08 | The two flight overlays | #206 |  |
| P4-09 | M3 verification pass | #207 |  |
| P4-10 | Router — gate a tab on its screen shipping | #219 |  |
| P4-11 | Storage — sibling-hold selection | #220 |  |
| P4-12 | Engineering — editing the station's own rig | #221 |  |
| P4-13 | Research — the Codex | #222 |  |
| P4-14 | NPC logistics — spreading stock across holds | #223 (design issue, closed 2026-08-25 — deferred, see §12.30.3) |  |
| P5-01 | The status display | #233 |  |
| P5-02 | The flight HUD | #234 |  |
| P5-03 | The comms surface | #235 |  |
| P5-04 | Multi-scale territory navigation | #236 |  |
| P5-05 | Icon culling and substitution | #237 |  |
| P5-06 | Act I — the prologue | #238 |  |
| P5-07 | Signature, detection, and ECM | #255 |  |
| P6-01 | `Grade` in the taxonomy (§12.19a) | #240 |  |
| P6-02 | Stat pools and rolled quality (§12.21) | #241 |  |
| P6-03 | The item model (§12.19c) | #242 |  |
| P6-04 | The widening (§12.19d) | #243 |  |
| P6-05 | `core/economy/Pricing.h` | #244 |  |
| P6-06 | `ManufacturingSystem` | #248 |  |
| P6-07 | Manufacturing, Queue half | #249 |  |
| P6-08 | Market — the trading half | #251 |  |
| P6-09 | Per-item faction stock | #245 |  |
| P6-10 | Vessel assembly takes time | #250 |  |
| P6-11 | The gathering beam | #246 |  |
| P6-13 | Act II — the frontier tutorial and the tithe | #252 |  |
| P6-12 | Build `tools/element_check` — the four content checks | #247 |  |
| P7-01 | Knowledge networks reach gameplay |  |  |
| P7-02 | Reverse engineering, end to end |  |  |
| P7-03 | Template creation |  |  |
| P7-04 | Template negotiation, royalties and the market |  |  |
| P7-05 | Factions manufacture what you sold them |  |  |
| P8-01 | Construction gating and build mode |  |  |
| P8-02 | The local command system |  |  |
| P8-03 | The comms module and the sensor datalink |  |  |
| P8-04 | AI sub-commanders |  |  |
| P8-05 | The bridge — two scales, one interface |  |  |
| P8-06 | The contract board, distress calls and escorts |  |  |
| P9-01 | The coarse tick, and `FactionDecisionEngine` gets a caller |  |  |
| P9-02 | The four operational facets, and border skirmishes |  |  |
| P9-03 | Faction survival and elimination |  |  |
| P9-04 | Seeding drives world generation |  |  |
| P9-05 | Territory, adjacency and expansion |  |  |
| P9-06 | Warp: hyperdrive, fuel, charge and range |  |  |
| P9-07 | Promotion and demotion round-trip |  |  |
| P9-08 | The player as a faction |  |  |
| P9-09 | Boss encounters — a verification task |  |  |
| P9-10 | Warp-in and no-bay arrival cinematic |  |  |
| P10-01 | `RigState` — the live-rig snapshot |  |  |
| P10-02 | The world save |  |  |
| P10-03 | Save, Load and Settings in the system menu |  |  |
| P10-04 | The main menu grows up |  |  |
| P10-05 | Crafted content survives |  |  |
| P10-06 | M8 verification pass |  |  |
| P11-01 | The element roster |  |  |
| P11-02 | The material families |  |  |
| P11-03 | The module roster |  |  |
| P11-04 | The shell roster and the faction registry |  |  |
| P11-05 | The preset ship roster |  |  |
| P11-06 | Nebulae, planet types and hazard content |  |  |
| P11-08 | Audio — the game makes a sound |  |  |
| P11-09 | The asset pipeline, and the end of placeholder shapes |  |  |
| P11-07 | Documentation sweep |  |  |
| T-01 | The headless combat harness |  |  |
| T-02 | CI gains the content checks |  |  |
| T-03 | A wiring regression check |  |  |
| T-05 | The combat balance pass |  |  |
| T-04 | Playtest gates |  |  |
