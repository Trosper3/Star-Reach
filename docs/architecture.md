# Star Reach: Architecture & Engineering Guidelines

**Version:** 2.0
**Target Audience:** Core programmers, AI agents, and open-source contributors
**Tech Stack:** C++20, CMake, Ninja, vcpkg, raylib, EnTT, nlohmann_json, ENet, Catch2, GitHub Actions

---

## Status Legend

This document describes a target architecture. Most of it is **not built yet**. Every section is
marked so that no contributor — human or AI agent — builds on an assumed foundation.

| Mark | Meaning |
|:---:|---|
| ✅ | **Built.** Exists in the repository and works. |
| 🚧 | **Partial.** Scaffolded or incomplete; do not assume behavior. |
| 📋 | **Planned.** Designed, agreed, not yet written. |
| 🧊 | **Deferred.** Deliberately unbuilt. Do not start until a shipped feature demands it. |

**Current reality (updated 2026-08-10 by §14.1; this paragraph was last written 2026-07-29 and
undercounted until then):** the enforcement infrastructure, the First Vertical Slice (§10), and
**thirty-one of the §4 system inventory's thirty-three rows** are built — thirty of them scheduled
in `SystemSchedule.cpp` and audited row-by-row in §13.1. Only `ManufacturingSystem` and
`HazardSystem` remain 📋. **Built is not the same claim as wired** — §13 exists precisely because
most of those thirty-one rows do less than their ✅ implies. What remains beyond wiring is what
§3/§5/§6 mark 🧊 — deliberately deferred until a shipped feature demands it — plus the new **§12**
batch.

> 🚨 **The most important operational fact about this repository, re-verified 2026-08-08 (second
> pass): there is no playable game loop at all.** Everything below marked ✅ compiles and passes
> tests. Pressing **Start Game** produces an empty world at the origin with no player in it.
>
> *An earlier version of this block described this as "the meso-loop UI is not connected." That was
> true and too narrow — it named the fourth missing layer as if it were the first. The list below is
> ordered by what has to exist before the next line can matter.*
>
> **1. There is no world.** `SpaceFlight::OnEnter()` is empty. `world_gen::PopulateSystem` is called
> from exactly one place — `WarpToSystem` — and `WorldGen.h`'s own doc comment says *"call once,
> from `SpaceFlight::OnEnter`, before ShipFactory places the player rig."* That call was never
> written. Thirty scheduled systems tick over an empty registry every frame.
>
> **2. There is no player.** Nothing emplaces `PlayerControlled` outside `WarpToSystem` and tests —
> and `WarpToSystem` returns early when it finds no player, so the only path that creates one
> requires one to already exist. `CockpitHud`, `AvionicsMenu`, and `BridgeView` each open with
> `view<PlayerControlled>()` and draw nothing.
>
> **3. There is no player control.** The player's `ThrustInput` is written only by `RigFactory`
> (zero-initialised) and `DockingSystem` (zeroed while docked); `NpcAiSystem` runs
> `entt::exclude<PlayerControlled>` by design. `FireIntent` is written only by `NpcAiSystem`.
> **Across all of `src/`, exactly one gameplay key is polled** — the dock key in `AvionicsMenu.cpp`.
> Even given a spawned player, it would sit motionless and never fire.
>
> **4. There is no camera.** `cameraTarget_` is default-constructed and never assigned. The view is
> nailed to the origin.
>
> **5. `WorldGen` spawns no stations.** Sun, planets, asteroids, NPC presence — that is the whole
> list. `station_factory::Spawn` is reachable only from `ConstructionSystem`, which has no
> producers. So the docking path is not merely unexercised; **in a generated world it has no target
> to dock at**, and docking is gated on the bay's rig sharing the player's `FactionRef`.
>
> **6. Only then: the meso-loop UI.** `SpaceFlight::Draw()` calls three UI functions. **Nine menu
> files** (`BuildMenu`, `CustomizeMenu`, `EngineerMenu`, `ModulesMenu`, `NavigationMap`,
> `RefactorMenu`, `StationServicesMenu`, `StorageMenu`, `InventoryGrid`) are referenced only by
> their own translation units and their own tests. **None of the nine handles input** — each is a
> pure `Build*Request()` plus a stateless `Draw()`, with no selection state and no open/closed
> state anywhere.
>
> ⚠️ **Two mechanisms are called "intents" and only one of them is Law 9's.** A previous draft of
> this block reported "exactly two intents are produced outside tests." That counted *request
> components*, not `core::Intent`:
>
> | | Mechanism | State |
> |---|---|---|
> | **Request components** | `DockRequest`, `BuyItemRequest`, `MountModuleRequest`, … — emplaced on an entity, drained by a system | Two producers (`DockRequest`/`UndockRequest`, `AvionicsMenu.cpp`). The other nine request types are *built* by a menu and **placed by nobody** |
> | **`core::IntentQueue`** | Law 9's actual channel | **Zero producers anywhere.** `SetThrottleIntent`, `FireWeaponsIntent`, `SetTargetIntent`, `SpawnBlueprintIntent` have **zero consumers too** — dead in both directions. Only `PitchTemplateIntent` and `SaveTemplateIntent` are drained |
>
> Consequently seven scheduled systems — `ConstructionSystem`, `ModuleEquipSystem`,
> `EngineerSystem`, `RefactorSystem`, `StationServicesSystem`, `TemplateMarketSystem`,
> `ResearchSystem` — have **zero producers**. `ResearchSystem` is one rung further back than the
> other six: it has no request component and no menu at all, and `StationFacility::researchJobs`
> has no writer outside `ResearchSystem` itself.
>
> **`ActorId` has no resolution path.** Every `core::Intent` addresses an actor, there is no
> `ActorId` component on any entity, and the type appears in exactly one signature outside
> `Intent.h` (`CustomizeMenu.h`). Nothing can turn an intent's actor into an `entt::entity`.
>
> **Five `SystemContext` pointers are `nullptr` at runtime.** `SpaceFlight.cpp` populates only
> `economy`; `discovery`, `knowledge`, `diplomacy`, `reputation`, and `craftedModules` are all null,
> so five more systems silently no-op against their own null guards. `DiplomacyMatrix` has zero
> writers anywhere — the exact failure `features.md` §5.3 was written to prevent, reproduced.
>
> **Two one-line-class defects found in the same pass:**
> - 🐛 **`BridgeView::AvailableTabs` omits `FacilityKind::Engineering`.** Its `kAllKinds` array
>   lists five of the enum's six values. `EngineerSystem` and `RefactorSystem` both gate on a living
>   Engineering facility, and the enumerator's own comment names both menus — so those two can never
>   surface as tabs even once a router exists. A hand-maintained parallel list with no
>   enumerator-count guard, written before `Engineering` was added.
> - 🐛 **`CustomizeMenu::ConsumeSaveTemplateRequests` inverts Law 9.** A `modes/space/ui/` file
>   drains the intent queue and calls `KnowledgeStore::Grant` — the only place a UI file mutates
>   `core/` state. It is a system wearing a menu's filename. Called only from tests.
> - `SpaceFlight::Draw()` calls `render::DrawWorld` **twice per frame** with identical arguments.
>
> **The one piece of good news, and it shrinks the work:** `BridgeView` is already the docked-menu
> router's skeleton. It gates on `Docked`, reads the host station's *living* facility hardpoints,
> and lists them — which is `features.md` §3.4's player-location model exactly. It needs selection
> state and a dispatch table, **not new construction.**
>
> §12.13 gestured at this as a component-producer problem and §11.9 called it a UI-wiring problem.
> It is neither: it is the game loop. **§12.24 sequences it, and nothing else in §12 should start
> first.** Until it lands, every new system ships into the same void.
>
> 🔍 **§13 audits all thirty systems against this, one row each, and the picture is worse in four
> specific ways.** Populating the world is not enough to make it *visible*: `WorldGen`'s sun,
> planets and asteroids satisfy neither of `WorldRenderer`'s views, and asteroids carry a drift
> velocity `PhysicsSystem` never integrates. **Nothing in the codebase can damage an asteroid**, so
> mining, the tutorial's asteroid step and all material loot are unreachable behind one view
> predicate. `HierarchySystem` runs *before* `PhysicsSystem`, so every hit test in the game resolves
> against hardpoint positions one tick stale. And a docked NPC already thrusts and fires — the exact
> failure §12.24 step 2 predicts for the player, live today. **Eight of thirty systems are wired end
> to end;** §13.5 is the task list, and §13.4 tracks the decisions it raised. **The first of those
> is resolved — §12.28.**

**Start here if you are picking up a design feature.** `features.md` gained a set of 📋 design
sections (knowledge networks, sub-commanders, faction survival by Three Pillars, seeding, the
navigation map, template negotiation) that had *no architectural counterpart* — a specification of
behaviour with no answer to which directory, which layer, or which system. **§12 is that answer**,
one subsection per feature, and §11.9's dependency table now has rows again because this batch has
real ordering. Read §12 before §11.3's recipe, not after.

- ✅ **CI is running.** `build.yaml` moved to `.github/workflows/`. Structural enforcement,
  formatting, a three-platform matrix build, `.clang-tidy`, and an ASan job all gate `main`.
- ✅ **Layer targets** — `sr_shared`, `sr_core`, `sr_engine`, `sr_shared_ui`, `sr_menu`, `sr_space`,
  `StarReach` (§2.1).
- ✅ **Enforcement scripts** — size caps, layer includes, content pipeline, empty scaffolding
  (§2.2–2.4), in `tools/ci/`. All four verified to fail on planted violations.
- ✅ **`CMakePresets.json`**, raylib + Catch2 in `vcpkg.json`, a real `.clang-format`
  (it was previously an empty file, which clang-format errors on).
- ✅ **Blueprint schema** (`shared/blueprints/`), **POD components** (`shared/components/`),
  **JSON registries** (`core/registries/`), **intent queue** (`core/events/`), **fixed timestep**
  (`core/time/`), **`SystemWorld`** and the **system contract** (`modes/space/`).
- ✅ **Factories** — `RigFactory`, `NpcFactory`, `StationFactory`, `WorldGen`
  (`modes/space/factories/`).
- ✅ **Thirty of the §4 systems** registered in `SystemSchedule.cpp` (§13.1), plus
  `core/ai/FactionDecisionEngine` — the tier-3 decision engine §4 lists as `FactionDecisionSystem`.
  It lives in `core/ai/` rather than `modes/space/systems/` because it reads and writes
  `core/economy/`/`core/diplomacy/` state directly rather than taking a per-tick `SystemContext`.
- ✅ **Renderer** — `WorldRenderer`, `LightingPass`, `IconRenderer` (`modes/space/render/`).
- ✅ **`shared/ui/` HUD theme** (`sr_shared_ui`) and **`modes/space/ui/`** — `CockpitHud`,
  `AvionicsMenu`, `BridgeView` built against it.
- ✅ **`IGameMode` + `modes/main_menu/`** — the second mode. `main.cpp` opens on `MainMenu` and
  switches to `SpaceFlight` on `ShouldStartGame()`.
- ✅ **`core/diplomacy/`** (`DiplomacyMatrix`, `Reputation`, `Territory`), **`core/economy/`**
  (`FactionEconomy`), **`core/galaxy/`** (`Discovery`) — the Law 8 galaxy-wide state.
- ✅ **Unified serialization** (`core/serialization/`: `BlueprintSerialization`, `ByteStream`,
  `SaveFile`) and **save schema migration** (`SaveMigrator`).
- ✅ **424 tests pass** (1097 assertions, re-verified 2026-08-11 by an actual build per §14.4's own
  instruction — the 281/690 figure this line previously carried was stale since 2026-08-02's
  ResearchSystem PR and had drifted through four documentation passes uncorrected), including the
  coverage the previous snapshot listed plus docking/warp/comms/contract/distress/tutorial/mining/
  loot lifecycle coverage, faction economy/decision-engine coverage, diplomacy/reputation/territory
  coverage, discovery coverage, research coverage, and blueprint-serialization/save-migration
  coverage.

Not built, and the two categories are **not** the same thing:

- 🧊 **Deferred, do not start** — everything §3/§5/§6 mark: `net/` (multiplayer), the asset pipeline
  (atlases, UUID asset IDs, audio banks, hot-reload), memory pooling, release packaging. Prohibited
  until a shipped feature demands one (§2.5).
- 📋 **Designed and startable** — the §12 batch: `core/knowledge/`, `core/galaxy/Seeding`,
  `ResearchSystem`, `CommanderSystem`, `TemplateMarketSystem`, Faction Survival, the recovery-run
  wreck record, and `NavigationMap`. These are the opposite of 🧊: they are waiting for someone to
  pick them up. Check §11.9 for ordering first — `KnowledgeNetwork` gates three of them, Faction
  Survival gates on `CommanderSystem`, and two entries (§12.4 seeding, §12.5 recovery run) are
  independently startable today.

Four of §12's open ❓s have since been settled (economic footprint §12.3, wreck-survives-demotion
§12.5, sensor-gated fog of war §12.6, and the negotiation roll's three-step shape §12.7), and
`ResearchSystem` — previously named only as a `KnowledgeNetwork` writer with no home of its own — now
has the same five-part treatment as everything else, folded into §12.1. All eight items in the §12
batch are now startable, none blocking. **The remaining two — §12.1's network raiding and §12.2's
sub-commander recruitment/loyalty — were also settled, 2026-08-11**, both in `features.md` directly
(§2.5 and §4.5 respectively) rather than needing further architecture-side design; see those
subsections above for the corrected text. Nothing in the §12 batch is open anymore.

**A second §12 batch (§12.9–§12.12) covers the docked-station "meso loop" UI** — Template creation,
station trade/repair/merge, cargo & hardpoint equip, and construction/refit/grafting. Unlike the
first batch, these were not `features.md` design sections waiting on an architecture home; they were
`../StarReach2` menu files (`CustomizeMenu`, `StationServicesMenu`, `StorageMenu`, `ModulesMenu`,
`BuildMenu`, `RefactorMenu`, `EngineerMenu`) with no representation anywhere in either document —
not ✅, not 📋, not 🧊. `TemplateMarketSystem` (§12.7) has had nothing to sell and `CargoHold`'s own
doc comment named its future reader by name; this batch closes both. §12.9 (Template creation) is
the load-bearing one and depends on `KnowledgeNetwork` (#78); §12.10–§12.12 are independently
startable today, except `MergeModuleRequest` within §12.10, which is blocked on a module-tier
content decision that does not yet exist anywhere in the schema.

**A third §12 batch (§12.24–§12.27) is different in kind from both above, and it comes first.** Where
§12.1–§12.12 gave designed behaviour an architectural home, this batch gives **built code a caller**:
the world-population call `WorldGen.h` already prescribes, the player-input producer four
`core::Intent` variants were declared for, the camera assignment `SpaceFlight` already reserves a
member for, the tab dispatch `BridgeView` already computes a tab list for, and — in §12.27 — the
party membership `PartySystem` has never once received and the `Commander::orders` value nothing has
ever read. Almost none of it is new design.

| | |
|---|---|
| **§12.24** | Wiring the game loop — world, player, control, camera, a station, the docked router, five null pointers. **Startable today; §11.9 puts it ahead of everything** |
| **§12.25** | Deleting the `mobile` movement gate, so capability is emergent from living hardpoints |
| **§12.26** | `FacilityKind::Construction` and build mode — the gate `ConstructionSystem` never had |
| **§12.27** | The local command system — selection, order queues, stance. Switches on two inert systems |
| **§12.28** | World bodies, hittability and the star hazard — **§13's first resolved cluster, and part of §12.24 step 1.** Makes `WorldGen`'s output visible and shootable |
| **§12.29** | The system menu and returning to the main menu. `main.cpp`'s mode transition is one-way today. **Imposes a re-entrancy requirement on §12.24 step 1** |

**§13 is the system-by-system wiring audit** — five columns per system (producer, consumer, UI,
content, matches-docs), covering all thirty scheduled systems plus the factories, the renderers,
and `FactionDecisionEngine`. It found that **eight of thirty systems are wired end to end**, and it
adds twenty-two verified defects §0's list did not contain. Four of them change §12.24's own
sequence: `WorldGen`'s entire output is undrawable, so step 1 needs a world-body component set to
be judgeable at all; nothing in the codebase can damage an asteroid; `HierarchySystem` runs a tick
ahead of the transforms it depends on; and a docked NPC already flies away under thrust, which is
the exact failure step 2's ordering table predicts for the player. **Read §13.5 before picking up
any §12 issue** — it is the task list, grouped by what unblocks what.

---

## 0. Why This Document Is Structured Around Enforcement

This project is the second attempt. The first (`../StarReach2`, ~39,000 lines) is a playable game
with multiplayer, procedural galaxies, and convex-hull collision. It is also unmaintainable, and
**it failed despite having a designed directory structure nearly identical to the one below.**

The evidence is unambiguous:

- `src/modes/space_flight/entities/` — created, correctly named, **left empty**. The entire world
  data model (`SpacePlanet`, `SpaceSun`, `SpaceStation`, `NpcMeta`, `PlayerMeta`, `LootDrop`,
  `DerelictWreck`, `DistressCall`, `NpcParty`, `SystemWorld`) was declared inside `SpaceFlight.h`
  instead — 466 member declarations in one header.
- `SpaceFlight.cpp` reached **12,947 lines**, 33% of the codebase. A single function, `Update()`,
  is **2,379 lines**. `UpdateNpcShips()` is **1,213**.
- `src/components/` holds one file. `src/ui/menus/` holds one file. `src/shared/ui/menus/` and
  `src/modes/space_flight/hud/` are empty. Four plausible homes existed for a UI file; three went
  unused.
- `TransformComponent.h` — the most fundamental component in any ECS — is included by **one** file
  outside its own directory. The ECS was scaffolded and never adopted.

Meanwhile, one part of that architecture held perfectly across all 39,000 lines: **the data-driven
boundary.** 15 of 26 registries load real JSON from `config/`. That boundary survived because a
JSON file and a C++ struct are separated by a *parser*. You cannot accidentally blur them at 2am.

That yields the principle this document is built on:

> **Boundaries enforced by a format, a parser, or a linker survive.
> Boundaries enforced by a document do not.**

The failure mode was never absence of design. It was absence of *resistance*: appending a function
to the file already open was always cheaper than creating a file, wiring CMake, and adding an
include. Do that 132 times and you have a god object.

So every law below is paired with a mechanism where one exists. Laws without mechanisms are
labeled **[review-time]** so we know which ones are load-bearing and which are aspiration.

---

## 1. The Twelve Laws

### Law 1 — EnTT Is The ECS ✅ *(dependency present)*

**We do not write our own ECS.** `entt::registry` is the entity store. Its sparse-set-with-packed-
arrays design and swap-and-pop removal are exactly what this project needs for dynamic component
attachment (critical for localized hardpoint destruction) with no heap fragmentation.

*Prior versions of this document specified hand-rolling that structure. That was a description of
EnTT's internals written as if it were our work. StarReach2 shows what rolling your own actually
produces: an unfinished ECS that loses every argument against adding one more function to the file
that already works.*

Components are plain-old-data structs. No virtual methods, no inheritance, no owning pointers,
no `std::vector` members where a child entity would do (see Law 4).

**Mechanism:** the dependency is in `vcpkg.json`. Any hand-rolled entity container fails review.

### Law 2 — One Registry Per Star System 📋

Each `SystemWorld` owns its own `entt::registry`. There is no global entity store.

| Consequence | Effect |
|---|---|
| **LOD tiers** | Tick the active registry at 60 Hz, neighbors at 5 Hz, background not at all. No per-view `SystemId` filtering. |
| **Warp** | A clean handoff: serialize the player's entities out of registry A, instantiate into registry B, tear down A if nobody's left. |
| **Saves** | Serialize per system. A galaxy save is a set of independent blobs. |
| **Memory** | Distant systems unload entirely. |
| **Multiplayer** | Matches the per-system-worlds model StarReach2's protocol already proved. |

**Two hard rules follow from this:**

1. **Entity handles never cross a system boundary, a save file, or the wire.** They are
   registry-local and invalidated by travel. Anything persistent uses a stable string or integer
   ID (see Law 3).
2. **Galaxy-wide state does not live in a registry.** Diplomacy, reputation, faction stock, trade
   routes, and territory live in `core/` as plain data, mode- and registry-agnostic. This is
   already what Law 8 requires; the two laws reinforce each other.

*In-transit fleets are the awkward case. They are modeled as galaxy-level records in `core/` with
an origin, destination, and ETA — not as entities in either registry. They instantiate on arrival.*

### Law 3 — Blueprint Form vs. Live Form 📋

Every composite object exists in two representations, and the distinction is absolute.

| | **Blueprint form** | **Live form** |
|---|---|---|
| Type | Plain struct, JSON-shaped | Entities + components |
| Identity | Stable string ID | `entt::entity` handle (volatile) |
| Lives in | `data/base_game/*.json`, save files, wire packets, player Templates | `entt::registry` |
| Read by | Registries, factories, validation, serialization | Systems |
| Lifetime | Authored / persisted | One session, one registry |

A **factory** is the only bridge: `Factory(blueprint, registry) -> entt::entity`. Nothing else
converts between the two forms.

This resolves a direct contradiction between the previous versions of this document (which called
`Hardpoint` "a foundational struct") and the design document (which called hardpoints "ECS
entities"). **Both were right about different stages.** Blueprints are structs. Live rigs are
entity graphs.

It also solves the problem that would otherwise appear on day one: entity handles must never enter
a save file or a Template, and without this split there is nothing stopping them.

### Law 4 — The Unified Rig Law 📋

Starships, orbital stations, and ground structures share one composition model:

```
Shell  ->  Module
```

**Two tiers, not three** (corrected 2026-08-07 — `features.md` §2). This diagram previously read
`Shell -> Component -> Module`, which caused two separate sessions to conclude that a missing
`ComponentDef` type was an outstanding gap. It is not: **shell and component are two names for the
same thing** — the housing a module mounts into, which is also the unit of localized damage and the
thing a hardpoint *is*. `ShellDef` and `ModuleDef` are the complete authored set. "Component" stays
acceptable in conversation and UI copy; it must never become a third authored type.

In **live form**, a hardpoint is an **entity**, not a struct field. A rig is a parent entity
carrying `Rig { std::vector<entt::entity> children; }`; each child carries `ParentRig`,
`LocalTransform`, `WorldTransform`, `Health`, and whatever role components apply
(`Turret`, `ShieldGenerator`, `PowerCell`, `FacilityBay`).

**Why entities and not a nested struct.** StarReach2 used the struct approach, and:

- `shared/entities/Hardpoint.h` grew to **1,146 lines** — a fat struct with nested
  `std::vector<ModuleSlot>`, wrapped by `HardpointMountRig`, subclassed by `HardpointRig`, ringed
  by free functions taking `std::vector<Hardpoint>&`: `TotalHardpointHull`, `TotalEquippedMass`,
  `TotalEngineStats`, `RecalculatePowerBudget`, `RecalculateAdjacency`,
  `RebakeStationHardpointRing`. Every one of those is a system that could not become a system,
  because the data was not queryable.
- `WeaponSlots()` / `ShieldSlots()` / `AuxSlots()` appear **twice each in const and non-const
  pairs** — once on `Hardpoint`, once on `HardpointMountRig`.
- Most damning: `net/Protocol.h` contains **four** hardpoint wire formats —
  `CapitalHardpointSnapshot`, `PlayerStationHardpointSnapshot`, `FighterHardpointSnapshot`,
  `FighterMountReport`. One concept, four serializers, because capitals, stations, and fighters
  were each built separately. The Unified Rig Law was in the document the whole time and the code
  disagreed with it.

With uniform components those four collapse into one. A damage system runs
`registry.view<Health, ParentRig>()` and does not care whether it is chewing through a fighter
wing, a station battery, or a capital's dorsal turret.

**On performance** — the concern that motivated the struct approach is unfounded. A capital carries
~20 hardpoints, a fighter ~5; a couple hundred craft in an active sector is 2,000–5,000 hardpoint
entities. EnTT iterates millions per frame. We are three orders of magnitude from this mattering.

The one real cost is the parent-transform lookup, and it has a standard fix: **one hierarchy pass
per tick** iterates parents and writes each child's `WorldTransform`. Every later system then reads
a flat, local component with no indirection. Budget it as a single sequential pass, not as
per-hardpoint random access into another pool.

### Law 5 — Factories First 📋

Object assembly is decoupled from frame-by-frame simulation. `NpcFactory`, `StationFactory`,
`ShipFactory`, `WorldGen` read blueprints and emit entities. Systems never construct composite
objects.

**Mechanism:** systems live in `systems/`, factories in `factories/`, and `systems/` must not
include `factories/`. Checkable in CI.

### Law 6 — Orchestrators Own No State 📋 **[new]**

**A mode class holds no gameplay state.** It holds a registry handle, a camera, UI view state, and
nothing else. No entity data, no world data, no faction data.

This is the law that would have prevented StarReach2's central failure. That god object was not
caused by file size — it was caused by **ownership**. `SpaceFlight` owned all 466 members, so
every new feature arrived to find its natural home already open, already compiling, already
holding everything it needed. The path of least resistance pointed at one file for three years.

When the registry owns state, that pressure structurally disappears: a new system cannot reach
into the mode class for data, because the data is not there.

**Mechanism:** cap the mode header's member count in CI. `SpaceFlight.h` had 466; the cap is 25.

### Law 7 — Orchestrator Exclusivity 📋

Mode files (`SpaceFlight.cpp`) manage lifecycle (`OnEnter`, `Update`, `Draw`, `OnExit`) and system
routing. Presentation math — camera interpolation, viewport scaling, UI layout — belongs here.
**Simulation math — AI, combat resolution, physics — does not.**

**Mechanism:** the 600-line file cap and 80-line function cap (§2) make a god object impossible
regardless of intent. The math distinction itself is **[review-time]**.

### Law 8 — The Diplomacy / Economy Bridge Law 📋

Galaxy-wide state — the diplomacy matrix, reputation, faction stock, trade routes, territory —
lives in `core/`. It is mode-agnostic, registry-agnostic, and **render-agnostic**.

**Mechanism, and this one is strong:** `sr_core` does not link raylib. It cannot. That single
constraint is what makes the headless `tools/economy_sim` possible, what makes the faction
simulation unit-testable in CI, and what will keep `core/` honest when `planet/` eventually exists.
A link error is a better guardian than a paragraph.

### Law 9 — The Multiplayer Authority Law 📋 / 🧊

**Development is single-player-first.** ENet integration is 🧊 deferred.

The *discipline* is not deferred. From the first line of gameplay code:

- UI and input emit **Intents** (`PlaceShipRequest`, `BuildStationRequest`, `FireWeapon`) into a
  command queue. UI never mutates game state directly.
- Systems consume intents, validate against `core/economy/`, and apply results.

In single-player the queue is drained locally in the same frame. Adding ENet later means changing
where intents come from, not restructuring every menu. StarReach2's `InputCommand`,
`BuildStationRequest`, and `PlaceShipRequest` in `net/Protocol.h` are the proof this shape works —
they port nearly as-is.

**Mechanism:** `modes/*/ui/` must not include `systems/`. Checkable.

### Law 10 — Single Content Pipeline 📋 **[new]**

**JSON in `data/` is the only source of authored content.** No C++ definition tables.

StarReach2 ran two content pipelines simultaneously and never noticed: `config/modules.json` with
a `ModuleRegistry.cpp` parser, *and* 883 lines of hardcoded factories in `data/modules/*Defs.h`:

```cpp
inline ModuleDef Weapon_PulseCannon_I() {
    m.id = "pulse_cannon_i";  m.weapon.damage = 15.0f;  ...
}
```

Both survived to the end, because nothing declared which one won. Even a well-enforced boundary
leaks where the *rule* is ambiguous.

If content is genuinely awkward to express in JSON, that is a schema conversation — not an escape
hatch back into C++.

**Mechanism:** CI rejects new `ModuleDef`/`ShipDef` literals outside `tests/` and registry parsers.

### Law 11 — No Dump Folders, And A Tie-Breaker 📋

Directories named `utility/`, `helpers/`, `common/`, or `misc/` are banned. Domain-agnostic helpers
are fine but must be scoped by technical purpose (`core/math/Random.h`, `core/string/Format.h`).

**The tie-breaker matters more than the ban.** StarReach2's problem was not junk drawers — it was
*four plausible homes* for a UI file, three of which ended up empty. Ambiguity under fatigue always
resolves to "whatever I already have open."

> **When in doubt, put it in the mode. Promote to `shared/` only when a second consumer actually
> appears.**

Promotion is cheap, mechanical, and evidence-based. Premature sharing is what created
`src/shared/ui/menus/` and left it empty.

### Law 12 — The Anti-Spaghetti Event Law 📋

Systems may listen to the Event Bus. **A system may not emit a new event in direct response to
receiving one.** Multi-step chain reactions are sequenced explicitly by an orchestrator, not by
bouncing blindly through the bus.

**[review-time]**, and worth the review cost — this is the rule that keeps a debugger session from
becoming an archaeology expedition.

---

## 2. Enforcement ✅

Five mechanisms, in descending order of value. None is expensive; together they are the reason to
expect a different outcome this time.

### 2.1 Layers Are CMake Targets ✅

The single highest-value structural change. Each layer is a real library with explicit
dependencies:

```
sr_shared    -> EnTT, nlohmann_json      # NO raylib. NO core. NO modes.   <- bottom layer
sr_core      -> sr_shared                # NO raylib. NO engine. NO modes.
sr_engine    -> raylib                   # NO game knowledge at all.
sr_space     -> sr_core, sr_shared, sr_engine
StarReach    -> sr_space
```

Illegal dependencies become **link errors**, not code-review opinions. Laws 8 and 9 enforce
themselves permanently and for free.

**Correction to the original graph.** Earlier versions specified `sr_shared -> sr_core, sr_engine`.
That cannot link. `core/registries/` parses JSON *into* the blueprint structs in
`shared/blueprints/`, so `sr_core` depends on `sr_shared`; having `sr_shared` also depend on
`sr_core` is a cycle. §2.3 already pointed at the resolution by forbidding `core/` only from
`shared/ui/` rather than from all of `shared/`. So **`sr_shared` is the bottom layer** — POD
blueprints, POD components, pure math, no renderer — and the one part that genuinely needs a
renderer, `shared/ui/`, becomes `sr_shared_ui` above `sr_engine` when the first HUD primitive
lands. This is a good example of the document being wrong in a way only a linker could find.

**Not yet declared, deliberately** (§2.4 — no dead abstractions): `sr_net` (deferred with ENet),
`sr_menu` (`modes/main_menu/` has no files), `sr_shared_ui` (`shared/ui/` has no files). Each is
added in the commit that puts the first file in it.

**What the graph does *not* catch.** Every target shares one include root (`src/`), so an illegal
`#include` still *compiles*. It fails at link only if a symbol is actually used — which
header-only violations avoid entirely. That gap is exactly what §2.3's grep covers. The two
mechanisms are complementary and neither is sufficient alone.

### 2.2 Size Caps

| Unit | Cap | StarReach2's worst |
|---|---:|---:|
| File | 600 lines | `SpaceFlight.cpp` — **12,947** |
| Function | 80 lines | `Update()` — **2,379** |
| Mode-class members | 25 | `SpaceFlight.h` — **466** |

The function cap is not redundant with the file cap. Four StarReach2 functions individually exceed
the 600-line *file* limit:

| Function | Lines |
|---|---:|
| `Update()` | 2,379 |
| `UpdateNpcShips()` | 1,213 |
| `Draw()` | 762 |
| `DrawHUD()` | 583 |

Caps force extraction at line 601, when it is a twenty-minute job — instead of at line 12,000,
when it becomes a twelve-phase refactor plan that never gets started.

Hard CI failure, no warnings. A warning is a document.

### 2.3 Layer Include Rules ✅ `tools/ci/check_layers.py`

Grep-based CI job, complementing 2.1 for header-only violations:

| Layer | May not include |
|---|---|
| `shared/` | `core/`, `engine/`, `modes/`, `net/`, `raylib.h` |
| `shared/ui/` | `modes/`, `net/` — the one part of `shared/` that *may* use `engine/` and raylib |
| `core/` | `engine/`, `modes/`, `net/`, `shared/ui/`, `raylib.h` |
| `engine/` | `core/`, `modes/`, `shared/`, `net/` |
| `modes/space/systems/` | `factories/`, sibling `ui/`, sibling `render/` |
| `modes/*/ui/` | `systems/` |
| `modes/space/` | `modes/planet/` (and the reverse) |

Each violation prints the *reason*, not just the rule. A rule whose rationale you have to go
look up is a rule people route around.

### 2.4 No Dead Abstractions ✅ *(directory half automated)*

**No component, system, or interface lands without a consumer in the same commit.**
`TransformComponent` with one include is the tell. Anything with zero external includes is deleted
at review, not kept "for later."

The **directory** half of this is now mechanical: `tools/ci/check_dead_dirs.py` fails on any
directory under `src/` with no source beneath it. §0's headline evidence was a directory —
`src/modes/space_flight/entities/`, created, correctly named, left empty — and this repository
had already accumulated **sixteen** of them before a single system was written, including a full
`modes/planet/` tree that §3 explicitly marks DO NOT BUILD. They have been deleted. Create a
directory in the commit that puts the first file in it; that is also when you find out whether
you actually wanted it there (Law 11's promotion rule).

Note this check is mostly a *local* guard — git cannot track an empty directory, so a fresh clone
never has one. That is the point. The empty tree exists on the machine where the 2am decision
about where to put a file actually gets made.

The **symbol** half (a component with no reader) stays **[review-time]**. Detecting it properly
needs the compiler, and a grep that approximates it would produce false positives on exactly the
POD components that are supposed to have many readers later.

### 2.5 Deferral Is Explicit

🧊 sections are not backlog. They are **prohibited until a shipped feature demands them.** You
already applied this instinct correctly to `modes/planet/`; §5 applies it to `engine/`, which is
where speculative framework actually accumulates.

---

## 3. Master Directory Blueprint

**Directories that do not exist yet are not listed as empty folders — they are created by the
commit that adds their first file (§2.4).** The tree below marks what is real today.

```
StarReach/
├── .github/workflows/
│   ├── build.yaml           # ✅ Enforcement + format + 3-platform matrix + tests
│   └── deploy.yml           # 🧊 Apple notarization & Steam deploy on release tags
│
├── data/
│   ├── base_game/           # ✅ shells.json · modules.json · ships.json
│   │                        #    The ONLY content source (Law 10)
│   └── mods/                # 🧊 Runtime overrides
│
├── docs/
│   ├── architecture.md      # This file — laws, layering, enforcement, migration
│   ├── features.md          # Game design document
│   └── lore.md              # Narrative, faction, and simulation bible
│
├── tools/
│   ├── ci/                  # ✅ check_sizes · check_layers · check_content_pipeline ·
│   │                        #    check_dead_dirs · check_all (run this before pushing)
│   ├── asset_viewer/        # 🧊 Modular rig assembly visualizer
│   └── economy_sim/         # ✅ Built 2026-08-11 — headless grade-ladder cost/time derivation
│                             #    (links sr_options only, no sr_core; see EconomyModel.h)
│
├── tests/
│   ├── unit/                # ✅ Math, blueprint validation, timestep, intents, SystemWorld
│   └── integration/         # ✅ Real content-set load + validation
│
├── src/
│   ├── main.cpp             # ✅ Load content -> open window -> fixed-timestep loop
│   │
│   ├── engine/              # Hardware abstraction. Zero game knowledge.
│   │   └── platform/        # ✅ Window.h/.cpp — the ONLY place raylib is included
│   │                        # 🧊 graphics/ input/ memory/ assets/ — add with first consumer
│   │
│   ├── core/                # Universe truths. No raylib, no modes, no registry instance.
│   │   ├── ai/               # ✅ FactionDecisionEngine — the 4-facet decision engine
│   │   ├── diplomacy/        # ✅ DiplomacyMatrix, Reputation, Territory (Law 8)
│   │   ├── economy/          # ✅ FactionEconomy
│   │   ├── events/          # ✅ Intent.h, IntentQueue.h (Law 9)
│   │   ├── galaxy/           # ✅ Discovery — system discovery as shared faction knowledge
│   │   │                     # 📋 Seeding — the §12.4 seed cascade (pure, deterministic)
│   │   │                     # 📋 WreckRecord — death wrecks that outlive a demoted
│   │   │                     #    registry (§12.5). Entity form lives in LootSystem.
│   │   ├── knowledge/        # 📋 KnowledgeNetwork — §12.1. The Law 8 store for
│   │   │                     #    unlocks, Templates, and intel. NOT a component.
│   │   ├── registries/      # ✅ JsonReader, BlueprintJson, ContentLibrary
│   │   ├── serialization/    # ✅ BlueprintSerialization, ByteStream, SaveFile, SaveMigrator
│   │   ├── time/            # ✅ FixedTimestep.h
│   │   │                    # 📋 algorithms/
│   │
│   ├── shared/              # THE BOTTOM LAYER (§2.1). No raylib, no core/.
│   │   ├── blueprints/      # ✅ Ids, Taxonomy, ModuleDef, ShellDef, RigBlueprint,
│   │   │                    #    ShipBlueprint, DefLibrary, Validation
│   │   ├── components/      # ✅ Identity, Transform, Rig, Health, Physics, Power,
│   │   │                    #    Combat, Targeting  — all POD
│   │   │                    # 📋 Commander (§12.2), NetworkOwner (§12.1) — both hold a
│   │   │                    #    stable id into core/, never the data itself
│   │   ├── math/            # ✅ Vec2.h, Angle.h
│   │   └── ui/              # ✅ HudTheme.h — sr_shared_ui, the only lib above
│   │                        #    both sr_shared and sr_engine
│   │
│   ├── net/                 # 🧊 Deferred; Law 9's intent discipline is NOT deferred
│   │
│   └── modes/
│       ├── IGameMode.h      # ✅ Lands with the second mode
│       ├── main_menu/       # ✅ MainMenu — the second mode
│       │
│       ├── space/
│       │   ├── SpaceFlight.cpp/.h   # ✅ Lifecycle + routing ONLY. 6 members, cap 25 (Law 6)
│       │   ├── data/               # ✅ SystemWorld — owns the registry + NetworkId mapping
│       │   ├── systems/            # ✅ System.h (the contract) + all 30 scheduled systems in §4,
│       │   │                       #    registered in SystemSchedule.cpp (§13.1)
│       │   │                       # 📋 ManufacturingSystem, HazardSystem (§12.18, §12.28) —
│       │   │                       #    designed, not yet written
│       │   ├── factories/          # ✅ WorldGen, NpcFactory, StationFactory, RigFactory
│       │   ├── render/             # ✅ WorldRenderer, LightingPass, IconRenderer
│       │   │                       # 📋 IconRenderer gains the §12.6 map icon bake
│       │   └── ui/                 # ✅ CockpitHud, AvionicsMenu, BridgeView
│       │                           # 📋 NavigationMap — §12.6. Stays in the mode per
│       │                           #    Law 11 until a second consumer appears.
│       │                           # 📋 CustomizeMenu, StationServicesMenu, StorageMenu,
│       │                           #    ModulesMenu, BuildMenu, RefactorMenu (§12.9-12.12) —
│       │                           #    the docked "meso loop" menus, ported from StarReach2
│       │
│       └── planet/          # 🧊 YAGNI BOUNDARY — DO NOT BUILD
│                            #    Not present on disk. Keeping core/ mode-agnostic is a
│                            #    design constraint, not a folder to pre-create.
│
├── CMakeLists.txt           # ✅ Layer targets (§2.1)
├── CMakePresets.json        # ✅ debug | release | asan
├── vcpkg.json               # ✅ EnTT, nlohmann_json, raylib, Catch2 (ENet deferred)
└── .clang-format            # ✅ Was an EMPTY file; now a real config
```

---

## 4. The System Inventory ✅

Previous versions of this document named three systems (`CombatSystem`, `NpcAiSystem`, `WorldGen`).
StarReach2 actually needed about twenty. **The gap between those numbers is where 12,947 lines
went** — work with nowhere named to go lands in the orchestrator.

So the systems are enumerated up front. This list is derived from what the legacy game actually
does, not from guesswork.

| System | Responsibility | Tier | Status |
|---|---|:---:|:---:|
| `HierarchySystem` | One pass: parent → child `WorldTransform` propagation (Law 4) | 1 | ✅ |
| `PhysicsSystem` | Thrust, mass, momentum, drag, angular acceleration | 1 | ✅ |
| `OrbitSystem` | Planet/moon orbits, sun gravity wells, deterministic fast-forward | 1–2 | ✅ |
| `CollisionSystem` | Broad-phase spatial grid + narrow-phase convex hull | 1 | ✅ |
| `ProjectileSystem` | Advance, expire, cull | 1 | ✅ |
| `DamageSystem` | Shield typing, bypass, localized hardpoint destruction | 1 | ✅ |
| `WeaponSystem` | Fire control, cooldowns, charge/burst/spread modes | 1 | ✅ |
| `TargetingSystem` | Target acquisition, aim-point selection per rig type | 1 | ✅ |
| `PowerSystem` | Power budget, load shedding, throttle gating | 1 | ✅ |
| `NpcAiSystem` | Steering, state machine (Patrol/Chase/Attack/Flee/Escort) | 1 | ✅ |
| `SpawnSystem` | Safe spawn placement, culling, respawn-around-anchor | 1 | ✅ |
| `PartySystem` | Escort formations, retaliation propagation, party warp | 1 | ✅ |
| `LootSystem` | Drops, material salvage, derelict wrecks, pickup radius | 1 | ✅ |
| `MiningSystem` | Asteroid depletion, station mining ticks | 1–2 | ✅ |
| `DockingSystem` | Proximity prompts, dock/undock, seated turrets, capture | 1 | ✅ |
| `WarpSystem` | Local warp, system warp, galaxy warp, registry handoff | 1 | ✅ |
| `CommsSystem` | Hail, dialogue lines, message log | 1 | ✅ |
| `ContractSystem` | Contract lifecycle: accept, tick, complete, fail | 1–2 | ✅ |
| `DistressSystem` | Distress call generation and response | 1–2 | ✅ |
| `TutorialSystem` | Step gating and advancement | 1 | ✅ |
| `FactionEconomySystem` | Production, stock, spending, station rebuild/upgrade | 2–3 | ✅ |
| `FactionDecisionSystem` | The 4-facet engine, colonization, raid dispatch | 3 | ✅ *(as `core/ai/FactionDecisionEngine`, see §3)* |
| `DiscoverySystem` | Sensor intel, system discovery, shared knowledge | 2–3 | ✅ |
| `ResearchSystem` | Reverse-engineering jobs: cost, progress, unlock into a network | 1–2 | ✅ *(scheduled; see §13.1 for wiring gaps)* |
| `CommanderSystem` | AI sub-commander standing orders, fleet dispatch, death | 1 / 2–3 | ✅ *(scheduled; see §13.1 for wiring gaps)* |
| `TemplateMarketSystem` | Template pitch, valuation, negotiation roll, royalty accrual | 2–3 | ✅ *(scheduled; see §13.1 for wiring gaps)* |
| `StationServicesSystem` | Buy/sell modules & materials, hull repair, module merge (§12.10) | 1 | ✅ *(scheduled; see §13.1 for wiring gaps)* |
| `ModuleEquipSystem` | Mount/unmount modules onto an already-live rig (§12.11) | 1 | ✅ *(scheduled; see §13.1 for wiring gaps)* |
| `ConstructionSystem` | Player-initiated station/ship build via `StationFactory`/`RigFactory` (§12.12) | 1 | ✅ |
| `EngineerSystem` | Merge two owned same-kind modules into one, level-scaled loss (§12.12) | 1 | ✅ |
| `RefactorSystem` | Delete a live hardpoint, returning its modules to storage (§12.12) | 1 | ✅ |
| `ManufacturingSystem` | Queued module/shell/craft production; the consumer research has lacked (§12.18) | 1–2 | 📋 |
| `HazardSystem` | Environmental `PendingDamage` from hazard volumes — a star's corona today; nebulae, radiation belts and minefields on the same shape (§12.28) | 1 | 📋 |

**Thirty-one of these thirty-three rows are ✅.** Thirty are `SystemSchedule.cpp`'s exact schedule
(§13.1 audits all thirty against `src/`); the thirty-first, `FactionDecisionSystem`, is built as
`core/ai/FactionDecisionEngine` with its own entry point rather than a `TickSchedule()` slot (§13.2
— and §13.3 L records that nothing calls it yet, a wiring gap rather than a build gap). Each landed
as its own GitHub issue (§11.9 tracked the few cross-issue dependencies among them). Only
`ManufacturingSystem` and `HazardSystem` remain 📋 — designed (§12.18, §12.28) but not yet written.

*(§14.1 corrects five of those thirty-one: `ResearchSystem`, `CommanderSystem`,
`TemplateMarketSystem`, `StationServicesSystem` and `ModuleEquipSystem` were still marked 📋 in this
table until this pass, even though they were already scheduled when §13 was written the day
before.)*

**They landed in three passes.** `ResearchSystem`, `CommanderSystem` and `TemplateMarketSystem` came
from the §12 pass that gave `features.md`'s design sections architecture homes.
`StationServicesSystem`, `ModuleEquipSystem`, `ConstructionSystem`, `EngineerSystem` and
`RefactorSystem` (§12.10–§12.12) came from a second pass surveying `../StarReach2`'s menu files
directly — they back UI that was never a numbered `features.md` section but existed as working code
and as components already built anticipating them (`CargoHold`'s doc comment names `StorageMenu` as
a future consumer by name). `ManufacturingSystem` and `HazardSystem` (§12.18, §12.28) are the third
pass, and remain the only rows still to write. They are all listed here rather than left to be
discovered because that is this section's entire purpose — §4 exists because naming only three
systems is how 12,947 lines ended up in an orchestrator. Two further design sections deliberately do
**not** appear as new systems:

- **Knowledge networks** (`features.md` §2.5) are a `core/` store, not a ticking system — nothing
  about them needs to run per frame. See §12.1.
- **Faction survival** (`features.md` §5.1) is a new entry point on the existing
  `core/ai/FactionDecisionEngine`, not a system of its own. See §12.3.

**Tier** refers to the LOD model in `features.md` §1. Tier 1 runs in the active system's registry
at 60 Hz; Tier 2 in neighbor registries at ~5 Hz; Tier 3 against `core/galaxy/` on the macro tick.
A system spanning tiers exposes separate entry points (`Tick`, `TickCoarse`) — never one function
branching on tier.

---

## 5. Subsystem Standards

**Fixed Timestep Simulation** ✅ — gameplay ticks at a fixed 60 Hz via a time accumulator
(`core/time/FixedTimestep.h`). Rendering interpolates. Determinism must hold regardless of refresh
rate or frame spikes, because Law 2's coarse-tick catch-up depends on it.

**Unified Serialization** ✅ — one pipeline (`core/serialization/`: `BlueprintSerialization`,
`ByteStream`, `SaveFile`) writes `.sav` files and is shaped to pack wire packets once `net/`
un-defers. It operates on **blueprint form only** (Law 3). Entity handles are never serialized.
StarReach2's `ByteWriter`/`ByteReader` ported directly as `ByteStream`.

**Save Schema Migration** ✅ — saves carry a `schemaVersion` header, processed through
`SaveMigrator` (`core/serialization/`) to convert older layouts forward.

**Memory Pooling** 🧊 — object pools for projectiles and particles. **Do not build until profiling
shows allocation stalls.** EnTT's storage already gives contiguous iteration; this is a second
optimization on top of one that may already suffice.

---

## 6. Asset Pipeline 🧊

**All of this is deferred.** It is the largest block of speculative framework in the project, and
every piece of it will be subtly wrong until a real feature exercises it.

Ship raylib `LoadTexture` calls behind a thin `engine/assets/` seam, and revisit when the seam
hurts:

- **Texture atlases & compositing** 🧊 — bake static rig layers into spritesheets at runtime;
  dynamic parts (turrets, exhaust) stay separate draw calls.
- **UUID / hashed asset IDs** 🧊 — no hardcoded paths in gameplay code. The *seam* enforces this
  now; the ID system can come later.
- **Audio banks** 🧊 — chunked, scene-context loading.
- **Hot-reloading** 🧊 — filesystem watch on `data/`. Genuinely valuable, and worth nothing until
  there is content to reload.

---

## 7. Build & Toolchain

**Package manager** — vcpkg in manifest mode (`vcpkg.json`), for identical environments across
Windows, macOS, and Linux.
**Build system** — CMake with the Ninja generator and precompiled headers.
**Dependency automation** 🧊 — Renovate Bot monitoring `vcpkg.json` with pinned versions.

### Building ✅

`CMakePresets.json` supplies the vcpkg toolchain file, so CI, IDEs, and contributors share one
configuration. **`VCPKG_ROOT` must be set** — the preset reads it.

```bash
export VCPKG_ROOT=/path/to/vcpkg      # $env:VCPKG_ROOT on PowerShell
cmake --preset release                 # presets: debug | release | asan
cmake --build --preset release
ctest --preset release
```

Run: `.\build\release\bin\StarReach.exe` (Windows) or `./build/release/bin/StarReach`.

Before pushing, run the same structural checks CI runs — they take under a second:

```bash
python tools/ci/check_all.py
```

**Windows note.** The presets use the Ninja generator, which builds with whatever compiler is
first on `PATH`. If MSYS2/MinGW is installed, CMake will pick its `g++` and produce an ABI
mismatch against vcpkg's `x64-windows` (MSVC) triplet — the configure log says
`GNU 13.1.0` instead of `MSVC`. Configure from a Developer Command Prompt, or set `CC=cl` and
`CXX=cl`. The presets deliberately do not pin a compiler, because doing so would break Linux and
macOS.

---

## 8. CI/CD

### ✅ Current state: CI is running

`.github/build.yaml` sat one directory too high — GitHub Actions reads only
`.github/workflows/*.yml|*.yaml`, so CI was silently off. Moving that one file turned it on.

| Job | Status | Purpose |
|---|:---:|---|
| **Structural enforcement** | ✅ | §2.2–2.4 — size caps, layer rules, content pipeline, empty scaffolding. Four Python scripts, seconds each. **Gates the matrix build.** |
| **Format** | ✅ | `.clang-format`; unformatted PRs fail |
| **Matrix build** | ✅ | `windows-latest` (MSVC), `ubuntu-latest` (GCC), `macos-latest` (Clang) |
| **Tests** | ✅ | Catch2 via `ctest --preset release` |
| **Content validation** | ✅ | Every `data/base_game/` blueprint must pass `Validate()` — runs inside the test suite, so unbuildable authored data fails CI rather than a player's session |
| **`.clang-tidy`** | ✅ | Configured (`.clang-tidy`), its own job in `build.yaml`, gates `main` |
| **ASan** | ✅ | The `asan` preset is wired into its own CI job |

The enforcement job runs **first and independently of any compiler**, so a PR that grows a god
object fails in under a minute rather than after three platform builds.

**One deliberate removal:** the old workflow's final step ran `StarReach.exe` as a smoke test.
The executable now opens a window and the runners are headless, so that step is gone. Coverage of
the same startup path lives in `tests/integration/ContentTests.cpp`, which loads and validates the
identical `data/base_game/` files `main()` does.

### Release 🧊

Universal macOS binaries (`-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`), `.app` bundle packaging,
codesign + notarytool, and `steamcmd` deployment to the Steamworks internal branch on release
tags. All deferred until there is a game to ship.

---

## 9. Legacy Migration Plan 📋

**Strategy: port the clean parts, rewrite the rest.** `../StarReach2` is an executable
specification — 39,000 lines of debugged behavior, tuning constants, and solved edge cases. A
rewrite that ignores it re-debugs problems already solved once.

The dividing line is empirical: **subsystems that were already successfully extracted into their
own files port well. Everything that lived inside the god object gets rewritten.**

| Legacy artifact | Lines | Disposition |
|---|---:|---|
| `config/*.json` | — | **Port as-is** → `data/base_game/` |
| `data/registry/*` (15 JSON-backed) | — | **Port**, light adaptation → `core/registries/` |
| `systems/CollisionHull.cpp` | 221 | **Port** — already standalone and clean |
| `systems/HostileTargeting.cpp` | — | **Port** → `TargetingSystem` |
| `systems/DockRepair.cpp` | — | **Port** → `DockingSystem` |
| `systems/SpatialGrid.h` | — | **Port** → `CollisionSystem` broad-phase |
| `systems/WorldGen.cpp` | 384 | **Port**, adapt to per-system registry |
| `net/Protocol.h` — `ByteWriter`/`ByteReader` | ~90 | **Port** → `core/serialization/` |
| `net/Protocol.h` — 4 hardpoint snapshots | ~130 | **Rewrite as one** (Law 4) |
| `core/SaveManager.cpp` | 682 | **Rewrite** — must become blueprint-based with `schemaVersion` |
| `shared/entities/Hardpoint.h` | 1,146 | **Split**: blueprint struct → `shared/blueprints/`; the ~10 free functions become `PowerSystem`, `DamageSystem`, `PhysicsSystem` |
| `data/modules/*Defs.h` | 883 | **Delete.** Migrate content to JSON (Law 10) |
| `MainMenu.cpp` | 1,249 | **Port**, decompose to the 600-line cap |
| `GalaxyMap.cpp` | 1,556 | **Port**, adapt to `core/galaxy/` |
| Menu files (`CustomizeMenu`, `StationServicesMenu`, `StationModuleMenu`, …) | ~5,000 | **Port view/layout code; rewrite state ownership** as intent-emitting views (Law 9) |
| `SpaceFlight.h` | 1,287 | **Rewrite.** The 466 members become components and `core/` records |
| `SpaceFlight.cpp` | 12,947 | **Rewrite** as the §4 systems. Read it for behavior; copy none of its structure |

**Porting rule:** a ported file is not ported until it satisfies the size caps and layer rules. Copy
it in, then decompose it, in the same commit. A file copied across and left oversized is how the
first project started.

---

## 10. First Vertical Slice ✅

**Target: fly one ship with three hardpoint entities, engage one NPC, and destroy a specific
hardpoint with targeted fire.**

This slice exists to exercise the two decisions most expensive to get wrong — Law 3's
blueprint→factory→entity pipeline and Law 4's uniform rig — and it targets exactly what StarReach2
got wrong four separate ways in its wire protocol.

**In scope:**

1. Layer CMake targets (§2.1) and the size-cap CI job (§2.2). *These come first; they are what
   makes everything after them stick.*
2. raylib in `vcpkg.json`; `CMakePresets.json`; move `build.yaml` into `workflows/`.
3. One `SystemWorld` owning one `entt::registry`.
4. `ShipBlueprint` + `RigBlueprint` JSON in `data/base_game/`, parsed by `core/registries/`.
5. `RigFactory`: blueprint → parent entity + N hardpoint entities.
6. `HierarchySystem`, `PhysicsSystem`, `WeaponSystem`, `ProjectileSystem`, `DamageSystem`,
   `TargetingSystem`.
7. One NPC from `NpcFactory`, minimal `NpcAiSystem` (approach and fire).
8. Minimal renderer: ship sprite, hardpoint layers, projectiles.
9. Unit tests: blueprint validation, damage/shield-bypass resolution, hierarchy propagation.

**Explicitly out of scope:** multiplayer, saves, warp, factions, economy, stations, mining,
contracts, tutorial, comms, atlases, hot-reload, object pools, audio.

**The slice succeeds when** destroying the weapon hardpoint disables that firing arc, destroying
the engine hardpoint stalls the ship, and neither outcome required a special case for what kind of
craft it was attached to.

**Items 1–9 are done.** `SystemWorld`, `ShipBlueprint`/`RigBlueprint` JSON, `RigFactory`,
`HierarchySystem`/`PhysicsSystem`/`WeaponSystem`/`ProjectileSystem`/`DamageSystem`/
`TargetingSystem`, `NpcFactory`/`NpcAiSystem`, `WorldRenderer`, and unit coverage for blueprint
validation, damage/shield-bypass, and hierarchy propagation all exist and pass
(`ctest --preset release`, and the full suite via `sr_tests.exe`: 281 test cases / 690 assertions
as of 2026-07-28). The "slice succeeds when" behavioral claim above is backed by those unit tests
but **has not yet been confirmed in an actual play session** — that verification is still open,
and per the project's build/verify split it is the kind of check the user runs, not something
confirmed by a build log.

All remaining §4 systems, `modes/space/ui/`, `shared/ui/`, `modes/main_menu/`, and unified
serialization have since landed too (§0's "Current reality" summary). What's left is only what
§3/§5/§6 mark 🧊.

---

## 11. Conventions ✅

*Read this before writing code. The laws above say what the architecture is; this section says
what to type. It is written for contributors who arrive with no memory of the previous
conversation — including AI agents working one system at a time.*

### 11.1 The five-minute version

| You want to… | Do this |
|---|---|
| Add a simulation behavior | New file in `modes/space/systems/`, free functions, register in `SystemSchedule.cpp` **same commit** |
| Add a stat, weapon, ship, module | Edit `data/base_game/*.json`. **Never** write a C++ definition |
| Add per-entity state | New POD struct in `shared/components/`. No methods, no owning pointers |
| Add galaxy-wide state | `core/`. Not a component, not a mode member |
| Build a composite object | `modes/space/factories/`. Systems never construct rigs |
| Respond to a click | Push an Intent. UI never mutates state |
| Add a helper | Put it in the mode. Promote to `shared/` when a **second** consumer appears |

### 11.2 Naming and layout

- **One namespace per layer:** `sr::` (shared), `sr::core::`, `sr::engine::`, `sr::space::`.
  Components and blueprints are in bare `sr::` because they are the shared vocabulary.
- **Includes are always project-root-relative and quoted:** `#include "core/events/Intent.h"`.
  Never relative (`"../data/SystemWorld.h"`). There is exactly one include root.
- **Files are `PascalCase.h/.cpp`** and match the primary type they declare.
- **Constants are `kCamelCase`**, members are `trailingUnderscore_`, free functions are
  `PascalCase`.
- **No `utility/`, `helpers/`, `common/`, `misc/`** (Law 11). Scope by technical purpose:
  `core/math/`, `core/string/`.

### 11.3 Adding a system — the exact recipe

This is the most common task in the project, so it is spelled out completely.

1. Create `modes/space/systems/YourSystem.h` and `.cpp`.
2. Declare **free functions in a snake_case namespace** — not a class:
   ```cpp
   namespace sr::space::your_system {
   void Tick(const SystemContext& ctx);
   }
   ```
   A class invites members, and a member in a system is per-frame state that belongs in a
   component where other systems can see it. The one legitimate cache — a collision broad-phase
   grid — lives as a component on a singleton entity, not as a private field.
3. Take **only** `SystemContext` (`modes/space/systems/System.h`). It gives you the world, the
   tick's intents, read-only content, `dt`, and the tick count. It deliberately does **not** give
   you a pointer to `SpaceFlight`. That omission is Law 6 made structural: you cannot reach for
   mode state, because there is nothing to reach for.
4. Add the entry to `TickSchedule()` in `SystemSchedule.cpp` **in the same commit** (§2.4). Honor
   the ordering rules documented there: `HierarchySystem` first, `PowerSystem` before anything it
   gates, `DamageSystem` last.
5. Add a unit test. Systems taking a `SystemContext` are testable against a bare `SystemWorld`
   with no window and no content file.
6. Run `python tools/ci/check_all.py`.

**A system spanning LOD tiers exposes a second entry point** (`TickCoarse`), never one function
branching on tier. Tier 2 genuinely has different data available — no projectiles, no physics —
so a single branching function always ends up reading state that does not exist.

### 11.4 Components

- **POD only.** No virtual methods, no inheritance, no owning pointers, no constructors beyond
  aggregate initialization.
- **No `std::vector` where a child entity would do** (Law 4). The sole exception is `Rig::children`,
  which *is* the list of child entities.
- **No `entt::entity` in anything persistent.** Handles are registry-local and invalidated by
  travel. Saves, wire packets, Templates, and Intents use `NetworkId` or a stable string id.
- **Document the writer.** If exactly one system may write a component, say so in its comment —
  `WorldTransform` names `PhysicsSystem` and `HierarchySystem`. This is the cheapest possible
  defense against the component that everything writes and nothing owns.

### 11.5 Content

All authored content is JSON in `data/base_game/`, parsed by `core/registries/`. `ModuleDef`,
`ShellDef`, `ShipBlueprint`, `RigBlueprint`, and `MountBlueprint` may only be *constructed* in
`core/registries/`, `tests/`, and `tools/` — CI enforces this.

Blueprint validation (`shared/blueprints/Validation.h`) runs **before instantiation**, so an
invalid design can never become a live entity and a corrupt save cannot inject one. It reports
every violated rule, not the first, and every message is specific enough to show in the
Engineering UI verbatim.

*Two rules beyond features.md §2.3's nine were added because the content model implies them:*
`MountCapacity` (module count within the shell's slot count) and `ModuleCompatibility` (module
kind legal for the shell kind). Both are mechanical consequences rather than design choices, and
catching them in validation is what keeps `RigFactory` free of defensive branches.

*One reinterpretation:* features.md rules 6 and 8 overlapped — both described "the shell exists".
Rule 6 is implemented as **structural** integrity (every `attachedTo` resolves; exactly one root)
and rule 8 as **registry** resolution. Read together they are non-redundant; read literally they
were the same check twice.

### 11.6 What breaks the build

| Check | Fails on |
|---|---|
| `check_sizes.py` | File > 600 lines · function > 80 · mode class > 25 members |
| `check_layers.py` | Any include crossing a layer boundary the wrong way |
| `check_content_pipeline.py` | Constructing a content type outside registries/tests/tools |
| `check_dead_dirs.py` | A directory under `src/` with no source in it |
| `clang-format --Werror` | Unformatted code |
| Link | An illegal *used* dependency, e.g. raylib from `sr_core` |
| `ctest` | Any authored blueprint that fails validation |

Run all of it locally with `python tools/ci/check_all.py`. **That script does not run
clang-format** — it only covers the four structural Python checks. Format separately (§11.8) or a
correctly-built, correctly-layered PR will still fail CI on whitespace.

### 11.7 When a rule is wrong

**Change the rule, in the same PR, with the reasoning written down.** §2.1's original layer graph
was cyclic and unbuildable; the fix is documented above rather than worked around. A check that
gets bypassed with a comment is a check that has stopped existing — which is the entire failure
mode this document was written to prevent.

### 11.8 Task workflow

**One branch per task, verified locally against exactly what CI runs, then stop for review.** This
applies to every contributor, human or agent — an agent working a task from this document's §10
list follows it without being asked each time.

1. `git checkout main && git pull origin main` — start from the tip of `main`, not from whatever
   the previous task left checked out. Two agents working the same feature name in parallel without
   this step is exactly how a branch can end up duplicating work that already merged.
2. `git checkout -b <descriptive-branch-name>` — a new branch per task, never reusing one from an
   earlier task or stacking unrelated work onto it.
3. Implement the task.
4. Run, locally, the same checks `.github/workflows/build.yaml` runs, in this order:
   - `python tools/ci/check_all.py` — the four structural jobs (§2.2–2.4).
   - `clang-format --dry-run --Werror` over every changed `.h`/`.cpp`, using **the same
     clang-format version CI uses** (currently 18.1.3; check the `format` job in
     `build.yaml` if that drifts). MSVC's `clang-format.exe` is frequently a different version and
     will pass locally while failing CI — if no matching binary is on `PATH`, install one with
     `pip install clang-format==<version>` rather than trusting whatever is already installed.
   - `cmake --preset release` (first time or after a `CMakeLists.txt` change) then
     `cmake --build --preset release`. **Windows note:** run this from a Developer Command Prompt,
     or `vcvars64.bat` first — see §7's Windows note; without it, MSVC's own standard headers
     (`<string>`, `<filesystem>`, …) fail to resolve, which looks like a broken environment but is
     really just a missing `INCLUDE` path.
   - `ctest --preset release --output-on-failure`.
5. **Do not commit.** Leave the branch's changes staged in the working tree and report what
   changed and what the checks showed. The user reviews the diff and decides whether it lands —
   committing, amending, or discarding is their call, not an automatic next step.

A task whose checks fail locally is not done — fix it before handing back, the same way a red CI
run would not be handed back as finished.

### 11.9 Cross-issue dependencies

Most work in §3/§4/§5 is independent — pick an issue, build it standalone (a legacy port or a fresh
system against a bare `SystemWorld`), no other issue needs to land first. A few issues say
otherwise directly in their body; when one does, add a row here so a contributor picking a task
does not have to re-read every open issue to notice it.

The §12 work is the first batch since the §4 inventory to have real ordering. `KnowledgeNetwork` is
the load-bearing one — three separate features read or write it, and building any of them against a
store that does not exist means inventing a throwaway one:

| Task | Depends on | Why |
|---|---|---|
| **Everything below** | **Wiring the game loop** (§0's 🚨 block, sequenced in §12.24) | Pressing Start Game produces an empty world with no player in it. A new system built before this cannot be reached, exercised, or judged. *Not a hard dependency — a priority one.* |
| §12.24 step 5 — the docked-menu router | §12.24 steps 1–4 (the micro loop) | The router keys on `Docked`, which needs a player who can fly to a station `WorldGen` does not yet spawn |
| §12.24 step 5's `EngineerMenu`/`RefactorMenu` tabs | The `BridgeView::kAllKinds` fix (§12.24 🐛) | `FacilityKind::Engineering` is absent from the tab list, so both menus are unreachable through the router itself |
| Moving `ConsumeSaveTemplateRequests` out of `CustomizeMenu` (§12.24 🐛) | §12.24 step 6 — the five `SystemContext` pointers | Its new home is a system, which reads `ctx.knowledge`; that pointer is `nullptr` today |
| **Shipping `ModulesMenu` / live refit** (`features.md` §2.7) | §12.23's `RecomputeRigTotals` | `ModuleEquipSystem` does not recompute `BodyMass`/`Propulsion` on mount or unmount. Live refit is now sanctioned combat play, so a swap that does not change how the hull flies is the mechanic broken |
| §12.25 — the `mobile` movement gate | §12.23's `RecomputeRigTotals` | Always emplacing `Propulsion` only helps if something recomputes it when engines are mounted or die |
| §12.27 — local command | §12.24 steps 1–4 · §12.25 · §12.26 | Needs a player who can fly; **Move** availability reads `Propulsion` (§12.25) and **Build** reads a Construction facility (§12.26) |
| **Strategic command** (`features.md` §4.2) | §12.17 Galaxy Topology · §12.27 | "Dispatch that fleet there" has no *there* until systems carry galactic coordinates |
| `ManufacturingSystem` (§12.18) | §12.19 Item Model **and** the Element/Material content set | Needs `ItemId`/`ItemKind`, and needs something to consume. `data/base_game/` has no `elements.json` or `materials.json`. **It is also where `features.md` §2.10's attribute propagation is computed**, so the whole material-attribute model is invisible in play until it exists |
| Per-item faction stock (§12.20) | §12.19 Item Model | The ledger is keyed on `ItemId`, which does not exist |
| Stat pools (§12.21) | §12.19 Item Model | A `Quality` roll is stored per instance alongside the item's identity; both land together |
| §12.19 Item Model | **Element and Material content** *(authoring, not an issue)* | Mass and price derive from recipes. With no element set there is nothing to derive from. `features.md` §2.10 now specifies the roster, the eight attributes, and `tools/element_check` as its gate |
| Navigation map levels 1–2 (§12.6) | §12.17 Galaxy Topology | Systems carry no galactic coordinate, so there is nothing to lay out |
| The `std::hash` warp fix (§12.15 🐛) | §12.17 Galaxy Topology | The placeholder exists *because* systems have no coordinate to seed from |
| ECM module (§2.11) | Fog of war migration (`features.md` §8.3) | Suppressing sensor coverage means nothing until coverage is per-viewer |
| Cloak / stealth (§2.11) · ECM · §8.3's per-viewer fog · §2.10's Optical and Semiconductive attributes | **A signature/detection model** — *agreed in principle 2026-08-09 (`features.md` §8.3), not yet specified* | Sensors carry only a range, hardcoded to 2000; there is nothing to detect *against*. This is a system, not a module. Four separate features now wait on it, including §2.9's "run silent," which promises a mechanic that does not exist |
| **Material attributes reaching the game at all** (`features.md` §2.10) | **`ManufacturingSystem` (§12.18)** | Material → craft → module attribute propagation has to be *computed* somewhere, and §12.18 is that somewhere. It does not exist, and is itself blocked on the item model and the materials content set. **Not circular** — content lands first — but the materials work is invisible in play until §12.18 is built |

**Added 2026-08-09 by §13's wiring audit** — these are dependencies the audit found, not new design:

| Task | Depends on | Why |
|---|---|---|
| §12.24 step 1 — world and player | **§12.28** — world bodies, hittability and the star hazard (§13.3 A, B) | `WorldGen`'s sun, planets and asteroids satisfy neither `WorldRenderer` view, and nothing can shoot an asteroid. Calling `PopulateSystem` from `OnEnter` produces a world the player can neither see nor interact with, so step 2's motion cannot be judged against anything. **§12.28 has no dependency of its own and can land first** |
| §12.24 step 1 — the player rig | `RigFactory` emplacing `CargoHold`/`Wallet` (§13.3 P) | Four systems bail on a null `CargoHold`; only `WarpToSystem` emplaces one today |
| **§12.29 — quit to main menu** | §12.24 step 1 being written **re-entrant** | Returning to the menu and starting a second game calls `OnEnter` twice in one process. Without a `world_` reset it populates on top of the first world — two suns, two players. *The dependency runs backwards: §12.29 imposes a requirement on step 1, so write step 1 with the reset even if §12.29 lands later* |
| §12.24 step 4 — a station to dock at | **§12.25's `LinearDamping` fix** (§13.3 J) | A station carries `BodyMass`+`Velocity` and no damping, so gravity accelerates it without bound — not merely "drags" it |
| §12.24 step 5 — `StationServicesMenu` | `StationFactory` emplacing `CargoHold` (§13.3 O) | Buy and sell both `try_get<CargoHold>(station)`; no station has one, so a routed menu still trades nothing |
| Mining · tutorial · material loot | **§12.28's narrowed `FindHit` view** (§13.3 B) | Nothing in the codebase can damage an asteroid, so all three are unreachable behind one view predicate |
| `EngineerSystem`'s merge scaling | `FacilityStats::level` being parsed and forwarded (§13.3 K) | Neither half of the path exists, so the merge is permanently level 1 |
| Live refit (`features.md` §2.7) | **`MountTraverse`** (§13.3 D) · §12.23's `RecomputeRigTotals` | A runtime-mounted weapon gets a zero-width firing arc and never fires |
| `FactionDecisionEngine` having any caller | **§1.1's coarse tick** (§13.3 L, M) | It is a tested library with no invocation path, and three `TickCoarse` functions have no driver |

**Added 2026-08-10 by §12.30, the docked-screen pass:**

| Task | Depends on | Why |
|---|---|---|
| **The seven docked screens** (§12.30) | **§13.5 group 4a — the widget layer** | There is no list, scroll, focus or hit test in `shared/ui/`, and the one shared row widget already has two verbatim clones. Screens written before the widget layer is the mechanism that produced them |
| The **Market** and **Storage** screens | `StationFactory` emplacing `CargoHold` (§13.3 O) | Unchanged from step 5's row above, and now stronger: under §12.30 the station's `CargoHold` **is** the storage capability, so the component is the feature rather than a precondition for it |
| Meaningful prices on **Market** | `core/economy/Pricing.h` (§13.5 group 2c) | `BuildBuyRequest(module, cost)` takes cost as a *parameter*; nothing anywhere derives one. A finite station `CargoHold` is what finally makes `features.md` §2.10's local scarcity computable |

**Added 2026-08-10 by §12.30.3, the Market/Storage pass:**

| Task | Depends on | Why |
|---|---|---|
| **The Market screen being reachable at all** | **§12.24 step 6 — `ctx.diplomacy`** | `DockingSystem::FindEligibleBay` filters on `FactionRef` **equality** (`DockingSystem.cpp:38`), so you can dock only at your own faction's stations. §12.10's *"docking anywhere, including a station they do not own"* and `features.md` §5.3's six-row docking band table are both unimplementable against an equality test. **Land it with `TargetingSystem`'s relation check (§13.3 N)** — same predicate, opposite direction |
| **Prices the system can be trusted with** | `core/economy/Pricing.h` **and** deleting `cost`/`value` from the requests | Filling the fields in is not the fix. Under Law 9 the request states *what and how many*; the system decides what it costs, calling the same pure function the screen calls to display it |
| **Trading anything that is not a module** | §12.19 `ItemId` · §13.5 group 2b | `BuyItemRequest` carries a `ModuleId` and `ProcessSellRequests` searches only `CargoHold::modules`. **The half `MiningSystem` fills is the half the Market cannot touch.** The two lists also stack inconsistently — `LootSystem` merges a `MaterialStack`, buying pushes a duplicate `ModuleId` per unit — so they become one `std::vector<ItemStack>` in the same pass |
| **The ownership test every screen needs** (*"is this station mine?"*) | The player's `FactionId` moving onto the **player record** (§12.30.3, amending §12.30.1) | Identity is read off the controlled rig root today, and §12.30.1 makes that root the **station** while docked. `DiscoverySystem.cpp:13` and `ConstructionSystem.cpp:15` are the two live readers that would invert. Not reachable until the docking gate widens; lands with it |
| A finite station hold at all | `StationFactory` emplacing `CargoHold` **with a non-zero `capacity`** · `CargoHoldHasRoomFor` gaining its other four callers | An unbounded hold makes `LocalPrice` meaningless — scarcity is only computable against a real local quantity |

**Added 2026-08-10 by §12.30.4, the Repair pass:**

| Task | Depends on | Why |
|---|---|---|
| **NPCs repairing at all** | §12.24 step 6 — **`ctx.economy`** | `HealAndImmobilize` views `<Docked, Rig>` with no player filter, so the free heal *is* the whole "NPCs retreat to a friendly bay" behaviour. Deleting it (§13.4 decision 1) removes NPC repair, and NPCs carry no `Wallet`. The order re-homes onto faction stock, giving `FactionEconomy::Spend` its first Tier 1 consumer |
| **Repair rate meaning anything** | `FacilityStats::ratePerSecond` gaining a reader | Parsed at `BlueprintJson.cpp:38`, merged at `EngineerSystem.cpp:90`, **read by no behaviour anywhere.** Repair is its first reader; `features.md` §2.7's Repair crew role has been listed as ✅ *"Buildable now"* against a consumer that would be deleted |
| **Repairing a station — any station** | This screen's subject selector | The **only two** sites that raise a `Health.current` in `src/` both require `Docked` on the healed rig, and a station is never `Docked`. Every fixed asset in the galaxy decays monotonically with no counterpart |
| The Repair screen's **destroyed rows** pointing anywhere real | **§12.30.5 — rebuild** | `RefactorSystem` deletes a leaf hardpoint; **nothing anywhere adds one back**, in code or design. The blueprint still holds the `MountBlueprint` (§12.31's own observation), so rebuild is the delete inverted — but it does not exist yet |

**Added 2026-08-10 by §12.30.5, the Engineering pass:**

| Task | Depends on | Why |
|---|---|---|
| **Merging being bounded at all** | **§12.21's `Quality`** | `features.md` §2.4 says the band clamp *"requires no code change — it ratifies what is built."* **It is not built.** `MergeField` is `p + s × (level × 0.1)` — additive on raw stats with no ceiling — while §2.4's formula moves a normalised quality against `bandMax`. The two share one factor and nothing else. **Merge is reachable-and-wrong the moment a gate passes**, unlike every other inert-but-correct surface in this batch |
| **The facility level mattering to a merge** | §13.3 K — `ParseFacilityStats` reading `level` **and** `ModuleAttachment.cpp:65` forwarding it | `emplace_or_replace<FacilityRef>(hardpoint, module.facility.kind)` passes the kind alone. **Every merge in the game has preserved exactly 10% of the secondary**, and the facility-level axis §2.4 calls settled has never held a value other than its default |
| Choosing **which** bench, bay or lab you use | `PlayerLocation` naming the hardpoint (§12.30's tab fix) | `DockedEngineeringLevel` returns the **first** living Engineering facility in `Rig::children` order. The tab-entity change was justified solely by §3.4's death predicate; it turns out to be what makes every graded facility addressable |
| **Rebuild** | *nothing for the mechanism* — §12.19 only for the cost | The blueprint still holds the `MountBlueprint`, so rebuild needs no snapshot, no tombstone and no placement validation. Ships with 4b |
| A crafted module surviving a save at all | A **short generated id** (§12.30.5) · the crafted-module save section (§12.31 🐛) | `merged.id` is `primary + "+" + secondary + "@L" + level`, so ids **double in length per merge generation** — and that string is the key `craftedModules_` is stored under |

**Added 2026-08-10 by §12.30.6, the Research pass:**

| Task | Depends on | Why |
|---|---|---|
| **Research being startable at all** | This screen · `StationFacility`'s and `NetworkOwner`'s producers | **Five links, each built and tested, forming a mechanism with no entry point.** `ResearchJob`, `StationFacility` and `NetworkOwner` have **zero producers**; `researchTier` is written only by two lines in `ResearchSystemTests.cpp`; `ctx.knowledge` is `nullptr`. There is no `StartResearchRequest` anywhere. Every `ResearchSystem` test passes because each one builds the job by hand |
| A meaningful **Research** screen | **§12.24 step 6 — `ctx.knowledge`** | The one screen where step 6 is a prerequisite rather than a degradation: no network to grant into, and none to check `ALREADY KNOWN` against |
| `NetworkOwner` · `StationFacility` producers | The same factory pass as §13.3 O and P | `CargoHold`, `Wallet`, `StationFacility`, `SpawnAnchor` and `NetworkOwner` are five components with zero producers in **two** factories. One commit |
| **Research surviving a warp** | **§12.31** | `CollapseResearchJobs`/`PromoteResearchJobs` are built, correct, and **have no callers.** `WarpToSystem` carries `CargoHold`, `Wallet` and `wreckLedger_` and nothing else, so a job in the departing system is destroyed with the world. A fourth thing §12.31's work must actually invoke |
| The `Row::fill` field | *nothing* | Group 4a. Two consumers — the Research and Manufacturing queues |

**Added 2026-08-10 by §12.30.7, the two flight overlays:**

| Task | Depends on | Why |
|---|---|---|
| **Wiring the loadout button at all** | **§13.4 decision 2** | `EquippableMounts` offers every blueprint-mounted hardpoint on a fresh ship as an empty slot, because `EquippedModule` tags only runtime mounts. The first player to refit destroys a module or duplicates one, and it reads as a UI bug. **Reachable-and-wrong, like Engineering's merge — do not route it before decision 2** |
| The loadout overlay meaning anything | §12.23's `RecomputeRigTotals` · §13.3 D's `MountTraverse` | §11.9's existing row, unchanged and now with a screen behind it: a swap that does not change how the hull flies is the mechanic broken, and a runtime-mounted weapon gets a zero-width arc and never fires |
| **A full `CargoHold` having any exit** | **Jettison** (§12.30.7) | Once `CargoHoldHasRoomFor` gains its other four callers (§12.30.3), a full hold refuses loot, refunds and buys — and unmount, scrap and deconstruct all *fill* it. Selling at a `Trade` station is the only drain in the game. Jettison also gives **`LootDrop`/`MaterialDrop` their first producer** (§13.3 T) |
| §3.9's status projection knowing it has a consumer | *nothing* — it is an addition, not a prerequisite | The loadout overlay is §3.9's **fourth** use and its most spatial one. When group 2e lands, the projection becomes a second selector over the *same* selection state and the same pure hit-test — so nobody should build a second hardpoint-selection model for it |
| The **Bay** screen's *"ask to purchase"* row | §12.19 Item Model | `BuyItemRequest` carries a `ModuleId`; buying a **vessel** needs `ItemId`/`ItemKind` |
| The **Bay** screen's *"ask to escort"* row | §12.27 local command | It is a legitimate UI producer for `PartySystem`, whose `PartyLeader`/`PartyMember` have zero producers — the `AvionicsMenu` → `DockRequest` shape, not the invented-producer §13.5 warns against |
| The `Storage` → `Trade` enum swap | *nothing* — but it touches six sites, two of them silent defaults (§12.30) | Cheapest now: `data/base_game/` authors no `storage` facility, so there is no content to migrate |
| **Parked hulls** (§12.30.2) · warp damage persistence · cross-system refit · §13.3 Y's world save | **`RigState`** — §12.31 (finding §13.3 AC) | `WarpToSystem` re-spawns the player from `BlueprintRef` and destroys everything else in the departing world. **Four features wait on one missing capability** |
| **§12.31 `RigState`** | §13.4 decision 2 (`MountedModules` unification) · §12.21's `Quality` type | `MountState::modules` is **one** list, and the codebase has two unreconciled answers to what is in a mount (§13.3 C). A snapshot cannot be written against both — this is the argument that makes decision 2 load-bearing rather than tidy |
| **§13.3 Y's world save** | **§12.31** | `RigState` is the player-rig half of a save that today has *"no registry serialization, no player rig state."* It has four callers instead of one, so it lands first and alone |
| Merged modules surviving a save **or a warp** | A crafted-module save section (§12.31 🐛) | `ContentLibrary::craftedModules_` is an in-memory map written by `EngineerSystem` and **serialized by nothing**. A restored hull would reference a `ModuleId` that no longer resolves |
| The Bay screen's *"crew it"* option (§12.30.2) | **`ModuleKind::Crew`** (§13.3 Z) | Zero occurrences of "crew" anywhere in `src/`. *Board* ships without it; delegating a parked hull to an NPC pilot does not |
| The Bay screen's escort roll (§12.30.2) | §12.24 step 6 **and** §12.27 | The roll weights on `ctx.diplomacy` and `ctx.reputation`, both `nullptr`. A refusal model consulting `nullptr` refuses everything unconditionally — `TemplateMarketSystem`'s exact current failure |
| **§12.24 step 1 — the player** | **§12.30.1** — `PlayerLocation` as sole source of truth | *The dependency runs backwards, like §12.29's.* Step 1 plans `emplace<PlayerControlled>`; it must emplace `PlayerLocation` instead and let the tag derive. **One write site exists today** (`SpaceFlight.cpp:120`) against twenty-nine readers — write step 1 this way even though §12.30 lands later |

**Startable today, nothing blocking:** **§12.24 steps 1–4, the micro loop — the one to take first**
· §12.24 step 6's five `SystemContext` pointers · the `BridgeView::kAllKinds` fix · §12.17 Galaxy
Topology (and it unblocks the most) · §12.22's shield coverage fix · §12.22's collision type B and
structural cascade · §12.23's aggregation rule and `RecomputeRigTotals` · the
`Sensor`/`CargoBay`/`FireControl` kinds · the fog-of-war migration (`DiscoveryState` →
`KnowledgeNetwork`) · the one-line duplicate `DrawWorld` call in `SpaceFlight::Draw` · **all of
§13.5's group 2** (the `HierarchySystem` reorder, the `Docked` exclusions, `PowerShed`'s missing
reader, `FiringArc::currentOffset`, the facility-level parse, and the two content fixes).

**Deleted 2026-08-08:** the eight rows this table previously held. `core/knowledge/KnowledgeNetwork`,
`ResearchSystem`, `CommanderSystem`, and `TemplateMarketSystem` have all merged, so every dependency
they named is satisfied — per this section's own rule, a satisfied row is deleted rather than
marked done.

**Before starting a task, check this table.** If it names a dependency, confirm that dependency has
actually **merged to `main`** — an open PR is not enough, since the branch you cut in §11.8 step 1
still would not have the code. If the dependency has not merged, stop and pick a different task
rather than implementing against code that does not exist yet; the alternative is a branch that
cannot build, or one that silently re-implements what the dependency was going to supply.

These are prose notes in each issue body, not GitHub's native tracked-dependency links (an issue's
`blocked_by`/`blocking` count is 0 for all of them as of this writing) — this table is what makes
them enforceable in practice. Maintain it by hand: add a row in the same PR that notices a new
dependency, and delete the row (not mark it done) once its target merges — an empty table is the
signal that everything open is independently startable again.

---

## 12. Implementing The 📋 Design Sections

`features.md` says **what** the game does. This section says **where the code goes**, and it exists
because the two were written months apart: the design sections landed as agreed design with no
architectural counterpart, which meant a contributor picking one up had a specification of behaviour
and no answer to "which directory, which layer, which system."

Every subsection below gives the same five things, because those are what §11.3's recipe actually
needs: **home**, **types**, **systems**, **persistence**, **tests**. Where a real design question is
still open it is marked ❓ and left open — those are decisions for the project owner, and guessing
them here would bury a choice inside an architecture note where nobody would look for it.

**None of this is built.** Everything in §12 is 📋.

### 12.1 Knowledge Networks — `features.md` §2.5

**Home:** `core/knowledge/KnowledgeNetwork.h/.cpp` (new directory, `sr_core`).

**Why `core/` and not a component — this is forced, not chosen.** Two existing laws decide it
between them:

- Law 8 puts galaxy-wide, mode-agnostic state in `core/`. A faction's network has to be readable on
  the Tier 3 macro tick, where `features.md` §1.1's boundary rule says *no registry exists at all*.
  A component is therefore unreadable exactly when the faction simulation needs it.
- Law 2's first hard rule says entity handles never cross a system boundary, a save file, or the
  wire. A network outlives any particular registry — it survives the player warping away, and it
  survives the death of the ship that was carrying its owner.

**The apparent contradiction, resolved.** `features.md` §4.5 says each sub-commander *holds* a
network, which sounds like per-entity data. It is not: the commander **entity** holds a stable
`NetworkId`, and the network itself lives in `core/`. This is the same shape Law 3 already uses for
blueprints — the volatile thing points at the durable thing, never the reverse.

```
shared/components/NetworkOwner.h   POD: { NetworkId network; }   <- on the entity
core/knowledge/KnowledgeNetwork    the store, keyed by NetworkId <- the actual data
```

**Types:**

| Type | Notes |
|---|---|
| `NetworkId` | Stable integer or string id. **Not** an `entt::entity` (Law 2, §11.4) |
| `NetworkOwnerKind` | `Player`, `Commander`, `Faction` — the three owners in `features.md` §2.5 |
| `KnowledgeNetwork` | Holds unlocked blueprint ids, saved Templates, discovered system ids |
| `KnowledgeStore` | Owns all networks; `Get`, `Grant`, `Copy`, `Create`, `Destroy` |

**Systems:** none. This is a store, not a ticking system — nothing about it needs to run per frame.
Its writers are `ResearchSystem` (grants an unlock), `TemplateMarketSystem` (copies on sale), and
`DiscoverySystem` (already built; would grant discovered systems). Name the writers in the header
comment, per §11.4's "document the writer" rule — a store that everything writes and nothing owns is
exactly what that rule exists to prevent.

**Persistence:** this is the part with real consequences. Networks are save state, so
`core/serialization/SaveFile` gains a networks section and `schemaVersion` **must** be bumped with a
matching `SaveMigrator` step. Old saves have no networks; the migration decides what an existing
save's player starts with. Serialize blueprint **ids**, never blueprint bodies — the content library
already owns those, and duplicating them into saves would break Law 10's single content pipeline the
moment a JSON stat changes.

**Tests:** grant/copy/destroy round-trips; a save→load→save cycle preserving contents; destroying a
network leaves other owners' networks untouched. All headless — `sr_core` links no renderer.

✅ **Settled 2026-08-11 — `features.md` §2.5.** A network is raidable exactly when it already has a
location and owner entity: a sub-commander's network is anchored to that commander's own vessel
(`NetworkOwner`, above), so capturing (not destroying) that vessel converts its crew — commander
included — and their network comes with them, via the wholesale-crew-conversion §3.2's capture
already performs. **No new type is needed** — the concern this paragraph raised turned out to be
already answered by `NetworkOwner`'s existing shape once capture itself was specified. A faction's
general network has no such anchor and stays permanently un-raidable by design.

#### ResearchSystem — `features.md` §2.4

**Why this lives inside §12.1 rather than its own numbered entry.** `ResearchSystem` appears in §4's
system table and in §11.9's dependency table, but until now it had no home/types/persistence/tests of
its own — only a passing mention above as a `KnowledgeNetwork` writer. Its only externally visible
output *is* a grant into the network, so it is specified here, next to the store it writes into,
rather than opening a new top-level number for one system.

**Home:** `modes/space/systems/ResearchSystem.*`, Tier 1–2 (§4).

**Types:**

| Type | Notes |
|---|---|
| `ResearchJob` | `{ ItemId item; float progress; float cost; NetworkId targetNetwork; }` — POD, §11.4 |
| Held by | A `StationFacility` component on the station entity running it — a job is per-station state, not a rig, so `Rig::children` (Law 4) does not apply. A station running two reverse-engineering jobs at once holds `std::vector<ResearchJob>` on that one component. |

**Systems:** `ResearchSystem::Tick` advances `progress` for every resident station's jobs against
`dt` and the facility's tier (`features.md` §2.4's "time" and "facility-tier" cost factors). On
completion it calls `KnowledgeStore::Grant(targetNetwork, item)` and clears the job. No
`TickCoarse` — see persistence below for why one isn't needed.

**Persistence — the same hole §12.5 found, resolved the same way.** A job only advances while its
station's system is resident (Tier 1–2). If the system demotes mid-job — the player warps away,
`features.md` §1.1 — an entity-only job would either silently vanish or silently freeze with no
record of why. Following §12.5's precedent exactly: demotion writes a `core/galaxy/ResearchRecord`
(station id, item, progress, cost) and tears down the entity's `ResearchJob`; promotion
re-instantiates it, with elapsed time banked the same way `FactionEconomy`'s aggregate production
already fast-forwards. A job resumes at the progress it would have reached had the station stayed
resident, not at the progress it had when the player left.

**Tests:** progress advances correctly against facility tier and `dt`; completion grants exactly
once and clears the job; a demote→promote cycle resumes at the caught-up progress, not the frozen
one; two concurrent jobs at the same station complete independently.

### 12.2 AI Sub-Commanders — `features.md` §4.5

**Home:** `shared/components/Commander.h` (POD) + `modes/space/systems/CommanderSystem.*`.

**Types:** `Commander { NetworkId network; CommanderOrders orders; FactionId faction; }`, POD per
§11.4 — no methods, no owning pointers. The commanded vessel is the entity carrying the component,
which is what makes "destroying the capital destroys them" fall out for free rather than needing a
special case.

**Systems:** `CommanderSystem`, spanning tiers, so **two entry points, never one function branching
on tier** (§4, §11.3):

| Entry | Tier | Does |
|---|---|---|
| `Tick` | 1 | Standing orders in the active system: dispatch, retreat, defend |
| `TickCoarse` | 2–3 | Abstract order resolution against `core/galaxy/` records — no steering, no projectiles |

**Persistence:** commanders are entities, so they serialize through the normal per-system save (Law
2). Their `NetworkId` is stable and survives; their `entt::entity` does not.

**Tests:** a commander's death releases its network reference without destroying the network; orders
survive a Tier 1 → 2 demotion and back.

✅ **Settled 2026-08-11 — `features.md` §4.5.** Recruitment: any `Crew` module with a non-zero
`command` roll, mounted to a Bridge via the existing equip flow — no separate track, hire Living or
manufacture Artificial exactly like any other crew. Competence: the module's own rolled
`operation`/`command` stats, no personality system. Whether a rival can turn one: yes, via crew
bribery (`features.md` §2.7) — the same roll that can turn any crew module, not a sub-commander-
specific mechanic. All three of this paragraph's original questions are answered; none needed a new
component beyond what §12.2 above already specifies.

### 12.3 Faction Survival — `features.md` §5.1 ✅ Built

**Home:** a new entry point on the existing `core/ai/FactionDecisionEngine`, **not** a new system.
`EvaluateSurvival`/`HasCollapsed`/`CollapseFaction` (2026-08-01). `hasCommandStructure` and
`hasLeadership` are supplied by the caller rather than computed here -- Tier 3 has no registry to
answer either question against, the same "caller supplies what `core/` can't compute" shape
`EvaluateRaidDispatch`'s `roll` and `EvaluateColonization`'s `candidateSystemId` already use.
`CollapseFaction` applies the one Tier-3-native consequence (`Territory::ReleaseAll`, new);
scattering surviving ships to rogue needs a registry and is left for a future Tier 1 consumer, the
same "correctly-shaped, independently-tested bridge, no caller yet" shape `WreckRecord` (§12.5)
landed in before #97's warp-system wiring. `FactionEconomy` gained `TotalProduction` (cumulative
lifetime deposits, never reduced by `Spend`) as the Economic Footprint pillar's real data source.

**Why not its own system.** It runs on the same macro tick as the 4-facet engine and reads exactly
the state that engine already reads — `core/economy/FactionEconomy` (footprint),
`core/diplomacy/Territory` (territory), and the command-module query. A separate system would
duplicate the reads and add a directory with one function in it, which §2.4 exists to prevent.

**Types:** `SurvivalPillars { bool commandStructure; bool leadership; bool economicFootprint; }` —
returning all three rather than a bare bool, so the UI and the collapse handler can say *which*
pillar went, and so a test can assert on one pillar at a time.

**The player is not a special case.** `features.md` §3.3 defines Hard Game Over as this same
predicate applied to the player's faction. One implementation, eleven possible factions. If the
player ends up needing a separate code path, something has gone wrong in the modelling — that
equivalence is the whole reason `features.md` §5.1 was rewritten.

**Tests:** each pillar independently sufficient; all three lost ⇒ collapse; collapse scatters ships
to rogue and unclaims territory; the same predicate returns the same answer for a player faction as
for an AI one, given the same inputs.

**Economic footprint, settled:** any production greater than zero anywhere in
`core/economy/FactionEconomy` counts as a surviving footprint. The pillar fails only when the
faction produces nothing at all — not a threshold, not territory alone. This is the simplest
predicate that still matches §5.1's intent ("can this faction still function"), and it keeps the
test in the row above (each pillar independently sufficient) a one-line assertion:
`economy.TotalProduction(faction) > 0.0f`.

### 12.4 Procedural Seeding — `features.md` §7

**Home:** `core/galaxy/Seeding.h/.cpp` — pure functions, no state, `sr_core`.

**This is the most implementation-sensitive section in §12, and the reason is not obvious.**
`features.md` §7.1 specifies a *schema* (`parent seed + coordinates -> child seed`), not an
*algorithm*. Two contributors given that sentence will produce two incompatible galaxies from the
same seed, and neither will be wrong by the document.

So the algorithm is pinned here:

- **Use an explicitly specified 64-bit mixer** — SplitMix64 or PCG, with the constants written into
  the header. Not "a hash function."
- **Never `std::hash`.** It is implementation-defined; libstdc++, libc++, and MSVC disagree. A
  galaxy generated on Windows would not match the same seed on Linux, which breaks determinism
  across exactly the three platforms CI builds.
- **Never `std::shuffle`/`std::uniform_int_distribution` for persisted results.** Distribution
  implementations are also unspecified across standard libraries. Roll the arithmetic by hand.
- Coordinate encoding — bit widths and packing order — is part of the format. Write it down; changing
  it silently regenerates every galaxy.

**Tests, and these are not optional here:** same seed ⇒ same output, asserted against **hardcoded
expected values** committed in the test. That is what turns "deterministic" from an aspiration into
something CI can catch, and it fails loudly on any platform that disagrees. Also: generation is
order-independent (visiting C first yields the same C).

**The boundary that will actually bite:** `features.md` §7.2's rule that anything a player or faction
could have changed is *not* seed-derived. Seeds supply the stage; `core/galaxy/` records supply the
play. A `Seeding.h` that regenerates mutable state on revisit silently undoes player actions, and it
will look like a save bug rather than a generation bug.

### 12.5 The Recovery Run — `features.md` §3.3 Tier 2

**Home:** `LootSystem` (already built) for the live entity; `core/galaxy/WreckRecord` for the
durable one.

**This section exists because the design has a hole, and it is better to name it than to let it be
discovered mid-implementation.** `features.md` §3.3 says the player's cargo drops as a wreck
recoverable within a time limit. But the player respawns at an allied station, potentially in another
system — so the system holding the wreck demotes to Tier 2 or 3. `features.md` §1.1's boundary rule
is absolute: *Tier 3 must never require entity data.* A wreck that exists only as an entity therefore
evaporates when the player leaves, which is precisely when the recovery run is supposed to be
happening.

**Recommended resolution:** the wreck is dual-form, the same way everything else in this codebase is
(Law 3). It is an entity while its system is resident, and a `core/galaxy/WreckRecord`
(position, manifest of blueprint ids, expiry) when the system demotes. Demotion writes the record;
promotion re-instantiates the entity from it. That reuses the promotion/demotion path
`features.md` §1.1 already requires rather than inventing a parallel one.

**Settled: the wreck survives demotion.** The dual-form resolution above is confirmed, not merely
recommended — a wreck that expired on demotion would make the recovery run impossible whenever the
player respawns in another system, which is the common case, not the exception.

Related and still open in `features.md` §9: the window's duration, wall-clock vs. in-game time, and
whether the wreck is marked on the navigation map.

### 12.6 The Navigation Map — `features.md` §8

**Home:** `modes/space/ui/NavigationMap.*`, plus a map-icon bake in the existing
`modes/space/render/IconRenderer`.

**Why the mode and not `shared/`.** Law 11's tie-breaker: put it in the mode, promote when a second
consumer actually appears. `modes/planet/` is 🧊 and explicitly must not be built, so there is no
second consumer today. Promoting early is what left `src/shared/ui/menus/` empty in StarReach2.

**The layer rule that constrains the whole design** (§2.3): `modes/*/ui/` **must not include
`systems/`**. So the map reads `core/galaxy/` records and emits **Intents** (Law 9) for anything that
changes state — an RTS move order at zoom level 3 is a `MoveFleetRequest`, not a direct write. This
is checkable in CI, so a design that requires the map to call into a system will fail the build
rather than fail review.

**Zoom levels 1–3 are UI over `core/galaxy/`; level 4 is the existing render path.** That split is
what keeps the map cheap: levels 1–3 need no registry at all, which is also why they still work for
systems the player has never visited (§12.4 supplies those).

**Icon culling is a correctness rule, not an optimisation.** `features.md` §8.2 scopes ship and fleet
icons to level 3 and culls them entirely above it. Below level 3 there is no per-ship data to draw —
Tier 3 systems have no entities — so an implementation that tries to draw ship icons at level 1 is
not slow, it is reading state that does not exist.

**Settled: sensor-coverage only.** Level 3 does not show every icon present in the resident registry
— it queries `Discovery` plus a live sensor-range check against the player's own sensors, so a
hostile fleet outside detection range does not appear. This is the query layer's actual shape: not
"read the registry," but "read the registry, then filter by the same sensor gate `DiscoverySystem`
already uses for system-level intel." Requires `DiscoverySystem` (or a range check it exposes) to be
queryable from `modes/space/ui/`, which — per §2.3's `modes/*/ui/` must not include `systems/` rule —
means the sensor check itself must live in `core/galaxy/` or be exposed as read-only data, not as a
call into the system.

### 12.7 Template Negotiation — `features.md` §2.6

**Home:** `modes/space/systems/TemplateMarketSystem.*`, Tier 2–3.

**Types:** a pitch is an **Intent** (Law 9) — the UI never resolves a sale directly. Royalty accrual
is faction-level economic state, so it belongs in `core/economy/FactionEconomy` next to the stock it
pays out of, not in a component.

**The sale itself is a network copy** (§12.1) — `Copy(sellerNetwork, buyerNetwork, templateId)`.
That single line is why `features.md` §2.1's "selling a Template sells data, not an object" works,
and why a faction keeps manufacturing your design after its stations die.

**Tests:** a sold Template survives destruction of the buyer's stations; selling the same Template to
two factions at war is permitted (`features.md` §2.6 says so explicitly); royalties accrue against
Tier 3 production without the player present.

**The roll, settled as three steps, not one.** `features.md` §2.6 reads as three separate
mechanics compressed into one paragraph — "on speaking terms," "evaluates the design on its own
terms," and "rolls against disposition" are three different questions, not one roll standing in for
all of them. Splitting them is what makes each one independently testable:

1. **Gate — can the player even pitch?** Relation band (`features.md` §5.3) must be Neutral or
   better. Below that, `TemplateMarketSystem` refuses the Intent before evaluating anything else —
   this is what "on speaking terms" means structurally.
2. **Accept — does the faction want this design at all?** Deterministic, no roll: the Template's
   category must fit the faction's `features.md` §6.2 archetype weighting, beat what they currently
   manufacture, and be affordable against `core/economy/FactionEconomy` stock. A design that fails
   any of these is rejected outright and stays in the seller's network (§2.5) — nothing here is
   random, so this step is unit-testable as a pure predicate.
3. **Rate roll — settled 2026-08-11 as accept/refuse against a rolled ceiling, not a payout
   multiplier.** Runs only if step 2 accepted. `features.md` §2.6: there is no authored base royalty
   rate to multiply — the seller proposes a specific percentage of `BaseValue` as part of the pitch,
   and the buyer either takes it or refuses it outright.

   ```cpp
   float disposition =
       0.5f * reputation.Score(faction)              // core/diplomacy/Reputation, -100..100
     + 0.4f * relation.Value(faction, sellerFaction)  // core/diplomacy/DiplomacyMatrix, -100..100
     + (archetypeFits ? 20.0f : -10.0f);
   disposition = std::clamp(disposition, -100.0f, 100.0f);
   // Maps disposition's -100..100 range onto features.md 2.6's 0-50% acceptance ceiling.
   float acceptanceCeiling = ((disposition + 100.0f) / 200.0f) * 0.50f;
   bool accepted = proposedRoyaltyRate <= acceptanceCeiling;  // proposedRoyaltyRate: 0.0-1.0, from the pitch Intent
   ```

   Reuses the same three disposition inputs the pre-2026-08-11 `payoutMultiplier` formula computed —
   this is a re-derivation of an existing score for a new purpose, not a second formula added beside
   it. **Lump-sum pitches keep the old `payoutMultiplier` shape** (§2.6 only changed how royalty rate
   negotiation works; a lump-sum offer still has a payout the buyer's disposition can raise or lower,
   since there is no separate "accept this specific number" step for a one-time payment).

**The disposition weights (`0.5f`, `0.4f`, `20.0f`) are a placeholder, not a balance pass** — they make
`TemplateMarketSystem` buildable and testable now, in the same sense §6.4 flags its two tuned numbers
as the *only* tuned numbers in the design, and get tuned later against `tools/economy_sim` once there
is a game to playtest against. Do not treat the specific constants as final; treat the three-step shape
as final.

✅ **Resolved 2026-08-11 — see `features.md` §2.6.** The base royalty rate scale turned out not to be a
constant to author at all: the seller proposes it per pitch, and `BaseValue` (now real, since
`tools/economy_sim` exists) is what it's a percentage of. Whether a royalty stream survives the
seller's death was already settled the same day royalty rate itself was reopened — see the note above
this step.

### 12.8 The Constraints That Apply To All Of It

Restated here because they are the ones most likely to be missed by someone implementing a single
feature end-to-end:

| Rule | Where | Bites when |
|---|---|---|
| No `entt::entity` in anything persistent | Law 2, §11.4 | A network, wreck record, or Template holds a handle. Works locally, corrupts on warp. |
| `sr_core` links no raylib | Law 8, §2.1 | Anything in `core/knowledge/` or `core/galaxy/` touches a `Texture2D`. Fails at link, which is the point. |
| `modes/*/ui/` must not include `systems/` | §2.3 | The navigation map calls a system directly instead of emitting an Intent. |
| Tier 3 must not require entity data | `features.md` §1.1 | Wrecks, commanders, or networks are read on the macro tick as components. |
| Content is JSON only | Law 10 | A research recipe or royalty table gets written as a C++ table. CI rejects it. |
| One entry point per tier | §4, §11.3 | `CommanderSystem` branches on tier inside `Tick` instead of exposing `TickCoarse`. |
| Register in the same commit | §2.4, §11.3 | A new system lands without its `SystemSchedule.cpp` entry, so it never runs and nothing says so. |

### 12.9 Template Creation — `features.md` §2.2–2.3 (`CustomizeMenu`)

**This is the load-bearing gap in the batch above it.** `TemplateMarketSystem` (§12.7) sells a
Template out of a network; `KnowledgeNetwork` (§12.1) stores "saved Templates" as one of its three
categories. Neither has ever had a producer. Nothing today lets a player actually assemble shells,
components, and modules into a named design and put it in their network — StarReach2's
`CustomizeMenu.h` (237 lines) is the only place that ever did, and it was never migrated.

**Home:** `modes/space/ui/CustomizeMenu.*`. Stays in the mode per Law 11 — `modes/planet/` is 🧊
and not a second consumer.

**Types:** none new. A Template *is* a `ShipBlueprint`/`RigBlueprint` (`shared/blueprints/`,
already built) — the wizard assembles one in local UI state (not a component, not persisted) and
runs the existing `Validation.h` against it before offering to save. The draft never touches a
registry; nothing is instantiated until (if ever) it is equipped.

**Systems:** none new. `Draw`/`Update` emit a `SaveTemplateRequest` Intent (Law 9) carrying the
finished, validated blueprint; the consumer is `KnowledgeStore::Grant(playerNetwork, blueprint)`
(§12.1) — a store call, not a tick, so no new system owns it. `modes/*/ui/` must not include
`systems/` (§2.3), which this satisfies exactly the way `AvionicsMenu` already does for
`DockRequest`.

**Persistence:** none beyond what §12.1 already specifies — the Template is stored by
`KnowledgeNetwork` as a blueprint id, not re-specified here.

**Depends on:** #78 (`core/knowledge/KnowledgeNetwork`) — there is nowhere to `Grant` into
without it. Confirm it has merged before starting (§11.9).

**Tests:** an invalid draft (fails any of `Validation.h`'s nine rules) cannot be saved and reports
which rule failed; a valid draft round-trips into the network and back out with identical content.

❓ **Open:** whether saving a Template with an already-used name overwrites it, versions it, or is
rejected. Not raised in `features.md` — a genuine gap in the design doc, not just the architecture
one.

### 12.10 Docked Station Services — `StationServicesMenu`

**Not a numbered `features.md` section — closed over the Meso loop's "dock at vessels with
Facility... modules" (§1) and the "component-driven menus" idea `features.md` §4 states for the
Bridge specifically.** `BridgeView` (built, `modes/space/ui/BridgeView.h`) already says explicitly
that its tabs have "no CONTENT behind them yet — Repair/Manufacturing/Research/Storage are each
their own future system." This section is that content for the *visiting-any-station* case: buying
and selling modules/materials for credits, paying to repair hull, and merging duplicate modules to
a higher grade — StarReach2's `StationServicesMenu.h` (124 lines).

**This is a distinct interaction from `BridgeView`.** `BridgeView` is the command surface for a
station or capital the player *owns* (`features.md` §4, Bridge & Fleet Command). This is what
happens when the player docks anywhere, including a station they do not own and never will —
ordinary commerce, not fleet command. Both are gated by `FacilityRef` (`shared/components/
Facility.h`), which is why the underlying "which tabs exist" query should be shared rather than
reimplemented — see the promotion note below.

**Home:** `modes/space/ui/StationServicesMenu.*` (mode-owned per Law 11), plus a new
`modes/space/systems/StationServicesSystem.*` (Tier 1) to consume its Intents — this needs a
system, unlike §12.9, because buy/sell/repair/merge validate against and mutate live components
(`Wallet`, `CargoHold`, `Health`) that must stay consistent under the same-tick intent-consumption
idiom every other system already uses (`DockRequest`, `SpendRequest`).

**Promotion note (Law 11):** StarReach2's `StorageMenu.h` explicitly says its slot-rendering and
trash-can widgets are "used by `ModulesMenu` too." That is the second-consumer case Law 11's
tie-breaker exists for. Do not fork the drag-and-drop grid between this menu and §12.11's — land
the shared widget where the second consumer actually appears, in the same commit.

**Types:** none new beyond components already built (`Wallet`, `CargoHold`, `shared/components/
Loot.h`; `FacilityRef`, `shared/components/Facility.h`). New Intents: `BuyItemRequest`,
`SellItemRequest`, `RepairRequest { float fraction }` — same POD-Intent shape as `Docking.h`'s
`DockRequest`.

**Systems:** `StationServicesSystem::Tick` consumes the three requests above the same tick they're
set, debits/credits `Wallet`, moves entries between `CargoHold` and the station's own stock, and
restores `Health` proportional to `RepairRequest::fraction` × credits spent.

**Tests:** buying debits `Wallet` and adds to `CargoHold`; selling is the exact inverse; repair
spend scales with the requested fraction and refuses when `Wallet.credits` is insufficient.

> ⚠️ **`MergeModuleRequest` is deleted from this section** (2026-08-08). An earlier draft gave
> `StationServicesSystem` a merge that consumed "two same-tier modules" and produced "one
> higher-tier module," blocked on a module grade concept that did not exist. **That was a second,
> conflicting specification of a mechanic §12.12 already owns**: `features.md` §2.4 defines merging
> as *engineering*, `EngineerSystem` implements it against `MergeModulesRequest`, and it is built.
> Two near-identically-named intents for one mechanic is exactly how a duplicate issue gets written.
> Merging lives in `EngineerSystem` and nowhere else. The grade blocker is also gone — `features.md`
> §2.7's rarity ladder and quality band supply it — and merging now **clamps to the grade's band**
> and is refused at the ceiling, per `features.md` §2.4.

### 12.11 Cargo & Hardpoint Equip UI — `StorageMenu` / `ModulesMenu`

**Already anticipated and left unwired.** `shared/components/Loot.h`'s `CargoHold` comment says
plainly: "the ids/quantities a save or a future StorageMenu UI would read... this is simply where
`LootSystem` puts what it collects until a consumer reads it." This section is that consumer:
StarReach2's `StorageMenu.h` (46 lines, cargo/module slot grid) and `ModulesMenu.h` (73 lines,
equip/unequip onto a rig's live hardpoints).

**Home:** `modes/space/ui/StorageMenu.*` and `modes/space/ui/ModulesMenu.*` — or one shared grid
widget between them per §12.10's promotion note, decided in whichever commit lands second.

**Types:** none new beyond `CargoHold`/`Wallet`. New Intents: `MountModuleRequest { ModuleId
module, entt::entity mount }`, `UnmountModuleRequest { entt::entity mount }`.

**Systems:** a new `modes/space/systems/ModuleEquipSystem.*` (Tier 1) consumes the two requests
above.

✅ **Resolved, and resolved in the code.** The layering question this section raised — runtime equip
performs the same attach `RigFactory` does at construction, but `modes/space/systems/` may not
include `factories/` — was settled by **option 1**, extracting the shared attach/detach logic into
`shared/rig/ModuleAttachment.h/.cpp`. Both sides call it; neither includes the other; the layer rule
is unamended. `ModuleEquipSystem` is built and registered in `SystemSchedule.cpp`.

`ModuleAttachment.cpp` is also the single `switch (ModuleKind)` that emplaces a module's components
(`Weapon`/`FiringArc`, `Shield`, `FacilityRef`/`DockingBay`, `PowerSource`/`PowerLoad`). **A new
module kind is a new case there**, which is where `features.md` §2.7's `Operator` and `Commander`
crew kinds attach their components — and its `PowerPriorityFor(ModuleKind)` returning
`Facility 0, ShieldGenerator 1, Weapon 2, Engine 3` is already exactly §2.9's four power-allocation
categories, under a different name.

**Tests:** equip attaches a module component set `PowerSystem`/`DamageSystem` already read and
removes it from `CargoHold`; unmount is the exact inverse; equip refused when the module's
`ModuleKind` doesn't match the mount's declared kind (mirrors `Validation.h`'s existing
`ModuleCompatibility` rule, applied live instead of at blueprint time).

### 12.12 Construction, Refit & Grafting — `BuildMenu` / `RefactorMenu` / `EngineerMenu`

**The least specified of this batch — bundled deliberately, because each piece is smaller and
less certain than §12.9–12.11 and none blocks the others.** Both ❓s below have since been
settled by the project owner (2026-08-01); this section records the resolution rather than the
open question, the same way §0 tracks every other settled ❓ in this batch.

**`BuildMenu` — player-initiated construction. ✅ Built.** `architecture.md` Law 9 already names
`BuildStationRequest` and `PlaceShipRequest` as the canonical Intent examples — this was
anticipated from the first draft of the intent-queue discipline and never built against. StarReach2's
`BuildMenu.h` (59 lines) reads player `CargoHold`/`Wallet` for affordability, then hands off to
placement mode. **Home:** `modes/space/ui/BuildMenu.*`, a new `modes/space/systems/
ConstructionSystem.*` (Tier 1) consuming `BuildStationRequest`/`PlaceShipRequest` and calling the
already-built `StationFactory`/`RigFactory` — construction is assembly (Law 5), so this is the one
place in this batch where a system legitimately drives a factory as its documented job, not a
violation of §12.11's layer question (`tools/ci/check_layers.py` carries a narrow, named exemption
for `ConstructionSystem.cpp`, per §11.7). **Tests:** a request is refused when unaffordable against
`Wallet`/`CargoHold`; an affordable request spends the cost exactly once and instantiates via the
existing factory.

**`EngineerMenu` — module merging. ✅ Built, settled.** Not a `ResearchSystem` (§12.1) variant —
genuinely the distinct mechanic the original ❓ raised: the player merges two owned modules of the
*same* `ModuleKind` into one, at a level-scaled loss on the secondary module's contribution.
**Home:** `modes/space/ui/EngineerMenu.*`, a new `modes/space/systems/EngineerSystem.*` (Tier 1).
Gated by docking at a station/ship carrying a living `FacilityKind::Engineering` hardpoint (a new
facility kind); `FacilityStats::level`/`FacilityRef::level` (1–5, `ModuleDef.h`/`Facility.h`) is the
engineer's skill tier. **Formula, per numeric field on the module** (mass, power draw/generation,
hull bonus, and whichever kind-specific stat block matches):
`primaryValue + secondaryValue * (level * 0.1)` — level 1 preserves 10% of the secondary module's
stats, level 5 preserves 50%. A placeholder scale, not a tuned value, the same category §12.7's
rate-roll weights are flagged as. **The mechanism this needed and didn't have:** a merged module is
genuinely new content, generated at runtime, not authored in `data/base_game/`. It is built by
plain declaration and field assignment (never an initializer), the same Law 10 shape §12.9's
`CustomizeMenu` Template draft already uses, and registered via a new
`ContentLibrary::RegisterCraftedModule` — a small runtime-registered overlay `FindModule` checks
before the JSON-loaded set, so the merged id resolves everywhere `content.FindModule` already does
(equip, construction, a future save). **Tests:** a same-`ModuleKind` merge scales the secondary's
stats by level and produces a module that resolves via `FindModule`; refused when not docked at a
living Engineering facility, when the two modules are not the same `ModuleKind`, or when either id
is not actually held (merging a module with itself requires two distinct owned copies, not one
counted twice).

**`RefactorMenu` — hardpoint deletion, settled as a narrower scope than "component swap."** The
original wording speculated a live Shell swap at "the Component tier" of Law 4's
`Shell -> Component -> Module` model; no `ComponentDef` type exists to swap (`ShellDef`/`ModuleDef`
are the only two authored tiers — **and per §12.14 item 7 that is correct and final, not a gap: a
shell IS a component**), and the settled mechanic is simpler: delete a hardpoint from the
player's own live rig while docked at a living `FacilityKind::Engineering` facility (the same gate
`EngineerMenu` uses). **Home:** `modes/space/ui/RefactorMenu.*`, a new `modes/space/systems/
RefactorSystem.*` (Tier 1). **Types:** a new `shared/components/Rig.h` component,
`MountedModules { std::vector<ModuleId> ids; }`, written once by `RigFactory` at construction so a
later deletion knows what to hand back. **Rules:** a hardpoint another hardpoint's
`StructuralAttachment` still points at cannot be deleted (would orphan its children — deletion is
scoped to leaves); a deletion whose returned modules would not fit the player's `CargoHold` is
refused whole, not partially applied — `CargoHold` gained a `capacity` field (0 = unlimited, so
every pre-existing `CargoHold` is unaffected) and `CargoHoldHasRoomFor` for this check. **Tests:**
deleting a leaf hardpoint returns its `MountedModules` to `CargoHold` and removes it from `Rig`;
refused when `CargoHold` has no room, when another hardpoint depends on it, or when the hardpoint
does not belong to the requester's own rig.

### 12.13 Design Decisions of 2026-08-07 — Required Code Changes

*A design walkthrough on 2026-08-07 settled six items in `features.md` and raised four open ones.
This subsection is the engineering half: which require code, where it goes, and which are content or
documentation only. Nothing here is built yet.*

**The finding that motivated the walkthrough.** An audit of all thirty scheduled systems found
roughly fourteen that are complete, unit-tested consumers whose input component **no producer ever
writes** — the exact failure mode `features.md` §5.3 was written to prevent after StarReach2's
relation matrix shipped with `Set` never called from gameplay. `DiplomacyMatrix` has, today, one
reader in the entire mode layer and zero writers. Separately, `SpaceFlight.cpp` builds its
`SystemContext` with only `economy` populated, so `discovery`, `knowledge`, `diplomacy`,
`reputation`, and `craftedModules` are all `nullptr` at runtime and five systems silently no-op
against their own null guards. **Producers, not more consumers, are the work.**

| # | Decision (`features.md`) | Code change | Home |
|---|---|---|---|
| 1 | Player aims manually, no target lock (§3.2) | **Yes** | `WeaponSystem` + new input producer |
| 2 | NPCs roll a random living hardpoint (§3.2) | **Yes** | `TargetingSystem::SelectAimPoint` |
| 3 | Targeting priority is skill-driven, never role (§3.2) | **No — not yet** | §2.4 forbids the unread abstraction |
| 4 | Fighters take localized damage uniformly (§3.2) | **No** | Already true; Law 4 |
| 5 | Research / engineering / deconstruction are distinct (§2.4) | **Yes** — deconstruction only | `EngineerSystem` |
| 6 | Object scale & hardpoint separation (§3.5) | **Yes** — once numbers agreed | `Validation.h` rule #10 + `ships.json` |

#### 1 & 2 — Aiming

`WeaponSystem.cpp` currently gates all fire on `Target`: with no target, `aimPoint` is `nullopt` and
**every weapon on the rig skips firing entirely.** That is a hard lock, and it applies to the player.
The change is narrow because hit resolution is already physical — `ProjectileSystem` resolves the
first hardpoint along the path and never consults `Target`. Only *aiming* is locked.

- The player's aim point becomes their cursor in world space, supplied by the new input producer as
  a component on the player rig — **not** by reaching into raylib from inside a system (Law 8).
- `WeaponSystem` takes its aim point from that component when present, falling back to `Target` for
  everything else. Firing must no longer require a `Target` at all.
- `Target` survives as NPC targeting and as the player's reticle/subtarget readout. It stops being
  the permission to fire.
- `TargetingSystem` must skip the *hardpoint* roll for `PlayerControlled` while still acquiring the
  *rig*, mirroring `NpcAiSystem`'s existing `entt::exclude<PlayerControlled>`. Otherwise the reticle
  shows a random hardpoint while the player's shots go where the cursor points.

**Three traps in the random roll.**

1. **It must not use `rand()` or a distribution object.** Law 2's coarse-tick fast-forward requires
   every time-dependent value to be a pure function of tick count. `MiningSystem` and `CommsSystem`
   already solve this with an FNV-1a hash of `(entity, tick)`; `TargetingSystem` uses the same idiom.
2. **That hash is already duplicated, and this would be the third copy.** `MiningSystem.cpp` and
   `CommsSystem.cpp` hold byte-identical FNV-1a implementations — Law 11's promotion trigger fired at
   the second consumer and was missed. Promote it to `shared/math/` **in the same commit**; do not
   paste it a third time.
3. **Keep the existing across-tick persistence of `Target::hardpoint`.** A roll re-evaluated every
   tick makes an NPC spray incoherently. The roll happens on acquisition, and again only when the
   selected hardpoint dies — which is already the shape of the code.

#### 5 — Deconstruction needs no new system

**Home: `EngineerSystem`, as a second intent — not a new file.** Deconstruction shares
`EngineerSystem`'s exact gate (requester `Docked`, station carrying a living
`FacilityKind::Engineering` hardpoint) and exact state (`CargoHold`, `ctx.content`). A separate
system would duplicate every one of those reads and add a directory with one function in it, which is
precisely the reasoning §12.3 used to keep faction survival out of its own system. `EngineerSystem.cpp`
is 167 lines against a 600-line cap, so no size pressure argues the other way; if it later grows past
the cap, *that* is the signal to split — mechanically, not speculatively.

- **Types:** `DeconstructModuleRequest { ModuleId module; }` in `shared/components/Engineer.h`, the
  same POD-intent shape as `MergeModulesRequest`.
- **Content:** ~~the module → materials yield is authored data (Law 10) — a `deconstructsTo` field on
  `ModuleDef` in `modules.json`. Never a C++ table.~~ ⚠️ **Superseded 2026-08-10 by §12.30.5.**
  §12.19 gives every item a `Recipe`, and `features.md` §2.4 states recovery as a **percentage** —
  which is a percentage *of the inputs*. **Deconstruction yields the item's own `Recipe` inputs at the
  facility grade's recovery band**; a second authored answer to *"what is this made of"* would drift
  the moment either was edited. Cheapest possible correction — `modules.json` has no `deconstructsTo`
  today, so there is nothing to migrate. Folds into §12.19's content pass.
- **Refused when:** not `Docked`; no living Engineering facility; the module is not in the requester's
  `CargoHold`; or the yield would not fit (`CargoHoldHasRoomFor`, the same whole-or-nothing rule
  `RefactorSystem` already applies).

#### Costs are now specified in `features.md` and absent from the code

§2.4 states that **engineering costs credits and/or materials/crafts** and **research costs time plus
materials/crafts and/or credits.** Neither is implemented:

- `EngineerSystem` charges **nothing** — no `Wallet` read, no material consumption. The merge is free.
- `ResearchSystem` charges **time only** — `ResearchJob::cost` is a progress threshold advanced by
  `researchTier`. No material or credit component exists.

Both need the debit added alongside their existing gates, and both must refuse before mutating
anything, matching the "a refused build never costs anything" rule `ConstructionSystem` documents.

#### 6 — Scale becomes a validation rule

Once §3.5's numbers are signed off, the minimum hardpoint separation lands as a **tenth rule in
`shared/blueprints/Validation.h`**, joining the nine `features.md` §2.3 defines, and runs in CI
against `data/base_game/` through the existing content check. `aegis_vanguard`'s `core`/`reactor`/
`emitter` cluster (≤ 8 units apart) fails the proposed 20-unit minimum and needs respreading in
`ships.json` — content, no C++.

#### Still open, and each blocks work

| Question | Blocks |
|---|---|
| Does skill attach to a crew/officer entity, or is it a flat stat? (§2.7) | Engineering merge quality, §12.2 commander competence, NPC targeting priority |
| What manufactures a module? (`features.md` §9) | Research's entire payoff — the unlock has no consumer |
| Object scale numbers (§3.5) | Validation rule #10, and every hull authored before it lands |
| Deconstruction yield — flat or skill-scaled? (§2.4) | The `deconstructsTo` schema in `modules.json` |

### 12.14 Second Round, 2026-08-07 — The Component Tier And What It Costs

*The same walkthrough continued and settled five more items. Two of them are large: the Component
tier becomes real, and mass/power recomputation stops being scoped out. Nothing here is built.*

| # | Decision (`features.md`) | Code change | Home |
|---|---|---|---|
| 7 | **Shell and component are the same thing** — two tiers, not three (§2) | **No — doc fix only** | Law 4 wording; §12.12's "gap" is not a gap |
| 8 | Shells add mass; modules add mass + power; both recomputed on every change (§2.2) | **Yes** | `ModuleAttachment`, `ModuleEquipSystem`, `RefactorSystem` |
| 9 | A shell cannot be removed while it holds modules (§2.2) | **Yes** | `RefactorSystem` — **reverses existing behaviour** |
| 10 | Skill is a crew module mounted in a shell (§2.7) | **No — not yet** | content first; §2.4 forbids the unread abstraction |
| 11 | Docked craft cannot be shot, but die with their host (§3.4) | **No — not yet** *(§15.1 findings 2–3: neither half is implemented; fix specified in §12.34)* | `DockingSystem` / `DamageSystem` |
| 12 | Engineering merge scales with facility level (§2.4) | **No** | ratifies `FacilityRef::level`, already built |
| 13 | Manufacturing modules is not `ConstructionSystem`'s job (§2.8) | **Yes** | new system |
| 14 | Crew modules draw zero power (§2.7) | **No** | a value in `modules.json`, not a `PowerSystem` carve-out |
| 15 | Cockpit integration is decided per hull by the scale rule (§3.2) | **No** | content; falls out of §12.13 item 6 |
| 16 | The uncrewed hull — crew death disables rather than destroys (§3.2) | **Yes** | `NpcAiSystem` / `WeaponSystem` gating |

#### 7 — There is no `ComponentDef`, and that is the correction

An earlier draft of this section called for a new authored `ComponentDef` tier and rated it the
largest item in either round. **That is withdrawn.** `features.md` §2 settled that *shell* and
*component* are two names for one thing: the housing a module mounts into, which is also the unit of
localized damage and the thing a hardpoint is.

So `ShellDef` and `ModuleDef` remain the complete set. **No new type, no JSON schema change, no
`SaveMigrator` bump, no new validation rules, and no ordering constraint against §12.13's item 6.**

The one real action is documentation: **Law 4 currently describes a `Shell → Component → Module`
model and should read `Shell → Module`**, and §12.12's note that "no `ComponentDef` type exists to
swap" should be marked resolved-as-unnecessary rather than left reading as an outstanding gap. That
wording is what made this look like a missing tier twice in two separate sessions, which is reason
enough to fix it.

#### 17 — `ProjectileSystem::FindHit` resolves ties by iteration order, and must not

**A latent bug that §12.13's manual-aim decision promotes into a real one.** `FindHit` returns the
*first* hardpoint in EnTT iteration order whose `hitRadius` the projectile's path crosses — not the
one nearest along that path. Its own doc comment says "nearest-in-iteration-order," so this was
known and deliberate under auto-targeting, where the player never chose an aim point anyway.

With manual aim it is no longer acceptable: wherever two hit circles overlap, *which hardpoint the
player hits is arbitrary and unstable* — it can change when unrelated entities are created or
destroyed, because that reorders the view. Precision aiming cannot be the core combat decision
(`features.md` §3.2) on top of a tie-break the player cannot see or predict.

**Fix:** walk every candidate and keep the one with the smallest distance *along the segment* — the
closest intersection to the muzzle, not the closest centre. Overlap is then resolved by geometry,
which is what the player is aiming with.

Note this is independent of `features.md` §3.5's separation rule and both are wanted: separation
stops circles overlapping *by construction* on authored hulls, and the nearest-hit fix makes the
outcome correct and deterministic when they overlap anyway — on a player-built Template, a
battle-damaged rig, or any hull authored before the rule lands.

**Also settled by reading the code:** a projectile carries no radius (it is a zero-width segment; the
`hitRadius` belongs to the hardpoint), and `FindHit` returns on the first hit and the projectile is
then destroyed. **One projectile damages exactly one hardpoint.** Clustering hardpoints therefore
causes no splash or multi-hit — clustering is purely an aiming and UI-selection problem, never a
damage-amplification one.

### 12.15 Numeric Ranges: Aggregates, Coordinates, and One Live Bug

*Raised 2026-08-08 from the question "could creative scaling keep us out of overflow?" The answer
differs completely for the two things that question covers, and one of them turned up a defect.*

#### Aggregates: scaling down does not help, and it hurts

Dividing every contribution by a constant (system count, or any other) before summing is a **change
of units, nothing more**. Ratios between fleets are unchanged, so every comparison behaves
identically — and the operation *introduces* the precision loss it was meant to prevent, because
small addends disappear into a large running total.

`double` spans ~1.8 × 10³⁰⁸. There is no galaxy size at which a Military Weight or resource total
overflows it. **Accumulate at natural magnitude in a `double` and scale only for display.**

**The real limit at a million systems is iteration, not arithmetic** — see `features.md` §2.2's note.
Maintain faction-wide totals incrementally per system; never re-sum a million records on a macro tick.

#### Coordinates: the hierarchy is the answer, and it is already the architecture

The instinct that galaxies and systems want **separate coordinate values** is correct, and Law 2
already delivers it:

| Level | Type | Range | Precision |
|---|---|---|---|
| **In-system position** | `Vec2` (`float`) | Local to the system origin — never grows with galaxy size | See budget below |
| **System position in galaxy** | Integer grid coordinate | ±2.1 × 10⁹ per axis in `int32` | **Exact** |
| **Galaxy position in universe** | Integer grid coordinate | Same | **Exact** |

Because one registry is one star system (Law 2), a world position is always measured from that
system's own origin. **Galaxy size therefore cannot affect in-system precision at all** — the two
numbers never meet. A million-system galaxy is a 1000 × 1000 integer grid, which fits in `int16`;
overflow is not reachable.

`core/galaxy/Seeding.h` already implements exactly this, including coordinate packing as part of the
seed format, and already refuses `std::hash` for being implementation-defined across platforms.

**The `float` budget for in-system positions.** A `float` carries a 24-bit mantissa, so its ULP is
constant within a binade and doubles at each power of two. `features.md` §3.5 works the exact binade
values and they are canonical; the numbers below are those, not the *V* × 1.19e-7 average an earlier
draft of this table used:

| System radius | ULP at edge | Movement slower than this is **lost** at 60 Hz |
|---|---|---|
| 1,000,000 | 0.0625 | ~1.9 units/sec |
| **2,000,000** | **0.125** | **~3.8 units/sec** |
| 4,000,000 | 0.25 | ~7.5 units/sec |

**The ceiling is 2,000,000 units of usable radius**, and system radius scales with star radius
(`features.md` §3.5's star-class table). *An earlier draft of this section said ~1e6; where the two
disagree, §3.5's figure wins.* Beyond 2e6, `position += velocity * dt` stops changing the value for
anything drifting under ~7.5 units/sec — which would freeze uncrewed hulls (`features.md` §3.2),
debris, and fine docking adjustments. The loss is deterministic, so it is a gameplay artefact and not
a multiplayer desync risk.

#### 🐛 `SpaceFlight::WarpToSystem` seeds with `std::hash` — a cross-platform desync

`SpaceFlight.cpp` derives a destination system's seed as
`static_cast<unsigned int>(std::hash<std::string>{}(targetSystemId))`. Its comment presents this as a
placeholder awaiting a topology store, which understates it:

> **`std::hash` is implementation-defined.** `Seeding.h`'s own header says so and refuses it for
> precisely this reason — libstdc++, libc++, and MSVC disagree, "which would desync a galaxy across
> exactly the three platforms CI builds."

So warping to the same system id produces **a different system on Windows than on Linux**, and in a
multiplayer session two clients would disagree about the contents of a system they are both standing
in. It is also truncated to `unsigned int`, discarding half of a 64-bit hash.

**The fix needs the missing piece to land first.** Systems are identified by string id (`"sol"`) and
carry **no galactic coordinate**, so §7.1's cascade has nothing to derive from — which is why the
placeholder exists. The real dependency is a `core/galaxy/` topology store giving every system a
grid coordinate; `Seeding.h`'s cascade then works as designed and this line becomes a call into it.
Until then the placeholder should at minimum use `SplitMix64` over the id's bytes rather than
`std::hash`, which removes the platform divergence for a few lines of code.

### 12.16 The Bridge Batch, 2026-08-08 (`features.md` §4)

*A walkthrough of §4 settled the Bridge's shape. Two items are corrections to built code; the rest is
new. Nothing here is built.*

| # | Decision (`features.md`) | Code change | Home |
|---|---|---|---|
| 22 | `Commander` moves from the rig root to the **crew hardpoint** — bridge *or* cockpit (§4.0) | **Yes — small** | `Commander.h`, `CommanderSystem` |
| 22b | `ModuleKind::Operator` + `ModuleKind::Commander` collapse into one **`ModuleKind::Crew`** with `operation`/`command` as rollable stats (§2.7, 2026-08-09) | **Yes — small** | `Taxonomy`, `ModuleAttachment`, `modules.json` |
| 23 | ~~Operator/Commander are two exclusive player modes~~ | ❌ **Superseded 2026-08-09** | **Do not build.** See below |
| 24 | Upkeep is a general `ModuleDef` property (§2.7) | **No — 🧊 cut** | `features.md` §2.7 deferred it the same day |
| 25 | Tactical orders address a party; strategic fleets need a `core/galaxy/` record (§4.3) | **Yes** | `PartySystem` + new `core/galaxy/` type |
| 26 | ~~Pilots get three cursor-free order verbs~~ | ❌ **Superseded 2026-08-09** | Replaced by full selection — §12.27 |
| 27 | Strategic outcomes are value comparisons plus deterministic rolls (§4.3) | **No new design** | Wire up `FactionDecisionEngine` |

> ❌ **Items 23 and 26 are withdrawn, 2026-08-09.** `features.md` §4.0 reversed the two-mode model —
> the player now operates and commands simultaneously, with no mode switch — and §4.3 reversed
> cursor-free verbs in favour of full right-click selection from a fighter. **Item 23 was the "Yes —
> large" row in this table; deleting it removes the largest single piece of work §12.16 scheduled.**
> What replaces item 26 is bigger than it was, and it has its own section: **§12.27**.
>
> **Item 22 survives with one amendment:** the `Commander` component still moves from the rig root to
> the crew hardpoint, but that hardpoint is now a bridge **or a cockpit**, since §4.0 lets a fighter
> command. The rule it buys is unchanged and still good — destroy the crew shell, lose the commander,
> keep the ship.

#### 22 — `Commander` belongs on the bridge hardpoint

`Commander.h` currently says "the commanded vessel is the entity carrying this component... destroying
the capital destroys the commander for free." That was written before `features.md` §2.7 made a
commander **a module in a bridge shell**, and a shell is a hardpoint entity.

Moving the component to the bridge hardpoint is a smaller change than it sounds — `CommanderSystem`
already reads the commander's own rig and health — and it buys a sharper rule: **destroy the bridge,
lose the commander, keep the ship.** Decapitation becomes tactically distinct from destruction, which
is the same shape §12.14 item 16's uncrewed hull already has. Update that header comment in the same
commit; it is the sort of stale rationale that misleads the next reader into re-deriving the old
model.

#### 24 — `UpkeepSystem` 🧊 **cut, do not build**

*An earlier draft of this subsection argued at length that upkeep earns its own file. It is
withdrawn.* `features.md` §2.7 deferred upkeep on 2026-08-08, hours after specifying it, on the
grounds that **the game already has upkeep**: repairing hulls, refuelling, and assembling replacement
vessels are recurring credit and material sinks that already scale with fleet size. An abstract
recurring bill on top would tax the player twice for the same thing and add a subsystem to maintain.

No `UpkeepSystem`, no `upkeep` field on `ModuleDef`, no new `TickCoarse`. Revisit only if automation
later removes enough of those manual sinks that fleet size stops costing anything to sustain — and if
it is revisited, the design reasoning is preserved in `features.md` §2.7 rather than here.

**Living vs. artificial officers survive the cut intact**, because salary was never their real
difference. You cannot mass-produce people: living officers are hired one at a time, artificial ones
are researched once and manufactured as fast as the production base allows.

#### 23 — Two player modes ❌ **withdrawn 2026-08-09; do not build**

*This subsection asked whether the Operator/Commander mode switch should be a second `IGameMode` or
a UI layer inside `SpaceFlight`.* **`features.md` §4.0 removed the mode switch entirely on
2026-08-09** — the player now operates and commands simultaneously — so the fork has no subject.

**The reasoning still lands, though, and §12.27 inherits it.** This subsection concluded that a UI
layer inside `SpaceFlight` beat a mode swap, because §3.4 forbids pausing and the commanded hull
stays live throughout, so *"the simulation keeps running unchanged underneath and only the input
mapping and camera change"* — a mode swap would imply a lifecycle boundary the simulation must not
cross. That is exactly what the command system is: an input and UI layer over an unchanged tick, with
no `IGameMode` involved. The fork resolved itself by the mode disappearing rather than by the
question being answered, and the answer would have been the same.

#### 21 — Researching and deconstructing shells (`features.md` §2.4)

**The knowledge layer is already generic; only the job is not.** `KnowledgeStore::Grant` takes a
`NetworkEntryKind` plus a bare `std::string` id, and unlocks land in `unlockedBlueprints` as stable
string ids with no notion of what kind of thing they name. **A researched shell id stores with zero
changes to `core/knowledge/`.**

The one typed spot is `ResearchJob::item`, declared `ModuleId`. Its own comment gives the reason —
"there is no separate `ItemId` type in `Ids.h` yet, and a researched item always arrives as a
collected `ModuleId`" — and that premise stops holding the moment shells are researchable. The fix
is to carry a kind alongside the id rather than to introduce a variant type; `NetworkEntryKind`
already exists for exactly this and `Grant` already consumes it.

Deconstruction (§12.13 item 5) widens the same way: `DeconstructModuleRequest` becomes kind-tagged,
and `EngineerSystem`'s refusal checks resolve against shells as well as modules. **Merging does not
widen** — `features.md` §2.4 rules it out for shells on the grounds that a merged shell has no
answer to its position or its mounted children, which is a design constraint rather than a coding
one, so `MergeModulesRequest` stays module-only.

#### 18 — Power allocation levels (`features.md` §2.9)

**`PowerBudget` gains structure.** It currently carries one rig-wide `satisfaction` in [0, 1], read
by `WeaponSystem` (fire rate) and `PhysicsSystem` (thrust). Four-level allocation needs a
**per-category** figure instead, and it must be able to exceed 1.0 — today's clamp makes boost
impossible to express, since a rig can only ever be throttled.

- The player's chosen level per category is an **intent**, not a component the UI writes directly
  (Law 9) — same shape as `DockRequest`.
- Draw and effect multipliers per level are authored on `ModuleDef` (Law 10), never constants in
  `PowerSystem`.
- `PowerSystem` resolves demand at the requested levels, refuses a boost that does not fit rather
  than browning out the rig to fund it, and keeps its existing priority-shed path for the case where
  *generation* drops (a dead power cell) rather than where allocation was overcommitted. Those are
  two different failures and should not share a code path.
- The afterburner is not a new system: `Ctrl` sets engines to Boosted through the same intent
  (`features.md` §3.6 — it moved off `Shift` on 2026-08-09 so the command system could take that
  modifier).
- `NpcAiSystem` must eventually produce these intents too (§6.3 forbids an AI-only path), but not in
  the same issue — the player's producer comes first, and the AI one is a second consumer of a
  mechanism that already works.

#### 19 — Shell sizing, placement bounds, draw layers, and penetration (`features.md` §3.5)

**Shell radius stays per-type and authored** — `ShellDef.radius` already works this way and needs no
change. A uniform-radius model was specified on 2026-08-07 and withdrawn on 2026-08-08; ignore any
surviving reference to it. What `shells.json` *does* need is re-authoring against §3.5's scale table
(chassis at ~50% of hull radius, 50-unit fighters), which every existing blueprint fails today.

**Two new validation rules** (`Validation.h`), both cheap and both pure geometry:

- **Rule 10 — separation:** no two mounts closer than `r(A) + r(B)`, so hit circles stay disjoint and
  §3.2's manual aim has something to bite on.
- **Rule 11 — attachment:** every mount within `A extent + B extent` of what it attaches to, so a
  mount cannot satisfy graph connectivity (rule 7) while floating visually detached.

**`ShellDef` needs a draw-layer field.** `spriteLayer` is a *string asset key* — which image, not what
draw order. §3.5's five-layer stack needs a separate integer (1–5), defaulted per shell type with an
optional per-mount override in the blueprint. One string carrying both concepts is how they drift.

**Penetration** becomes a `ModuleDef` weapon stat. Item 17 already requires `FindHit` to gather
candidates and sort by distance along the segment; penetration is then "take the first N" instead of
"take the first one." Implement it in the same change or immediately after, while that code is open.

**Draw order within a layer is local y, sorted once at spawn.** `features.md` §3.5 settles this as
*local* y — measured against the hull — rather than world y, because world y re-orders a hull's own
parts as it rotates and makes sprites pop. Since local y is fixed for the life of a rig, the sorted
order is a build-time product of `RigFactory`, not a per-frame sort. Ordering *between* rigs can stay
arbitrary.

**Turret sprites rotate by `FiringArc::currentOffset`**, added to the mount's world rotation. No new
component is needed to identify which shells rotate — carrying `FiringArc` is the signal, and
everything without one inherits hull rotation unchanged. Collision is unaffected, since a hit circle
is rotation-invariant.

⚠️ **Rule 11 must measure a turret's base extent, not its drawn extent.** A long barrel legitimately
sweeps over neighbouring hardpoints; validating against the sprite's full length would reject hulls
that are correctly built.

#### 20 — Projectile collision cost: use the rig hierarchy before reaching for a grid

`FindHit` is a full `HitRadius` view scan per projectile per tick — O(projectiles × hardpoints), no
broad phase. Ranked by value-per-unit-of-work:

**1. Rig-level rejection first — the cheapest large win, and it needs nothing new.** Test the
projectile segment against each *rig root's* bounding circle, and only descend into a rig's
hardpoints when that passes. The data already exists: rig roots carry `CollisionRadius` (used by
`CollisionSystem`) and every hardpoint carries `ParentRig`. A system holds tens of rigs but hundreds
to thousands of hardpoints, and a fighter is 7 hardpoints while a station is 50+, so this removes
the overwhelming majority of tests for one extra loop level. **No spatial structure, no cell-size
tuning, no new component, no Law 11 promotion.** A rig's bound is
`max(distance from root to a hardpoint + that hardpoint's radius)`, computable once at spawn.

**2. Batch the inner loop.** Collect this tick's projectile segments once, then make a single pass
over the surviving hardpoints testing each against all of them. Same asymptotics, far better cache
behaviour and trivially vectorizable — the current shape re-walks the whole view per projectile.

**3. ~~Uniform radius as a constant-factor bonus.~~ Withdrawn.** This assumed §3.5's uniform hit
radius, which was **specified and then withdrawn on 2026-08-08** — `ShellDef.radius` is per shell
type and matches its drawn size. There is no free constant to hoist.

**4. The spatial grid — out of scope.** `CollisionSystem` already owns a grid, so the route would be
Law 11 promotion with `ProjectileSystem` as second consumer, never a bespoke copy. But at
`features.md` §9.1's budget items 1–2 bring the cost to ~33 M tests/second with the rig bounds
resident in L1, so the grid buys nothing it needs to. Do not build it.

**What will not work:** replacing the segment test with a point test. `features.md` §3.5 puts
projectile speed at 1,200–1,800 units/sec — 20–30 units per step — against hardpoint radii as small
as 5, so a point check tunnels straight through. The swept segment is mandatory.

**Sequencing — settled 2026-08-08, and it reverses this section's earlier "wait for a measurement."**
`features.md` §9.1 now supplies the budget, and it is the measurement:

| | Tests per tick | Per second |
|---|---:|---:|
| Naive `FindHit` at 5,000 projectiles × 5,000 hardpoints | **25,000,000** | 1.5 **billion** — infeasible |
| With item 1's rig-level rejection | ~550,000 | ~33 M — comfortable, and cache-resident |

So **items 1 and 2 are required, not optional**, and they land together with item 17's nearest-hit
tie-break fix — three changes, one file, one issue. **Items 3 and 4 are out of scope**: item 3 rests
on a withdrawn model, and item 4 is machinery the budget does not need.

One thing to watch rather than build: 5,000 projectiles at 60 Hz is ~5,000 entity create/destroys per
second, which is the one place §5's 🧊 memory pooling might genuinely trigger. §5 says profile first;
this is the reason to profile, not a reason to pre-build.

#### 15 & 16 — Cockpit placement and the uncrewed hull

**15 is content, not code.** Whether a hull's crew shell is integrated into its chassis or sits apart
as a bridge is decided per hull by §12.13 item 6's separation minimum: a ~36-unit fighter cannot
legally place a cockpit 20 units off its own chassis, a ~280-unit capital trivially can. One rule,
correct answer at both scales, no per-craft-type branch (Law 4).

**16 is the one genuinely new mechanic in this round.** A rig whose crew module is destroyed must
stop steering and stop firing while remaining physically present, collidable, and targetable:

- `NpcAiSystem` must skip a rig with no living crew module — no `ThrustInput`, no `FireIntent`. It
  already excludes `PlayerControlled`, so the shape of the filter exists.
- `WeaponSystem` needs nothing new: with no `FireIntent`, nothing fires. Confirm that holds once
  §12.13 item 1 decouples the player's fire path from `Target`.
- Do **not** tag the hull `Destroyed`. `DamageSystem` marks a rig destroyed only when every hardpoint
  is gone, and an uncrewed hull is explicitly not that. The drift-and-be-captured behaviour depends
  on it staying alive.
- ✅ **Capture is now specified — `features.md` §3.2, settled 2026-08-11: boarding-in-place.** A
  Troop Bay-carrying vessel holds position adjacent to a capturable hull for a duration; completing
  the hold flips `FactionRef` and converts the crew aboard, commander included. This paragraph's
  earlier "unspecified, should not be guessed at" framing is stale — corrected here rather than left
  to mislead a later reader. Disabling remains a complete mechanic on its own, unchanged.

#### 8 & 9 — Mass, power, and the removal rule

**This closes the largest gap between the Meso loop as designed and as built.**
`ModuleEquipSystem.h` documents that "rig-wide `BodyMass`/`Propulsion` are not recomputed on
mount/unmount," so §2.2's constraints puzzle currently enforces power and gives mass away free.

- Recomputation belongs in **one place both systems call**, not pasted into each — `ModuleEquipSystem`
  (mount/unmount), `RefactorSystem` (hardpoint deletion), and eventually the construction path all
  need identical results. `shared/rig/ModuleAttachment.h` already holds the shared
  attach/detach helpers and is the natural home for a `RecomputeRigTotals`.
- `PowerSystem` already recomputes `PowerBudget` from living hardpoints every tick, so power needs
  no new trigger — only components and modules contributing correctly.
- `DamageSystem`'s all-or-nothing engine invalidation ("a rig with two engines keeps full thrust
  until BOTH are gone") should be revisited in the same pass, since a real per-hardpoint propulsion
  contribution is exactly what the recompute produces.

**Item 9 changes behaviour that already exists.** `RefactorSystem` today deletes a hardpoint and
*returns its `MountedModules` to `CargoHold`*. The new rule is the opposite: refuse the deletion
while modules remain. The `CargoHoldHasRoomFor` check that guards the current return path becomes
unnecessary for this case, because nothing is returned.

**Enforce in the system, not only the menu.** `features.md` §2.2 states this explicitly, and it is
the same reasoning §2.3 gives for blueprint validation — a UI-only rule is bypassed by a corrupt
save or a wire packet. The menu greys the action out; `RefactorSystem` refuses the intent. Both.

#### 10 — Crew modules need content before code

A crew module is a `ModuleDef` with a skill rating and a mountability rule. **No system reads it
yet**, and §2.4 deletes abstractions with no consumer, so the ordering is: author the module kind and
its mountability first, then wire the first reader (`TargetingSystem`'s roll bias, or
`CommanderSystem`'s order quality) as its own issue. Do not land a `SkillRating` component that
nothing queries.

The open typing question (one `ModuleKind` gated by `ShellRole`, or two) is a `features.md` §2.7
decision and blocks the content authoring, not the code.

#### 11 — Docking

Two halves, and only the second is new work:

- **Cannot be shot** — already built. `DockingSystem` removes `Targetable` on dock. Keep it.
- **Dies with its host** — not built. When a rig carrying a `DockingBay` hardpoint is destroyed,
  every rig `Docked` to it must be destroyed too. `DamageSystem` is where rig death is decided, and
  it must run after the bay's destruction is known — it already runs last, so the ordering holds.
- **Leave wrecks.** `features.md` §3.4 notes that a station's destruction should leave the wrecks of
  what was inside, so the recovery run (§12.5) is reachable from a dock death. That reuses
  `LootSystem`'s existing `DeathWreck` path rather than adding one.

#### 13 — Manufacturing is a new system, and it is the missing consumer

`ConstructionSystem` keeps ships and stations: building a vessel *is* factory assembly, which is why
that file carries the codebase's one narrow named exemption to `check_layers.py`. Manufacturing a
module or component produces **inventory**, not an entity — routing it through the same file would
widen that exemption to cover work that never needed it.

A separate system is justified here on §2.4's own test, unlike deconstruction (§12.13 item 5): it
does **not** share `EngineerSystem`'s gate. Engineering requires a living `FacilityKind::Engineering`
hardpoint; manufacturing requires a Manufacturing one, plus a knowledge-network check
(`ctx.knowledge`) that no existing system performs. Its closest structural sibling is
`ResearchSystem` — a facility-hosted job queue advanced against `dt`.

**This is what closes the research loop.** `ResearchSystem` grants an unlock into a network today
and nothing consumes it. Manufacturing reads that network as its gate. Until it exists, research's
entire payoff is unreachable, which is the single clearest instance of the producer/consumer
asymmetry §12.13 opens with — here inverted, with the consumer missing rather than the producer.

### 12.17 Galaxy Topology — the missing prerequisite

**Nothing in `features.md` or this document named this, and roughly half of §12 waits on it.**
`SpaceFlight::WarpToSystem` derives a destination seed with `std::hash` (§12.15's 🐛), and its comment
presents that as a placeholder awaiting a topology store. The real finding is what the absence blocks:
**systems are identified by a string id (`"sol"`) and carry no galactic coordinate at all.**

Without coordinates there is nothing for `core/galaxy/Seeding.h`'s cascade to derive from, so the
seeding work landed complete and unusable. The same gap blocks navigation-map Zoom Levels 1–2 (§12.6),
strategic fleet records (`features.md` §4.2), per-system fog (`features.md` §8.3), territory
adjacency, and §6.4's "two rivals sharing a border."

**Home:** `core/galaxy/Topology.h/.cpp` — `sr_core`, no raylib, no registry.

**Types:**

| Type | Notes |
|---|---|
| `SystemCoord` | Integer grid coordinate. `int32` per axis (±2.1 × 10⁹), so extent is not the binding constraint |
| `SystemId` | **Derived from the coordinate**, not authored. A system's identity *is* its location |
| `SystemRecord` | Coordinate, discovered display name, faction claim, and the per-system state `features.md` §7.2 lists as persisted |
| `Topology` | The store: coordinate ↔ id, neighbour queries, warp-route adjacency |

**Identity is location-derived, and that is the decision the rest hangs on** (settled 2026-08-08).
Systems are procedurally generated from position, so position is the only stable identity available —
a name is display text layered on top, assigned on discovery and freely changed. This is what lets
§7.1's cascade produce a system on demand with no stored record, and it is what removes the
`std::hash` placeholder: the seed comes from `Topology` + `Seeding`, never from hashing a string.

**Systems:** none. Like `KnowledgeNetwork` (§12.1) this is a store, not a ticking system.

**Persistence:** records exist only for systems something has touched. A never-visited system is seed
output and costs nothing.

> ⚠️ **The rule that decides whether the galaxy scales, and it is easy to break silently:**
> **the macro tick iterates existing `SystemRecord`s, never the coordinate space.** With it held, a
> billion-system galaxy costs what a two-thousand-system one costs (`features.md` §9.1). One wrong
> loop caps galaxy size permanently and presents only as "the game got slow."

**Tests:** coordinate ↔ id round-trips; the same coordinate yields the same id on every platform (the
same hardcoded-expected-value discipline §12.4 requires of `Seeding`); neighbour queries are
symmetric; a system with no record still resolves to a seed.

**Blocks:** §12.6 (nav map levels 1–2), `features.md` §4.2 strategic fleets, §8.3 per-system fog,
territory adjacency, and the `std::hash` fix. **Nothing blocks it.** It belongs near the front of the
queue.

### 12.18 Manufacturing — the missing consumer, specified

*§12.16 item 13 established that manufacturing needs its own system and why. This is the five-part
treatment it never got. `features.md` §2.8 is the design.*

**Home:** `modes/space/systems/ManufacturingSystem.*` (Tier 1–2) + `core/galaxy/ManufacturingRecord`.

**Why not `ConstructionSystem`.** Building a vessel *is* factory assembly, which is why that file
carries the codebase's one narrow named exemption to `check_layers.py`. Manufacturing a module, shell,
or craft produces **inventory**, not an entity — routing it through the same file would widen that
exemption to cover work that never needed it.

**Why not `EngineerSystem`** — the same test §12.13 item 5 used to keep deconstruction *out* of a new
file, applied here and giving the opposite answer. It does not share the gate: engineering requires a
living `FacilityKind::Engineering` hardpoint, manufacturing requires a `Manufacturing` one **plus** a
knowledge-network check (`ctx.knowledge`) that no existing system performs.

**Types:** `ManufacturingJob { ItemId item; ItemKind kind; float progress; ... }` on the station's
facility component — the same shape as `ResearchJob`, on a **separate queue**. Needs the
`ItemId`/`ItemKind` pair `features.md` §2 introduces, since output may be a module, a shell, or a
~~craft~~ **Material**. `ResearchJob::item`'s `ModuleId` typing has the same problem and should widen
in the same pass (§12.16 item 21).

> ⚠️ **Superseded in shape by §12.30.8**, which is this system's screen. The job gains a
> **`MountId facility`** — the queue lives on a per-*station* component, and every grade-dependent
> rule (duration, slots, the destroyed-bench gate) needs to know which bench it is on; the id/kind
> pair becomes §12.19's **`ItemRef`**, since grade is not optional and an unlock is keyed on
> `(ItemId, Grade)`; and it gains **`remaining`** and **`pendingOutput`** for the per-unit
> consumption rule. §12.30.8 amends `ResearchJob` with the same `MountId` for the same reason.

**Systems:** `ManufacturingSystem::Tick` advances jobs against `dt` and `FacilityStats::ratePerSecond`;
concurrent slots are `FacilityStats::capacity`. **This retroactively gives `ResearchSystem` a slot
limit it lacks** — today a station may hold unbounded concurrent research. On completion the item
lands in `CargoHold` with **quality rolled fresh in its grade's band** (`features.md` §2.7), which is
what makes wide top-end bands safe.

**Job duration is derived, not authored** (`features.md` §2.8): base time **doubles per grade**
(craft base 5s, module/shell base 10s) and facility grade divides it by up to ~3.3×. A **vessel's**
build time is the sum of its parts' build times × an assembly factor — so mass, base price, and build
time all derive from the recipe by one rule (§12.19), and none of the three is authored per item.

⚠️ **`ConstructionSystem` needs a build timer it does not have.** Vessel assembly is instantaneous
today. Deriving vessel time from parts makes a capital under construction a raidable window, which is
a feature rather than a regression — but it is a behaviour change to a built system and belongs in
its own issue.

**Research cannot fail** (`features.md` §2.4). The sample-survival roll *is* the failure mechanic; a
second roll would compound two probabilities into four outcomes, two of which are indistinguishable
to the player, on top of material and time costs. Do not add one.

**Persistence:** `core/galaxy/ManufacturingRecord`, exactly parallel to the built `ResearchRecord` —
demotion writes it and tears down the entity job, promotion re-instantiates with elapsed time banked.
A job must not silently freeze or vanish because the player warped away.

**Tests:** refused without a living Manufacturing facility; refused when the design is absent from the
actor's network; refused when inputs are unaffordable, **before** mutating anything (the "a refused
build never costs anything" rule `ConstructionSystem` documents); a demote→promote cycle resumes at
caught-up progress; concurrent jobs capped at `capacity`; two units of one item roll independent
qualities.

**Depends on:** `core/knowledge/KnowledgeNetwork` (#78) for the gate, and on an Element/Material content
set that does not exist — `data/base_game/` holds only `modules.json`, `shells.json`, and `ships.json`.
**That content pass is the real blocker**, and `features.md` §9 now names it as such.

**This is what closes the research loop.** `ResearchSystem` grants an unlock into a network today and
nothing consumes it.

### 12.19 The Item Model — `features.md` §2.10

*Rewritten 2026-08-10, scoped against what the six specified docked surfaces (§12.30.2–.7) actually
ask of it rather than against `features.md` §2.10 alone. Every claim below was verified against
`src/` by grepping for readers and callers. **This section was written before §2.10's 2026-08-09
rewrite and is five statements behind it**, all superseded in place below. It also revises one
signature in §12.30.4, one key in §12.30.3, one request type in §12.30.6, and one scheduled
line-fix in §13.5 group 2.*

**The content set has three files and needs six.** `data/base_game/` holds `modules.json` (7
entries), `shells.json` (7), and `ships.json`. There is no `elements.json`, no `materials.json`, no
`ItemId`, **no `Grade` and no `Quality` anywhere in `src/`** — a grep for either returns nothing but
comments. `ResearchJob::item` is typed `ModuleId` with a comment admitting the gap (§12.16 item 21).
Everything §2.4 prices in materials has, until now, had nothing to be priced in.

**Home:** `shared/blueprints/` for the types, `core/registries/` for the parsers, `data/base_game/`
for the content.

#### The thesis, because three axes keep being collapsed into one

> **Grade buys breadth. Composition buys depth. Quality buys magnitude. No two of them touch the
> same number.**

That sentence is the whole model, and every correction below is a consequence of it. `features.md`
§2.10 already forbids confusing the second and third — *"attributes propagate; quality is rolled
once"* — and §2.7 already forbids applying a tier twice to one property. What neither says is that
there are **three** axes, that only one of them is a roll, and that the first two are absent from
this section entirely as it stood.

#### What the six specified surfaces demand — checked one at a time

*This is the requirements list, taken from §12.30.2–.7 rather than from §2.10, and every row was
tested for whether it was satisfiable against this section as written.*

| # | Demand | Named in | Satisfiable? | Answered in |
|:--:|---|---|---|---|
| 1 | `BaseValue(ItemId, content)` | §12.30.3 | ❌ An `ItemId` carries no grade, and the recipe's size *is* a function of grade | *Grade is a property of the instance* |
| 2 | `LocalPrice(ItemId, quantity, stock, content)` walking the drawdown | §12.30.3 | ⚠️ Needs the grade, and needs a **stated scarcity key** — the stack key cannot be it | *Two granularities* |
| 3 | `RepairCostPerHp(const ShellDef&, Grade, content)` reading §2.10's **Inert** | §12.30.4 | ❌ **Wrong twice.** `ShellDef` has no attributes — Inert is an *element* property that propagates to an instance — and **a hardpoint does not record which shell it is** | *The corrected signatures* |
| 4 | `ItemId`/`ItemKind` widening `BuyItemRequest`, `SellItemRequest`, `TransferItemRequest`, `DeconstructModuleRequest`, `ResearchJob::item` | §12.30.3, .5, .6 | ✅ Stands — with one addition: a request must name an **instance**, not a def | *The types* |
| 5 | `CargoHold` becomes one `std::vector<ItemStack>` | §12.30.3 | ⚠️ Stands as a shape; **the key is not `ItemId`** | *Stacking is by indistinguishability* |
| 6 | Grade as the single ladder — `FacilityStats::level` folds in, and so does `StationFacility::researchTier` | §12.19, §12.30.6 | ✅ Stands, and it **revises §13.5 group 2's `level` fix** | *Grade is a property of the instance* |
| 7 | `Quality` per instance — merge's band clamp and `RigState` both block on it | §12.21, §12.30.5, §12.31 | ✅ Stands; this section settles **where it lives** | *Three facts per instance* |
| 8 | `baseDuration(item.grade)` for research | §12.30.6 | ✅ **Confirmed — and it is a quote, not a fill** | *Duration* |
| 9 | `Recipe` as the single answer to *"what is this made of"* — deconstruction reads it backwards | §12.30.5 | 🐛 **No.** A recipe names **roles**, so it cannot say what any particular item is made of | *A recipe is a demand* |

**Rows 3 and 9 are the two that change the model.** The rest are corrections to a signature or a key.

#### This section is five statements behind `features.md` §2.10

*§2.10 was substantially rewritten on 2026-08-09 — the supply tiers were renamed, the rarity bands
were deleted, elements gained an eight-attribute vector, and recipes stopped naming elements. This
section predates all four, and §13.5 group 2b already records that the rename **flips the meaning**
of words used here.*

| Was | Is |
|---|---|
| ~~`MaterialDef` — the raw tier~~ | **`ElementDef`.** `Element` is the raw tier and `Material` the manufactured one (§13.5 group 2b) — the two names in this section's own type table are **inverted** |
| ~~`CraftDef` — id, display name, per-material weighting~~ | **`MaterialDef`** — id, display name, abbreviation, per-**role** weighting |
| ~~`ItemKind` = `Material · Craft · Module · Shell · Vessel`~~ | **`Element · Material · Module · Shell · Vessel`.** *Craft* is retired vocabulary — `features.md` §2 rules that a vessel is a vessel, never a craft, and §13.5 already carries a 🐛 for the one surviving string in `Validation.cpp` |
| ~~`MaterialDef` carries an **availability band**~~ | **Deleted 2026-08-09** with the rarity bands. *"No element is rarer than another"* — there is nothing for a band to express |
| ~~**Authored mass and authored base price** per raw item — *"the only two authored numbers"*~~ | **One authored mass per element, and exactly one authored price for all of them.** §2.10: *"One unit of any element is 1 credit."* Price is not authored per element at all |
| ~~*"Fourteen authored masses and fourteen authored prices"*~~ | **~50 authored masses and one authored price.** Fourteen was the roster before 2026-08-09; it is now 36 drafted toward ~50, and the price column collapsed to a single constant |

*Nothing above is a design change. It is this section catching up to a rewrite that happened a day
after it was written, and it matters because §12.30.3's `Pricing.h` and §12.30.5's deconstruction
were both scoped against the stale wording.*

#### 🐛 A recipe is a demand, not a bill of materials

*This is the finding that reshapes the section, and it invalidates a sentence in §12.30.5 unless
one clause is added to it.*

`features.md` §2.10 settles, and calls it *"what makes the whole model safe"*:

> *"A Field Emitter recipe does not ask for 5 × Neodymium. It asks for five units of a **magnetic**
> element — and Nd, Sm, Co and Fe all qualify, **with different results**."*

**So the recipe cannot say what any particular item is made of.** Two Common Alloy Plates built from
the same recipe at the same bench are iron in a poor system and iridium in a rich one, and §2.10
requires them to differ — *"a Conductive Coil made of silver genuinely is a better coil than one made
of aluminium."* Three settled mechanics read *what actually went in*, and none of them can get it
from a def:

| Reader | Needs | Named in |
|---|---|---|
| Attribute propagation | The elements consumed | §2.10, and §11.9 records it is computed in `ManufacturingSystem` and nowhere else |
| An item's **mass** | The same — density is §2.10's universal cost, so a silver coil weighs more than an aluminium one **by design** | §2.10's attribute table |
| `RepairCostPerHp`'s **Inert** term | The same, one hop further up | §12.30.4 |

> **The recipe is the demand. The composition is what the demand got. Only the second is a fact
> about the object, and it is not derivable from the first.**

⚠️ **This does not make §12.30.5's deconstruction rule wrong — it needs one clause.** That section
deletes `deconstructsTo` and rules that *"deconstruction yields the item's own `Recipe` inputs at the
facility grade's recovery band."* **The rule stands and the deletion stands**; what "the recipe's
inputs" resolves to is settled two subsections below, and it is deliberately **not** the item's own
composition.

#### Three facts per instance, and only one of them is a roll

| Fact | Set by | Rolled? | Stored? |
|---|---|:--:|---|
| **Grade** | The builder — the recipe you chose to run; or §2.7's drop table for loot | No | Yes |
| **Composition** | The builder's stock, resolved slot by slot at manufacture | No | **No — see below** |
| **Quality** (§12.21) | §2.7's band, once, at creation | **Yes** | Yes |

**Composition is not stored, and that is the one non-obvious call in this section.** What the
consumers above actually read is not *which* elements went in but *what that produced* — an
attribute vector and a mass. Storing the resolved bill as well would be a second answer to the same
question, which is the §13.3 C shape this document has now found five times.

> **An instance stores what the composition *produced*, never the composition itself: eight
> attributes and one mass.**

⚠️ **These are derived values that must nonetheless be stored, and the exception needs its reason
stated or someone will "fix" it.** §12.19's own discipline — and §12.21's, and §12.15's — is that a
derived value is never stored, because a stored derivation drifts against a content edit. **The
inputs here are consumed.** A module's attribute vector was derived from materials that no longer
exist, so the derivation is not repeatable and there is nothing for it to drift against. That is the
opposite of `mass = f(def)`, which *is* re-derivable and therefore must not be stored.

```
shared/blueprints/Item.h

struct Attributes {                       // features.md 2.10's eight, in its order
    float structure = 0.0f;  float conductive = 0.0f;  float semiconductive = 0.0f;
    float energetic = 0.0f;  float magnetic  = 0.0f;   float optical       = 0.0f;
    float thermal   = 0.0f;  float inert     = 0.0f;
};

struct ItemRef      { ItemId id; Grade grade = Grade::Common; };            // blueprint form
struct ItemInstance { ItemRef item; Attributes attributes; float mass = 0.0f; Quality quality; };
struct ItemStack    { ItemInstance instance; int quantity = 0; };
```

**`ItemRef` and `ItemInstance` are Law 3 in miniature** — the blueprint form names a design and a
grade, the live form adds what one particular unit turned out to be. A Template can say *Mythic
chassis*; it cannot say *made of iridium*, because that depends on the yard that builds it.

##### How attributes propagate — the rule §2.10 states and does not specify

§2.10 says attributes propagate by *"sums and weighted averages"* and never says which is which. The
split is forced by what the numbers mean:

> **Attributes average; mass sums.**

An attribute is a property of the *substance* — a coil made of four units of silver is not four times
as conductive as one made of one unit. Mass is a property of the *quantity*. So:

```
attributes(item) = Σ(inputᵢ.attributes × unitsᵢ) / Σ(unitsᵢ)      // quantity-weighted mean
mass(item)       = Σ(inputᵢ.mass × unitsᵢ) × gradeMassMultiplier   // features.md 2.7's 100% -> 70%
```

**Two hops, and no more** (§2.10's chain): Element → Material → Module/Shell. An Element's attributes
come straight from `ElementDef`; its mass is its authored density × one unit.

*This is what makes density-as-universal-cost actually cost something.* Reaching for iridium raises
the Structure average **and** the summed mass, so §2.2's constraints puzzle is played at the recipe
as well as at the fit. Under a summing rule it would not be — more units would mean more of
everything, and the choice would collapse.

##### How an attribute vector reaches a stat, and why it is not a new mechanism

§2.10's table already names a consumer per attribute (Structure → `Health.max`, Thermal →
`Weapon::fireIntervalSeconds`, Semiconductive → `Weapon::spreadRadians`, …) and rules that *"an
attribute with no reader is the same defect as a system with no producer."* Two of those eight —
`spreadRadians` and `fireIntervalSeconds` — **improve by decreasing**, which is the exact hazard
§12.21 built `Direction` for.

> **The attribute→stat map is a `StatPool` in `StatPool`'s own shape** — `{ StatRef stat; float
> weight; Direction dir; }` per attribute per `ModuleKind`, authored in JSON, validated by the same
> CI rule that rejects a pool entry naming an enum or an integer field.

One mechanism, two producers (a quality roll and a composition), and no second table to keep in sync.
**Order of application is fixed and must be:** `stat = def.stat × attributeFactor × qualityMultiplier`.

⚠️ **`attributeFactor` is bounded at roughly ×0.8–1.2 and that bound is a working value, not a
settled one.** Quality reaches ×5.0; composition must stay the *smaller* axis or the grade ladder
stops reading. Like §2.10's quantity curve this is a number for `tools/economy_sim` to produce, not
for this document to argue.

#### Grade is a property of the instance, never of the def

*Neither this section nor `features.md` §2.7 ever says which, and every consumer below needs the
answer.*

`features.md` §2.7 settles it implicitly and decisively: *"a shell's authored `moduleSlots` is its
Common value and the tier adds to it."* **A def is the Common baseline.** So:

- **`modules.json` and `shells.json` gain no `grade` field.** An authored def is grade-neutral, and
  §2.10's *"every material exists at every grade"* is the same statement for the generated tier.
- **`MountBlueprint::shell` and `::modules` widen from `ShellId`/`ModuleId` to `ItemRef`.** Without
  it every authored ship and every player Template is all-Common, and a Template that cannot name a
  Mythic chassis makes §2.7's whole ladder unreachable from §2.2's design surface.
- **A live hardpoint gains `ShellInstance { ItemInstance shell; }`** and `MountedModules::ids`
  (`std::vector<ModuleId>`) becomes `MountedModules::items` (`std::vector<ItemInstance>`).

🐛 **A hardpoint does not record which shell it is, and §12.30.4 needs it to.** Verified
2026-08-10: `RigFactory::CreateHardpoint` emplaces `MountRef`, `ParentRig`, `ShellRole` (the *kind*,
not the id), `HitRadius`, `LocalTransform`, `WorldTransform`, `PreviousTransform`, `MountedModules`
and `Health` — **and no `ShellId`.** Today the only route from a hardpoint to its `ShellDef` is a
three-hop join: root → `BlueprintRef` → `ContentLibrary::FindShip` → `RigBlueprint::Find(MountRef)`
→ `shell`. `ShellInstance` closes it and carries the grade, the attributes and the mass that join
could never have produced.

##### `FacilityStats::level` folds in — and that revises a group-2 line-fix

⚠️ **`FacilityStats::level` (1–5) folds into `Grade` (7 tiers)**, and so does
`StationFacility::researchTier` (§12.30.6 — a *third* ladder, a bare `float`, written only by tests).
`FacilityRef::level` becomes `FacilityRef::grade`, copied from the facility module **instance** at
attach time.

**§13.5 group 2 schedules the opposite work.** Its line reads *"`ParseFacilityStats` reads `level`;
`AttachModuleComponents` forwards it"* (§13.3 K), and §12.30.5's test list asks for *"a merge at
facility level 5 preserves more than one at level 1."*

> **Neither is parsed today, so fixing it as `level` costs exactly what fixing it as `grade` costs,
> and one of the two then has to be deleted.** The group-2 fix should land as **grade**:
> `FacilityRef::grade`, forwarded by `AttachModuleComponents`, and `FacilityStats::level` deleted
> rather than revived. §12.30.5's test becomes *"a merge at a Mythic bench preserves more than at a
> Common one."*

*This is the cheaper order and it is only visible because the two sections were read together — on
their own each is correct.*

#### `ItemInstance` is a value, and that retires `RegisterCraftedModule`

*This reverses `features.md` §2.7's implementation note, and the reason is that §12.21 already
settled the opposite one section later.*

There are two possible homes for per-instance state, and the codebase currently contains the first:

| | Where instance state lives | Cost |
|---|---|---|
| **A — the runtime overlay** | `ContentLibrary::craftedModules_`; every rolled instance registers as a new `ModuleDef` with a generated id, and every holder keeps carrying a bare `ModuleId` | The def table grows one entry **per manufactured unit**, must be saved (§12.31), is never collected, and needs generated ids |
| **B — the value record** | `ItemInstance` travels inside `CargoHold`, `MountedModules`, requests, loot and saves | Every holder widens once |

**`features.md` §2.7 chose A** — *"a rolled instance registers through the identical path"* —
pointing at `RegisterCraftedModule`. **§12.21 chose B one section later**, and states it as a
persistence rule: *"an instance stores its budget point and chosen distribution, not its derived stat
block — the block recomputes from `def + quality` at load."* §12.31's test list repeats it: *"a
restored rig re-derives its stat block from `def + quality`."*

> **B is correct and A cannot survive manufacturing.** A merge is a rare event, so an overlay entry
> per merge went unnoticed; **manufacturing is a loop.** A player who builds ten thousand modules
> gets ten thousand permanent definitions, saved forever, with no owner and no collector — and every
> one of them is a *definition* describing a single object, which is a category error before it is a
> leak.

**What this retires, all verified:**

| | Today | After |
|---|---|---|
| `ContentLibrary::RegisterCraftedModule` + `craftedModules_` | One gameplay caller (`EngineerSystem.cpp:146`) and three tests | **Deleted.** A merge writes a new `Quality` onto the primary instance; §2.4's merge moves quality within a band, and a band move is not a new definition |
| `SystemContext::craftedModules` (`System.h:75`) | A second `ContentLibrary*` beside `ctx.content`, nullable, read by one system | **Deleted with it** |
| §12.30.5's 🐛 *"the crafted id grows without bound"* | `a+b@L1+a+b@L1@L1`, exponential in merge count, and it is a map key | **Dissolved.** There is no generated id, so there is nothing to concatenate |
| §12.31's 🐛 *"a crafted module does not survive its own process"* | `craftedModules_` is serialized by nothing | **Dissolved.** The instance is inside the hold and the hold is already in the save |

*Three findings in three sections turn out to be one finding: instance state was living in a
definition table.* ⚠️ **The overlay itself is not deleted — it is on the wrong type.** See §12.30.8,
where a drafted Template is the definition that genuinely needs one and does not have one.

#### Stacking is by indistinguishability — this revises §12.30.3's key

§12.30.3 settles that *"`CargoHold` holds one `std::vector<ItemStack>` **keyed on `ItemId`**"* and
argues that *"uniform stacking is what keeps ~50 Elements and 8 Material families across 7 grades
from becoming several hundred single-unit rows."* **The shape is right and the key is too narrow** —
under it, a Mythic-rolled cannon and a Common one stack, and the hold forgets which is which.

> **Two units stack when they are indistinguishable: same `ItemId`, same `Grade`, same attributes,
> same mass, same `Quality`. A rolled instance is distinguishable by construction.**

One rule, and it produces exactly the behaviour each tier already wants:

| Tier | Quality? | Stacks |
|---|:--:|---|
| **Element** | — | Always. Mined iron is mined iron; ~50 rows is the ceiling for the whole tier |
| **Material** | ❌ §2.10: materials carry a grade and **do not roll quality** | **Per production run.** A hundred coils off one bench from one stock are one row of 100 |
| **Module · Shell** | ✅ | Effectively never — which is what §12.30.3 already observes happening today, and is correct rather than a defect |

**This resolves §12.30.3's stacking-inconsistency finding without forcing uniformity.** That finding
— *"buying three of the same module makes three rows; mining three iron makes one"* — is real, and
its cause is that the two halves had two *rules*. One rule that happens to produce different
outcomes for rolled and unrolled items is not the same defect; it is the rule explaining itself.
And the failure §12.30.3 actually cared about, mining producing hundreds of single-unit rows, is
fixed outright, because an Element has nothing to distinguish it by.

#### Two granularities: the stack key is not the price key

*Found by asking what `LocalPrice`'s scarcity is measured against, which §12.30.3 leaves as "the
station's hold."*

If scarcity were measured per **stack key**, every rolled instance would be its own market of one:
infinitely scarce, infinitely expensive, and the drawdown curve would never move.

> **Price is keyed on `(ItemId, Grade)`. A stack is keyed on the whole instance. `LocalPrice` walks
> the drawdown across every stack sharing the price key.**

⚠️ **And base value must carry a quality term, which revises §2.10's *"base value = its recipe,
recursively down to elements."*** Without one, the settled no-spread rule becomes an exploit:

- §12.30.3 settles that buying and selling at one station nets **exactly zero**.
- If price ignores quality, a player buys ten Mythic modules, keeps the 5.0 roll, and **sells nine
  back for exactly what they paid.**
- That is a free reroll machine, and it bypasses precisely the brake `features.md` §2.8 built:
  *"an unconstrained player would spam Mythic production fishing for a 5.0 roll; each attempt costing
  a full Legendary-Material pipeline run is the brake."* **The market is a cheaper slot machine than
  the factory.**

> **`BaseValue = recipeValue(ItemId, Grade) × quality.multiplier`.** The recipe measures what went
> in; quality measures what came out; a market that prices only the first lets a player launder rolls
> at zero cost.

*This does not disturb §2.10's actual rule, which is that value is a property of **the design, not of
where it was built**. A facility grade still changes nothing about value — that remains cost-to-build.
Quality is a property of the object.* Elements and Materials roll no quality, so the term is identity
for both and the two lower tiers price exactly as §2.10 describes.

#### Derivation, restated with all three axes in it

```
n(G)          = distinct role slots, 2 at Common .. 8 at Mythic    // features.md 2.10's grade table
unitsPerSlot(G)                                                     // ~2x per grade, 2.10's knob 1
massMult(G)   = 1.00 .95 .90 .85 .80 .75 .70                        // features.md 2.7's mass ladder
band(G)       = x0.90-1.10 .. x3.00-5.00                            // features.md 2.7's quality band
timeFactor(F) = 1.00 .88 .76 .64 .52 .40 .30                        // features.md 2.4, facility grade

recipe(item, G)   = n(G) role slots, filled per 2.10's weighting, unitsPerSlot(G) each
attributes(item)  = quantity-weighted mean of the inputs' attributes
mass(item)        = sum of the inputs' masses * massMult(G)
recipeValue(item, G) = sum over inputs of recipeValue(input) * (1 + margin)   // element = 1 credit
BaseValue(item)   = recipeValue(item.id, item.grade) * item.quality.multiplier
duration(item, F) = baseTime(kind) * 2^(G-1) * timeFactor(F)
```

**One authored mass per element and one authored price for the whole roster; everything else
follows.** This is the same discipline §12.15 applies to system radius and §12.4 to seeds.

##### Duration — §12.30.6's fill is confirmed, and it is a quote

§12.30.6 settles `duration(item, facility) = baseDuration(item.grade) × gradeTimeFactor(facility.
grade)` and flags the left-hand factor as *"a fill rather than a quote: §2.4 settles the right-hand
factor and is silent on the left."* **§2.4 is silent; `features.md` §2.8 is not**, and it authors
both halves:

| | `features.md` §2.8 | This formula | |
|---|---|---|:--:|
| Research, Common target | 1m | 60s × 2⁰ × 1.00 | ✅ |
| Research, Mythic target | ~64m | 60s × 2⁶ = 3,840s | ✅ |
| Research, Mythic target at a Mythic lab | ~19m | 3,840s × 0.30 = 1,152s | ✅ |
| Manufacturing, Mythic module | *"about three minutes"* at a Mythic bench | 10s × 2⁶ × 0.30 = 192s | ✅ |

**Four values, four agreements, to the digit.** `baseTime(kind)` is §2.8's own column — **5s** for a
Material, **10s** for a Module or Shell, **60s** for a research job — and a vessel's is
`Σ(parts) × an assembly factor` rather than a grade. §12.30.6's note should be upgraded from *fill*
to *quote*; the gap was in §2.4, and §2.8 had already closed it.

##### Deconstruction — what "reads the recipe backwards" resolves to

A recipe names roles, so reading it backwards yields roles, and roles are not matter. **The missing
definition is the nominal fill**, and it needs no authoring:

> **The nominal fill of a role is the roster's lowest-density element scoring ≥ 1 in that role.**
> Cheapest available — which is exactly what generic reclaimed stock should be.

**Three consumers, so it is not a §2.4 dead abstraction:**

1. **Deconstruction yield** — the nominal fill of the item's recipe at the facility grade's recovery
   band (§2.4's 20–45% … 80–100%).
2. **An authored or looted instance's attributes and mass.** A `pulse_cannon_i` picked up off a wreck
   was never manufactured and has no composition; it takes the nominal fill's. No content authoring,
   no dependency on where it dropped.
3. **`recipeValue`** for anything not manufactured.

⚠️ **Conservation is enforced by mass, not by count.** §2.10's rule is that deconstruction *"returns
up to the item's own mass, never the mass originally consumed."* A lithium-built module reclaimed at
a nominal fill heavier than lithium would return **more** mass than it contained. **Scale the
recovered bill so its total mass never exceeds the instance's `mass`** — one clamp, and §2.10's
conservation rule becomes mechanical rather than aspirational.

*Losing the premium composition on deconstruct is correct rather than stingy: §2.10 already rules
that build → deconstruct → build always loses, and losing the good feedstock is one more form of the
same loss — legible, and it bounds an instance to four fields instead of a nested bill.*

#### ⚠️ The cost curve compounds three knobs, and §2.10 counts one

*Not a proposal — a check that §2.10 invites and that nobody has run.*

§2.10 lists **three knobs** (quantity per grade, refinement loss, margin), revises quantity from ~3×
to ~2×, and validates the result against §2.4's hardest constraint: *"cost ×64, combat value ~×6."*
**Two of the largest terms are not in that ×64:**

| Term | Across the ladder | In §2.10's 64×? |
|---|---:|:--:|
| Quantity per slot, ~2×/grade | ×64 | ✅ |
| **Distinct slots, 2 → 8** — §2.10's own grade table | **×4** | ❌ |
| **The input-grade chain** — §2.8: a grade-*N* item needs grade ≥ *N*−1 Materials, each themselves ×256 | **×10²ish** | ❌ |

Multiplied out, a Mythic module costs on the order of **10⁴** Common modules, not 64. §2.4's
constraint (*cost must outpace stat benefit*) is satisfied by an enormous margin — **which is its own
problem**, because §2.10's governing principle is *"industry gives you more of it"* and a 10⁴
multiplier is the scarcity ladder rebuilt on the cost side, which is the exact failure the 3× → 2×
revision was made to avoid.

> **The ~2× quantity knob was chosen against a one-knob model. Against three it is probably ~1× —
> breadth alone already delivers ×4 per rung and the input chain delivers the rest.**

✅ **Built and run 2026-08-11.** §2.10 ruled that *"prices are outputs"* and named
`tools/economy_sim` as what settles it. **This finding promoted `economy_sim` from 🧊 to a
prerequisite for authoring the content set** — the curve cannot be read off three compounding knobs
by inspection — and the tool now exists and confirms exactly this shape: the quantity-per-grade knob
is settled at ~1×, not the ~2× this section's own quote above still shows as a live guess. See
`features.md` §2.10 for the table.

#### The corrected signatures

```
core/economy/Pricing.h                         free functions beside FactionEconomy, sr_core, no raylib

int BaseValue (const ItemInstance&, const ContentLibrary&);
int LocalPrice(const ItemInstance&, int quantity, const CargoHold& stock, const ContentLibrary&);
int RepairCostPerHp(const ItemInstance& shell, Grade facilityGrade, const ContentLibrary&);
```

| Was (§12.30.3, §12.30.4) | Why it changes |
|---|---|
| `BaseValue(ItemId, const ContentLibrary&)` | An `ItemId` names a design; the recipe's size and the quality term are both properties of the instance |
| `LocalPrice(ItemId, int, const CargoHold&, …)` | Same, plus: the drawdown is walked over the **price key** `(ItemId, Grade)`, not over the stack key |
| `RepairCostPerHp(const ShellDef&, Grade, …)` | **A `ShellDef` has no Inert attribute and no grade.** Inert is an element property that propagates to an instance, and §12.30.4 is the only named reader of it |

**Everything else about those three functions is unchanged** — pure, stored nowhere, ticked never,
called by the screen to display and by the system to charge (Law 9), and `LocalPrice` still prices a
**quantity** by walking the curve rather than multiplying a spot price.

#### The types

| Type | Home | Notes |
|---|---|---|
| `Grade` | `shared/blueprints/Taxonomy.h` | The seven-tier ladder, beside the four enums content and components already share. `ToString`/`FromString` like the rest, and **`FromString` must reject rather than default** — Law 10, and §12.30's `kind`-is-optional 🐛 is the cost of the alternative |
| `ItemKind` | `Item.h` | `Element · Material · Module · Shell · Vessel` |
| `ItemId` | `Item.h` | `{ ItemKind kind; std::string id; }`. **Does not replace `ModuleId`/`ShellId`** — those stay strong types for kind-specific lookups; `ItemId` is the kind-agnostic envelope, with conversions each way |
| `ItemRef`, `ItemInstance`, `ItemStack`, `Attributes` | `Item.h` | Above |
| `ElementDef` | `ElementDef.h` | id, name, **periodic abbreviation** (§12.30.3's `Row::glyph` spends it), authored density, eight authored attributes 0–3. **No price and no availability band** |
| `MaterialDef` | `MaterialDef.h` | id, name, abbreviation, per-**role** weighting. **Recipes generated**, never authored per grade |
| `Recipe` | `Item.h` | `n(G)` **role slots**, generated for a Material and authored-as-roles on `ModuleDef`/`ShellDef`. Never a list of element ids |
| `Quality` | §12.21 | Unchanged. Identity on Element and Material — §2.10: they carry a grade and roll no quality |

**Widened, not new:** `CargoHold` → one `std::vector<ItemStack>`; `MountedModules::ids` →
`::items` of `ItemInstance`; `MountBlueprint::shell`/`::modules` → `ItemRef`; `LootDrop`/
`MaterialDrop`/`DeathWreck` → `ItemStack` (which also ends §13.5 group 2b's `MaterialStack`/
`MaterialChance`/`AsteroidComposition` string-id problem in the same pass); `FacilityRef::level` →
`::grade`; **`CargoHold` from a rig-root entry count to a per-bay `{ stacks, slotCount, slotCapacity }`**
(§12.23 — `CargoHoldEntryCount` and `CargoHoldHasRoomFor` are deleted and re-expressed as
`shared/rig/CargoView.h` over the `ItemStack` masses this section derives); new `ShellInstance` on
every hardpoint.

**Deleted:** `ContentLibrary::RegisterCraftedModule`, `craftedModules_`,
`SystemContext::craftedModules`, `FacilityStats::level`, `StationFacility::researchTier`,
`CargoHold::modules`/`::materials`, `MaterialStack`.

**Systems:** none new beyond `ManufacturingSystem` (§12.18, screen at §12.30.8). This is content,
derivation, and one widening.

#### Validation and CI

- ⚠️ **`Validation.h` rule 13** — a shell's recipe-derived mass lands within a wide band of
  `k · density · radius²`. A check on the recipe, not a formula for mass.
- **Rule 14 — role coverage.** Every recipe slot names a role that at least one element in the
  roster scores ≥ 1 in. §2.10's seeding invariant is unsatisfiable otherwise, and *"a player can
  never be hard-stuck"* stops being an enforced property.
- **`tools/element_check`** (§13.5 group 2c) is unchanged and becomes blocking rather than advisory:
  pairwise dominance, Pareto validity, role coverage, density spread.
- **The attribute→stat map reuses §12.21's CI rule** — reject an entry naming an enum or an integer
  field. One rule, two producers.

#### Persistence

`ItemStack` is POD and encodes as one — `ItemId`, `Grade`, eight floats, a mass, a `Quality`, a
count. `SaveFile` and `SaveMigrator` both bump. **`RigState` (§12.31) carries `ShellInstance` and
`MountedModules::items` per mount**, which §12.31's mount list already has a slot for and its test
list already half-states — *"a restored rig re-derives its stat block from `def + quality`"* becomes
*"from `def + attributes + quality`"*, and the crafted-module save section it scoped is deleted
rather than written.

#### Tests

- Derived mass, attributes and base value are identical for the same inputs on every platform.
- A generated Material recipe honours §2.10's grade table — `n(G)` distinct roles, no slot naming an
  element — at every one of the seven tiers.
- **A silver Conductive Coil and an aluminium one of the same def and grade have different
  attributes and different masses, and the same `recipeValue`** — the three-axis rule asserted in one
  test.
- Attributes average and mass sums: doubling every input quantity doubles the mass and leaves the
  attribute vector unchanged.
- A ↓-direction attribute (`spreadRadians`, `fireIntervalSeconds`) **improves** with a higher score —
  §12.21's `Direction` hazard, asserted on the composition path too.
- Two units stack iff every field of their `ItemInstance` is equal; a rolled module never stacks with
  another roll of the same def.
- `LocalPrice` moves as the `(ItemId, Grade)` pool draws down and is unaffected by how that pool is
  split across stacks.
- **Buying ten and selling nine back cannot net a profit at any quality distribution** — the reroll
  exploit, asserted directly.
- Deconstruction yields the nominal fill within the facility grade's band, never above 100%, **and
  never more mass than the instance carried** — the conservation clamp.
- Rule 13 rejects a planted mass/radius mismatch; rule 14 rejects a recipe naming an uncovered role.
- `ItemStack` round-trips through a save for all five `ItemKind`s, and a rig carrying a merged module
  restores it **without any content-library entry existing for it** — the regression test for the
  overlay's retirement.

#### Scheduling

**Its own issue, and it is large.** It has no dependency on group 1 and can be verified headless, but
it touches every holder of a `ModuleId` in the tree.

| | Scope | Order |
|---|---|---|
| **19a** | `Grade` in `Taxonomy.h` · `FacilityRef::grade` · `FacilityStats::level` and `researchTier` deleted | **Replaces §13.5 group 2's `level` line.** Startable today |
| **19b** | §13.5 group 2b's Element/Material rename, folded in — one commit or none | Before any content |
| **19c** | `Item.h` · `ElementDef` · `MaterialDef` · `Recipe` · parsers · `elements.json` · `materials.json` · `element_check` · `economy_sim` | After 19b |
| **19d** | The widening: `CargoHold`, `MountedModules`, `MountBlueprint`, loot, requests, `RigState`, saves. **`RegisterCraftedModule` deleted here** | After 19c, and it wants §12.21's `Quality` to already exist |

**Depends on:** §12.21 (`Quality` is a field of `ItemInstance`) and the Element/Material content set.
**Blocks:** §12.18 and its screen (§12.30.8), §12.20's ledger, §12.30.3's Market half, §12.30.5's
Merge and Deconstruct verbs, §12.30.6's shells-as-samples, and `Pricing.h` entirely.

### 12.20 Per-Item Faction Stock — `features.md` §5.0

**`core/economy/FactionEconomy` models an abstract quantity.** `Deposit`, `Spend`, and
`TotalProduction` carry no notion of *what* is stocked, which was survivable while nothing could be
manufactured. §12.19 gives items identity and §12.18 gives factions a reason to consume them, so this
has to widen.

**Home:** `core/economy/FactionEconomy` (existing), `sr_core`, no raylib.

**Shape, and the first rule is the one that decides whether it scales:**

- **The station is the container, not the system.** `StationServicesSystem` already trades against
  "the station's own stock" (§12.10), so per-station is what Tier 1 does today; a system-level ledger
  would give the codebase two granularities for one concept. It also means **destroying a station
  destroys its stockpile**, which a per-system model silently loses.
- **Sparse, never dense.** Key on *(FactionId, StationId)* → a small **sorted vector** of
  `(ItemId, quantity)`. Ten factions × ~200 systems held × 1–5 stations × 10–30 live item types is
  ~150k–300k entries worst case; a dense table would be almost entirely zeros. A sorted vector beats
  a hash map below ~32 entries, which is where nearly every ledger sits.
- **Dual-form, like everything else crossing a tier boundary** (Law 3; §12.5's wrecks, §12.1's
  research jobs): a component on the station rig while resident, a `core/galaxy/` record when the
  system demotes. **The record holds per-station ledgers, never a system sum** — summing on demotion
  and redistributing on promotion is lossy in a way players notice. Per-system totals are a derived
  query, never stored.
- **One write path, totals maintained inside it.** §6.1's facets, Military Weight (§12.16), and
  §5.1's Three Pillars all want faction-wide aggregates. Re-summing them on the macro tick is the
  iteration wall `features.md` §9.1 warns about; a cache with invalidation rules is worse, because
  every write site that forgets produces a number the strategic simulation then decides from. **So
  there is exactly one `Deposit`/`Withdraw` API and it updates the totals as it writes.** The write
  *is* the update — the same shape §12.16 gives Military Weight, and the same enforcement-over-
  documentation principle Law 0 is built on.
- **Price is a function, never state.** Base price is §12.19's load-time derivation; the local
  supply/demand modifier is computed on query from one ledger. Nothing about price is stored or
  ticked, so a galaxy of any size costs nothing to price.

**Internal logistics is event-driven, never a scan.** A faction must move goods between its own
stations to meet a shortfall, and the naive implementation walks every ledger looking for imbalances
on the macro tick — precisely the iteration wall `features.md` §9.1 warns about. Instead: each macro
tick a faction inspects only its **jobs blocked on missing inputs** (a short list by construction),
finds the nearest own station holding a surplus, and dispatches an **in-transit fleet record** —
origin, destination, ETA, manifest, which Law 2 already models. Cap concurrent transfers per faction.
Cost scales with blocked jobs, not with holdings.

This is also the missing step in `FactionDecisionEngine`'s Material Security path: **ship before you
raid.** Today low Material Security jumps straight to expansion or raiding, which skips the obvious
option and makes factions read as unreasonable.

**Why locality is not optional.** `features.md` §5 promises "global market awareness but localized
physical inventories" — a faction knows the price of iridium everywhere and can spend only what it
physically holds, where it holds it. Per-faction totals alone cannot express that, and blockades
(§6.1's Material Security facet) stop meaning anything without it.

**Persistence:** `SaveFile` gains an item-aware stock section; `SaveMigrator` maps an old abstract
balance onto a starting ledger.

**Tests:** deposit/withdraw round-trips per system; faction totals equal the sum of ledgers after an
arbitrary write sequence (the property that catches a second write path being added later); a
withdrawal exceeding local stock fails without partially applying; save→load preserves sparsity and
totals.

**Depends on:** §12.19 (`ItemId` must exist first). **Blocks:** §12.18's manufacturing gate for AI
factions, and `features.md` §2.6's royalty accrual against real production.

### 12.21 Stat Pools and Weighted Rolls — `features.md` §2.7

**What the quality band actually needs from the schema.** §12.19 establishes that every instance rolls
a point in its grade's band; §2.7 establishes that the roll is a *budget* distributed across a subset
of the module kind's stats. Neither says what a pool entry is, and it is not just a field name.

**Home:** `shared/blueprints/` — a pool is authored content (Law 10), parsed by `core/registries/`.

**Types:**

| Type | Notes |
|---|---|
| `StatRef` | Which field a pool entry targets. Kind-scoped, so a `Weapon` entry can only name `WeaponStats` fields |
| `PoolEntry` | `{ StatRef stat; float weight; Direction dir; }` — **all three are required** |
| `StatPool` | The kind's entries. Defaults per `ModuleKind`, **overridable per `ModuleDef`** |
| `Quality` | The rolled result: the budget point plus the chosen distribution. Stored per instance |

**`Direction` is not optional.** `fireIntervalSeconds`, `spreadRadians`, and `rechargeDelaySeconds`
all improve by *decreasing*. A band multiplier applied naively makes a Mythic module strictly worse
than a Common one, and it would pass every test that only checks "the number changed."

**Weight is price per unit of relative improvement**, not importance — `improvement = share / weight`.
High weight means expensive means a roll buys less. This is the single balance dial for the whole
loot system, and it lives in JSON, so tuning never touches C++.

**Identity fields are excluded and must stay excluded**: `damageType`, `Shield::absorbs`,
`FacilityKind`, and every integer field including `projectilesPerShot`. A multiplicative band on an
`int` of 1 is still 1, and rolling 1 → 3 turns a cannon into a shotgun — an identity change, not a
quality change. **CI should reject a pool entry naming an enum or integer field**; it is a cheap
check and the failure mode is otherwise silent.

**Persistence:** an instance stores its budget point and chosen distribution, not its derived stat
block — the block recomputes from `def + quality` at load, the same way mass and price derive from
the recipe (§12.19). Storing the derived block would let a content edit and a save disagree forever.

**Tests:** a roll's total value is invariant across which stats it picks (the property the whole
weighting scheme exists to guarantee); a ↓-direction entry improves rather than degrades; a one-stat
pool applies its band directly; per-def overrides beat kind defaults; the same `(item id, tick)`
yields the same roll on every platform.

#### Three stats `features.md` §2.7 names as boost targets have no authored input

Found by grepping the systems rather than the docs, and each blocks a crew role:

| Stat | Status |
|---|---|
| `FiringArc::turnRatePerSecond` | Read by `WeaponSystem`, but **`ModuleAttachment.cpp` hardcodes it to `kPi`**. `halfWidthRadians` comes from the *mount*, not the module. Turret traverse is not authored anywhere |
| `FacilityStats::ratePerSecond` | **Zero readers.** `ResearchSystem` uses `facility.researchTier`; `DockingSystem` uses a hardcoded `kDockHealPerSecond` ported from StarReach2. An authored-but-dead field |
| `SensorRange` | The component exists and `DiscoverySystem` reads it, but **no `ModuleKind` produces it** |

So of the six boost targets §2.7 lists, three are live (weapon cooldown, shield regen, thrust/turn)
and three need a stat to exist first. **The `Operator` boost pool is therefore one entry today**, and
the repair officer role is blocked outright. Making `ratePerSecond` and `turnRatePerSecond` flow from
`ModuleDef` is a small change that unblocks two roles and retires one dead field — it belongs ahead of
the crew work, not inside it.

#### Weapon features considered and rejected, with reasons

*Recorded so they are not re-litigated. A comprehensive external weapon schema was reviewed on
2026-08-08; most of it is **identity**, not quality, and several entries conflict with settled
decisions.*

| Rejected | Why |
|---|---|
| Homing / target-seeking | §3.2 removes target lock; the cursor is the aim point. Homing needs a lock to seek toward |
| A six-type damage roster | §3.1 settled two shield types on the argument that *more shield types make shields weaker* — a one-generator fighter would be bypassed five times in six |
| Projectile gravity | `OrbitSystem` has gravity wells, but projectiles are straight segments and §12.15 makes the swept-segment test **mandatory** against tunnelling. Curved paths need sub-stepping |
| Ricochet off geometry | There is no geometry. Hull-bounce would be a new mechanic |
| Ammo / resource drain | No ammo system; power is the resource |
| Child projectiles, lingering fields | Each multiplies against `features.md` §9.1's 5,000-projectile budget |
| Per-weapon subsystem-disable bitmask | **Already solved better.** Ion suppresses power generation and `PowerSystem`'s existing shed order decides what goes offline — facilities, shields, engines, weapons. No bitmask to author, and the effect reads differently against differently-configured targets, which makes §2.9's priority list matter defensively |

**Kept as identity attributes** (authored, never rolled): fire mode, burst count, intra-burst
interval, charge time, spread pattern, detonation trigger, penetration count, chain count, knockback.
`WeaponSystem`'s §4 row already names "charge/burst/spread modes" as its responsibility, so these were
anticipated rather than added.

### 12.22 Shields, Collision and Structural Destruction — `features.md` §3.1, §3.7

*Settled 2026-08-08. Unlike most of §12 this batch is largely **changes to built code**, including one
defect that makes a headline mechanic inert.*

#### 🐛 Shields protect only their own housing

Two facts combine badly:

```cpp
// Taxonomy.cpp — a shield module may only mount in a Shield shell
case ModuleKind::ShieldGenerator: return shell == ShellKind::Shield;

// DamageSystem.cpp — the shield consulted is the one on the damaged hardpoint
auto* shield = registry.try_get<Shield>(hardpoint);
```

So a generator protects **exactly one hardpoint: the housing it sits in.** Every other hardpoint on
the rig is unshielded. `features.md` §3.1 calls type-matching *"the strongest mechanic in the
design"*, §3.1 describes fire landing on "the hardpoints and hull **beneath**", and §3.1 says losing
the generator disables restoration "**for that rig**" — all three describe a layer that does not
exist. `PowerSystem` also contains **zero** shield references, so §2.9's shields category gates
nothing.

**The code implements `Personal` correctly; `Bubble` and `Conformal` do not exist.**

**Why this passed review, and the general lesson.** The absorb/bypass logic itself is *correct* — type
match absorbs and resets the recharge cooldown, mismatch passes through. The defect is that shields
follow the codebase's house pattern **exactly**: `ModuleAttachment` puts a module's components on the
hardpoint carrying that module, which is right for `Weapon` (the turret fires), right for
`PowerSource` (the cell generates), and right for `FacilityRef`.

> **A shield is the only module kind whose effect is supposed to extend *past its own mount*, and
> nothing in the per-hardpoint component pattern can express that.**

So the code looked exactly like its neighbours, which is why a reader skims past it — and it would not
surface in play either, since §10's slice is unit-tested and unflown. The `coverage` enum is the fix
precisely because it makes reach an **authored property rather than an assumption**.

**Generalized, this is a review rule worth applying to every future module kind:** when a module's
effect is not confined to the hardpoint it mounts on, the component pattern alone will silently
confine it. Say so in a field.

| Change | Home |
|---|---|
| `coverage` enum on `ModuleDef` (identity, never rolled) | `shared/blueprints/ModuleDef.h` |
| Resolve coverage before absorbing | `DamageSystem::ApplyToHealthAndShield` — Conformal is a `ParentRig` comparison, Bubble one distance check |
| Shield contribution to `PowerBudget` and `boostMultiplier` | `PowerSystem` — currently absent entirely |
| `bleedThrough`, `ionResistance`, reflect fraction | `DamageSystem`; reflect flips a live `Projectile`'s velocity and source rather than spawning one |
| Semitransparent field on draw layer 5 | `WorldRenderer` |

#### `ShellDef.acceptsKinds` replaces the hardcoded mountability table

`IsMountable(ModuleKind, ShellKind)` is a `switch` in `Taxonomy.cpp` — a content rule in code, which
Law 10 forbids. It becomes an authored list on `ShellDef`. `Validation.h`'s `ModuleCompatibility` rule
reads the list instead of calling the function; the function is deleted rather than left as a second
source of truth.

#### `CollisionSystem` narrow phase: per-hardpoint circles, not a convex hull

`BuildWorldHull` (ported from StarReach2's `CollisionHull.cpp`) makes every rig a solid blob.
Replacing it with per-hardpoint circle tests gives **destroyed hardpoints opening holes you can fly
through** — damage changing a hull's physical shape, which combines with §12.14 item 17's
nearest-hit rule so that killing a capital's core means stripping its ring or threading a gap.

- Bounded cost: 7 × 50 = 350 circle tests per fighter/capital pair, after the existing broad phase.
- **Check first** whether `BuildWorldHull` already excludes `Destroyed` hardpoints; if not, a stripped
  capital collides at full size under any model, which is a bug today.
- This discards debugged ported code, so it is a deliberate trade rather than a cleanup.

#### `DamageSystem`: destruction cascades along `StructuralAttachment`

Today a rig dies only when `HasLivingHardpoint` is false — every hardpoint must die — and
`ShellKind::Chassis` is special-cased **nowhere**, so a chassis-less fighter flies on its wings.

**Destroying a shell destroys everything attached to it.** `StructuralAttachment` already exists in
`shared/components/Rig.h` and `RefactorSystem` already reasons about orphaning, so the graph is there.
Everything hangs off the chassis, so chassis death is rig death **with no special case** — not the
protected core §3.2 rejects, since the player is never *required* to kill it: `features.md` §3.2's
structural-integrity threshold destroys the rig regardless. Killing the spine is a **fast path you
earn**, not a mandatory weak point.

Orphaned children are destroyed and drop salvage through `LootSystem`'s existing death-wreck path.

> 🔄 **Two claims in this subsection were revised 2026-08-09** by `features.md` §3.2's damage pass.
>
> **Superseded:** *"the chassis is the most exposed part of a hull (50% of hull radius, dead centre)."*
> Under §3.2's structural-coverage rule, armour segments tile the hull envelope and functional mounts
> attach to **them**, so the chassis becomes the **least** exposed part — you strip plating to reach
> the spine. That is better, and it is still not a protected core, for the reason above.
>
> **Superseded:** the content precondition *"chassis hull must dominate peripheral hull, or 'shoot the
> middle' is the fastest kill and localized damage becomes decorative."* **The constraint eats
> itself** — chassis-focus is slower only if the chassis exceeds ~70% of total hull, at which point
> peripherals genuinely are rounding errors. Measured on `aegis_vanguard`: 120 damage to kill via the
> chassis against 227.5 by attrition, a 1.9× advantage that no authoring can remove.
>
> **The framing was wrong, not the numbers.** Localized damage is not a way to kill *faster*; it is
> how you achieve what killing does not — stranding, disarming, disabling, and above all **capture,
> which must deliberately avoid the chassis because killing it destroys the prize.** A fighter dying
> to a burst through the middle is the honest outcome when killing was the goal.

#### `ApplyRamDamage` should scale with absolute mass

`baseDamage = kRamDamagePerSpeed * speedChange` is a flat constant, so two dreadnoughts colliding at
10 units/sec deal the same base damage as two fighters at 10 units/sec — only the *split* differs
(`heavyShare = lighterMass / total`, which is correct). Kinetic energy goes as ½mv²; scale by reduced
mass. §12.19's derived masses make the numbers real.

#### `ModuleKind::FireControl`, and no `Auxiliary`

A new kind supplying automated tracking, with a `ModuleAttachment` case of its own. It drives
`FiringArc::turnRatePerSecond` — **read by `WeaponSystem` but hardcoded to `kPi`** and authored
nowhere, one of the three dead boost targets §12.21 lists.

**Do not add `ModuleKind::Auxiliary`.** A catch-all kind cannot tell `ModuleAttachment`'s switch which
components to attach, and cannot express mountability. Grouping is a **display category** on
`ModuleDef` for the menus, never a type.

#### Recorded as rejected

| | Why |
|---|---|
| **A rig-wide hull pool** for shots missing every hardpoint | It is the health bar §3.2 defines vessels *against*; it weakens precision aiming; and it would make the same gaps mean one thing for flying and another for shooting |
| **`shipType`** to gate overflight | Law 4. §12.14 removed the last per-craft-type special case and §3.5 exists so class is emergent from `hullRadius`. Derive from a **ratio**, never a tag |
| **Physical (hull-blocking) shields** | Would hand every shielded vessel free anti-ram defence, neutering a working mechanic, and entangle collision, docking, and friend/foe. **Ramming bypassing shields is the wanted property** |
| **Altitude bands** 🧊 | Coherent as designed (`features.md` §3.7) but deferred: it is the only feature adding a positional dimension, and every spatial system inherits it. Revisit with escort/docking/formation work |

### 12.23 The Module Roster and Rig Aggregation — `features.md` §2.11

**Four new `ModuleKind`s, one aggregation rule, and a class of bug it fixes.** Each new kind below
exists because a **built system has no module feeding it** — the same shape as §12.22's shield defect,
and found the same way: by grepping the systems rather than reading the schema.

#### The aggregation rule

> **Every rig-level attribute is the sum — or maximum — of contributions from *living* hardpoints.**

`PowerSystem` already does this for `PowerBudget`. Three others do not, and each is a separate defect
today:

| Attribute | Current behaviour | Home of the fix |
|---|---|---|
| `Propulsion` | Zeroed only when the **last** engine dies (all-or-nothing) | `DamageSystem` — this is §12.16 item 8's "per-hardpoint propulsion contribution" |
| `BodyMass` | **Never recomputed** on mount/unmount | `shared/rig/ModuleAttachment.h` — the `RecomputeRigTotals` §12.16 item 8 calls for |
| Cargo capacity | Unspecified | New, with `CargoBay` |

**Each attribute declares `Sum` or `Max`, and it is not cosmetic.** `thrust` and `turnTorque` sum;
**`maxSpeed` maxes** — two engines double acceleration, not top speed. `sensorRange` and `jumpRange`
max; `fuelCapacity` sums, so multiple hyperdrives are redundancy rather than range.

⚠️ **`RecomputeRigTotals` must run on every hardpoint death**, not only on mount/unmount, or
propulsion silently keeps a dead engine's contribution. `DamageSystem` runs last in `TickSchedule`, so
the ordering already works.

#### `powerDraw` moves to the grade ladder, not to any pool

`mass` and `powerDraw` are §2.2's two costs and must behave identically — mass is already grade-driven.
Pooling `powerDraw` would also add the same undifferentiated entry to every pool, pushing Weapon to 7
and ShieldGenerator to 8 against §12.21's 4–6 budget. Curve is gentler than mass (~100% → 85%) because
capability ×5, mass ×0.7 and draw ×0.7 compound hard against `Validation.h` rule 3's power balance,
which is a hard gate that must stay binding.

`ionResistance` moves the same way, for the same budget reason.

#### New kinds

| Kind | Why it exists | Code touched |
|---|---|---|
| **`Sensor`** | `SensorRange` exists in `Targeting.h` and `DiscoverySystem` reads it; **nothing produces it.** `features.md` §8.3 names the module explicitly | `ModuleAttachment` case; `Taxonomy` |
| **`CargoBay`** | §2.2 specifies capacity from mounted bays; the kind was never added. `slotCount` × `slotCapacity`, **total derived and never stored** | `ModuleAttachment`; `CargoHold` **moves onto the bay** — see below |
| **`FireControl`** | Drives `FiringArc::turnRatePerSecond` — read by `WeaponSystem`, **hardcoded to `kPi`**, authored nowhere | `ModuleAttachment`; `WeaponSystem` |
| **`Hyperdrive`** | `WarpSystem` has **no module and no fuel reference at all** | `WarpSystem` gate; new fuel component |

**Do not merge `FireControl` into `Weapon`.** It would undo §12.22's turret decision: with tracking
baked into the gun, a cheap weapon could never be independent and the withdrawn tier gate returns.
Separate modules are what let a cheap turret with a good gunner match an expensive automated one — and
the tier progression re-emerges correctly through `moduleSlots`, since a 2-slot turret fits weapon plus
fire control while a 1-slot one does not.

**`PowerPriorityFor(ModuleKind)` absorbs all four with no fifth category**: FireControl → Weapon
priority; Sensor, CargoBay, Hyperdrive → Facility priority.

#### The hold lives on the bay — `CargoHold` moves off the rig root

*Settled 2026-08-10 by the project owner, and it resolves a contradiction between two sections of
`features.md` written the same day. Verified against `src/`.*

§2.2 rules that *"slots are presentation, not a second constraint"* while §2.11's `CargoBay` roster
entry authors **`slotCount` — how many distinct stacks — and `slotCapacity` — mass per stack**, with
a worked example showing the two are not interchangeable. **§2.11 wins**: §2.2's line predates the
bay module existing and was written about a bare entry count with nothing to author two numbers on.

> **`CargoHold` is a component on each cargo-bay hardpoint** — `{ std::vector<ItemStack> stacks; int
> slotCount; float slotCapacity; }`, the two limits copied from the bay module **instance** at attach
> exactly as `FacilityRef` copies its kind and grade. **The rig-level hold is a view over the living
> bays, never a stored aggregate.**

**This is §2.11's own aggregation law**, which that section names `PowerSystem` as the one system
already following: a rig attribute is the sum of contributions from living hardpoints, and destroying
one removes its contribution. For cargo the contribution *is* the contents, so *"shoot the bay, lose
what was in it"* stops needing a spill rule and becomes the default behaviour.

##### Two limits mean two refusals, which is what the row model wanted

| Refusal | When | Row |
|---|---|---|
| **`HOLD FULL`** | Every slot that could take this item is at `slotCapacity` and no free slot exists | §12.30.3's disabled row |
| **`NO FREE SLOT`** | Mass to spare, but every slot holds something else | New, and it is §2.11's variety constraint becoming visible |

*A hold can be out of bulk with variety to spare or the reverse, and §3.10's degrade-never-remove
asks the row to say which. One number could not.*

##### Placement is automatic, and it must not read `Rig::children` order

The player never chooses a bay. A deposit resolves in three steps:

1. **Top up an indistinguishable stack** that has room — §12.19's stacking rule, so a hundred iron
   join the iron already there.
2. Otherwise take a free slot in the **emptiest living bay**, by fraction of `slotCount` used.
3. **Split across slots and bays as needed**, and **refuse whole** when the rig cannot take the entire
   quantity — §12.30.3's all-or-nothing contract, evaluated at the rig level so a purchase that fits
   across two bays is not refused for fitting in neither.

⚠️ **Emptiest-first, tie-broken by `MountId` — never first-found.** `EngineerSystem::Docked
EngineeringLevel` returning *"the first living Engineering facility found while walking
`Rig::children`"* is a defect §12.30.5 already names, and a placement rule with the same shape would
make which cargo you lose depend on entity creation order. Emptiest-first also spreads a mixed
manifest, so a lost bay costs a fraction of everything rather than all of something — the lower-
variance loss, for an event the player cannot steer.

**Withdrawal mirrors it**: draw from the *fullest* matching slot first, so bays drift back toward
even and empty slots reappear for new item types.

##### `shared/rig/CargoView.h` — the one write path

Five systems call `try_get<CargoHold>(root)` today — `StationServicesSystem`, `LootSystem`,
`EngineerSystem`, `ModuleEquipSystem`, `RefactorSystem` — plus `SpaceFlight` and `TutorialSystem`.
None of them should learn to walk a rig.

```
shared/rig/CargoView.h        beside ModuleAttachment and DockedFacility (12.30.5)

float Capacity (const registry&, entt::entity rigRoot);   // sum of slotCount * slotCapacity, living
float TotalMass(const registry&, entt::entity rigRoot);
bool  Deposit  (registry&, entt::entity rigRoot, const ItemStack&);        // whole or nothing
bool  Withdraw (registry&, entt::entity rigRoot, const ItemInstance&, int quantity);
void  Merged   (const registry&, entt::entity rigRoot, std::vector<ItemStack>& out);  // for display
```

**That is §12.20's one-write-path rule applied a layer down** — *"there is exactly one
`Deposit`/`Withdraw` API and it updates the totals as it writes."* Seven consumers on day one, so
Law 11's tie-breaker is satisfied several times over, and it is the same promotion §12.30.5 makes for
`DockedFacility`.

##### Three defects this exposes, all in built code

🐛 **`LootSystem.cpp:15` calls `get_or_emplace<CargoHold>(collector)`.** Any entity that picks
something up silently acquires a hold. Under the bay model that is a capability reached by
**omission** — §13.3 W's class — because a rig with no `CargoBay` is supposed to be unable to carry
anything. **`get_or_emplace` becomes a lookup that fails**, and a collector with no bay does not
collect.

🐛 **Warping deletes all cargo.** `SpaceFlight.cpp:86` copies the root's `CargoHold` out of the
departing registry and `:123` emplaces it on the newly spawned root. Under the bay model the cargo
lives on hardpoints that `RigFactory::Spawn` rebuilds from the blueprint **empty**, and the root has
no hold to copy. **The carry-over belongs in §12.31's `RigState`**, which is already a per-mount delta
against a `BlueprintId` and already has to carry each mount's `MountedModules` and `ShellInstance`
(§12.19). One mechanism, one more field, and §13.3 AC's *"warping undoes every refit"* and this are
then the same fix.

✅ **§12.30.3's *"`StationFactory` also needs a non-zero `capacity`"* dissolves.** A station's hold is
however many cargo bays it was built with, so there is no number for the factory to invent — and
`features.md` §5.0's *"destroy a station and its stockpile goes with it"* becomes true at **hardpoint**
granularity for free, which is a better blockade mechanic than the station-level version it was
written for.

⚠️ **Content prerequisite, and it blocks group 1.** `modules.json` holds seven modules and no cargo
bay, so **until the starter chassis authors one the player cannot pick up, buy, or carry anything.**
Under the old root-level `CargoHold` that was hidden — §13.5 group 1 simply emplaces one. This is the
honest version, and the fix is one authored module plus one mount, not a mechanism.

##### Systems

`LootSystem` gains the **producer** half it never had: a view over `<CargoHold, Destroyed>` that
spawns each stack as a `LootDrop` and clears the vector. `DamageSystem` only *tags* `Destroyed`
(`DamageSystem.cpp:38`) and never destroys the hardpoint entity, so the component is still there to be
read on the following tick — the ordering works with no new lifetime rule. `ModuleEquipSystem` routes
an unmounted bay through the same helper, so destruction and removal are one path.

##### Tests

- Rig capacity is the sum over **living** bays; destroying one reduces it and destroying all of them
  makes it zero.
- A deposit that fits in no single bay but fits across two **succeeds**; one that fits in neither is
  refused whole, with nothing partially applied.
- A deposit of a new item type into a hold with free mass but **no free slot** is refused with
  `NO FREE SLOT`, and the same deposit of an item already stacked there succeeds.
- Placement is emptiest-first and **independent of `Rig::children` order** — the same rig built in a
  different mount order places identically.
- Destroying a bay drops **exactly that bay's stacks** as recoverable `LootDrop`s, and the other bays
  are untouched.
- Unmounting a loaded bay takes the same path as destroying one.
- A collector with no `CargoBay` collects nothing and gains no `CargoHold` component.
- A warp preserves per-bay contents (with §12.31), and a rig whose blueprint authors two bays does
  not arrive with one.
- A station's stock dies with the bay that held it, not with the station.

#### `WarpSystem` gains a gate, and fuel is a new resource

A hyperdrive is required to jump between systems, so `WarpSystem` must check for a living Hyperdrive
hardpoint before permitting a system or galaxy warp. That is a **behaviour change to built code**, and
it makes the drive a target — destroying it prevents escape, which gives `NpcAiSystem`'s flee state a
counter it currently lacks.

**Fuel is consumed by jumps only, never by engines.** Power is the tactical resource, fuel the
strategic one; putting fuel on engines would add a second clock to every dogfight. Consumption scales
with jump distance and hull mass. Refuel joins `StationServicesSystem`'s buy/sell/repair as a fourth
intent. Running dry strands a rig, which `DistressSystem` already exists to handle.

`features.md` §2.7 cut upkeep partly on the grounds that refuelling already provides a recurring sink —
so this is the design collecting a debt rather than adding a system.

#### Armour flat reduction is scoped, not universal

Flat damage reduction is **nonlinear in effect**: against a 50-damage shot, 5 is a 10% nerf and 25 a
50% nerf, and past that everything floors — it hard-counters weapon families. So it belongs to **one
armour family**, with a **~25% floor** rather than an absolute block (an absolute block means a weapon
does literally nothing, with no feedback). Percentage resistance is rejected as redundant and as a
multiplicative stacking hazard.

**Tests for this batch:** propulsion falls proportionally as engines die and reaches zero only at the
last; `maxSpeed` does not scale with engine count; cargo total equals `slotCount × slotCapacity` and is
never serialized; a warp intent is refused with no living hyperdrive; fuel debits scale with distance
and mass; flat reduction never reduces a hit below the floor.

### 12.24 Wiring The Game Loop — §0's 🚨 block, sequenced

*Specified 2026-08-08. This is the first §12 entry that is not a `features.md` design section
needing an architectural home. It is the opposite: **built code with no caller.** Every claim below
was verified by grepping for readers and callers, not by reading a schema.*

**Nothing else in §12 should start before this.** Not as a hard dependency — most of §12 compiles
fine without it — but because a system built today cannot be reached, exercised, or judged. That is
how twenty-two ✅ systems and nine menus accumulated with no way in.

#### The order is forced, and it is not the order §0 originally implied

An earlier draft of §0 prescribed "a docked-menu router plus the five missing context pointers."
Built today that would route a player who does not exist, to a station that is never spawned, from
a docked state reachable only by a key that requires a `DockPrompt` that requires a `DockingBay` on
a same-faction rig `WorldGen` never creates. **The dependency runs the other way**, so:

| # | Step | Why it must precede the next |
|:---:|---|---|
| **1** | **World and player on entry** | Everything below needs an entity to act on |
| **2** | **Player control** | A player who cannot move cannot reach a station |
| **3** | **Camera follow** | Motion is unobservable without it, so step 2 cannot be judged |
| **4** | **A station to dock at** | The docked state is the meso loop's only entry point |
| **5** | **The docked-menu router** | Now has a reachable state to key on |
| **6** | **The five `SystemContext` pointers** | Independent of 1–5; may land any time |

Steps 1–4 are the micro loop and are worth shipping as one issue — each is a few lines, and none is
independently verifiable. Step 5 is its own issue. Step 6 is a third, and is startable today.

#### Step 1 — world and player

**Home:** `SpaceFlight::OnEnter()`, which is currently empty.

`world_gen::PopulateSystem(world_, content_, seed)` then `rig_factory::Spawn(...)` then
`emplace<PlayerControlled>` — the sequence `WarpToSystem` already performs, and the sequence
`WorldGen.h`'s own doc comment already prescribes. Extract the shared body rather than writing it
twice; `WarpToSystem` is that same routine plus wreck demotion and inventory carry-over.

> ⚠️ **The seed.** `WarpToSystem` seeds from `std::hash<std::string>` on the system id, which
> §12.15 flags as a placeholder pending §12.17's galaxy topology. `OnEnter` must use the **same**
> placeholder, not invent a second one — one bug to fix later, not two.

**Types:** none new.

#### Step 2 — player control, and the `ActorId` gap

*Settled 2026-08-08.* **Player input goes through `core::IntentQueue`, not through a direct
component write.** Two mechanisms both claimed this step:

| | Pattern | Precedent |
|---|---|---|
| **A** | Input writes `ThrustInput`/`FireIntent` directly on the player entity | Every request component in the codebase. `AvionicsMenu.h` calls this *"this codebase's established intent-emission idiom"* |
| **B** ✅ | Input pushes `SetThrottleIntent`/`FireWeaponsIntent` into `ctx.intents`; a new system drains them onto `ThrustInput`/`FireIntent` | `core/events/Intent.h`, which exists **for exactly this** and is otherwise dead |

**B, for three reasons in order of weight:**

1. `SetThrottleIntent` and `FireWeaponsIntent` exist for this and nothing else. Choosing A means
   they have neither producer nor consumer forever, and **§2.4 then requires deleting them** —
   which would delete Law 9's only concrete expression in the codebase. Law 9 is explicitly *not*
   deferred even though `net/` is.
2. Flight input is the highest-frequency, most latency-sensitive thing that will ever cross a wire.
   If one path is going to prove the intent queue, it is this one — and proving it on the cheapest
   possible case now is far less work than restructuring input later.
3. Request components resolve by `entt::entity` handle. That is correct for *"this docked rig wants
   to buy X"* and wrong for *"actor 3 pressed W,"* which Law 2 requires to resolve by stable id.

**Cost, stated honestly:** one small system, and closing the `ActorId` gap. This is not building
`net/` and does not violate §2.5 — the queue already exists and already compiles.

> **This does not retroactively condemn the request components.** The two mechanisms are not
> competing conventions to be unified later; they answer different questions, and the split is
> now the rule: **`core::Intent` for what an *actor* did** (resolved by stable `ActorId`, must
> survive a wire, produced every tick), **a request component for what a *specific entity* wants
> done to it** (resolved by handle, produced by a deliberate click, consumed once). `DockRequest`
> naming a bay entity is correctly a component; `SetThrottleIntent` is correctly an intent.
> `AvionicsMenu.h`'s claim that request components are "this codebase's established intent-emission
> idiom" should be narrowed to the second row when that file is next touched — it is describing
> half the picture as the whole of it.

**Types** — one new component, in `shared/components/Identity.h` beside `PlayerControlled`:

```cpp
// The stable actor this entity belongs to (core/events/Intent.h). Every core::Intent addresses
// an ActorId; this is what resolves one to an entity. Written by RigFactory's caller at spawn,
// never by a system. Single-player has exactly one non-zero actor -- the type is what matters,
// not the count.
struct ActorRef {
    core::ActorId id;
};
```

> ⚠️ **`shared/` may not include `core/`** (§2.3). `core::ActorId` is defined in
> `core/events/Intent.h`, so declaring `ActorRef` there as written **would fail
> `check_layers.py`.** Two ways out, and the second is correct:
>
> 1. Duplicate the id type in `shared/` — rejected, two id types is exactly the drift Law 2 bans.
> 2. **Move `ActorId` down into `shared/blueprints/Ids.h`**, where `BlueprintId`, `FactionId`,
>    `MountId`, and `KnowledgeNetworkId` already live, and let `core/events/Intent.h` include it
>    from there. `ActorId` is an identifier, not an event; it is in `Intent.h` only because that is
>    where it was first needed. **Do this move in the same commit** — it is a two-line relocation
>    plus an include, and it is what makes the component legal.

**Systems** — one new file, `modes/space/systems/PlayerInputSystem.{h,cpp}`:

- Drains `SetThrottleIntent` and `FireWeaponsIntent` from `ctx.intents`, resolves each `ActorId`
  against the `ActorRef` view, writes `ThrustInput` / emplaces `FireIntent`.
- **Ignores an unresolvable `ActorId`** rather than asserting — `IntentQueue.h`'s own contract is
  that "an intent from a remote machine can name something that died locally two ticks ago."
- **It must not poll raylib.** A system takes a bare `SystemContext` with no window, and
  `features.md` §2.7's proposed headless combat harness depends on that staying true. *(Cited as
  §9.1 until 2026-08-09; §9.1 is the performance budget and never mentions a harness. The combat
  harness itself still does not exist — `tools/economy_sim` does, as of 2026-08-11, and demonstrates
  the same "headless, no window, `SystemContext`-free" pattern the harness would need.)*

**Schedule position** — in `TickSchedule()`, immediately after `HierarchySystem` and **before
`PowerSystem`**:

```cpp
{"HierarchySystem", &hierarchy_system::Tick},
{"PlayerInputSystem", &player_input_system::Tick},   // new
{"ConstructionSystem", &construction_system::Tick},
```

Three ordering constraints, all of which that slot satisfies — add them to `SystemSchedule.cpp`'s
comment block in the same commit, per §2.4:

| Must run | Relative to | Why |
|---|---|---|
| **Before** | `PhysicsSystem` | It writes the `ThrustInput` `PhysicsSystem` integrates this tick |
| **Before** | `WeaponSystem` | It writes the `FireIntent` `WeaponSystem` consumes and then `clear`s |
| **Before** | `DockingSystem` | `DockingSystem` zeroes a docked rig's `ThrustInput`; running input *after* it would let a docked player fly away. This is the constraint that would silently break |

**The poller** — a new `modes/space/ui/FlightControls.{h,cpp}`, beside `AvionicsMenu`:

- `void Poll(core::IntentQueue& out, core::ActorId self);` — reads raylib, pushes at most one
  `SetThrottleIntent` and at most one `FireWeaponsIntent`.
- Called from `SpaceFlight::Update` **before `clock_.Advance`**, next to the existing
  `avionics_menu::Update` call. That placement is already correct and already commented there:
  `intents_.Clear()` runs after the whole step loop, so one intent pushed pre-loop is visible to
  every fixed step that frame — which is what makes a held key work at any frame rate.
- **Not `engine/input/`.** §3's blueprint marks that 🧊 "add with first consumer," but `engine/`
  may not include `core/` (§2.3) and a producer of `core::Intent` must. `modes/*/ui/` may include
  `core/` — `CustomizeMenu` already does. `AvionicsMenu` is the precedent for the whole shape.

**CMakeLists.txt** — two lines in the `sr_space` source list (§11.5: sources are explicit, never
globbed), keeping the existing ui/ and systems/ groupings:

```
src/modes/space/ui/FlightControls.cpp
src/modes/space/systems/PlayerInputSystem.cpp
```

##### 🐛 The player has automatic target lock, and `features.md` §3.2 forbids it

*Found 2026-08-09 by grepping the readers. **This blocks player firing and must be resolved inside
step 2**, not deferred — it is not a polish item.*

`TargetingSystem`'s header states the problem in its own words:

> *"A seeker is any entity carrying `Target`, `WorldTransform`, `FactionRef`, and `SensorRange` —
> **the player's rig and NPC rigs alike**, since Law 4 means neither is a special case."*

`RigFactory` emplaces `Target` and `SensorRange` on **every** rig root, so the player is a seeker.
`TargetingSystem` therefore auto-acquires a hostile for them *and* auto-selects which hardpoint to
aim at, and `WeaponSystem::AimPointPosition` derives its aim point from that `Target` and from
nothing else. `features.md` §3.2 says the opposite in as many words:

> *"The player aims manually. **There is no target lock.** The player's cursor is the aim point…
> A subtarget-cycling UI would return the same information at no risk and is explicitly rejected."*

**So a `FireWeaponsIntent` wired today would fire at an auto-locked enemy rather than at the cursor**
— and a player with no hostile in sensor range could not fire at all, because `WeaponSystem` skips
any rig whose `Target` is null.

**Two changes, both small:**

1. **`TargetingSystem` excludes `PlayerControlled`** — the same `entt::exclude<PlayerControlled>`
   predicate `NpcAiSystem` already uses, for the same reason. This does not violate Law 4: the player
   is not a different *kind* of rig, they are the one rig whose input comes from a human.
2. **`WeaponSystem` gains an aim source that is not an entity.** A new
   **`AimPoint { Vec2 world; }`** component on the rig root, written every frame by the input
   producer from the cursor's world position. `AimPointPosition` prefers `AimPoint` when present and
   falls back to `Target` otherwise — so player rigs aim at a point, NPC rigs keep aiming at a
   selected hardpoint, through one function with one branch.

**This is also what makes §3.2's projectile rule true.** Shots damage whatever they physically pass
through, including hulls the player never selected, precisely because the player is shooting *at a
point in space* rather than at a locked object.

> ❌ **Do not add optional auto-targeting as a gameplay feature.** It hands back the free hardpoint
> selection §3.2 rejected, and §3.2's skill expression — *"which hardpoint you destroy is a question
> of marksmanship"* — rests on its absence. It is defensible later as an **accessibility** setting,
> which is a different conversation with different constraints.

##### Weapon groups gate firing

`features.md` §3.6 settles ten toggleable weapon groups. Two additions, both cheap:

- **`WeaponGroup { std::uint8_t index; }`** on each weapon hardpoint, defaulted at spawn so each
  distinct weapon `ModuleId` takes the next free group — a fresh ship is pre-grouped with no player
  action.
- **`EnabledWeaponGroups { std::uint16_t mask; }`** on the rig root. `WeaponSystem` skips a hardpoint
  whose group bit is clear, alongside the `Destroyed` check it already performs.

Session state, not saved. A destroyed hardpoint simply stops contributing, so no group bookkeeping is
needed when one dies.

##### The dock key moves off `E`

`AvionicsMenu.cpp` declares `constexpr int kDockKey = KEY_E`, and it is **the only gameplay input
that exists in this repository.** `features.md` §3.6 gives `E` to strafe-right and docking to `R`, so
this constant changes in the same commit that adds flight controls — otherwise the first thing the
new input producer does is fight the one input that already worked.

#### Step 3 — camera

**Home:** `SpaceFlight`, which already owns `cameraTarget_`/`cameraZoom_` as *"the only state this
class is permitted to own"* (Law 7, presentation math). No new type, no new system, no new member —
the members exist and are simply never assigned.

In `Update`, after the step loop, set `cameraTarget_` from the `PlayerControlled` entity's
`WorldTransform`. **Interpolate in `Draw`, not here** — `Draw` already computes `alpha` and passes
it to `DrawWorld`; a camera snapped to the un-interpolated tick position would judder against
sprites that are interpolated. Leave `cameraZoom_` at its 1.0 default; zoom is §8.1's navigation-map
continuum and does not belong to this step.

**Also here:** delete the duplicate `render::DrawWorld` call in `Draw()` (`SpaceFlight.cpp:145`).

#### Step 4 — something to dock at

`WorldGen::PopulateSystem` gains a station. Everything needed exists: `station_factory::Spawn`,
and `aegis_outpost` which already carries a `docking_bay_i` on a `shell_facility_bay`. Follow
`SpawnNpcPresence`'s existing shape — a `SpawnStation` static in the same anonymous namespace,
called from `PopulateSystem` **before** `SpawnNpcPresence` so it draws from `rng` at a fixed point
in the sequence and does not shift the NPC layout for a given seed.

Three constraints the placement must satisfy, and none is arbitrary:

| Constraint | Why |
|---|---|
| **Same `FactionRef` as the player** | `DockingSystem` gates the prompt on `FactionRef` equality, not on a relation lookup — a simplification `DockingSystem.h` records as predating `core/diplomacy/`. A neutral or hostile station is undockable, so step 5 would be unreachable |
| **Outside the sun's `kSunGravityRange` (2200)** | Or `OrbitSystem`'s gravity well drags a station that has no engines |
| **Reachable at the player's spawn** | Not so far that step 2 cannot be judged. `PickNpcPosition`'s existing band is the right reference |

> ⚠️ **`FactionRef` equality is a placeholder, not the design.** `features.md` §5.3 wants a
> relation check. Tightening it is a separate issue — do not fold it in here, because doing so
> would make this step depend on `ctx.diplomacy`, which is step 6's `nullptr`.

**A test will break, and it should be updated rather than worked around.**
`tests/integration/WorldGenTests.cpp` asserts *"PopulateSystem seeds a plausible NPC presence with
no PlayerControlled entity"* and *"PopulateSystem is deterministic for a given seed."* The second
still holds; the first needs the station added to its expectations. **`PopulateSystem` must still
spawn no `PlayerControlled` entity** — that is step 1's job in `OnEnter`, and keeping the factory
player-free is what lets `WarpToSystem` reuse it.

#### Step 5 — the docked-menu router

**`BridgeView` is already this, three-quarters built.** It gates on `Docked`, walks the host
station's `Rig::children`, filters `Destroyed`, and lists the surviving `FacilityRef` kinds as tabs
— which is `features.md` §3.4's player-location model implemented already. What it lacks:

| Missing | Note |
|---|---|
| **Open-tab state** | See "Where UI state lives" below — a singleton entity, not a `SpaceFlight` member |
| **A dispatch table** | `FacilityKind` → menu `Draw()`, per step 5a |
| **Input** | None of the nine menus handles any. Each is `Build*Request()` + a stateless `Draw()` |
| **Placing the built request** | The universal gap: nine request types are built by a menu and emplaced by nobody |
| **Its own hardpoint's health** | §3.4 requires every facility menu to show it, or dying in a menu reads as a gotcha |

**Types:**

- **`PlayerLocation { entt::entity shell; }`** — the component §3.4 describes and no code carries.
  *"The player is always associated with exactly one shell"*: their cockpit while flying, the
  facility hardpoint they selected while docked. Selecting a tab **is** moving, and `DamageSystem`
  killing that hardpoint kills the player — the predicate §3.4 promises would need no special case.

  ⚠️ **Amended 2026-08-10 — §12.30.1.** `PlayerLocation` is not a component *alongside*
  `PlayerControlled`; it is the **only** one written, and `PlayerControlled` is derived from it.
  Two components naming where the player is would let the death predicate and the camera disagree.
  **This changes step 1**, which plans to `emplace<PlayerControlled>`: it emplaces `PlayerLocation`
  naming the spawned rig's cockpit hardpoint instead. One write site exists in the whole tree
  (`SpaceFlight.cpp:120`), so the change is cheap now and touches twenty-nine readers if deferred.

  **This is also how the player takes a capital's bridge.** `features.md` §4.0's "assume command" is
  not a separate mechanism: the Bridge is a facility hardpoint like any other, so selecting it in
  this router moves `PlayerLocation` there. The player then operates *that hull* and commands from
  it, and dies if that bridge dies — all of it inherited, none of it special-cased.

#### Where UI state lives — decided once, for every menu

*Settled 2026-08-09, by precedent rather than by new invention.* Open tab, selected units, and the
armed order are **presentation state**, not gameplay state — and they need a home that is neither.

> **A singleton entity, tagged, created lazily. Not a `SpaceFlight` member.**

`CommsSystem` already does exactly this and `System.h` names it as the sanctioned exception:
*"`CommsLog` lives on a singleton entity — System.h's 'one legitimate cache' exception."* Following
it here means no new pattern, and it dodges two problems at once: Law 6's 25-member cap on
`SpaceFlight` (which UI state would eat steadily), and multiplayer, where one client's selection must
not appear in another's view of the registry.

*Law 7 does permit presentation state in the mode class, so `SpaceFlight` is not forbidden — it is
simply the worse of two legal homes, and the codebase already chose the better one once.*

🐛 **Fix `BridgeView::kAllKinds` first.** It lists five of `FacilityKind`'s six enumerators;
`Engineering` is absent, and it is the gate `EngineerSystem` and `RefactorSystem` both read. Two of
the nine menus are unreachable *through the router itself* until this is corrected. The array is a
hand-maintained parallel list — either derive it or add a `static_assert` on the enumerator count,
because this defect will recur otherwise.

#### Step 5a — every docked menu is facility-gated, and most systems do not enforce that yet

*Settled 2026-08-09. **This reverses a recommendation made earlier the same day in this section.***

An earlier draft of §12.24 recommended **against** a `FacilityKind` → menu dispatch table, on the
grounds that only one kind is actually enforced anywhere and a one-row table dressed as a general
mechanism is worse than no mechanism. **That inferred the design from the code's current state, and
the inference was wrong.** The rule is:

> **A docked menu is available exactly when the host carries a living facility hardpoint of the kind
> that menu needs.**

The systems have simply not implemented their gates. What each enforces *today* versus what it should:

| Menu | System | Gate today | Gate it should have |
|---|---|---|---|
| `EngineerMenu` | `EngineerSystem` | `Docked` + living Engineering ✅ | unchanged |
| `RefactorMenu` | `RefactorSystem` | `Docked` + living Engineering ✅ | unchanged |
| `StationServicesMenu` — repair | `StationServicesSystem` | `Docked` only | **+ `FacilityKind::Repair`** |
| `StationServicesMenu` — buy/sell | `StationServicesSystem` | `Docked` only | **+ `FacilityKind::Trade`** — resolved 2026-08-10, §12.30 |
| ~~`StorageMenu`~~ | — | — | ⚠️ **Superseded 2026-08-10 — §12.30.** Not a docked screen: live refit is unrestricted (`features.md` §2.7), and §3.10 makes inventory and loadout flight-HUD overlays |
| ~~`CustomizeMenu`~~ | — | — | ⚠️ **Superseded 2026-08-10 — §12.30.** No new facility kind: drafting is a section of the Manufacturing screen |

⚠️ **Splitting `StationServicesSystem`'s three requests across two gates is a behaviour change** —
a station with a docking bay but no repair bay stops being able to repair. That is the intent: it
makes station *composition* matter instead of "a station is a station."

> **§12.30 is the completed form of this step.** It settles the full screen inventory (six tabs,
> seven screens), replaces `FacilityKind::Storage` with `FacilityKind::Trade` — storage becomes the
> station's `CargoHold`, which §13.3 O already required — specifies the shared widget layer that
> stops the screens diverging, and records a defect in *this* step: `AvailableTabs` returns kinds and
> discards the entity `PlayerLocation` needs.

**Two menus are not facility-gated and belong elsewhere.** `BuildMenu` is a *command* surface, opened
by the `B` key against the player's own Construction facility and issued at a unit (§12.26) — not a
docked-host menu at all. `NavigationMap` is the zoom continuum (§8.1), reachable everywhere. Neither
belongs in the router's table.

🐛 **Fix `BridgeView::kAllKinds` first.** It lists five of `FacilityKind`'s six enumerators;
`Engineering` is absent, and it is the gate `EngineerSystem` and `RefactorSystem` both read. Two
menus are unreachable *through the router itself* until this is corrected. The array is a
hand-maintained parallel list — either derive it or add a `static_assert` on the enumerator count,
because this defect will recur otherwise.

🐛 **Move `CustomizeMenu::ConsumeSaveTemplateRequests` into `TemplateMarketSystem`.**

*This revises §12.9, which explicitly settled the opposite:* **"the consumer is
`KnowledgeStore::Grant` — a store call, not a tick, so no new system owns it."** That instinct was
half right and the conclusion was wrong. Four reasons:

1. **Law 9 is a permission rule, not a performance one.** A `modes/space/ui/` file calling
   `KnowledgeStore::Grant` is the only place a UI file mutates `core/` state, and
   `check_layers.py` **cannot catch it** — `core/` includes are legal from `ui/`. That makes it a
   boundary enforced only by a document, which is §0's whole thesis about what fails.
2. **It produced a dead abstraction from the other side.** Nothing in `SpaceFlight` drains
   `SaveTemplateIntent`; the function is called only from tests. §12.9 avoided one §2.4 violation
   and arrived at another.
3. **It does not survive Law 9's own premise.** An intent may be produced on one machine and
   consumed on another; a consumer living in a menu's translation unit exists only where there is a
   menu.
4. **Its sibling already does it correctly.** `PitchTemplateIntent` — the other Template intent
   touching the same store — is consumed by `TemplateMarketSystem` through `ctx.knowledge`.

**§12.9 was right that no *new* system is warranted.** Fold the drain into `TemplateMarketSystem`,
which already holds `ctx.knowledge` and already consumes the other Template intent: it becomes
Template *lifecycle* — save, pitch, sell. Zero new abstractions, and Law 9 holds. `CustomizeMenu`
keeps `BuildSaveRequest` and keeps `CanSave` for live UI feedback; the system re-validates, which
the current code already does as defence in depth.

⚠️ **`data/base_game/modules.json` authors exactly one facility — `docking_bay_i`.** Even a correct
router surfaces a one-tab bridge until the content set gains Storage, Engineering, Repair, and
Construction facilities. That is authoring, not design, and it is the cheapest half of this step.

#### Step 6 — the five null pointers

`SpaceFlight::Update` constructs its `SystemContext` with only `economy`. Passing `discovery`,
`knowledge`, `diplomacy`, `reputation`, and `craftedModules` requires owning them — `main.cpp`
already owns `economy` and `wreckLedger` by exactly this pattern and passes them by reference, never
as globals. Same shape, five more.

**This is independently startable today** and unblocks five systems that currently no-op against
their own null guards. `DiplomacyMatrix` having zero writers anywhere is the failure `features.md`
§5.3 was written to prevent, and this is the first half of fixing it.

#### 🐛 `CustomizeMenu::ConsumeSaveTemplateRequests` — move it

A `modes/space/ui/` file drains `core::IntentQueue` and calls `KnowledgeStore::Grant`. It is the
only place a UI file mutates `core/` state, and it inverts Law 9 directly (*"UI and input never
mutate game state. They emit intents; systems consume them"*). It is a system in a menu's filename,
and it is called only from tests.

**It belongs in a system taking a `SystemContext`** — its `knowledge` pointer is step 6's, which is
why this is listed with the wiring batch rather than as an isolated cleanup. `CustomizeMenu` keeps
`BuildSaveRequest`; the consumer moves.

#### Persistence

**Nothing in this batch is saved.** `PlayerLocation` and the router's open tab are session state
rebuilt on load from `Docked`; `ActorRef` is derived at spawn. The one thing that *does* need to
round-trip is the world seed, and `WorldGen`'s determinism contract already covers it — which is
the reason step 1 must not invent a second seeding path.

#### Tests

Systems take a bare `SystemContext` with no window, so all of the below are headless:

- `OnEnter` leaves exactly one `PlayerControlled` entity, and a populated world (one sun, ≥1 station
  in the player's faction).
- A `SetThrottleIntent` for the player's `ActorRef` produces non-zero `ThrustInput`; one for an
  unknown `ActorId` produces none.
- A `FireWeaponsIntent` produces `FireIntent`; a docked rig's throttle is still zeroed by
  `DockingSystem` in the same tick.
- `AvailableTabs` returns **all six** `FacilityKind`s when all six are present and living — the
  regression test for the `kAllKinds` defect.
- Destroying the facility hardpoint named by `PlayerLocation` kills the player while the station
  survives — §3.4's stated behaviour, which nothing currently exercises.
- Each router-dispatched menu's request lands on the docked requester and is consumed by its system
  in the following tick. **This is the assertion the nine menus have never had**, and the one that
  would have caught this whole class of gap.

### 12.25 Capability Is Emergent — Deleting The `mobile` Movement Gate

*Settled 2026-08-09, from `features.md` §2.11 and §4.3. A correction to built code, not new design.*

> **A rig moves because it has living engines, not because a flag says it may.**

#### The defect

`RigFactory.cpp` reads a blueprint-authored boolean and, when it is false, **emplaces no
`Propulsion` component at all**:

```cpp
// Stations get no Propulsion at all, rather than a zeroed one. PhysicsSystem's view then
// excludes them structurally instead of by a branch on a `mobile` flag.
if (blueprint->mobile) {
    registry.emplace<Propulsion>(root, aggregate.propulsion);
    registry.emplace<LinearDamping>(root, 0.35f, 2.5f);
}
```

The comment is honest about the intent and the intent is wrong. Consequences:

- **A station can never move**, whatever is bolted to it. Mounting engine shells at runtime through
  `ModuleEquipSystem` changes nothing, because the component `PhysicsSystem` reads was never created.
- **It is a vessel-type branch**, which is precisely what Law 4 exists to eliminate. `mobile` is a
  static/mobile class distinction wearing a boolean.
- **It contradicts §12.23's aggregation rule**, which says every rig-level attribute is the sum of
  contributions from living hardpoints. `Propulsion` is the one attribute exempted by construction.

#### The change

1. **Always emplace `Propulsion` and `LinearDamping`.** A rig with no engine hardpoints aggregates to
   zero thrust, which `PhysicsSystem` already handles correctly — a zero-thrust rig does not move.
   Structural exclusion becomes numerical, which is the same answer every other attribute gives.
2. **`RecomputeRigTotals` (§12.23) recomputes it** on mount, unmount, and hardpoint death. That is
   what makes bolting engines to a station actually work, and it is the same call live refit already
   needs (below).
3. **`mobile` survives, with a narrower job.** It still records which factory a blueprint is meant
   for (`StationFactory::Spawn` rejects `mobile: true`) and still drives `Validation`'s "a mobile
   craft needs at least one engine shell" authoring check. It stops deciding whether physics applies.

#### Why this is now blocking rather than cosmetic

Two decisions made on 2026-08-09 depend on it:

| Depends on it | Why |
|---|---|
| **Live refit** (`features.md` §2.7) | `ModuleEquipSystem.h` states plainly: *"Scoped out: rig-wide `BodyMass`/`Propulsion` are not recomputed on mount/unmount."* Swapping engines mid-combat is now sanctioned play, so a refit that does not change how the hull flies is the mechanic's core loop, broken |
| **Emergent order availability** (`features.md` §4.3) | A unit is offered **Move** exactly when `Propulsion` is non-zero. With propulsion decided by an authored flag, the order list reflects authoring rather than damage — and "shoot their engines to strand them" stops working |

**Tests:** a `mobile: false` blueprint with engine shells produces non-zero `Propulsion` and moves
under thrust; a rig whose last engine hardpoint dies falls to zero thrust and coasts; mounting an
engine module at runtime raises `Propulsion` within the same tick `ModuleEquipSystem` runs.

### 12.26 Construction Gating — `FacilityKind::Construction` and build mode

*Settled 2026-08-09. `ConstructionSystem` is built and enforces **no location or capability gate at
all** — any rig with enough credits can conjure a station anywhere. This closes that.*

**Home:** `shared/blueprints/Taxonomy.h` (new enumerator), `shared/rig/ModuleAttachment.cpp` (the
`switch` case), `modes/space/systems/ConstructionSystem.cpp` (the gate),
`modes/space/ui/BuildMenu.cpp` (placement mode), `data/base_game/modules.json` (the module).

**Types:**

- **`FacilityKind::Construction`** — a seventh enumerator, carrying **`buildRange`** on `FacilityRef`.
- No new `ModuleKind`. Repair, Research, Docking, Storage, and Engineering are all already
  `FacilityKind`s on Facility modules; §12.23's "each functional module is its own kind" targets an
  `Auxiliary` catch-all, not the facility sub-taxonomy.

**Kept distinct from `Manufacturing`, deliberately.** `features.md` §2.8 already split the two
systems on principle — construction produces *entities*, manufacturing produces *inventory*. Two
systems, two gates, two kinds. Reusing `Manufacturing` would also couple build mode to §12.18's
`ManufacturingSystem`, which does not exist.

**Systems:** `ConstructionSystem` gains two checks it does not have — the executing rig must carry a
living `FacilityKind::Construction` hardpoint, and the requested position must be within that
hardpoint's `buildRange` **of the executor, not of the requester.** That second clause is what makes
a constructor's position matter and what makes escorting one to the frontier a real task.

> ⚠️ **`buildRange` is measured from the builder.** A player commanding a remote constructor is
> therefore placing a structure they may not be able to see. Two placement surfaces follow, and both
> already exist: the **world view** when the constructor is on screen, and **`NavigationMap` at
> `ZoomLevel::Tactical`** when it is not (§12.6, `features.md` §8.1). No third mechanism.

**Build is one entry in a unit's order queue** (§12.27), not a fire-and-forget facility job. The
constructor travels, builds, and only then advances to its next order — which is what makes a
shipyard a committed asset. *This revises an earlier proposal in this session that construction be
fully asynchronous like `ResearchSystem`'s job list; the order still issues instantly from anywhere,
but the executor is occupied for the duration.*

**Build mode (`B`) is gated on the player's own rig.** The key opens placement only when a living
Construction hardpoint is aboard the hull the player occupies; otherwise the player issues a Build
**order** to a unit that has one. Same menu, two paths, and the fallback is the normal one — most
players will not fly a constructor.

**Placement input** (`features.md` §3.6): the ghost follows the cursor, **left-click places**,
right-click or `Esc` cancels. Left-click is unambiguous here because the ghost is on screen. While
placing, the player has no aim point — so slaved turrets fall back to the hull's forward bearing and
**turrets with a fire-control module or a crewed operator keep fighting on their own** (§2.7). No
weapons-off state is introduced; the existing crewed/slaved rule does the work.

**Content:** one module in `modules.json` (a constructor bay on `shell_facility_bay`), plus the
Storage/Engineering/Repair facilities §12.24 step 5a needs. All authoring.

**Tests:** a build request from a rig with no living Construction hardpoint is refused and the
request cleared; a request beyond the executor's `buildRange` is refused; destroying the Construction
hardpoint mid-job cancels it; `B` does not open placement without a living Construction hardpoint.

### 12.27 The Local Command System — `features.md` §4.0/§4.3

*Specified 2026-08-09. This replaces §12.16 item 26 (three cursor-free verbs), which is withdrawn.*

**It is far less new construction than it looks**, because two built systems are waiting for a
producer that has never existed:

| Built and inert | What it needs | What supplies it |
|---|---|---|
| **`PartySystem`** — formation-keeping and shared retaliation, writing real `ThrustInput` | Something to create `PartyLeader`/`PartyMember`. **Nothing anywhere does** | A **Defend** order aimed at a friendly entity *is* a party membership |
| **`CommanderSystem`** — sets `Commander::orders` | Something to *read* `orders`. **Nothing does** — the only reader is the system itself, checking whether it already set `Retreat` | The order executor below |

#### Types

All in `shared/components/`, all POD:

```cpp
enum class OrderKind : std::uint8_t { Move, Attack, Defend, Build };
enum class Stance    : std::uint8_t { Hostile, Defensive, Peaceful };  // Hostile is the default

struct UnitOrder {
    OrderKind kind;
    Vec2 targetPoint;                        // Move, Defend-a-position, Build
    entt::entity targetEntity = entt::null;  // Attack, Defend-an-entity
    entt::entity targetHardpoint = entt::null;  // Attack a specific hardpoint; null = let the AI roll
    BlueprintId buildTarget;                 // Build
};

struct OrderQueue { std::vector<UnitOrder> pending; };  // Law 4's Rig::children exception applies
struct UnitStance { Stance value = Stance::Hostile; };
```

**Attack needs no new targeting type.** `Target { rig, hardpoint }` already models exactly the
distinction `features.md` §4.3 draws — attack-the-chassis writes `rig` and leaves `hardpoint` null
for the AI to roll; attack-a-hardpoint writes both. Its own doc comment already explains why it is
two fields.

#### Systems

- **`OrderSystem`** (`modes/space/systems/`) — reads `OrderQueue`, writes `ThrustInput`, `Target`,
  and party membership; drops an impossible order and advances; clears an order on completion.
- **Schedule position: immediately after `NpcAiSystem`.** `PartySystem` already documents this exact
  constraint and this exact reason — *"runs after `NpcAiSystem` so a member's formation steering is
  not immediately overwritten by `NpcAiSystem`'s per-tick `ThrustInput` reset."* `NpcAiSystem` drives
  every rig with `Target` + `ThrustInput` that is not `PlayerControlled`, which is exactly the set of
  ordered units, so running before it would produce twitching.
- **Arbitration, in one rule:** *player order wins; `Retreat` interrupts; an empty queue means AI.*
  `CommanderSystem`'s damage-triggered `Retreat` is the one AI behaviour permitted to override a
  standing player order, and the queue resumes afterwards.

⚠️ **`CommanderOrders { Dispatch, Retreat, Defend }` is a stance enum, not an order list** —
`Defend` and `Retreat` are postures and `Dispatch` means "engaged, no override." Either rename it
alongside `UnitStance` or fold it in; do not add tactical verbs to it.

#### Order availability is derived, never authored

Per `features.md` §4.3, and it is Law 4 on the command surface:

| Order | Offered when |
|---|---|
| **Move** | `Propulsion` is non-zero — **requires §12.25** |
| **Attack** | At least one living weapon hardpoint |
| **Build** | A living `FacilityKind::Construction` hardpoint — **requires §12.26** |
| **Defend** | Propulsion to escort an entity; nothing to hold a position |

A mixed selection offers the **intersection**. Both rows marked *requires* are why this section is
sequenced after §12.25 and §12.26 rather than beside them.

#### Gating, and the two modules that carry it

Command requires, **on the rig the player occupies**: a living `Comms` hardpoint (`commsRange` —
reach) and a living `Crew` module with a non-zero `command` roll (authority). Neither alone suffices.

**`CommsSystem`'s hail check moves from `SensorRange` to `commsRange` in the same change.** It gates
hailing on sensor range today only because that was the one range stat that existed; sensors detect
and comms talk. That gives the new module two consumers rather than one, which is its §2.4
justification.

> ➕ **A third consumer, added 2026-08-09: the sensor datalink.** `features.md` §8.3 settles that
> **linked allies share sensor coverage**, gated on this same `commsRange` check — no new component,
> no new range stat, no new system. Three consumers on one module is a comfortable §2.4 margin, and it
> is the reason `Comms` is worth authoring at all rather than folding into the sensor module.

**Symmetric for NPCs (§6.3).** An enemy commander runs the same gate, so **destroying a hostile's
comms hardpoint degrades their fleet coordination** — a tactical objective that costs nothing extra
to build because it is the same check running for both sides.

**And it now costs them their shared sensor picture too.** With the datalink on the same gate, one
comms hardpoint carries both fleet coordination *and* the fleet's combined coverage — so killing it
blinds a formation mid-engagement rather than merely desynchronising it. **The same shot, a much
larger consequence, and no additional code**, since both effects read the one check.

#### Selection and input

Selection is **presentation state on the singleton entity** (§12.24's "Where UI state lives"), never
a component on the selected rigs — one client's selection must not appear in another's registry.

Input per `features.md` §3.6: right-click selects, right-drag box-selects, double-right-click selects
all of that `BlueprintId` on screen, `Z`/`X`/`C`/`V`/`N` arm an order, right-click issues it, `T`
cycles stance, `Esc` clears. **`Shift` means *add* everywhere** — added to the selection, or appended
to the order queue instead of replacing it. **Left-click keeps firing throughout** — the aim point is
surrendered only for the instants of the right-clicks, which is what makes commanding-while-
dogfighting real.

*An earlier draft of this section carried a gesture split — click replaces, drag adds — invented
purely because `Shift` was the afterburner. `Shift` moved to `Ctrl` on 2026-08-09, so the
conventional modifier is free and the workaround is deleted.*

> **Orders are `core::Intent`s, not request components.** An order is player input addressed by
> stable `ActorId`, which is §12.24 step 2's rule exactly: *`core::Intent` for what an actor did,
> a request component for what a specific entity wants done to it.* Add `IssueOrderIntent` to the
> variant. This makes orders the third real user of a queue that had zero producers.

#### `BlueprintId` is what "same type" means

Law 4 removed vessel classes deliberately, so blueprint identity is the only honest definition for
double-right-click's "all of this type." Accepted consequence: two ships from one Template match; a
custom variant does not, however similar. That reads as *"select all my Vanguards,"* which is the
useful meaning.

#### What is deliberately not here

- **Strategic command** — cross-system dispatch, fleet movement, per-system build queues. It needs
  `core/galaxy/` fleet records and **galactic coordinates (§12.17)**, since "move that fleet there"
  has no *there* until systems have positions. `CommanderSystem::TickCoarse` is its entry point and
  already exists.
- ❓ **Whether the strategic layer stays bridge-gated** while tactical command travels with the
  player. Open, and it blocks nothing here.

#### Persistence

**Nothing in this batch is saved.** `OrderQueue` and `UnitStance` are registry-local, so local orders
die with the entity when a unit warps out — which is `features.md` §4.3's stated behaviour, arriving
free from Law 2 rather than needing a rule. Strategic orders will live in `core/galaxy/` records
instead, which is exactly why the two tiers need separate homes.

#### Tests

- A Defend order aimed at a friendly creates `PartyMember` with a distributed `formationOffset`, and
  `PartySystem` steers it — **the first test in the codebase where a party exists at all.**
- An order queue executes front-to-back; a Build entry occupies the unit until complete.
- An impossible order (target destroyed) is dropped and the next advances.
- `Retreat` interrupts a player order and the queue resumes after.
- Stance governs engagement independently of the queue: a Peaceful unit under fire keeps moving and
  does not return fire.
- Move is absent from a unit with zero `Propulsion`; Build is absent without a Construction
  hardpoint; a mixed selection offers the intersection.
- Commanding is refused with no `Comms` hardpoint, and refused with a crew module whose `command`
  roll is zero.
- The player's own rig never appears in the commandable set.

### 12.28 World Bodies, Hittability And The Star Hazard — §13.3's group 1 blocker

*Settled 2026-08-09, resolving §13.4 decision 4. This is the first cluster of the §13 wiring audit
to be worked through to a specification. It exists because §12.24 step 1 — calling `PopulateSystem`
from `OnEnter` — produces a world **nothing can see and nothing can shoot**, so steps 2 and 3
cannot be judged against anything.*

Three problems, one issue, because the fixes share a component:

| §13 finding | Symptom |
|---|---|
| **A** | The sun, every planet, every asteroid and every loot drop satisfy neither `WorldRenderer` view. Nothing draws them |
| **B** | Nothing in the codebase can damage an asteroid, so mining, the tutorial's asteroid step and all material loot are unreachable |
| — | The sun out-accelerates a fighter 10:1 and there is no consequence at the bottom of the fall |

#### Hittability needs no new component — it needs one narrowed view

`ProjectileSystem::FindHit` views `HitRadius, WorldTransform, ParentRig`. **`ParentRig` is in that
view only so the loop can skip the shooter's own rig** — an implementation detail leaked into a
view predicate. Move it into the body:

```cpp
for (auto [hardpoint, hitRadius, hpXf] : registry.view<HitRadius, WorldTransform>().each()) {
    const auto* parent = registry.try_get<ParentRig>(hardpoint);
    if ((parent != nullptr && parent->root == projectile.shooter) ||
        registry.all_of<Destroyed>(hardpoint)) {
        continue;
    }
    ...
}
```

The view becomes exactly *"things a shot can hit."* Rig roots carry `CollisionRadius`, not
`HitRadius`, so nothing is caught that was not caught before. **Give an asteroid `HitRadius` and
it is shootable through the path that already exists** — `DamageSystem` already drains its
`PendingDamage` against the `Health` `WorldGen` already gives it, already tags it `Destroyed`, and
`MiningSystem` already reacts. Four dead subsystems come back on one `emplace` and one narrowed
view, with no new concepts.

> This is the general lesson of the §13 audit stated once: **a view predicate is a contract about
> what a system acts on.** Adding a component to it for a reason that is really about the loop body
> narrows that contract silently, and the narrowing is invisible at the call site.

#### Types

Only *drawing* needs something new.

```cpp
// shared/components/Physics.h, beside CollisionRadius.

// Declaration order is draw order AMONG WORLD BODIES, back to front. It is NOT the world's
// full draw order -- rigs and projectiles are drawn by their own passes and always sit in
// front of every one of these. WorldRenderer::DrawWorld states the full order in one place;
// see it before adding an enumerator. Same convention as BridgeView::kAllKinds, and the same
// hazard, so the renderer's switch must be exhaustive with no default.
enum class BodyKind : std::uint8_t { Star, Planet, Wreck, Drop, Asteroid };

// A world object that is NOT a rig -- a star, a planet, an asteroid, a dropped item, a wreck:
// one entity, one shape, no hardpoints, no hierarchy. Vessels and stations are deliberately
// absent, and not because they are a missing enumerator: a rig is not one drawable (a root
// plus N hardpoint entities), and giving a root a `radius` here would put it alongside
// CollisionRadius as a second record of the same fact.
//
// Deliberately NOT features.md 3.5's scale system -- there is no hullRadius here, no art, no
// atlas, no per-shell sizing. Those need textures that do not exist (architecture.md 6).
struct WorldBody {
    float radius = 1.0f;
    BodyKind kind = BodyKind::Asteroid;
};

// The lethal volume around a star. Mirrors GravityWell field for field and uses the SAME
// falloff, deliberately: one shape, two effects, both centred on the same entity.
struct Corona {
    float range = 0.0f;            // Outer edge, world units from centre. Damage is zero here.
    float damagePerSecond = 0.0f;  // At the centre, scaled by (1 - d/range)^2 outward.
};
```

⚠️ **`BodyKind` is a new type**, and it is the third of three separate ordering concepts. Keeping
them apart is `features.md` §3.7's explicit ruling, recorded there because the earlier
"Z-layer / Z-level" naming differed by three characters and was confused:

| Concept | Scope | Governs | Status |
|---|---|---|---|
| **Draw layer** (`features.md` §3.5) | *Within* one rig — ventral, hull, detail, dorsal, overlay | Rendering only: which part of a hull draws over another part of **the same hull**, tie-broken by **local** y | 📋 needs a new `ShellDef` field |
| **Altitude band** (`features.md` §3.7) | World-space height, **vessel vs. vessel** | Collision **and** which hull draws on top. Occupancy scales with hull size; projectiles, shields, weapons and sensors ignore it | 🧊 deferred, fully designed |
| **`BodyKind`** (here) | Non-rig bodies vs. rigs | Rendering only | 📋 new |

`BodyKind` cannot be folded into either. §3.5's layers order *parts of one rig* and that section
explicitly declines the world-level question in its own words — *"Sorting between separate rigs can
stay arbitrary — two ships overlapping at the same altitude have no correct answer anyway."* A
planet drawing behind every ship in the system is precisely that declined question.

> ⚠️ **World bodies are outside the altitude-band system entirely, and stay outside it if §3.7
> un-defers.** *"Planets are background, everything flies over them"* reads like an altitude claim
> and is not one: a planet occupies **no band**, is never "below" anything, and simply never
> collides while always drawing behind. §3.7 scopes bands to **vessel-vessel** collision, so a
> future implementer must not give a star, planet, asteroid, drop or wreck a band — and
> `HazardSystem`'s corona ignores altitude for the same reason projectiles and sensors do.

**Why a kind and not a layer number.** The field does three jobs a `std::uint8_t` cannot: world
draw order, placeholder colour (there is no art), and — the load-bearing one — **which icon §8.2's
`IconRenderer` substitutes when the body shrinks below a few pixels on zoom-out** (§9.1 makes that
substitution a requirement, not an optimisation; §8.2 settles that icons are generated
programmatically as vector shapes per kind). An integer cannot answer the third.

> **The justification does not depend on planets staying background.** If §3.7's collision work
> ever makes celestial bodies solid, draw order evaporates *for planets specifically* — you cannot
> overlap what you collide with — but it survives for the other four kinds, which overlap ships by
> definition: you fly over a drop to collect it, over a wreck, over an asteroid, and **into** a
> star's corona. And the icon job is collision-independent entirely: a solid planet needs a planet
> marker at §8.1's zoom levels exactly as much as a background one does. **What would change is the
> ordering of the reasons, not the existence of the type.**

**Why an enum and not a tag component per kind.** Tags (`Star{}`, `Planet{}`, …) alongside a bare
`WorldBody { float radius; }` would be more EnTT-idiomatic, and `Asteroid` already exists as one.
Rejected: adding a sixth body type would then mean remembering a new render pass *and* a new
`IconRenderer` branch, with **no compiler help** — which is precisely how `BridgeView::kAllKinds`
shipped missing an enumerator (§13.5). An enum with an exhaustive `switch` and no `default` names
every site you missed at compile time. Mechanism over discipline (§0).

#### Systems and factories

**`WorldGen::SpawnSun`** — add `WorldBody{kSunRadius, BodyKind::Star}` and
`Corona{kCoronaRange, kCoronaDamagePerSecond}`. The existing `GravityWell` and `LightSource` are
unchanged.

**`WorldGen::SpawnPlanets`** — add `WorldBody{radius, BodyKind::Planet}`. Planets remain
**non-colliding background**: everything flies over them. That is a decision, not an omission
(below).

**`WorldGen::SpawnAsteroids`** — this is the one that changes shape.

> 🔄 **This revises a recommendation made earlier in this same session.** An earlier proposal gave
> asteroids `BodyMass` so `PhysicsSystem` would integrate the drift velocity `WorldGen` already
> rolls for them. **That was wrong.** `BodyMass` puts them in the physics view, `OrbitSystem`'s
> gravity loop already pushes anything with `Velocity` that is not an `OrbitBody`, and an asteroid
> has neither `LinearDamping` nor a `maxSpeed` clamp. Every asteroid inside the well would
> accelerate without bound into the star and **the belt would drain within a minute of the fix
> landing.**
>
> **Asteroids carry `OrbitBody`, like planets.** `OrbitSystem` excludes orbiting bodies from
> gravity, so they circle the star indefinitely and deterministically, the drift-velocity hack
> disappears entirely, and no asteroid needs `BodyMass`, `Velocity` or damping. It is also simply
> true: a belt orbits.

So an asteroid becomes: `Asteroid`, `AsteroidComposition`, `Health`, `WorldTransform`, `OrbitBody`,
**`HitRadius`**, **`WorldBody{radius, BodyKind::Asteroid}`** — and loses `Velocity`.

**`OrbitSystem`** — writes `PreviousTransform` before overwriting `position`, guarded by `try_get`.
Two lines. Without it an orbiting body renders un-interpolated and judders against everything else
on screen, which is the same defect `PhysicsSystem` already avoids for rigs.

**`WorldRenderer`** — a new `DrawWorldBodies` pass, first, before `DrawShips`. Iterates
`WorldBody, WorldTransform`, sorted by `kind`, and **interpolates only when `PreviousTransform` is
present** — a drop that never moves should not carry a component purely to satisfy a view.

> ⚠️ **`DrawWorld`'s body is the single statement of the world's draw order**, and it must be
> readable as one back-to-front list. `BodyKind` orders only the first line of it:
>
> ```
> DrawWorldBodies    // Star -> Planet -> Wreck -> Drop -> Asteroid   (BodyKind order)
> DrawShips          // rig roots
> DrawHardpoints     // rig children -- features.md 3.5's five intra-rig layers land here
> DrawProjectiles
> ```
>
> Nothing today wants a world body *in front* of a rig. If something ever does — a nebula the
> player flies into, a corona bloom — **it is a new pass, not a reordered enumerator**, because
> the thing that changed is where it sits relative to rigs, which `BodyKind` does not express.

🐛 **A station currently renders as a nose-forward arrowhead.** `DrawShips` draws *anything* with
`CollisionRadius` as a triangle, and `RigFactory` puts `CollisionRadius` on every rig root — Law 4
means a station and a fighter reach that view identically, which is correct, and the shape is then
wrong for one of them. There is no vessel-type flag to branch on, and §12.25 is deleting the one
flag that came closest.

> 🔄 **This section originally proposed making the silhouette emergent by reshaping the hull —
> triangle when `Propulsion` is non-zero, a disc when it is not — and that was withdrawn the very
> next day.** `features.md`'s "Collision shape, and drawing what you test" (settled 2026-08-09)
> retracts exactly this idea: swapping the drawn shape made the drawn hull disagree with the tested
> hit shape (`ProjectileSystem` tests circles; the triangle's flanks cut inside it, so shots could
> pass through drawn hull). **A ship must keep the shape it is actually tested against, whether its
> engines are live or dead — it is disabled, not transformed into a different kind of object.**

**The corrected mechanism: render the hit shape, and show heading separately.** `DrawShips` draws
each rig root as its real hit shape — a circle sized by `CollisionRadius` today, `ShellDef`'s
optional baked collision polygon once §3.5/§6 land — and a **nose marker** is drawn on top of it,
present when `Propulsion` is non-zero and absent when it is zero (the same living-hardpoint capability
§12.25 makes `Propulsion` express). This still answers the station-vs-fighter bug above — a station's
circle no longer looks like a pointed fighter — and still shows "engines destroyed" at a glance,
without lying about the hitbox or reshaping the hull into something structurally different from what
it still is.

*This is a placeholder shape either way — `WorldRenderer`'s own header notes every shape here dies
when the asset pipeline lands (§6) and §3.5 settles what replaces it. It is worth the few lines now
because the micro loop (§12.24 steps 1–3) has to be judged by eye, and a system full of identical
arrowheads is not judgeable.*

**`MiningSystem` / `LootSystem`** — every drop and wreck they create gains
`WorldBody{radius, BodyKind::Drop | Wreck}`. They are currently spawned with a bare
`WorldTransform` and are invisible.

**`HazardSystem`** — a new file, `modes/space/systems/HazardSystem.{h,cpp}`.

```cpp
// Queues environmental PendingDamage from hazard volumes. Today that is exactly one hazard --
// a star's Corona -- and the system exists as its own file rather than folded into OrbitSystem
// because OrbitSystem owns orbits and gravity, not damage, and because nebulae, radiation belts
// and minefields are all the same shape.
void Tick(const SystemContext& ctx);
```

Its whole body is one nested loop, and **it needs no branch for what it is damaging**:

> **View: `Health, WorldTransform`, `exclude<Destroyed>`.** That is hardpoints *and* asteroids,
> uniformly. Rig roots carry no `Health` and are excluded naturally; drops and wrecks carry none
> either.

This is §3.2's localized damage model applied honestly rather than exempted. A hazard is a
*volume*; hardpoints are entities with positions; so **each hardpoint burns according to its own
distance from the star**, and a capital hull half inside the corona burns only on the side that is
in. No whole-rig special case, no exception to §3.2, no branch on vessel type — Law 4 and §3.2
produce the behaviour between them.

> 🐛 **Hazard damage must set `PendingDamage::source = entt::null`.** `PartySystem::FindAttacker`
> scans its members' hardpoints for a non-null `source` and calls `AlertParty` with it. A star with
> a real `source` would be **set as the party's combat target**, and every escort would turn and
> attack the sun. `FindAttacker` already skips a null source, so the null is the whole fix — but it
> is not optional, and it is exactly the kind of cross-system consequence a new `PendingDamage`
> producer has to check for.

**Schedule position — two constraints, no fixed index:**

| Must run | Relative to | Why |
|---|---|---|
| **After** | `HierarchySystem` | It reads settled hardpoint `WorldTransform`s. *If §13.5 group 2's reorder lands first, that is after `PhysicsSystem` too* |
| **Before** | `CollisionSystem` / `ProjectileSystem` | Both `QueueDamage` helpers overwrite `PendingDamage::source` when accumulating. Running the hazard first means **a real attacker's `source` always wins the tick**, so retaliation is never lost to a coincidental burn |

Add both to `SystemSchedule.cpp`'s comment block in the same commit (§2.4).

#### Tuning — four radii, each with a job

Every number below is a placeholder in the same category as `kSunGravityStrength`, not authored
content (Law 10 governs ship/module/shell definitions, not procedural-generation constants).

| Radius | Value | What happens there |
|---|---:|---|
| `kSunLightRange` | 6,000 | Lighting reaches this far *(unchanged)* |
| `kMinOrbitRadius` | 3,200 | Innermost planet *(unchanged)* |
| **asteroid band** | **1,800 – 2,800** | **Moved out** from 1,200–2,200 *(below)* |
| `kSunGravityRange` | 2,200 | Gravity begins *(unchanged)* |
| *point of no return* | **≈1,500** | Emergent, not authored: gravity out-accelerates a fighter's engines |
| `kCoronaRange` | **1,200** | Burning begins — inside the point of no return by design |
| `kSunRadius` | **350** | The visible disc |
| `kCoronaDamagePerSecond` | **60** | At the centre. Fighter hardpoints hold 35–120 hull, so the surface kills in seconds |

**The asteroid belt moves, and this is a real bug being fixed.** It is authored at 1,200–2,200
today, and the point of no return is ≈1,500 — so **the inner half of the belt sits inside the zone
a fighter cannot climb out of under thrust.** That is harmless only while mining is unreachable; it
becomes a one-way trip the moment finding B is fixed. Moving the band to 1,800–2,800 puts the whole
belt outside both the burn and the point of no return, leaves a clean gap before the innermost
planet at 3,200, and keeps the inner edge close enough that mining there still means fighting the
star's pull. *Retuning the well instead was considered and rejected: its 10:1 authority over a
fighter is the drama, and weakening it to accommodate a misplaced belt trades the good number for
the arbitrary one.*

**The escalating sequence is the point.** 2,200 you feel it · 1,500 you are committed · 1,200 you
are burning · 350 you are dead. Each boundary is legible from the one before it, which is what
makes the fall a decision rather than a trap.

#### The two behaviours this deliberately does not build

**Planets and stars have no collision.** Everything flies over a planet; the star kills by burning,
not by contact. Ramming a celestial body is a real feature and §3.7 has the vocabulary for it, but
it needs a damage model of its own, and hanging that on the issue that merely makes the world
visible is how the micro loop stops being shippable. **Revisit trigger:** the first time a body
is expected to block line of sight or shelter a ship — that is when contact starts meaning
something.

> **State the honest version of this deferral, because it is partly scope control.** There is a
> real design argument *for* solid bodies: a system with none has no terrain, so nothing blocks a
> shot, nothing shelters a ship, and position matters only as distance-to-star — which §12.26's
> "escort a constructor to the frontier" and `features.md` §4.3's tactical positioning both quietly
> want. It is deferred because it is a **feature, not a wiring fix**, and the cost is concrete:
>
> - `CollisionSystem`'s broad phase documents its own precondition — *"`kCellSize` must be >= the
>   largest `CollisionRadius` any rig can have; `Query()` only visits the query point's own cell and
>   its 8 neighbors."* `kCellSize` is **300**. A planet of any believable size breaks that
>   invariant, so solid bodies need a separate static-body path or a rebuilt broad phase.
> - The narrow phase is hull-vs-hull SAT over sampled hardpoints. A body is a circle, so it needs a
>   circle-vs-hull path that does not exist.
> - Solid planets pull **solid asteroids** with them for consistency, which is its own mechanic
>   (dodging rocks in a belt) rather than a free consequence.

**Death by star is not a new death path.** The corona queues `PendingDamage`; `DamageSystem`
destroys the rig when its last hardpoint dies, exactly as it does for gunfire. *An earlier framing
in this session's discussion described a lethal radius. It was revised to a burn band for four
reasons: it needs no destruction rule of its own; `DamageType::Energy` makes an Energy-absorbing
shield let you dive deeper, an interaction that falls out of §3.1's existing roster without being
designed; it makes the star terrain rather than a wall, so grazing it to shake a pursuer is real
play; and §2.4's own standard for this class of thing is "a gamble the player took, not a gotcha
the game sprang" — a hull bar visibly falling is a warning, an invisible line is not.*

#### Tests

Headless; `HazardSystem` takes a bare `SystemContext` with no window.

- A projectile fired at an asteroid queues `PendingDamage`; `DamageSystem` tags it `Destroyed`;
  `MiningSystem` spawns a `MaterialDrop` the same tick. **The regression test for finding B**, and
  the first test in the codebase where mining happens at all.
- A projectile still never hits a hardpoint on its own shooter's rig — the regression test for the
  narrowed `FindHit` view.
- A rig inside `kCoronaRange` accumulates `PendingDamage` on every hardpoint, scaled by depth; one
  outside accumulates none.
- **Hazard `PendingDamage` carries a null `source`, and a party under corona damage does not
  acquire a target.** The regression test for the `AlertParty` hazard.
- An asteroid at the belt's inner edge is still at the belt's inner edge after 10,000 ticks — the
  regression test for the `OrbitBody`-not-`BodyMass` decision, and for the belt never draining.
- `PopulateSystem` emits at least one `WorldBody` of each of `Star`, `Planet` and `Asteroid`, and
  every entity it creates carrying `WorldTransform` also carries `WorldBody`. **This is the
  assertion that would have caught finding A**, and it is worth writing as a blanket invariant
  rather than three specific checks.
- A rig with non-zero `Propulsion` draws its nose marker and one with zero does not, and a rig
  whose last engine hardpoint is destroyed loses the marker on that same tick — the regression test
  for the corrected mechanism above — while the rig's drawn hit shape itself never changes between
  the two states. It needs no window if marker-presence is factored into a small pure predicate the
  test can call.
### 12.29 The System Menu And Returning To The Main Menu — §13.3 Y

*Settled 2026-08-09. Almost none of this is new design: `features.md` §3.6 **already** binds this
menu, already makes `Esc` contextual, and already restricts it to singleplayer. What it never had
is a home, a spec, a mode transition, or a resolution of the contradiction it walked into.*

**The defect it closes.** `main.cpp`'s mode loop is **one-way**: `QuitRequested()` is only checked
while `activeMode == &menu`, and there is no path from `SpaceFlight` back. Once the game starts, the
only exit is closing the window.

#### It pauses — and §3.4 has to be amended to say so

`features.md` §3.4 states *"The simulation never stops while the player is alive,"* and §3.6's
binding table cites §3.4 as its own justification while calling this **"the pause menu"** and
marking it **"Singleplayer only."** Those cannot both be read literally, and the "singleplayer only"
qualifier is the tell — a menu that did not stop time would have no reason to be restricted.

> **§3.4 forbids pausing on any surface that carries information or decisions. The system menu
> carries neither, by construction, and that is what makes it legal.**

The distinction is not a loophole; it is the same one §3.4's own list draws. The navigation map, the
Engineering view, station services and the Bridge all hand the player *tactical value* — routes, the
threat picture, repairs, orders — so freezing time while reading them is a free advantage and §3.4
rightly forbids it. The system menu offers Resume and Quit. You cannot repair from it, retarget from
it, reallocate power from it, or issue an order from it.

> ⚠️ **This is a live constraint on what the menu may ever contain, not a one-time ruling.** If
> Settings later gains anything with in-fight value — a power-priority editor (§2.9), a weapon-group
> editor (§3.6), a keybinding surface consulted mid-fight — **that surface stops being legal here
> and moves to a non-pausing home.** Add nothing to this menu without re-reading this paragraph.

Multiplayer is unaffected: §3.6 already scopes the pause to singleplayer, and Law 9's authority
model has no way to honour a client-side freeze. In a future session the menu opens and the
simulation keeps running — which is legal precisely because the menu confers nothing.

#### `Esc` is a ladder, and §3.6 specifies only its middle

§3.6 gives *"Clear selection; if nothing is selected, the pause menu."* Two more consumers land
before it, so the full precedence is **innermost first**:

| Order | If… | `Esc` does |
|:---:|---|---|
| 1 | Build placement is active (§12.26) | Cancel placement |
| 2 | A unit selection is non-empty (§12.27) | Clear the selection |
| 3 | The system menu is closed | Open it |
| 4 | The system menu is open | Close it |

Steps 1 and 4 are additions to §3.6's table and belong in it. **A menu that cannot be closed by the
key that opened it is the kind of gap that only shows up in play.**

#### Home and state

**`modes/space/ui/SystemMenu.{h,cpp}`.** Not `shared/ui/` — that is the theme layer (`sr_shared_ui`)
and knows nothing of modes. Law 11's tie-breaker keeps it in the mode until a second consumer
appears, which is the same call `NavigationMap` already got (§3's directory blueprint).

**Open/closed state goes on the singleton entity**, per §12.24's "Where UI state lives" — the
`CommsLog` precedent, not a `SpaceFlight` member.

**One new `SpaceFlight` member, and it is mode state rather than UI state:**

```cpp
bool ShouldReturnToMenu() const;   // Polled by main.cpp, exactly like MainMenu::ShouldStartGame()
```

`main.cpp` gains the mirror of the transition it already has:

```cpp
if (activeMode == &game && game.ShouldReturnToMenu()) {
    activeMode->OnExit();
    activeMode = &menu;
    activeMode->OnEnter();
}
```

> 🐛 **"The world ceases to exist on quit" is the intent, and nothing in the code makes it true.**
> `SpaceFlight` is constructed once in `main.cpp` and lives for the whole process; `world_` is a
> member of it; **`OnExit()` is empty.** Returning to the menu therefore leaves the entire previous
> world sitting in memory, and the moment §12.24 step 1 populates a world in `OnEnter`, starting a
> second game populates *on top of the first* — two suns, two players, two of everything.
>
> The teardown has to be written, in both halves, and the split matters:
>
> - **`OnExit()` releases the world** — `world_ = SystemWorld{}`, plus `intents_.Clear()`. This is
>   where it belongs semantically: quitting is what destroys the session, and it also stops a dead
>   world's registry occupying memory for as long as the player sits in the menu.
> - **`OnEnter()` does not assume it was called** — it resets `world_` and `clock_` before
>   populating, so a clean start does not depend on a prior exit having run.
>
> `WarpToSystem` already demonstrates the pattern (`world_ = SystemWorld(id)` destroys everything
> before replacing it — Law 2's clean handoff). **This is a few lines in step 1 and an invisible bug
> if step 1 lands without it**, because with `OnExit` empty the duplication is silent: the second
> world looks correct and the first one is still being ticked underneath it.

#### What the menu contains — and what it deliberately does not

*Confirmed 2026-08-09: this is the **only** pause in the game. Every other menu — docking, station
services, Engineering, the navigation map, the Bridge — runs with the simulation live, per §3.4.*

| Entry | Intended | Ships first |
|---|:---:|:---:|
| **Resume** | ✅ | ✅ |
| **Save** · **Load** | ✅ | ❌ — see below |
| **Settings** — audio, graphics | ✅ | ❌ — nothing configurable exists yet |
| **Quit to Main Menu** | ✅ | ✅ — with a confirmation, since **all progress is lost** (below) |

**Confirmed excluded: the power-priority list (§2.9) and the weapon-group editor (§3.6).** Both
carry in-fight value, so both would void the §3.4 exception this menu depends on. §2.9 already names
their home — *"the avionics surface, alongside the other ship-configuration readouts"* — which does
not pause, and where configuring under fire is the intended cost.

**Save and Load are absent rather than disabled.** §2.4 forbids dead abstractions, and a button that
cannot work is the UI form of one. They are absent for a reason bigger than wiring: per §13.3 Y,
**`SaveFile` could not save a game if it were called.** Its entire API is
`SaveShipBlueprint`/`LoadShipBlueprint`/`SaveKnowledgeStore`/`LoadKnowledgeStore` — no world seed, no
registry, no player rig, no `Wallet`/`CargoHold`, no `FactionEconomy`, no `DiplomacyMatrix`, no
`DiscoveryState`, no `WreckLedger`. `features.md` §3.3 settled the *save model* without anything
underneath it.

**That separation is the point of this section.** Quit-to-menu needs a two-way mode transition and
nothing else, so it ships now. Defining what a world save contains is a larger piece of work that
must not hold it up.

⚠️ **Until Save exists, quitting to the menu discards everything.** The confirmation prompt must say
so plainly rather than asking "Are you sure?" — the player has no way to avoid the loss, and a vague
prompt implies they do.

#### Tests

- `ShouldReturnToMenu()` is false on entry, true after the quit entry is confirmed, and false again
  after `OnEnter` runs — a latch that does not re-fire.
- **`OnEnter` twice in a row leaves exactly one sun, one player and one station** — the regression
  test for the re-entrancy bug above, and the one that would otherwise be found by playing.
- The `Esc` ladder resolves innermost-first: with placement active it cancels placement and does not
  open the menu; with a selection it clears the selection and does not open the menu; with neither it
  opens; opened, it closes.
- Opening the menu freezes tick advancement and closing it resumes, with no tick catch-up burst —
  `FixedTimestep`'s accumulator must not bank real time while paused, or unpausing fast-forwards the
  world through however long the player sat in the menu.
---

### 12.30 The Docked Screens — inventory, gates, and the widget layer

*Settled 2026-08-10. §12.24 step 5 designed the **router**; this section is what the router routes
to. Every claim below was verified against `src/` on 2026-08-10 by grepping for readers and callers.
**Two of §12.24 step 5a's rows are superseded here**, and one new content-pipeline defect was found
in the process.*

**The starting position, verified.** Six menus exist as code and are referenced by nothing but their
own tests — `StationServicesMenu`, `StorageMenu`, `ModulesMenu`, `EngineerMenu`, `RefactorMenu`,
`CustomizeMenu`. Two designed surfaces have no menu at all: Research (§12.1) and Manufacturing
(§12.18). `BridgeView` is the ninth, and the only one drawn — `SpaceFlight.cpp:150` is the sole menu
call anywhere in `src/`. **None of the nine handles input, and none places the request it builds.**

#### Two rows of §12.24 step 5a are superseded

##### 1 — `StorageMenu` and `ModulesMenu` are not docked screens at all

*This supersedes step 5a's* **`StorageMenu` · *(read-only)* · none · `FacilityKind::Storage`** *row.*

Three later-or-equal-dated decisions rule the gate out:

- `features.md` §2.7 ⚠️ — *"this previously read 'unmounting it **at a station** … at a workbench,'
  which described a gate that does not exist and should not."*
- `features.md` §5's resolution log — *"**live refit is unrestricted** (§2.7 — legal any time, priced
  by §3.4's no-pause rather than by a station gate)."*
- `features.md` §3.10 lists **inventory** and **loadout** among the flight HUD's on-demand overlays.

Live refit is sanctioned combat play (§11.9). A facility gate on *reading your own manifest* would
make it unreachable in exactly the fight it was legalised for. Both files move to §3.10's overlay set
and leave the router's table. (`ModulesMenu` was never in step 5a's table; `StorageMenu` was, and
that row is the wrong one.)

⚠️ **They do not become trivial by moving.** They need the same widget layer, the same `Row` type,
and the same input plumbing as the docked six; the only thing they shed is the facility gate and the
§3.4 health readout. **They ship with this batch**, keyed on a HUD button rather than on a tab.

##### 2 — no new `FacilityKind` for drafting

*This supersedes step 5a's* **`CustomizeMenu` · a drafting facility (new kind)** *row.*

`CustomizeMenu` writes a design into a knowledge network. `ManufacturingSystem` is the only system
that reads designs *out* of one, and §12.18 already gives it the `ctx.knowledge` gate that check
needs. A seventh enumerator, authored by no content, gating one screen, is §2.4's dead abstraction
in its purest form. **Drafting is a section of the Manufacturing screen**, not a facility.

#### `FacilityKind::Storage` is deleted. `FacilityKind::Trade` replaces it

*Decided by the project owner 2026-08-10.* `Storage` was a facility kind naming a **container**, and
the container already exists as a component.

> **A station can hold goods because it carries a `CargoHold`.
> A station can *trade* those goods because it carries a living `FacilityKind::Trade` hardpoint.
> One hold, two questions.**

| Capability | Gated on | Screen section |
|---|---|---|
| Deposit / withdraw | The station carrying a **`CargoHold`** — no facility hardpoint | **Storage** |
| Buy / sell | A living **`FacilityKind::Trade`** hardpoint | **Market** |

**Three reasons this is the better split**, in order of weight:

1. **It makes the fix for §13.3 O *be* the feature.** That finding already assigns `CargoHold` to
   `StationFactory` because `StationServicesSystem` bails on `try_get<CargoHold>(station) == nullptr`.
   Making the component the storage capability means one mechanism serves both, instead of a facility
   kind sitting beside a component that does the same job.
2. **The station's stock becomes finite and physical.** Buying draws the hold down; selling fills it
   up; depositing is the same motion without payment. That is the substrate `core/economy/Pricing.h`
   (§13.5 group 2c) requires — **local scarcity is only computable against a real local quantity**,
   and `features.md` §2.10 settles prices as derived outputs of exactly that. A shop with infinite
   stock has no local price.
3. **The enumerator count is unchanged at six**, so the `kAllKinds` parallel-array defect gets no
   worse while it is being fixed.

⚠️ **Deleting the enumerator touches six sites, and two of them are silent defaults.**

| Site | Change |
|---|---|
| `Taxonomy.h:52` — `enum class FacilityKind` | `Storage` → `Trade` |
| `Taxonomy.cpp:40` — the `FromString` table | `{"storage", …}` → `{"trade", …}` |
| `Taxonomy.cpp:96` — the `ToString` case | ditto |
| `Taxonomy.cpp:99` — `ToString`'s **fallback return** | returns `"storage"` for an unhandled value |
| `ModuleDef.h:42` — `FacilityStats::kind`'s **default** | `= FacilityKind::Storage` |
| `BridgeView.cpp:20` — `kAllKinds` | and the missing `Engineering` in the same edit (§12.24 🐛) |

🐛 **New finding, verified 2026-08-10 — a facility's *kind* is optional in JSON.**
`BlueprintJson.cpp:37` reads `stats.OptionalEnum("kind", out.kind)`. A module authoring
`"facility": {}` — or misspelling the key, or naming a kind `FromString` rejects — silently takes
`FacilityStats::kind`'s in-struct default. **That default is `Storage` today, and under this change
would become `Trade`: an unkinded facility would silently become a shop.**

This is finding §13.3 W's class exactly — a capability reached by *omission* rather than by
authoring — and it is worse than W, because `kind` is the facility's identity rather than one of its
stats. **`kind` must become `Require`, not `Optional`:** a facility with no kind is not a facility,
and Law 10's whole point is that a content error surfaces as a load failure naming the file and key.
`ToString`'s fallback should return a value no content can be mistaken for, not a real kind.

*This is the third defect of this shape the audit has found (`traverseRadians = 0`,
`FacilityStats::level` never parsed, and now `kind` optional), all in `ParseFacilityStats`'s
immediate neighbourhood. **They should be fixed in one pass**, with the parser's optional/required
split reviewed field by field rather than one field at a time.*

#### The screen inventory

**Every docked screen is exactly one `FacilityKind`. Not every `FacilityKind` is a docked screen** —
§12.26's `Construction` (not in the enum today) is a command surface opened by `B` and issued at a
unit, and is never a tab, per step 5a.

| Kind | Screen | Sections | Built from |
|---|---|---|---|
| `Docking` | **Bay** (§12.30.2) | Vessels · your own hull's status | new — and not thin; see below |
| `Trade` | **Market** (§12.30.3) | Buy · Sell | `StationServicesMenu`, split |
| *(none — `CargoHold`)* | **Storage** (§12.30.3) | Deposit · Withdraw | new verbs on `StationServicesSystem` |
| `Repair` | **Repair** (§12.30.4) | Repair | `StationServicesMenu`, split |
| `Engineering` | **Engineering** (§12.30.5) | Merge · Deconstruct · Delete · **Rebuild** | `EngineerMenu` + `RefactorMenu` |
| `Manufacturing` | **Manufacturing** (§12.30.8) | Queue · Draft | new + `CustomizeMenu`, and the **missing producer** for a second severed chain |
| `Research` | **Research** (§12.30.6) | Queue | new — and it is the **missing producer** for a five-link chain |

##### Where each screen is specified

*The subsections were written in a different order than the screens are numbered — Research was taken
before Manufacturing, because Manufacturing is §12.19's primary consumer (§11.9: `features.md` §2.10's
attribute-propagation chain is computed there and nowhere else) and specifying a UI over an undefined
recipe model is what this pass exists to avoid. **This table is the index; the screen numbers are
stable.***

| Screen | Section | State |
|---|---|---|
| 1 — Bay | §12.30.2 | ✅ Specified |
| 2 — Market · Storage | §12.30.3 | ✅ Specified |
| 3 — Repair | §12.30.4 | ✅ Specified |
| 4 — Engineering | §12.30.5 | ✅ Specified |
| 5 — Manufacturing | §12.30.8 | ✅ Specified |
| 6 — Research | §12.30.6 | ✅ Specified |
| The two §3.10 overlays | §12.30.7 | ✅ Specified |

> ⚠️ **The *Sections* column for Market and Storage is refined by §12.30.3.** They are not four
> sections but **four verbs over one layout** — two holds side by side, where the verb is the
> direction of transfer and the only difference between a trade and a deposit is whether the transfer
> crosses an ownership boundary. The tab arrangement below is unchanged.

**Six tabs, seven screens** — Storage rides the same tab as Market when the station has both, since
they are two sections over one hold, and stands alone on a station with a `CargoHold` and no `Trade`
hardpoint. That asymmetry is the point of splitting them: a warehouse that does not deal, and a
broker that has run dry, are both expressible.

**This makes §12.24's dispatch table total, which it was not.** As step 5 wrote it, `FacilityKind` →
menu `Draw()` had no well-formed signature: `Engineering` mapped to *two* menus and
`StationServicesMenu` spanned *two* kinds. Splitting `StationServicesMenu` (UI only — the system
keeps all three requests and gains two) and merging Engineering's two files behind their one shared
gate is what makes the mapping a function.

**Leaving the router**, per the supersessions above:

| Was | Becomes |
|---|---|
| `StorageMenu` | The **inventory** overlay (§12.30.7; `features.md` §3.10) — your own `CargoHold`, everywhere |
| `ModulesMenu` | The **loadout** overlay (§12.30.7) — live refit, unrestricted (§2.7) |
| `CustomizeMenu` | The Manufacturing screen's **Draft** section |
| `BuildMenu` | Already excluded by step 5a — `B`, a Construction facility, issued at a unit (§12.26) |
| `NavigationMap` | Already excluded by step 5a — the far end of the zoom continuum (§8.1) |

#### A defect in step 5 itself: the tab list cannot address a hardpoint

`BridgeView::AvailableTabs` returns `std::vector<FacilityKind>` — it dedupes by kind and **discards
the entity**. §12.24 step 5's `PlayerLocation { entt::entity shell; }` needs the entity, because
§3.4's promise is that *selecting a tab is moving into that hardpoint* and that dying with it kills
the player. **As both are currently specified, the router cannot set `PlayerLocation` from the tab
list it draws.**

**Fix:** `AvailableTabs` returns ~~`{ FacilityKind kind; entt::entity hardpoint; }`~~
**`{ ScreenId screen; entt::entity hardpoint; }`**, still deduped — first living hardpoint of each
kind, in `FacilityKind` declaration order. The tab list stays short on a station with fifty
hardpoints, and selecting a tab still names one specific entity, which is what §3.4's death predicate
and the mandatory per-screen health readout both evaluate against.

> ⚠️ **The `kind`-keyed half of that fix is superseded by §12.30.3, later the same day.** Keying on
> `FacilityKind` cannot express the **Storage** tab, which this section's own inventory says *"stands
> alone on a station with a `CargoHold` and no `Trade` hardpoint"* — a tab with no kind and no
> hardpoint entity. The key becomes a `ScreenId`; `hardpoint` may be `entt::null`, and the readout
> for such a tab measures the **station's** aggregate integrity, because that is what kills the
> capability. See §12.30.3.

#### The Docking tab is the one whose identity matters

*Decided by the project owner 2026-08-10. The Bay screen itself is specified in §12.30.2; what belongs
here is the consequence it has for the **tab model** above.*

A docking bay holds many vessels, so the arrival screen is a **roster** — and `Docked` already carries
what a roster needs: `struct Docked { entt::entity station; entt::entity bay; }`. **The roster is
bay-scoped**, `view<Docked>` filtered on `docked.bay == thisBay`, with no new component. A vessel in
another bay of the same station is not listed: §3.4's "movement is instant" governs the *time* cost of
moving, not whether every hull on a station is reachable from one list, and a station-scoped roster
would make this tab a station-wide vessel manager, which is the shape the Bridge is for.

⚠️ **Bay-scoping collides with the tab dedupe, and the collision has to be resolved here.** If
`AvailableTabs` returns the *first living* hardpoint of each kind, a station with two docking bays
shows one Docking tab, and the vessels in the second bay are **unreachable**. Two corrections, both
using information the components already carry:

- **On arrival, the Docking tab resolves to `Docked.bay`, not to the first living bay.** The player is
  standing in the bay they actually docked at, and `Docked` has recorded which one since it was
  written. `AvailableTabs`'s first-living rule is the fallback for a player who is aboard without
  having flown in — not the normal path.
- **Sibling bays are a selector inside the screen**, drawn with the `TabStrip` the widget set already
  carries for Market/Storage and Merge/Refactor. One tab, *n* bays behind it.

**`Docking` is therefore the one kind where the deduped tab is not the whole story**, and it is worth
naming rather than discovering: every other `FacilityKind` is fungible across duplicates — any living
Repair bay repairs identically — while a docking bay's *identity* is the whole point, because it is
where a specific hull is parked.

> ⚠️ **The fungibility half is superseded by §12.30.5**, later the same day. **It is already false for
> three of the six kinds.** A bench's `FacilityRef::level` scales the merge (`features.md` §2.4), a
> repair bay's grade scales the rate and the price (§12.30.4), and a research or manufacturing
> facility's grade decides sample survival, duration and recovery (§2.4's tables). **A duplicate is
> fungible only when the facility has no grade-dependent output** — and once §12.19 folds `level` into
> `Grade`, none does. **The sibling selector generalises to every tab**; Docking is simply where it was
> noticed first, because a bay's identity is visible while a bench's is only visible in the result.
> The tab list is unchanged — one tab per kind, *n* hardpoints behind it.

#### The shared widget layer

**There is no widget layer.** `HudTheme.h` is eight colours and four draw calls — `ChamferedRectPoints`,
`DrawBracketPanel`, `DrawChamferedRect`, `DrawChamferedButton`. No list, no scroll, no focus, no
layout, no hit test. `sr_shared_ui` is an `INTERFACE` target holding that one header.

**The drift has already started, before a single menu is reachable.** Verified 2026-08-10:

| File | Row rendering |
|---|---|
| `InventoryGrid.cpp` | `kRowHeight = 20.0f`, one `DrawText` — the shared widget §12.10's promotion note called for |
| `StorageMenu.cpp` · `ModulesMenu.cpp` | call it |
| `StationServicesMenu.cpp` | **its own verbatim copy** of the same constant and loop |
| `RefactorMenu.cpp` | **a second verbatim copy** |
| `EngineerMenu.cpp` | hardcoded `+8` / `+28` / `+48` |

One shared widget, two clones, one ad-hoc — in six files that nothing calls. Nine screens hand-rolling
row positioning is not a risk to be managed later; it is the state of the tree today.

##### The one decision: immediate-mode, stateless, pure functions

> **A widget takes its state and this frame's input as data, draws, and returns what the player did.
> No retained tree, no callbacks, no focus manager, no widget-owned state.**

Three existing decisions force it, and none of them is new:

1. **§12.24 already settled where UI state lives** — a lazily-created singleton entity, on the
   `CommsLog` precedent. A widget that owned its own scroll offset or selected index would be a
   second, competing home for the same data, and multiplayer is where that bites: one client's
   selection must not appear in another's view.
2. **Law 9.** A widget returning *"row 3 was clicked"*, and the screen turning that into a
   `BuyItemRequest`, is Law 9's exact shape. A callback-based widget invites the callback to mutate —
   which is the violation `CustomizeMenu::ConsumeSaveTemplateRequests` already committed once.
3. **`HudTheme.h` already documents the testability seam by name**: `ChamferedRectPoints` is *"pure
   math… unit-testable without a live GL context,"* while the `Draw*` functions *"have no automated
   coverage: there is nothing headless CI can safely call."* The widget layer splits on the same
   seam — **layout and hit-testing are pure and tested; `Draw` is a thin, untested shell over them.**

**Input arrives as data, polled once by the caller:**

```
shared/ui/UiInput.h    POD: { Vector2 cursor; bool clicked; float scroll; }
```

No widget calls `IsMouseButtonPressed`. That is what keeps hit-testing headless-testable, what makes
`features.md` §3.6's *"all bindings are rebindable"* possible at all, and what keeps §2.3's promise
that adding ENet later is *"a change of intent source rather than a rewrite of every menu."*

##### Home

**`src/shared/ui/`**, beside `HudTheme.h` — cross-mode presentation, already permitted raylib and
`engine/`, already forbidden `modes/`. Widgets take `Rectangle`, `Row`, and `UiInput`, all POD, so
the rule holds without an exemption. **`sr_shared_ui` becomes `STATIC`** from `INTERFACE` in the same
commit, since hit-testing has a `.cpp`.

*This is the second consumer Law 11's tie-breaker asks for, and it arrives with more than two: the
six docked screens plus the two §3.10 overlays plus `CockpitHud`'s existing bar. `InventoryGrid`'s
own header already made this argument for two consumers; it was right and it stopped one file short.*

⚠️ §2.2's 600-line file cap applies. If `Widgets.cpp` approaches it, split per widget rather than
raising the cap — that is the cap doing its job at line 601 instead of line 12,000.

##### The frame — a docked screen is full screen, and the edge channel survives it

*Decided by the project owner 2026-08-10. Nothing anywhere specified this: §12.24 step 5, this
section, and `features.md` §3.4 are all silent, and the only "over the viewport" ruling in either
document is §3.10's, which governs **flight** overlays. It applies to all seven screens at once, so it
is settled here rather than seven times.*

> **A docked screen takes the whole window. The viewport is not visible behind it, and the flight HUD
> does not persist under it.**

The docked and flight surfaces are not two views of one situation. Flying, the world *is* the
interface and §3.10 fights for every pixel of it — *"the middle stays flyable."* Docked, there is
nothing to fly, the player is standing in a facility, and the screen has ~50 Elements across 7 grades
or a fifty-hardpoint rig to lay out. **Reserving a viewport nobody is steering, to shrink the one list
the player came here to read, spends the space in the wrong place.**

⚠️ **But full screen collides with a settled rule, and the collision has to be resolved here rather
than discovered.** §3.4 does not pause while docked, §12.29 makes the system menu the only pause, and
§3.4's own promise is that **you die with your facility.** A screen that hides everything makes that
death a gotcha — which is the exact failure the mandatory per-screen `Gauge` was added to prevent, and
the Gauge shows *one hardpoint*, not an inbound raid.

**§3.10 already built the answer, for a different reason.** Its edge channel — hazard tint, sensor
contacts, directional damage indicators — was placed at the screen edge precisely because it
*"costs no layout."* A surface that costs no layout costs none here either:

| Survives a docked screen | Why |
|---|---|
| **Directional damage indicators** | The station taking fire is the one fact that changes what the player should do next |
| **Sensor contacts at the edge** | Module-gated exactly as in flight (§3.10) — a hull with no sensor sees nothing, docked or not |
| **Hazard tint** | The host is in the same corona you are |
| **The per-screen `Gauge`** | §3.4's mandatory readout, unchanged |
| ❌ The viewport · the bottom band · the centre projection | Replaced by the screen |

**So the docked frame is the flight frame with the middle swapped out**, not a different application.
One consequence worth naming: **the edge channel is drawn by the router, once, around whichever screen
is open** — never by the screens, which would be seven copies of it. That is the same argument that
put row rendering in `ListView`.

##### The set, and what is deliberately absent

Each earns its place by consumers that exist in the same batch (§2.4, Law 11):

| Widget | Consumers |
|---|---|
| **`PanelFrame`** — draws the bezel, returns the inner content `Rectangle` | All eight. Today every screen calls `DrawBracketPanel` and then invents its own padding |
| **`ListView`** — scroll + select over `std::span<const Row>`, plus the **empty-state string** below | All eight. **Supersedes `InventoryGrid` and both of its clones** |
| **`Button`** — the hit-test half `DrawChamferedButton` lacks | All eight — Buy, Sell, Repair, Merge, Delete, Queue, Switch, Undock |
| **`TabStrip`** | The router itself; Market/Storage; Engineering's Merge/Refactor; Manufacturing's Queue/Draft |
| **`Gauge`** — a labelled bar | §3.4's mandatory per-screen facility-health readout; job progress on Research and Manufacturing; `CockpitHud`'s existing hull bar |

> ✅ **The set held for four screens and needed one field on the fifth.** §12.30.2–.5 added nothing;
> §12.30.6's queue needs **per-row** progress, which the header `Gauge` cannot give — a queue of three
> jobs showing one bar is not a queue. **`Row` gains `float fill = -1.0f`** (negative meaning none) and
> `ListView` draws a fill behind the row when set. Two consumers exist in this batch — the Research
> queue and Manufacturing's — so Law 11's tie-breaker is satisfied rather than anticipated. *Recorded
> because it is evidence the set was scoped right, and because the remaining screens should be checked
> against it rather than assumed to fit.*

**Deliberately not built: a slider.** `RepairRequest::fraction` is the only continuous input in the
entire set. One consumer is a dead abstraction (§2.4), so **repair ships as discrete buttons** and a
slider lands with its second consumer. Recorded here so the omission reads as a decision rather than
as an oversight.

**`InventoryGrid` is deleted in the same commit**, not left beside `ListView`. Two implementations of
one widget is the exact condition this section exists to end.

##### `Row` is the type that fixes `StorageMenu`

`storage_menu::Rows(const CargoHold&)` returns `std::vector<std::string>` — presentation baked into a
pure function. It cannot be sorted, filtered, or grouped, and it will not survive `features.md`
§2.10's roster: **~50 elements plus 8 Material families across 7 grades.**

```
shared/ui/Row.h    POD: { std::string label; std::string value; char glyph[3]; RowStyle style;
                          float fill = -1.0f; }   // negative = no progress bar; §12.30.6
```

The `glyph` field is where `features.md` §3.9's settled **monogram placeholders** live — *"the same
stand-in the build and buy menus already use"* — so the docked screens inherit §3.9's
colour-is-condition / glyph-is-identity language rather than inventing a second one. `RowStyle`
carries §3.9's integrity gradient and the disabled state §3.10's *degrade-never-remove* rule requires.

**Grouping stays in the screen, not the widget.** A screen emits header rows; `ListView` renders what
it is given. That keeps the widget dumb enough to serve a job queue, a bay roster, and a fifty-element
manifest without branching on which it is.

##### Empty is a row, never a blank panel

*Settled 2026-08-10. Every screen in this batch has an empty case — a station hold with nothing in it,
a rig with no damage, an empty bay, a queue with no jobs — and none of them said what it draws.*

`features.md` §8.3's rule governs it directly: **absence must never look like emptiness.** A bare
panel is indistinguishable from a panel that failed to load, from a filter that matched nothing, and
from a screen that is still fetching — and the player cannot tell which without leaving and coming
back.

> **A `ListView` given no rows renders one row that says why there are none**, in `RowStyle`'s
> disabled style: `NO ITEMS IN HOLD` · `NO DAMAGE TO REPAIR` · `NO VESSELS IN THIS BAY` ·
> `NO JOBS QUEUED`.

The string is the screen's, not the widget's — `ListView` takes it alongside the span, the same way
grouping stays in the screen. **One field on the call, no branch in the widget**, and the empty case
stops being the one state nobody specified and nobody tested. *It is also the cheapest possible fix
for the class of bug where a screen looks broken because a component was never emplaced — which is
§13.3 O and P's failure mode exactly, and both would have been visible in play on day one had this
rule existed.*

#### Scheduling — §13.5's group 4 splits

§13.5 places the whole of group 4 *"after group 1."* **The widget layer is pure presentation over POD
and has no dependency on group 1 at all** — no player, no world, no station. Like §12.28, it can land
first and be verified by tests alone.

| | Scope | When |
|---|---|---|
| **4a** | The widget layer · `UiInput` · `Row` · `InventoryGrid`'s deletion · the four hand-rolled row loops folded in | **Startable today** |
| **4b** | The router · `PlayerLocation` · `AvailableTabs`'s signature · the seven screens · request placement · the facility content set | After group 1 |

Landing 4a first is also what prevents the failure this section documents: seven screens written in
parallel against no shared widget is how `InventoryGrid` came to have two verbatim copies before any
of its consumers could be run.

#### Tests — the widget layer and the router

The pure half is testable headless today, with no window and no world:

- `ListView` hit-testing maps a cursor position to a row index, and to none when the cursor is outside
  the content rect or past the last row.
- Scroll offset clamps at both ends and does not move when every row fits.
- `PanelFrame`'s content rect is inset from its bounds and never inverted for a small panel.
- `AvailableTabs` returns **all six** `FacilityKind`s when all six are present and living — §12.24's
  existing regression test for the `kAllKinds` defect, now also asserting the **entity** each tab
  names is the first living hardpoint of that kind.
- A facility module authoring no `kind` **fails to load**, naming the file and key (Law 10).
- Per-screen assertions are specified with each screen, but they share one shape, and it is the one
  §12.24 names as *"the assertion the nine menus have never had"*: **the request a screen builds lands
  on the docked requester and is consumed by its system on the following tick.**

##### A tab needs a working screen behind it, not just a living hardpoint

*Raised by the project owner 2026-08-23, during a UI design pass over the router; decided the same
day.* Line 4978's own test asserts `AvailableTabs` returns all six `FacilityKind`s whenever all six
are **living** — with no consideration of whether the screen behind that `ScreenId` has shipped.
This is not hypothetical: **§12.30 already schedules Market after Storage** (`core/economy/Pricing.h`
is not group 4b), so a station carrying a living `Trade` hardpoint produces a Market tab with nothing
behind it for the entire span between the router landing and P6-08. The same gap reopens for
Manufacturing's Queue half between P4-07 (Draft only) and P6-03/P6-06.

**Fix:** `AvailableTabs` (or its caller in `BridgeView::Draw`) gates each candidate tab on a second,
purely-static predicate — *is there a built screen for this `ScreenId` yet* — alongside the existing
*is the hardpoint living* check. Concretely, a small compile-time table (`ScreenId` → `bool shipped`)
that starts `{Bay, Storage, Repair, Engineering, Research}` true and `{Market, Manufacturing}` false,
and flips one entry the day its screen's `Draw` is wired into `SpaceFlight.cpp`. **A tab with no
screen simply does not appear** — no greyed-out placeholder, no "coming soon" state, nothing to click
that does nothing. This was considered and rejected in favor of the simpler rule: a tab the player can
select is a tab that does something, full stop; a station's *capabilities* (what hardpoints it has)
and the *game's* readiness (what screens exist yet) are two different questions, and only the first
one belongs to `AvailableTabs`'s existing per-hardpoint logic.

**Tests:** a station with a living `Trade` hardpoint shows no Market tab until the shipped-table entry
flips; flipping it is the only change P6-08 needs to make the tab reachable — no `AvailableTabs`
edit at that point, since the hardpoint check was already correct.

---

#### 12.30.1 `PlayerLocation` is the source of truth; `PlayerControlled` is derived

*Settled 2026-08-10. This amends §12.24 step 5's `PlayerLocation` bullet and step 1's planned
`emplace<PlayerControlled>`. It is cheap now and expensive later — see the count below.*

> ⚠️ **Amended by §12.30.3 later the same day, on one point this section did not reach.** Deriving
> `PlayerControlled` from `PlayerLocation` is right, and everything below stands. But the player's
> **`FactionId` currently lives on the rig root they control**, so the derivation also moves the
> player's *identity* into whatever hull they are standing in — and at a foreign station that hull
> belongs to someone else. `DiscoverySystem.cpp:13` and `ConstructionSystem.cpp:15` both read it that
> way today. **Identity is not location**, and the fix is that the player's `FactionId` moves onto the
> player record rather than being read off an occupied hull. It is not reachable until the docking
> gate widens (§12.30.3), and lands with it.

§12.24 introduces `PlayerLocation { entt::entity shell; }` alongside an existing `PlayerControlled`
tag on a rig **root**. Two components naming "where the player is" is finding §13.3 C's shape
(`MountedModules` vs. `EquippedModule`) waiting to happen, and it would break in the worst place:
§3.4's death predicate evaluates `PlayerLocation`, while the camera, the HUD and every input path
follow `PlayerControlled`. Let them disagree once and the player dies while the screen shows a
healthy hull.

> **`PlayerLocation.shell` is the only thing ever written. `PlayerControlled` is the rig root that
> shell belongs to, derived — never emplaced by gameplay code.**

**The migration is one line.** Verified 2026-08-10: `PlayerControlled` appears at **30 sites across
20 files, and exactly one of them writes it** — `SpaceFlight.cpp:120`, in `WarpToSystem`. Every other
site is a `view` or an `exclude`. So the change is: delete that write, add the derivation, and leave
twenty-nine readers untouched. §12.24 step 1's planned `emplace<PlayerControlled>` becomes an
`emplace<PlayerLocation>` naming the spawned rig's cockpit hardpoint.

*`Identity.h` already documents the intent this formalises:* `PlayerControlled` is *"exactly one per
registry, and it **moves** rather than duplicating when the player transitions to a capital's
Bridge."* It has always been derived data; nothing has ever derived it.

##### Three consequences, all verified against the code

**1 — While docked in a facility, the player controls the *station*.** `Docked` sits on the vessel's
root; `PlayerLocation` sits on a station hardpoint; the derived `PlayerControlled` is therefore the
station. This is correct per §3.4 — *"docking places the player in the docking bay, alongside the
vessel they arrived in"* — and the station is unflyable for the reason §12.25 made emergent: it has
no `Propulsion`. **No gate is needed; capability is already emergent.**

**2 — `CockpitHud` silently changes what it measures, and must say so.**
`cockpit_hud::Draw` views `PlayerControlled` and renders `AggregateHullFraction` of that root. Docked,
that is the **station's** aggregate rather than your vessel's. That is the right number — §3.4 says
you die with your facility, so the station's integrity is exactly what you need to watch — but an
unlabelled bar that changes subject is `features.md` §8.3's *"absence must never look like
emptiness"* in miniature. **The bar carries the name of what it measures**, and the Bay screen shows
your own vessel's integrity separately.

**3 — `LootSystem` sweeps loot into whatever hull you are standing in, and that sharpens an existing
defect rather than creating a new one.** `FindCollectorInRange` views
`PlayerControlled, WorldTransform, CollisionRadius`. Docked, the collector becomes the station — with
a station's much larger `CollisionRadius` — so drifting loot lands in the station's `CargoHold` and
credits in a `Wallet` that `get_or_emplace` creates on it. At a station you own that is defensible.
**At a foreign station you are giving them your salvage.**

The fix is not a special case here. §13.1 already scores `LootSystem` as ⚠️ *"NPCs cannot loot"* —
`PlayerControlled` is being used as a proxy for *"a collector"*, and it is the wrong predicate.
Widening the view to any rig carrying a `CargoHold` dissolves this interaction and fixes the recorded
defect in the same edit: a station collects because it is a collector with a hold, not because the
player happens to be standing in it.

#### 12.30.2 Screen 1 — the Bay

**Gate:** a living `FacilityKind::Docking` hardpoint. **Tab resolves to** `Docked.bay` on arrival,
with a sibling-bay selector behind it. **This is the default `PlayerLocation`** — the screen the
player lands on, and the only one that cannot be absent from a station they can dock at.

##### Layout

| Section | Contents |
|---|---|
| **Header** | Bay name · **occupancy `3 / 4`** · this bay hardpoint's integrity `Gauge` (§3.4's mandatory readout) |
| **Sibling selector** | `TabStrip`, one entry per living Docking hardpoint on the host. Absent when there is one bay |
| **Roster** | `ListView` over `view<Docked>` filtered on `docked.bay == thisBay` |
| **Footer** | The verb buttons for the selected row |

Each roster `Row` carries the vessel's name in `label`, its integrity in `RowStyle` (§3.9's gradient,
so a damaged hull reads as damaged in the list itself), and its `BlueprintId` monogram in `glyph`.
**Your own vessel is a row like any other**, marked as yours rather than pulled out into a special
panel — it is the row you press *Launch* on.

##### The verbs

| Row is | Verb | Mechanism | Blocked on |
|---|---|---|---|
| A hull you own | **Board** | `PlayerLocation.shell` ← that hull's cockpit hardpoint. **One write.** The derived `PlayerControlled` follows | — |
| The hull you occupy | **Launch** | `UndockRequest` on it — `AvionicsMenu`'s existing path, unchanged | — |
| A hull you do not own | **Ask to purchase** | A buy against a *vessel*: `BuyItemRequest` carries a `ModuleId`, so this needs §12.19's `ItemId`/`ItemKind` | §12.19 |
| A hull you do not own | **Ask to escort** | A UI-emitted intent consumed by `PartySystem` | §12.27 |

**Boarding and switching ships are the same operation as taking a capital's bridge** (§12.24 step 5).
Nothing is added for it; the Bay screen is simply the surface where the operation is offered against
a *hull* rather than against a facility tab.

⚠️ **`R` while docked means "board and launch," and that is two writes behind one key.** §3.6 binds
dock/undock to `R`. Standing in a facility, the player's derived `PlayerControlled` is the station,
which is not `Docked` — so a naive `UndockRequest` on it does nothing. The binding resolves as:
`PlayerLocation` moves to the selected hull's cockpit, *then* `UndockRequest` lands on that hull.
**`AvionicsMenu` keeps the key and gains the first half**; it does not move into this screen, because
it is the one gameplay input in the repository that already works end to end and §13.5's deferral
warning applies to breaking working paths as much as to inventing producers.

##### Capacity is enforced in the search, not at the handoff

*This is the non-obvious half, and it removes a stranding bug before it exists.*

**`FacilityStats::capacity` gets its first reader here.** §13.3 K records it as parsed and read by
nobody — *"docking bays have unlimited capacity"* — while `docking_bay_i` authors `capacity: 4`.
**A dead field acquires a consumer without a new type being invented**, which is the outcome §2.4
exists to produce.

`DockingSystem::UpdatePromptsAndRequests` recomputes `FindEligibleBay` **every tick, for every rig**,
and `DockPrompt`'s own contract is that it disappears the moment the bay stops qualifying — *"a rig
that drifts away must stop prompting the same tick it leaves, not linger."*

> **A full bay is not an eligible bay.** `FindEligibleBay` skips any `DockingBay` whose occupancy has
> reached its `FacilityStats::capacity`, exactly as it already skips one out of range or of the wrong
> faction.

Refusing at the handoff instead would strand an inbound NPC whose target bay filled while it was
flying: it holds a `DockRequest` against a bay that will never accept it. Filtering in the search
means the same NPC simply stops seeing that bay and `FindEligibleBay` returns the next nearest —
**re-routing rather than refusing, with no retry logic and no special case**, because the search is
already per-tick. The prompt vanishing *is* the player-facing feedback, and it needs no new UI.

⚠️ **Occupancy must be counted once per tick, not once per candidate.** Counting inside
`FindEligibleBay` is O(rigs × bays × docked). Build a bay→count map at the top of
`UpdatePromptsAndRequests` and pass it in. Bays are few and §1.1 caps the registry at 20 000, so this
is not urgent — it is written down because the cheap version is the one that gets typed at 2am.

**`capacity: 0` keeps meaning unlimited**, matching `CargoHold::capacity`'s existing convention so
one number does not mean two things in two components. `docking_bay_i` authors `4`.

##### Types

- **`PlayerLocation { entt::entity shell; }`** — §12.24 step 5's component, now the sole source of
  truth per §12.30.1.
- **Nothing else.** The roster is `Docked`, the occupancy is `FacilityStats::capacity`, the sibling
  list is `AvailableTabs`. **No new component for the Bay screen**, which is the test §2.4 sets.

##### Systems

`DockingSystem` gains the capacity filter and nothing else. The purchase and escort verbs emit into
systems that already exist (`StationServicesSystem` once §12.19 widens the request; `PartySystem`
once §12.27 supplies membership) — **this screen owns no system of its own.**

##### Tests

- A bay at `capacity` produces no `DockPrompt` for a rig in range; the same rig prompts again the
  tick a docked vessel launches.
- `capacity: 0` never blocks.
- An inbound rig whose nearest bay fills mid-flight prompts for the **next** eligible bay rather than
  holding a dead `DockRequest`.
- The roster lists exactly the rigs whose `Docked.bay` is this bay — not the station's other bays.
- Boarding writes `PlayerLocation` and nothing else; the derived `PlayerControlled` moves with it, and
  exactly one rig root satisfies it afterwards.
- Destroying the bay hardpoint the player occupies kills the player while the station survives —
  §12.24's existing §3.4 assertion, now with a screen that was showing its integrity beforehand.
- `R` while standing in a facility launches the selected owned hull; `R` with no owned hull in the
  bay does nothing rather than emitting a dead `UndockRequest`.

##### Escort requests are refusable, and the roll must be sticky

*Settled 2026-08-10.* **Acceptance is a chance, weighted by faction relation.** Not a gate — a hostile
pilot should be *unlikely* to join rather than mechanically forbidden, which is what makes the ask
worth making.

| Input | Source |
|---|---|
| Faction relation between asker and asked | `core::diplomacy::DiplomacyMatrix` — **zero writers today**, `ctx.diplomacy` is `nullptr` (§12.24 step 6) |
| The asker's standing | `ctx.reputation` — the same step-6 pointer |

**The roll happens in `PartySystem`, never in the menu** (Law 9). The Bay screen emits the intent;
the system consults the two pointers, rolls, and either emplaces `PartyMember` or records a refusal.

⚠️ **A refusal must be remembered, or the chance means nothing.** A re-rollable answer is a button
the player mashes until it succeeds, which is the same objection that ruled out silently accepting
every request. **The answer is sticky per (asker, asked)** until the input that produced it changes —
i.e. until the relation moves. This is the half that gets forgotten, so it is written down before the
verb is built rather than found in play.

**This is what `DiplomacyMatrix` has been missing.** §13.2 records it as having zero writers and one
reader — *"the exact failure `features.md` §5.3 was written to prevent."* An escort roll is a real
gameplay consumer of a relation value, which is more than the matrix has today. It does not fix the
writer half; it does mean step 6 unblocks something a player can see.

⚠️ **The escort verb is a legitimate producer for `PartySystem`, and this needs saying plainly.**
§13.5 defers `PartySystem` because `PartyLeader`/`PartyMember` have zero producers, and warns:
*"do not 'fix' them by inventing a producer in the system itself."* A **UI-emitted intent consumed by
the system** is the shape that warning points *toward*, not away from — the same shape
`AvionicsMenu` → `DockRequest` → `DockingSystem` already has, and the only shape that satisfies Law 9.

**Depends on:** §12.24 step 6 (both pointers) and §12.27 (`PartySystem` membership). **Ships with
§12.27, not before** — a refusal model that consults `nullptr` would refuse everything unconditionally,
which is precisely `TemplateMarketSystem`'s current failure (§13.1).

##### A parked hull stays where you parked it — and something has to remember it exists

*Settled 2026-08-10.* Two ways to fly a hull you left behind, and no third:

1. **Go and get it.** Physically return to that docking bay and Board it.
2. **Crew it.** Assign a `Crew` module and it becomes an asset an operator or commander flies for you.

**This is not new design — it is the consumer `features.md` §2.7 predicted and never had.** §2.7's
open block states the player's crew modules matter *"only for work they delegate: commanders running
fleets elsewhere, and **NPC pilots flying vessels the player owns but is not sitting in**."* That
sentence had no mechanism behind it. This is the mechanism.

It also keeps the fleet honest: there is **no remote recall, no fleet teleport**, so parking a hull is
a real decision with a real cost, which is what §3.4's "protection by circumstance, never by rule"
asks of every other choice in the game.

**Blocked on `ModuleKind::Crew`, which does not exist** — §13.3 Z records **zero occurrences of
"crew" anywhere in `src/`**, and four settled designs already wait behind it. The *Board* half ships
without it; the *crew it* half does not.

⚠️ **And parking a hull deletes it today.** Verified 2026-08-10: `SpaceFlight::WarpToSystem` does
`world_ = SystemWorld(targetSystemId)` — *"nothing survives this line except what was captured
above"* — then re-generates the destination with `PopulateSystem`. The only things carried across are
the player's `CargoHold`, `Wallet`, and demoted `DeathWreck`s via `wreckLedger_`. **A hull left in a
bay is destroyed by the move-assignment and never comes back.**

The fix is a fourth instance of a pattern this codebase already runs three times:

| Record | Demotes | Promotes |
|---|---|---|
| `core/galaxy/WreckRecord` + `WreckLedger` | ✅ built — `CollapseDeathWreck` | ✅ `PromoteDeathWreck`, keyed by system id |
| `core/galaxy/ResearchRecord` | ✅ built (§12.1) | ✅ |
| `core/galaxy/ManufacturingRecord` | 📋 §12.18 | 📋 |
| **A parked-hull record** | 📋 **this section** | 📋 |

Same shape, same directory, same reason: **a thing must not silently vanish because the player warped
away.** `WreckLedger::IdsForSystem` is the exact query a parked-hull ledger needs, so the promotion
half is a copy of a path that works.

##### The record needs a capability the codebase does not have

🐛 **A hull left behind is destroyed by the world teardown, and so is the player's own damage.**
`WarpToSystem` re-spawns the player from their `BlueprintRef`, so every jump is a free full repair and
a full loadout rollback. Recorded as finding §13.3 AC and **specified in §12.31** — which also settles
that the state form must *not* be a `ShipBlueprint`.

**Four features wait on that one capability**, so it is scoped there rather than special-cased here:
parked hulls, warp damage persistence, cross-system refit (`features.md` §2.7), and §13.3 Y's world
save. **None of it blocks this screen** — Board and Launch against hulls in the *current* system need
none of it.

#### 12.30.3 Screen 2 — Market and Storage

*Settled 2026-08-10. Every claim below was verified against `src/` by grepping for readers and
callers. **It revises two same-day decisions** — the tab key in §12.30, and one consequence of
§12.30.1 — and both revisions are marked where they occur. It also settles the deposit/withdraw
ownership question §12.30 left open.*

**Gate:** **Market** — a living `FacilityKind::Trade` hardpoint. **Storage** — the host carrying a
`CargoHold`, and no hardpoint at all. One tab, two verb pairs, per §12.30's split: *one hold, two
questions.*

##### 🐛 The premise of this screen is unreachable: docking is same-faction-only

*Verified 2026-08-10, and it is the finding that governs everything below.*

`DockingSystem::FindEligibleBay` (`DockingSystem.cpp:38`) rejects any bay whose root's `FactionRef`
does not equal the seeking rig's:

```
const auto* stationFaction = registry.try_get<FactionRef>(parent.root);
if (stationFaction == nullptr || !(stationFaction->id == faction)) { continue; }
```

The function's own comment says so plainly — *"belonging to a different, same-faction rig."* **You
can dock only at stations of your own faction.** Two settled sections say otherwise:

- §12.10 defines this whole surface as *"docking anywhere, including a station they do not own —
  ordinary commerce, not fleet command."*
- `features.md` §5.3 settles docking as a **relation-band** effect: Allied *"shared docking"*,
  Friendly *"docking permitted"*, **Distrustful *"docking refused"***. A band table with six rows
  cannot be implemented by an equality test with two outcomes.

And `features.md` §5.10 settles that **the player starts as an independent rogue operator** — *"not
a faction, not a member of one."* An equality gate against a rogue operator's own id admits exactly
the stations that operator built.

> **The faction-equality test becomes a relation-band test**, refusing at Distrustful and below.
> That is a **§12.24 step 6 consumer** — it reads `ctx.diplomacy`, which is `nullptr` today — and it
> is the same widening `TargetingSystem` needs for finding §13.3 N. **Both should land in one pass**,
> because they are the same predicate asked in opposite directions: *may I dock* and *may I shoot*.

⚠️ **Until it lands, Market is reachable only at stations you already own**, which is the least
interesting of the four ownership cases below, and **Storage is reachable everywhere you can dock.**
That is the reverse of the useful order, and it is why this screen's two halves are scheduled apart.

*This also dates two forward-looking notes elsewhere in this document. §12.30.1's third consequence
— "at a foreign station you are giving them your salvage" — and the ownership inversion recorded
below both describe states that **cannot be reached today**. They are correct about what happens once
the gate widens; neither is a live defect until it does.*

##### The ownership answer: whose hold is it?

*This is the question §12.30 marked and did not answer: depositing into a station's hold, when that
same hold is what others buy from, is not obviously right.*

`features.md` §5.0 has already settled the half that decides it — **"stock is held per *station*, not
per faction and not per system"**, and a station's stock dies with the station. So the station's
`CargoHold` is not a warehouse with tenants. It is **one owner's inventory**, and that owner is the
`FactionRef` on the station's rig root, which `ConstructionSystem` stamps from the builder
(`ConstructionSystem.cpp:33`).

> **A transfer within one owner is free, and is called deposit. A transfer across an ownership
> boundary costs credits, and is called trade. The station's `CargoHold` has exactly one owner; which
> pair of verbs you are offered is decided by whether that owner is you.**

| Station's `FactionRef` | Living `Trade` hardpoint | Verbs offered |
|---|---|---|
| Yours | Yes | **All four.** You stock your own shop, and it sells |
| Yours | No | **Deposit · Withdraw.** A warehouse that does not deal |
| Not yours | Yes | **Buy · Sell.** A broker |
| Not yours | No | **None** — the tab does not appear |

**Depositing into your own station's hold *is* stocking your own shop, and that is the feature, not a
leak.** It is the first concrete economic action on `features.md` §5.10's player-as-faction path, and
it is what makes a player-built trading post a thing that can run dry.

⚠️ **A per-visitor rented locker was considered and rejected.** Three reasons, in weight order:

1. **It needs a hold keyed by owner on the station** — a second granularity for a concept §5.0
   settled as per-station, which is the `MountedModules`/`EquippedModule` shape (§13.3 C) arriving
   for the fourth time.
2. **§5.0's "destroy a station and its stockpile goes with it" would have to except it**, or the
   player's stored goods die with a station they do not control. The first is a special case in the
   one rule that gives blockades teeth; the second is a feature nobody asked for.
3. **Multiplayer multiplies it per client**, and Law 9 would then have every client's locker in one
   component on a shared entity.

Recorded so the omission reads as a decision.

##### Layout

**One screen, two holds, and the verb is the direction of transfer.** Buy is station → you with
credits; Sell is you → station with credits; Deposit is you → station for nothing; Withdraw is the
inverse. Four verbs, one mechanic — which is why they are one screen rather than the two sections
§12.30's inventory table names.

| Section | Contents |
|---|---|
| **Header** | Station name · **owner** · the `Trade` hardpoint's integrity `Gauge` (§3.4's mandatory readout) · your credits |
| **Left `ListView`** | Your vessel's hold |
| **Right `ListView`** | The station's hold |
| **Footer** | The verb buttons for the selected row, enabled per the table above |

Each `Row` carries: **`glyph`** — the periodic abbreviation for an Element (`Fe`, `Ir`, `Xe`), the
monogram placeholder for anything else. `features.md` §2.10 chose real elements partly for *"free
icons — the periodic abbreviation"*, and this is the surface that spends them, so §3.9's
**glyph-is-identity** rule arrives with real glyphs rather than placeholders on at least one axis.
**`label`** — display name. **`value`** — quantity, and unit price when the Trade gate is live.
**`RowStyle`** — disabled, with the reason, when the row's verb is unavailable: `NO TRADE FACILITY`,
`HOLD FULL`, `CANNOT AFFORD`. That is `features.md` §3.10's *degrade-never-remove* rule applied per
row rather than per button.

**Grouping stays in the screen** (§12.30). The Market emits header rows by `ItemKind`, then by
Material family — because `features.md` §2.10's roster is **~50 Elements plus 8 Material families
across 7 grades**, and an ungrouped flat list of that is the exact failure `storage_menu::Rows`
returning `std::vector<std::string>` was flagged for.

##### The verbs

| Row is | Verb | Mechanism | Blocked on |
|---|---|---|---|
| In the station's hold, `Trade` living, not yours | **Buy** | `BuyItemRequest` on **the vessel root** | §12.19 · `Pricing.h` |
| In your hold, `Trade` living, not yours | **Sell** | `SellItemRequest` on the vessel root | §12.19 · `Pricing.h` |
| In your hold, station is yours | **Deposit** | `TransferItemRequest{ toStation = true }` | §12.19 |
| In the station's hold, station is yours | **Withdraw** | `TransferItemRequest{ toStation = false }` | §12.19 |

⚠️ **The request lands on the entity carrying `Docked`, which is *not* the entity the player
controls.** `StationServicesSystem::DockedStation` reads `Docked` off the requester and bails on
`entt::null`. Under §12.30.1 the player's derived `PlayerControlled` while docked is **the station**,
and a station carries no `Docked` — so **a screen that places its request on `PlayerControlled` trades
nothing, silently.** There is no log, no refusal, and no failing test today. The requester is the
**vessel root the player arrived in**, and §12.30.2's Bay roster is where that hull is identified.
This trap is worth one assertion in every screen that places a request (§12.30's shared test shape).

##### Price is computed by the system. The request must not carry one.

*This revises `StationServices.h`'s current shape, and the reason is Law 9 rather than convenience.*

`BuyItemRequest { ModuleId module; int cost; }` — **`cost` is supplied by whoever builds the
request.** `station_services_menu::BuildBuyRequest(module, cost)` takes it as a parameter and the
system spends exactly what it is handed (`StationServicesSystem.cpp:45`). The header is candid about
why: *"supplied already-resolved rather than looked up from a price registry."*

> **A menu that names its own price is a menu that can name zero.** Under Law 9 the UI states an
> intent — *buy three of this* — and the system decides what that costs. A request carrying a price
> is a client-authoritative wallet write wearing an intent's clothes.

**So `cost` and `value` are deleted rather than filled in**, and the request carries `item` and
`quantity` only. `StationServicesSystem` calls the same pure pricing function the screen calls to
*display* — one function, two callers, no trust:

```
core/economy/Pricing.h        free functions beside FactionEconomy, sr_core, no raylib

int BaseValue(ItemId, const ContentLibrary&);        // features.md 2.10: derived from the recipe,
                                                     // one credit per unit of any Element
int LocalPrice(ItemId, int quantity, const CargoHold& stock, const ContentLibrary&);
```

> ⚠️ **Both signatures are superseded by §12.19** (later the same day), and the reason is that an
> `ItemId` names a *design*: the recipe's size is a function of **grade**, and base value gains a
> **quality** term without which the settled no-spread rule below becomes a free reroll machine.
> They become `BaseValue(const ItemInstance&, …)` and `LocalPrice(const ItemInstance&, int quantity,
> …)`. **Everything this subsection argues is unchanged** — the drawdown walk, the quantity rule, the
> absent spread — with one clause added: the drawdown is walked over the **price key** `(ItemId,
> Grade)`, which is deliberately coarser than the stack key. See §12.19.

Three properties this has to hold, all of them already settled elsewhere:

- **Nothing is stored or ticked** (`features.md` §5.0 rule 4). `LocalPrice` is a pure function of one
  ledger, so a galaxy of any size costs nothing to price.
- **Scarcity is read off the station's own hold**, which is the whole reason §12.30 made the
  `CargoHold` the storage capability. Buying draws the hold down and the price up; selling does the
  inverse. **A shop with infinite stock has no local price.**
- **`LocalPrice` prices a *quantity*, never a unit — and that is not a convenience signature.**
  If the price is a function of stock, and the screen offers `1` / `10` / `All`, then computing a spot
  price and multiplying makes **one buy-of-ten strictly cheaper than ten single buys**: every unit is
  charged at the *pre-purchase* price instead of walking the curve down. Selling is biased the same
  direction — a naive sell-of-ten pays the pre-sale spot for all ten, while ten single sells each
  depress the next. **Bulk wins in both directions**, so the exploit is not *buy in bulk*, it is
  *never transact in more than one click.*

> **The price of a transaction depends on what moved, never on how many clicks it took.** `LocalPrice`
> walks the drawdown and returns the total, so bulk and repeated singles are identical by
> construction — there is nothing to discover and no anti-exploit rule to write.

Three things follow, and all three are wanted:

- **It self-scales.** Against a hold of 10,000, buying 10 barely moves the curve and the two paths
  agree to rounding. The divergence bites only when a trade is a real fraction of local stock — which
  is exactly when a small station's market **should** feel small. The texture arrives with no number.
- **It makes the no-spread rule below exact rather than approximate.** Buying ten and selling them
  back walks the same path in reverse and nets **precisely** zero. Under spot × quantity it nets a
  small profit, which is a money printer with no authored cause.
- ⚠️ **Accumulate in float and round once, at the end.** Rounding per unit reintroduces the divergence
  at a few credits per transaction, which is small enough to survive review and large enough to be
  found in play.

*This is also why `AffordableModules(stock, credits, pricePerModule)` is wrong in its **signature**
rather than merely in its body: a flat per-unit price is the naive version written into a type.*
- **There is no authored buy/sell spread.** One price per item per station, and it moves as the hold
  moves. A spread would be a second tuning number doing what the scarcity curve already does, and
  §2.10's whole pricing argument is that **prices are derived outputs**. Buying and selling the same
  unit at the same station nets zero, which is correct: profit comes from carrying goods **between**
  stations, not from a margin someone typed.

⚠️ **`features.md` §5.3's reputation modifier — Allied *"trade discounts"*, Neutral *"standard
prices"* — is a third input, and it reads `ctx.diplomacy`.** It ships as identity until §12.24 step 6,
the same dependency as the docking gate above. **State the omission in the header**, or a Market that
never discounts reads as a tuning failure rather than a missing pointer.

⚠️ `station_services_menu::AffordableModules(stock, credits, pricePerModule)` **is deleted with
them.** Its own comment concedes what it is: *"price is flat per module, so every item in stock is
equally affordable or none are; there is no per-item cutoff."* It is a pure, unit-tested function
whose entire content is the absence of pricing, and once `LocalPrice` exists the screen disables an
unaffordable row directly.

##### Capacity is enforced on every write, and this is the fourth writer that ignores it

*Verified 2026-08-10.* `CargoHoldHasRoomFor` has **exactly one caller in the tree** —
`RefactorSystem.cpp:78`. The other four writers of a `CargoHold` (`StationServicesSystem`,
`LootSystem`, `EngineerSystem`, `ModuleEquipSystem`) push unconditionally. **Buying into a full hold
succeeds today**, and so does selling into a full station.

That was survivable while nothing could trade. It is not survivable now, because §12.30 made the
finite hold the substrate the price curve is computed against — an overflowing hold is a hold whose
quantity no longer means anything.

> **Every transfer checks the destination and is refused whole, never partially applied** — the
> all-or-nothing contract `FactionEconomy::Spend` already documents and `features.md` §5.0's
> `Deposit`/`Withdraw` API repeats.

**And the refusal is visible before the click**, not after: the screen disables the row with
`HOLD FULL` (§3.10's degrade-never-remove). The system refuses anyway, because the UI is not
authority — the same two-sided discipline §12.30.2 applies to bay capacity, where the search filters
and the handoff still validates.

⚠️ **`FacilityStats::capacity`'s comment says *"Docking bays, storage slots."*** The storage half is
now dead: under §12.30 the hold **is** the storage capability, so `CargoHold::capacity` is the number
and a facility has nothing to say about it. §12.30.2 gives the docking half its first reader; **the
comment loses its second clause in the same edit**, before someone implements a facility-gated slot
count that nothing was ever going to read.

⚠️ **And `CargoHold` is neither a count nor one component — §12.23 moves it onto the cargo bays.**
The code checks list entries today (`CargoHoldEntryCount` returns `modules.size() +
materials.size()`), `features.md` §2.11 authors **`slotCount` stacks of `slotCapacity` mass each** per
bay, and §12.23 settles that the hold is a component on each bay hardpoint with the rig-level hold as
a *view*. `CargoHoldEntryCount` and `CargoHoldHasRoomFor` are deleted in favour of
`shared/rig/CargoView.h`.

**This screen is where it is visible three times.** `HOLD FULL` must be evaluated against the mass of
*the quantity being bought* — the `1` / `10` / `All` buttons make three answers out of one row; there
is a **second** disabled reason, `NO FREE SLOT`, when the hold has mass to spare and no slot for a new
item type; and the finite station hold that makes `LocalPrice` meaningful is finite because of the
bays the station was **built** with, which retires this section's own *"it also needs a non-zero
`capacity`"* note below.

##### A hold is one list, not two — and modules do not stack while materials do

*Found while scoping the row model, verified 2026-08-10.*

```
struct CargoHold {
    std::vector<ModuleId>     modules;
    std::vector<MaterialStack> materials;   // { std::string materialId; int quantity; }
    int capacity = 0;
};
```

Three defects fall out of the split, and every one of them lands on this screen:

1. **Half the hold cannot be traded at all.** `BuyItemRequest` and `SellItemRequest` carry a
   `ModuleId`, and `ProcessSellRequests` searches only `sellerCargo->modules`. **The half
   `MiningSystem` fills is the half the Market cannot touch** — which is the same unreachability
   §13.3 B records for mining, arriving from the other end.
2. **The two halves stack inconsistently.** `LootSystem` merges a `MaterialStack` by id
   (`LootSystem.cpp:21`) while buying pushes a duplicate `ModuleId` per unit
   (`StationServicesSystem.cpp:47`). Buying three of the same module makes three rows; mining three
   iron makes one. **One hold, two stacking rules, and the row list is where a player sees it.**
3. **Every consumer already treats them as one list.** `CargoHoldEntryCount` sums both sizes;
   `storage_menu::Rows` concatenates them; capacity counts them together.

> **After §12.19's `ItemId` and §13.5 group 2b's Element/Material rename, `CargoHold` holds one
> `std::vector<ItemStack>` keyed on ~~`ItemId`~~ the whole `ItemInstance`.** Two parallel lists for
> one concept is finding §13.3 C's shape, and this is its fourth instance in the audit.

> ⚠️ **The key is widened by §12.19.** Keyed on `ItemId` alone, a Mythic-rolled cannon and a Common
> one stack and the hold forgets which is which. **Two units stack when they are indistinguishable** —
> same id, grade, attributes, mass and `Quality` — which collapses Elements always (the case this
> subsection cared about, and the one that produced hundreds of single-unit rows), collapses
> Materials per production run, and correctly never collapses a rolled module. One rule, three
> outcomes, no uniformity forced.

**It also fixes the row explosion by itself**: uniform stacking is what keeps ~50 Elements and 8
Material families across 7 grades from becoming several hundred single-unit rows.

##### 🐛 The tab cannot be keyed on `FacilityKind`, and Storage is the case that proves it

*This revises §12.30's fix for its own tab defect, one section after that fix was written.*

§12.30 corrects `AvailableTabs` to return `{ FacilityKind kind; entt::entity hardpoint; }` so the
router can set `PlayerLocation` from the tab it draws. **That signature cannot express the Storage
tab.** Storage is gated on a *component*, not a kind, and §12.30 itself settles that it *"stands
alone on a station with a `CargoHold` and no `Trade` hardpoint."* Such a tab has **no `FacilityKind`
and no hardpoint entity**, and §3.4's mandatory per-screen readout has nothing to measure.

**Fix — the tab is keyed on the screen, not on the kind:**

```
struct Tab { ScreenId screen; entt::entity hardpoint; };   // hardpoint may be entt::null
```

`ScreenId` is the router's dispatch key and has seven consumers the day it lands, so it is not §2.4's
dead abstraction; `FacilityKind` stays what it has always been — a property of a *module*. Most tabs
still derive one-to-one from a living facility hardpoint, exactly as before.

**And the readout still has a subject.** For a hardpoint-less tab the `Gauge` shows the **station's
aggregate integrity** rather than a hardpoint's, which `CockpitHud::AggregateHullFraction` already
computes. That is not a fallback — it is the correct predicate: **the Gauge always measures what
kills this capability**, and a storage hold dies with the station rather than with any one hardpoint.
The rule reads identically for every screen and needs no branch on which one it is.

*§12.30's own dedupe test widens with it: `AvailableTabs` returns all six facility-derived tabs when
all six kinds are present and living, **plus** a Storage tab on any host with a `CargoHold`, and the
Storage tab names `entt::null`.*

##### ⚠️ Whose faction is it? — an amendment to §12.30.1

*The ownership table above asks one question — "is this station's `FactionRef` yours?" — and the
codebase cannot answer it once §12.30.1 lands.*

The player's `FactionId` lives on **the rig root they control** (`RigFactory.cpp:120`;
`SpaceFlight.cpp:84` reads it off `player` to carry it across a warp). §12.30.1 settles that while
docked, the derived `PlayerControlled` is **the station**. So at a foreign station, *"my faction"*
reads off a hull the player does not own, and **every identity test in the game inverts.**

**This has a named reader today, not a hypothetical one.** `DiscoverySystem.cpp:13` views
`<PlayerControlled, FactionRef>` and credits discovery to that faction; `ConstructionSystem`'s
`RequesterFaction` (`ConstructionSystem.cpp:15`) reads it off the requester and stamps it on
everything built. Docked in a Zenith station, the player would explore and build under Zenith's flag.

> **Identity is not location.** `PlayerLocation` answers *where the player is*, and §12.30.1 is right
> that it should be the only writer of that. It cannot also answer *who the player is*, because a
> player standing in a bay may own none, one, or three of the hulls parked there — ownership is not
> derivable from position in either direction.

**The player's `FactionId` moves off the occupied hull and onto the player record** — the lazily
created singleton §12.24 already settled as the home for player-scoped state, on the `CommsLog`
precedent. Reads of `FactionRef` **for identity** (`DiscoverySystem`, `ConstructionSystem`'s
requester, this screen's ownership test, §12.30.2's *"marked as yours"*) resolve against that record.
Reads of `FactionRef` **for a hull's allegiance** — `TargetingSystem`, `DockingSystem`'s gate,
`ContractSystem`'s kill credit — are unchanged and correct as they are.

**It is cheap now and expensive later, for the same reason §12.30.1 itself gave**: there are three
identity readers today and every screen in this batch adds one. It is not reachable until the docking
gate widens, so **it lands with that widening**, not before.

##### Types

- **`BuyItemRequest` / `SellItemRequest`** — `ModuleId module` → **`ItemId item`**, gain
  **`int quantity`**, and **lose `cost` / `value`** entirely (Law 9, above).
- ⚠️ **§12.19 amends the payload: a request names an `ItemInstance`, not an `ItemId`.** Under the
  stack rule above a hold may carry two stacks of one id at two grades or two rolls, so an id does
  not identify what the player clicked. The intent is *"trade ten units matching this"*, the system
  finds the stack, and nothing needs a row index that a same-tick sibling request could invalidate.
  This applies identically to `TransferItemRequest`, `DeconstructModuleRequest` (§12.30.5) and
  `StartResearchRequest` (§12.30.6).
- **`TransferItemRequest { ItemId item; int quantity; bool toStation; }`** — new, in
  `shared/components/StationServices.h`. **One type with a direction**, because deposit and withdraw
  are the same operation with a sign: no credits, no price, no gate difference, identical validation.
- **Buy and sell stay two types**, and this is deliberate rather than inconsistent. The free path and
  the paid path have different failure modes and, under Law 9, different authority requirements. Two
  types keep the price **out of the free path by construction** — the same argument §12.31 makes for
  splitting `core/serialization/` from the one factory that sees handles.
- **`core/economy/Pricing.h`** — `BaseValue` and `LocalPrice`, free functions (§13.5 group 2c).
- **No new component.** The station's stock is its `CargoHold`, ownership is its `FactionRef`, the
  gate is a `FacilityRef`. That is the test §2.4 sets, and this screen passes it — the one new type
  is an intent, which is what §12.30 says a screen is allowed to produce.

##### Systems

`StationServicesSystem` gains the transfer verb, the ownership check, the capacity check, and the
pricing call; it loses the caller-supplied prices. **No new system.**

`StationFactory` emplaces a `CargoHold` — §13.3 O's fix, which under §12.30 **is** this screen's
Storage half rather than a precondition for it. ~~It also needs a **non-zero `capacity`**, or the
finite hold that makes `LocalPrice` meaningful is finite in name only.~~ **Superseded by §12.23:**
the hold lives on the station's cargo-bay hardpoints, so its capacity is whatever it was built with
and there is no number for the factory to invent. What `StationFactory` needs instead is a station
blueprint that **authors cargo bays** — content, and the same prerequisite the player's starter
chassis has.

##### Tests

- Buying moves the item from the station's hold to the vessel's and debits the wallet by
  **`LocalPrice`**, not by a number the request carried — the request has no such field to carry.
- Selling is the exact inverse, and a buy immediately followed by a sell of the same unit nets zero
  credits at the same station (no spread), while the price moves between two stations with different
  stock.
- Buying into a **full** hold is refused whole: no item moves and no credits are debited.
- Selling into a **full** station hold is refused the same way.
- Deposit and withdraw move items with **no wallet change**, and are refused at a station whose
  `FactionRef` is not the requester's.
- Buy and sell are refused when the station has **no living `Trade` hardpoint**, and permitted when
  it has one — including on a station the requester owns.
- A request placed on the **station** entity rather than on the docked vessel is refused rather than
  silently dropped — the trap named above, asserted once.
- `AvailableTabs` returns a Storage tab naming `entt::null` on a host with a `CargoHold` and no
  `Trade` hardpoint, and no tab at all on a host with neither.
- A material (not a module) round-trips through buy and sell — the assertion that fails today and
  cannot pass before §12.19.
- **The request a screen builds lands on the docked requester and is consumed by its system on the
  following tick** — §12.30's shared per-screen shape.

##### Scheduling — the two halves split, and not where §12.30 implied

Both halves are §13.5 group 4b, but they do not unblock together:

| | Needs | When |
|---|---|---|
| **Storage** | §13.3 O (`StationFactory` emplaces `CargoHold`) · the capacity fix · 4a's widgets | **With group 4b.** Nothing else |
| **Market** | All of the above · §12.19 `ItemId` · `core/economy/Pricing.h` (2c) · §12.24 step 6 for the relation gate and the reputation modifier | **After the item model** |

⚠️ **This is the reverse of the useful order and it should be said out loud.** Storage lands first
and is reachable everywhere, while the Market — the half §12.10 was written for — waits on three
other batches. **Ship the tab with Storage alone** rather than holding it: a warehouse that does not
deal is one of the four ownership cases above, it is a complete screen, and it makes `CargoHold`'s
capacity, the row model, and the transfer path all exercisable before a single price exists.

##### What is deliberately not here

- **No quantity slider.** §12.30 rules a slider out until it has a second consumer; `quantity` ships
  as discrete `1` / `10` / `All` buttons, which is also what a stacked hold actually wants. **The
  footer shows the `LocalPrice` *total* for the selected quantity**, never a unit price the player is
  expected to multiply — under the quantity rule above, unit × quantity is not the answer, so
  displaying a unit price would show a number that is not true.
- **No price history, no order book, no trade route planner.** `features.md` §5.0's *"global market
  awareness"* is a §8 navigation-map surface — knowing the price of iridium elsewhere is a map
  question, not a shop question.
- **No station-to-station shipping from this screen.** §5.0's internal logistics is a faction's macro
  tick dispatching an in-transit fleet record; the player's version of that is flying the cargo.
- **No repair.** It is Screen 3 (§12.30.4), behind its own `FacilityKind`, and §13.3 I already
  records that docking grants free unlimited repair today regardless.

##### Sibling holds — a chosen destination, not an auto-routed one

*Raised by the project owner 2026-08-23; decided the same day, scoped as a follow-on to this screen
rather than a revision of it.* A rig can already carry more than one `CargoHold` — `shared/components/
Loot.h` puts the hold on the bay hardpoint, not the rig root, specifically so *"destroying one bay
loses exactly that bay's stacks."* That already makes hold placement strategic (spreading stock so one
lost bay is not a total loss); what is missing is a player's ability to **choose** the placement.
`cargo_view::Deposit` auto-picks the emptiest bay today, and `ItemStack` carries no bay identity for a
caller to display or target.

**This screen gains a sibling selector**, the same `TabStrip` pattern §12.30.2's Bay screen already
establishes for multiple docking bays — already generalised to every screen by the note above §12.30.1
("The Docking tab is the one whose identity matters"): *"the sibling selector generalises to every
tab; Docking is simply where it was noticed first."* One pill per living `CargoHold` hardpoint here,
each showing that hold's own integrity, Deposit targeting whichever is selected.

⚠️ **Two gaps this needs, neither of which exists today:**

- `ItemStack` (`shared/components/Loot.h`) needs a bay/hardpoint identity field — today's `Merged()`
  deliberately does *not* merge same-id stacks across bays *"to not hide which bay a given unit
  actually lives in,"* but the row it produces still cannot say which bay that is.
- `cargo_view::Deposit` needs a destination-choosing overload alongside its existing auto-routed one —
  the auto-pick stays the default for withdraw-side and non-screen callers, the screen is the one
  caller that names a specific hold.

**NPC parity, flagged here rather than decided:** NPC factions already run their own faceless
command-economy (repair/build/expand, all resource-gated via `FactionEconomy::Spend`) with no
per-hold reasoning at all — stock is spent against the faction's aggregate. **Decided by the project
owner 2026-08-23: NPC logistics should factor in spreading stock across holds too**, once this
screen's mechanic exists — a faction that keeps its whole stockpile in one hold is exposed to the
exact single-point-of-loss risk the player can now choose to avoid, and an AI that does not reason
about it is playing a strictly worse version of its own game. **Not yet scoped**: this needs a look at
`NpcAiSystem`/the faction command-economy's actual stock-allocation logic (wherever
`FactionEconomy::Spend`'s callers decide what to build and where) before it is buildable — filed as
P4-14 below, blocked on P4-11 (this section's own sibling-hold task) landing first.

#### 12.30.4 Screen 3 — Repair

*Settled 2026-08-10. Verified against `src/` by grepping for readers and callers. **This screen has
the most built code behind it of any of the seven, and the most of it is wrong** — three defects in
`ProcessRepairRequests`, one of which makes `features.md` §3.9's status display lie. It also supplies
a third answer to §13.4 decision 1 that the decision's own framing excluded.*

**Gate:** a living `FacilityKind::Repair` hardpoint. **Subject:** a rig you own that is present here
— the hull you arrived in, or the host you are standing in if it is yours. **One section**, and the
`ListView` is over **your own hardpoints**, which makes this the first screen whose list is your rig
rather than a hold or a roster.

##### 🐛 Three defects in the one repair path that already exists

`StationServicesSystem::ProcessRepairRequests` (`StationServicesSystem.cpp:83`) is built, tested, and
scheduled. Verified 2026-08-10:

**1 — There is no facility gate at all.** The function resolves `DockedStation` and then uses the
result *only* as a docked-ness check (`station == entt::null || …`). It never looks for a
`FacilityKind::Repair` hardpoint, living or otherwise. §12.24 step 5a assumed the gate existed to be
routed; it does not exist to be routed.

**2 — It heals `Destroyed` hardpoints, and that makes the status display lie.** The loop is:

```
for (const entt::entity hardpoint : rig->children) {
    Health* health = registry.try_get<Health>(hardpoint);
    if (health == nullptr) { continue; }
    const float missing = health->max - health->current;
    health->current = std::min(health->max, health->current + request.fraction * missing);
}
```

**There is no `Destroyed` check anywhere in the file** — zero occurrences. `DockingSystem`'s free
heal, ten lines of near-identical code, *does* check it and says why: *"Destruction is permanent
(`Health.h`) — docking never revives one."* So the paid path restores a destroyed hardpoint's
`Health.current` to `max` while leaving the `Destroyed` tag in place, and **`DamageSystem` never
removes that tag** — it only ever `emplace_or_replace`s one when health reaches zero
(`DamageSystem.cpp:37`).

The result is a hardpoint at full hull that is permanently dead, and `features.md` §3.9 spends
**colour on integrity** — so the schematic draws it green. *"Colour is condition"* becomes false for
that circle, on the one display §3.1 and §3.5 were both waiting for. **This is worse than a wasted
payment: it is a UI that reports the opposite of the truth**, and it would pass every test that only
checks "health went up."

**3 — `costForFullRepair` is supplied by the caller**, exactly as `BuyItemRequest::cost` is
(§12.30.3). It is also *flat*: a pristine hull and a wreck pay the same for `fraction = 1.0`, because
nothing in the request relates price to hull actually restored.

⚠️ **And partial repair is strictly dominated.** `spend = round(fraction × costForFullRepair)` while
the heal is `fraction × missing`. Two repairs at `0.5` cost a full repair's price and leave a quarter
of the damage. **No rational player ever passes anything but `1.0`**, which makes `fraction` — the
single continuous input in the whole docked set, and the reason §12.30 discussed a slider at all — a
field with one legal value.

##### §13.4 decision 1 has a third answer: the rate is not deleted, it moves

*This does not reverse decision 1's recommendation. It corrects what "delete the automatic heal"
takes with it.*

§13.3 I frames the choice as binary — *"either docking-heals is the intended baseline and
`StationServicesSystem`'s repair is redundant, or repair is a facility service and `DockingSystem`'s
heal must be deleted"* — and §13.4 recommends the second. **Deleting the free heal is right. Deleting
the *rate* with it is not, and two settled sections would break if it were.**

| What the free heal is | Where it must go |
|---|---|
| `kDockHealPerSecond` — a **rate**, 15% of max per second | `FacilityStats::ratePerSecond`, whose comment reads *"**Repair HP/s**, manufacturing progress/s, research points/s"* |
| `features.md` §2.7's **Repair crew role**, listed **twice** as ✅ *"Buildable now"* with `DockingSystem`'s dock-repair **rate** as its named consumer | The same rate, now on the facility path |

🐛 **`FacilityStats::ratePerSecond` is parsed (`BlueprintJson.cpp:38`), merged by `EngineerSystem`
(`EngineerSystem.cpp:90`), and read by no behaviour anywhere.** It is a stat that content authors,
that merging scales, and that nothing consumes — the audit's dominant defect, in a field §13 has not
yet catalogued. **Repair is its first reader**, the same way §12.30.2 gives `FacilityStats::capacity`
its first reader and §12.30.3 gives the station's `CargoHold` its first meaning. Three screens, three
dead fields revived, no new types.

> **Repair is a continuous, paid, facility-gated service.** The heal that exists today keeps its
> shape and loses its two lies: it stops being free, and it stops happening at bays that cannot do it.

**Instant-on-payment was considered and rejected.** It is simpler, and it costs three things: a Mythic
repair bay and a Common one would differ only in price, §2.7's Repair crew role would go from ✅ to ❌
with no consumer, and `ratePerSecond` would be deleted as dead rather than revived. **A rate is what
both the field and the crew role were authored for.**

##### Continuous billing, because it is the version with no refund logic

**You pay as it repairs, never up front.** This is not a flavour choice — an up-front lump sum needs
cancel semantics, undock-mid-job semantics, and a refund rule, and every one of those is a decision
that can be wrong. Paying per unit of hull restored needs none of them:

| Event | Result |
|---|---|
| Target reached | Order completes |
| Wallet empties | Repair stops where the money ran out. Nothing owed, nothing refunded |
| The player undocks | Order is dropped. You keep exactly the hull you paid for |
| **The Repair hardpoint is destroyed mid-job** | Order stops. §3.4's *"you die with your facility"* in its benign form — and the mandatory `Gauge` was showing it the whole time |

⚠️ **Bill in whole credits with the fractional remainder carried in the order.** At 60 ticks a second
a per-tick charge rounds to zero and the repair is free again — the same failure this section exists
to remove, reintroduced by the cheap implementation. Written down because the cheap version is the
one that gets typed at 2am, the same warning §12.30.2 attaches to bay occupancy.

##### 🐛 Nothing in this codebase can repair a station — and this screen is where that is fixed

*Verified 2026-08-10.* There are exactly **two** sites that raise a `Health.current` anywhere in
`src/`: `DockingSystem.cpp:91` and `StationServicesSystem.cpp:108`. **Both require `Docked` on the rig
being healed**, and a station is never `Docked` — `Docked` is written to the vessel that flew in.

**So a player-built station that takes a hit stays damaged forever**, and so does every NPC station.
That is a permanent, monotonic decay of every fixed asset in the galaxy, and it has no counterpart
anywhere: §12.20's faction economy models stock, not structure.

**The fix is a selector, not a system.** The repair order's subject is *a rig*, and while docked there
are at most two candidates — the hull you arrived in, and the host you are standing in when its
`FactionRef` is yours (§12.30.3's ownership test). A `TabStrip` above the list picks between them,
which is the same widget §12.30.2 uses for sibling bays. **A station with a repair bay repairs
itself**, which is what a repair bay is.

*The subject selector is also the answer to "can I repair another vessel parked in this bay?" —
deliberately no, for now. Ownership of a hull is a §12.30.2 question and the roster is that screen;
adding a third candidate here before the Bay screen answers it would fork the ownership test.*

##### Layout

| Section | Contents |
|---|---|
| **Header** | Facility name · the `Repair` hardpoint's integrity `Gauge` (§3.4's mandatory readout) · your credits · **rate**, HP/s |
| **Subject selector** | `TabStrip` — your vessel · this host, when it is yours. Absent when there is one candidate |
| **List** | `ListView` over the subject's `Rig::children`, one row per hardpoint |
| **Footer** | The verb buttons for the selected row, plus **Repair All** |

`Rig::children` is **flat** — every hardpoint of the root, not a tree — so one pass over it is the
whole rig and the list needs no traversal. (`StructuralAttachment` is the separate structural graph;
§3.9's LOD collapse reads that one, this list does not.)

Each `Row`: **`glyph`** — the hardpoint's `ShellRole` monogram (§3.9's glyph-is-identity). **`label`**
— display name. **`value`** — `current / max`, and the cost to bring it to full. **`RowStyle`** —
§3.9's integrity gradient, so the list *is* a text-mode status display and reads the same way the
schematic does. Rows sort by integrity ascending: **the thing most likely to kill you is the first
row**, without the player sorting anything.

##### The verbs

| Row is | Verb | Mechanism | Blocked on |
|---|---|---|---|
| A living hardpoint below full | **Repair** | `RepairOrder` naming that hardpoint | `Pricing.h` |
| Any state | **Repair All** | One `RepairOrder` with a null hardpoint, meaning the whole rig | `Pricing.h` |
| A `Destroyed` hardpoint | **none** — disabled, with the reason | It is rebuilt, not repaired (below) | §12.30.5 |
| An order in progress | **Stop** | Remove the order. Nothing is owed | — |

**No slider, and no `fraction`.** §12.30 rules out a slider until it has a second consumer; the
defect above shows `fraction` never had a first one, since only `1.0` is ever rational. The order
carries a **target integrity** instead, offered as discrete buttons — and *repair to 50%* is a
different and more useful thing than *repair half the missing hull*, because it is a statement about
the hull you will undock with rather than about arithmetic.

##### Price derives from what the hardpoint is made of

`features.md` §2.10's attribute table already assigns this: **Inert — *"corrosion resistance"* —
carries *"corona/hazard resistance, **repair cost**."*** That section also rules that *"an attribute
with no reader is the same defect as a system with no producer."* **This screen is the Inert
attribute's repair-cost reader**, and it is the only one named anywhere.

```
core/economy/Pricing.h        beside BaseValue / LocalPrice (§12.30.3)

int RepairCostPerHp(const ShellDef&, Grade facilityGrade, const ContentLibrary&);
```

> 🐛 **That signature cannot be implemented, and §12.19 corrects it twice.** **A `ShellDef` has no
> Inert attribute** — Inert is a property of the *elements* a shell was made of, propagated to the
> instance (`features.md` §2.10), and a def is grade-neutral besides. **And a hardpoint does not
> record which shell it is:** verified 2026-08-10, `RigFactory::CreateHardpoint` emplaces `MountRef`,
> `ParentRig`, `ShellRole` (the *kind*), `HitRadius`, the transforms, `MountedModules` and `Health`
> — **no `ShellId`** — so today the only route to the def is a three-hop join through `BlueprintRef`.
> §12.19 adds `ShellInstance { ItemInstance shell; }` to every hardpoint and the signature becomes
> `RepairCostPerHp(const ItemInstance& shell, Grade facilityGrade, const ContentLibrary&)`. **The
> pricing argument above is unchanged** — this screen is still the only named reader of Inert.

Three inputs, all settled elsewhere, none of them new:

- **The shell's own recipe base value** — an iridium hardpoint costs more to patch than an aluminium
  one, because §2.10 already derives value from the recipe.
- **Its Inert attribute**, reducing the cost. Corrosion-resistant material is cheaper to keep whole.
- **The facility's `Grade`**, as an efficiency divisor — §2.10's *"cost to build = recipe **×**
  facility grade"* applied to its second consumer, rather than a second cost rule invented here.

**And `costForFullRepair` is deleted from the request**, not filled in. Same reason as §12.30.3's
`cost`/`value`: under Law 9 the order states *what and to what integrity*; the system decides the
price, calling the same pure function the screen calls to display it.

⚠️ `features.md` §5.3's reputation modifier applies here as it does to the Market — *"trade
discounts"* is not scoped to goods. It ships as identity until §12.24 step 6, and the header says so.

##### A destroyed hardpoint is rebuilt, not repaired — and the verb does not exist yet

*This is a decision, and the honest half of it is that it names a door that is not built.*

`Health.h` is unambiguous — destruction is *"gone permanently for this rig."* Repair restores hull on
living hardpoints and never touches the `Destroyed` tag. That keeps one rule true everywhere and
keeps this screen from overlapping Engineering.

🐛 **But nothing anywhere can restore a destroyed hardpoint.** Verified: `RefactorSystem` **deletes**
a leaf hardpoint and refunds its `MountedModules`; §12.12 covers building whole units (`BuildMenu`),
merging modules (`EngineerMenu`), and deleting hardpoints (`RefactorMenu`). **Nothing adds a mount
back to an existing rig**, in code or in design. So today a rig degrades monotonically toward death
and the only remedy is buying a new hull.

**Rebuild belongs on the Engineering screen (§12.30.5), as the inverse of the delete that already
exists**, and it is cheap for a reason §12.31 already noticed while designing `RigState`: *"a
hardpoint `RefactorSystem` deleted is simply absent from `mounts`. No tombstone."* **The blueprint
still holds the `MountBlueprint`** — shell id, local offset, `attachedTo`, traverse. Rebuilding
consumes the parts and restores a mount whose entire definition is content that never went away. No
new type, no snapshot, no tombstone.

**Until it lands, the destroyed row is disabled and says where to go** (§3.10's degrade-never-remove),
and the pointer is honest only once §12.30.5 ships. Scoped there; recorded here because this is the
screen where a player first asks the question.

##### Who pays when the repaired rig is not the player's

⚠️ **Deleting the free heal takes NPC repair with it, and nothing else replaces it.**
`HealAndImmobilize` heals *every* docked rig — `view<Docked, Rig>`, with no player filter. It is the
whole of the *"NPCs retreat to a friendly bay and heal"* behaviour, and NPCs carry no `Wallet` to pay
a facility with.

**An NPC's repair is billed to its faction's stock**, through `ctx.economy` — a pointer that exists
and is nullable (`System.h:43`) and that **only `FactionEconomySystem` reads today**. That gives
`FactionEconomy::Spend`'s all-or-nothing contract a real Tier 1 consumer, and it makes combat
attrition cost a faction something, which is what §6.1's Material Security facet and §5.1's Three
Pillars both want to read and currently cannot.

The order component is identical either way — **placed by UI or by AI, exactly as `DockRequest`
is** — and only the payer differs: `Wallet` on a rig that has one, `ctx.economy` otherwise. **This is
a step-6 dependency**, and until it lands NPCs simply do not repair, which is a visible regression
rather than a silent one.

##### Types

- **`RepairOrder { entt::entity subject; entt::entity hardpoint; float targetFraction; float creditRemainder; }`**
  — replaces `RepairRequest`. `hardpoint == entt::null` means the whole rig. Lives in
  `shared/components/StationServices.h` beside the others.
- ⚠️ **It is an *order*, not a request, and that is a deliberate exception to a strong idiom.** Every
  intent in this codebase — `DockRequest`, `FireIntent`, `BuyItemRequest`, `DeleteHardpointRequest` —
  is consumed and cleared **the same tick**. A repair takes many ticks by construction, so this one
  **persists until met, stopped, undocked, or invalidated.** Name it and comment it, or the next
  contributor will "fix" it into the same-tick idiom and silently make repair instant again.
- **`core/economy/Pricing.h` gains `RepairCostPerHp`.**
- **No new component on the rig, and no new tag.** The subject is a `Rig`, the gate is a
  `FacilityRef`, the rate is `FacilityStats::ratePerSecond`, destruction is `Destroyed`. §2.4's test,
  passed.

##### Systems

`StationServicesSystem` keeps repair and gains: the facility gate, the `Destroyed` exclusion, the
per-tick rate, the derived price, and the `ctx.economy` payer branch. **No new system** — the same
verdict §12.30.2 reached for the Bay.

`DockingSystem::HealAndImmobilize` **loses its heal loop and keeps the immobilise half**, per §13.4
decision 1. The function should be renamed with it; a name that describes half of what it does is how
the next reader learns the wrong rule.

##### Tests

- A repair order at a station with **no living `Repair` hardpoint** is refused — the assertion that
  fails today, because no gate exists.
- Docking alone heals **nothing**, at any station, for any rig — the regression test for §13.4
  decision 1.
- A `Destroyed` hardpoint's `Health.current` is **unchanged** by any repair order, including
  *Repair All*, and no credits are charged for it.
- Repair progresses at `FacilityStats::ratePerSecond` and stops exactly at the order's target
  integrity, never above it.
- Credits debited equal `RepairCostPerHp` × hull actually restored, ±1 for the carried remainder —
  and a repair spanning hundreds of ticks charges a total, not zero (the rounding trap).
- An empty `Wallet` stops the repair with the hull it paid for, and owes nothing.
- Undocking mid-order drops it; re-docking does not resume it.
- Destroying the `Repair` hardpoint mid-order stops it that tick.
- A station the requester owns is a valid subject and its hardpoints heal — the assertion that fails
  today for every station in the galaxy.
- An NPC with no `Wallet` repairs against `ctx.economy` and is refused when its faction cannot
  afford it; with `ctx.economy == nullptr` it does not repair, and does not repair for free.
- **The order a screen builds lands on the docked requester and is consumed by its system on the
  following tick** — §12.30's shared per-screen shape, with the one difference that this order is
  *acted on* rather than cleared.

##### Scheduling

Group **4b**, and it is the **least blocked of the three screens so far** — no `ItemId`, no
`LocalPrice`, no item model. It needs:

| Needs | Status |
|---|---|
| 4a's widgets · the router · `PlayerLocation` | Group 4b |
| A `Repair` facility authored in `modules.json` | Content; `modules.json` holds exactly one facility today |
| `Pricing.h`'s `RepairCostPerHp` | Group 2c — **but a flat placeholder is honest here**, since the rate and the gate are what the screen is for |
| §12.24 step 6 for `ctx.economy` | Only for the **NPC** payer. The player path needs nothing |
| §12.30.5 | Only for the **rebuild** verb the destroyed rows point at |

**The three defects should be fixed with §13.5 group 2, not held for this screen.** The facility gate,
the `Destroyed` exclusion, and the free-heal deletion are each a few lines against built, tested code,
and the `Destroyed` one is actively producing a display that lies. Group 2 is *"one-line and one-view
corrections, independently startable today"*, and all three qualify.

##### What is deliberately not here

- **No in-flight repair.** `features.md` §2.7 records it as ❌ *"new mechanic first"* and blocks the
  Damage-control crew role on it. Nothing here changes that, and the deletion of the free heal makes
  the docking path the *only* heal rather than one of two.
- **No shield repair.** `DamageSystem::RegenerateShield` already recharges shields on a delay
  (`Health.h`'s `rechargeCooldown`), and a destroyed shield hardpoint is a rebuild question, not a
  repair one.
- **No repairing another player's or NPC's vessel.** Ownership of a parked hull is §12.30.2's
  question and the roster is its screen.
- **No repair queue.** One order per subject; a second replaces the first. A queue is a Manufacturing
  and Research shape (§12.18, §12.1) because those have discrete jobs; repair has one continuous
  quantity and would gain nothing but a data structure.

#### 12.30.5 Screen 4 — Engineering

*Settled 2026-08-10. Verified against `src/`. **This screen has four verbs, not two**, and only two of
them exist. It also revises §12.30.2's claim that duplicate facilities are fungible, corrects a
"requires no code change" claim in `features.md` §2.4 that is not true, and specifies the **rebuild**
verb §12.30.4 scoped here.*

**Gate:** a living `FacilityKind::Engineering` hardpoint — already the gate both built systems use,
duplicated locally in each (`EngineerSystem.cpp:19`, `RefactorSystem.cpp:18`) with a comment
explaining why. **Merging §12.30's two sections into one screen is what makes that duplication
visible**, and the shared gate becomes one function on the router rather than two copies.

##### Four verbs on two axes, and the shape is already proven

§12.30's inventory names the sections *Merge · Refactor*. That is the two **files** being folded, not
the operations, and the screen has four:

| Verb | Acts on | State |
|---|---|---|
| **Merge** | Two modules in your `CargoHold` | ✅ Built — `EngineerSystem` |
| **Deconstruct** | One module in your `CargoHold` | 📋 Designed (§12.13 item 5), not built |
| **Delete** *(scrap)* | One hardpoint on your rig | ✅ Built — `RefactorSystem` |
| **Rebuild** | One mount your blueprint has and your rig does not | 📋 **Specified here.** Nothing anywhere |

**They fall on two axes, and the axes are the layout**: the left list is your hold, the right list is
your rig. That is the same two-list-and-a-verb shape as the Market (§12.30.3), and the third screen in
a row that the five widgets of §12.30 cover with nothing left over — which is the check that matters
more than any one screen fitting.

##### 🐛 `features.md` §2.4's *"requires no code change"* is not true

§2.4 settles that **merging is bounded by the grade band and refused at the ceiling**, with the
formula:

```
newQuality = primaryQ + (bandMax − primaryQ) × secondaryNorm × (facilityLevel × 0.1)
```

and states that *"`EngineerSystem` already scales by `FacilityRef::level`, so this decision requires
**no code change** — it ratifies what is built."*

**Only the level-scaling half is built. The built formula is a different operation entirely:**

```
MergeField(p, s, level) = p + s * (level * 0.1)      // EngineerSystem.cpp:42
```

That is **additive on raw stat values with no ceiling of any kind** — not a move through quality space
toward a band maximum. The two agree on one factor (`level × 0.1`) and on nothing else. §2.4's version
cannot be built at all yet: it needs `bandMax`, `primaryQ` and `secondaryNorm`, and **§12.21's
`Quality` type does not exist**, which is the same dependency §12.31 hit from the other side.

Three consequences, and the first is the one that would ship:

1. **Merging is currently unbounded.** Nothing refuses a merge at a ceiling because there is no
   ceiling — §2.4's refusal *"joins `EngineerSystem`'s existing refusals"* and it is not among them.
2. **It is applied per raw field, so it is not even uniform.** `merged.weapon = primary.weapon` copies
   the whole block and then merges three of its seven fields; `fireIntervalSeconds`, `spreadRadians`,
   `projectilesPerShot` and `damageType` come from the primary alone. Under §2.4's quality model there
   is one number to move and this asymmetry disappears.
3. **`FacilityRef::level` is always `1`** — §13.3 K, `ModuleAttachment.cpp:65` passes only
   `module.facility.kind` to the `emplace_or_replace`, dropping the level, and `ParseFacilityStats`
   never reads it from JSON. **So every merge in the game preserves exactly 10% of the secondary**,
   and the entire facility-level axis §2.4 calls settled has never once had a value other than its
   default.

> **The merge formula is not built and must not be treated as built.** It lands with §12.21's
> `Quality`, and §2.4's claim should be corrected in the same pass — the decision it ratifies is
> *"level scales the merge"*, which is true, not *"the formula is implemented"*, which is not.

⚠️ **Merging also consumes nothing but the two modules.** §2.4 settles that it *"consumes Materials as
well as credits, scaled by the module's grade and by how close to its band ceiling the merge is
pushing it."* The built path debits no `Wallet` and no materials. Both wait on §12.19 and §12.21, and
until then merging is free — which, combined with being unbounded, is worth knowing before it is
routed to a screen a player can reach.

⚠️ **And the crafted id grows without bound.** `merged.id` is
`primary + "+" + secondary + "@L" + level` (`EngineerSystem.cpp:52`), so merging two merged modules
produces `a+b@L1+a+b@L1@L1`. Ids **double in length per generation**, they are the key
`ContentLibrary::craftedModules_` is stored under, and §12.31 already needs that map serialized. A
crafted module should take a **short generated id** and carry its lineage in `displayName` if
anywhere — a save format should not have a key whose length is exponential in how many times the
player used the bench.

> ✅ **Dissolved by §12.19, and the fix is a deletion rather than a shorter id.** A merge moves the
> primary's **quality** within its band (§2.4), and a band move is not a new *definition* — so a
> merged module is the primary `ItemInstance` with a new `Quality`, `ContentLibrary::
> RegisterCraftedModule` and `craftedModules_` are deleted, and there is no generated id left to
> concatenate. §12.31's *"a crafted module does not survive its own process"* dissolves with it, and
> §12.30.8 re-points the overlay at the type that genuinely needs one: a drafted Template.

##### Rebuild — the delete, inverted

*This is the verb §12.30.4's destroyed rows point at, and §12.30.4 verified it exists nowhere: §12.12
covers building whole units, merging modules, and deleting hardpoints. **Nothing adds a mount back to
an existing rig**, in code or in design.*

**It is cheap for the reason §12.31 already noticed while designing `RigState`:** *"a hardpoint
`RefactorSystem` deleted is simply absent from `mounts`. No tombstone."* The rig's `BlueprintRef`
still holds the `MountBlueprint` — shell id, `localOffset`, `localRotation`, `attachedTo`,
`traverseRadians`. **Every input rebuild needs is content that never went away.**

> **Rebuild restores a mount the rig's blueprint authors and the live rig does not have in working
> order.** Nothing else. You cannot invent a mount, and you cannot rebuild one the blueprint never had.

That single constraint is what keeps this verb small. Adding an *arbitrary* hardpoint would need
placement validation, §3.5's ring-capacity formula, hull-envelope coverage (rule 12), and a traverse
check — an entire feature. **Restoring an authored mount needs none of them, because the blueprint
already passed all four when it loaded.**

###### Delete refuses a non-leaf; rebuild refuses an orphan

`RefactorSystem` already refuses to delete a hardpoint another hardpoint's `StructuralAttachment`
points at — *"deletion is scoped to leaves."* Rebuild reads the same graph in the other direction:

> **A mount whose `attachedTo` parent is missing or `Destroyed` cannot be rebuilt.** You cannot hang a
> wing off a hull that is not there.

**Delete works leaves-inward. Rebuild works root-outward.** One graph, two opposite refusals, no new
type — and a player rebuilding a shattered flank does it in the order the structure implies rather
than in an order a rule told them.

###### A rebuilt mount is empty, and that is a duplication fix rather than a stinginess

`MountBlueprint` carries a `modules` list. **Restoring it would print modules**: delete a mount, keep
its `MountedModules` (which `RefactorSystem` already refunds to your hold), rebuild it, receive a
second copy of the same modules from the blueprint. Repeat.

> **A rebuilt mount comes back bare.** `MountedModules` is empty, `AttachModuleComponents` is not
> called, and the player re-equips from their hold through the §3.10 loadout overlay —
> `ModuleEquipSystem`, which is built.

###### Destruction stops being free, and this is a one-line change to a built system

`RefactorSystem` refunds a deleted hardpoint's `MountedModules` to the hold with **no `Destroyed`
check** (`RefactorSystem.cpp:76`). So scrapping a hardpoint that was shot off returns every module it
carried, intact. **Losing a hardpoint in combat currently costs you a shell and nothing else.**

> **Scrapping a `Destroyed` hardpoint returns nothing. Scrapping a living one returns its modules.**

That one branch separates the two operations that share a verb: **voluntary refit** — you chose to
strip a mount, you keep what was in it — from **involuntary loss** — it was destroyed, and what was
inside it was destroyed with it. It is what gives §3.2's localised damage an economic weight it does
not have, and it is what makes rebuild a decision rather than a formality.

*It also settles the one contradiction rebuild raises with built code.* `Health.h` says a destroyed
hardpoint's capability is *"gone permanently for this rig"*, and `DockingSystem` cites that comment
when refusing to heal one. **Rebuild does not contradict it — nothing is ever revived.** The destroyed
entity is removed and a new one is built from content. **The comment should say so**, because *"never
healed"* and *"never replaced"* are different rules and only the first one is true.

###### Cost

The mount's shell is content with a recipe (§12.19), so rebuild consumes **that shell's recipe
inputs**, scaled by the facility's grade — §2.10's *"cost to build = recipe × facility grade"* applied
to its third consumer, after manufacturing and §12.30.4's repair. **No new pricing rule and no
authored cost table.** Until §12.19 lands it is a flat placeholder, and that is honest: the gate, the
graph rules and the empty-mount rule are what the verb is for.

##### Duplicate facilities are not fungible — a revision of §12.30.2

§12.30.2 settles the sibling-bay selector and justifies it as a Docking special case:

> *"`Docking` is therefore the one kind where the deduped tab is not the whole story… every other
> `FacilityKind` is fungible across duplicates — any living Repair bay repairs identically."*

**That is already false for three of the six kinds, and this screen is where it breaks first.**
`EngineerSystem::DockedEngineeringLevel` returns the **first** living Engineering facility's level
found while walking `Rig::children`. On a station with two benches of different grade, which one you
get is iteration order. And §2.4 makes the bench's grade decide *the outcome of the operation*, not
merely where it happens — as §12.30.4 makes the Repair bay's grade decide the rate and the price, and
§2.4's tables make a Research or Manufacturing facility's grade decide sample survival, duration and
recovery.

> **A duplicate is fungible only when the facility has no grade-dependent output.** Once §12.19 folds
> `FacilityStats::level` into `Grade` and every facility carries one, **no kind is fungible.**

**So the sibling selector generalises from Docking to every tab.** It is not a Docking special case
that Engineering also happens to need; it is the general case, and Docking was simply where it was
noticed first — because a bay's identity is visible (a specific hull is parked in it) while a bench's
identity is only visible in the result.

**Two corrections follow, and neither costs a new mechanism:**

- **`AvailableTabs` still dedupes to one tab per kind**, and the selector inside the screen lists the
  living hardpoints of that kind. Unchanged from §12.30.2 — only the justification widens.
- **The facility's grade comes from `PlayerLocation`, not from a first-found scan.** `DockedEngineering
  Level`'s walk is replaced by reading the hardpoint the player is standing in. **This is where
  §12.30's tab-entity fix pays off outside §3.4's death predicate** — that change was justified solely
  by the death rule, and it turns out to be what makes every graded facility addressable.

##### ⚠️ One hardpoint, many modules, one `FacilityRef`

`MountedModules::ids` is a list and `AttachModuleComponents` is called once per module
(`RigFactory.cpp:36`), so a mount holding two facility modules **silently keeps only the last one's
`FacilityRef`** — `emplace_or_replace` overwrites. Every screen in this batch is keyed on a facility
hardpoint, so a content author fitting a Trade and a Repair module to one mount gets one tab and no
diagnostic.

*This is the same class as §12.30's `kind`-is-optional finding and belongs in the same parser/attach
pass: either a mount accepts at most one facility module (a `Validation` rule), or `FacilityRef`
becomes a set. **The rule is the cheaper and more honest of the two** — a hardpoint is a place, and
two benches in one place is a content error, not a feature.*

##### Layout

| Section | Contents |
|---|---|
| **Header** | Facility name · **grade** · this Engineering hardpoint's integrity `Gauge` (§3.4) · your credits |
| **Sibling selector** | `TabStrip`, one entry per living Engineering hardpoint. Absent when there is one |
| **Left `ListView`** | Your `CargoHold`'s modules — the Merge and Deconstruct axis |
| **Right `ListView`** | Your rig's mounts — living, `Destroyed`, and **absent-but-authored**, all three |
| **Footer** | The verb buttons for the current selection |

**The right-hand list is the screen's real contribution, and it is not `Rig::children`.** It is the
**blueprint's mount list**, joined against the live rig — which is the only list in the game that can
show a mount that is *missing*. Three row states, and §8.3's *absence must never look like emptiness*
is the reason all three are drawn:

| Row state | Source | `RowStyle` | Verb |
|---|---|---|---|
| Living | `Rig::children` | §3.9's integrity gradient | **Delete** |
| Destroyed | `Rig::children` + `Destroyed` | Disabled, `DESTROYED` | **Delete** *(returns nothing)* |
| **Absent** | In the blueprint, not in the rig | Outline only, `MISSING` | **Rebuild** |

*A rig that has been shot apart and stripped reads, in one list, as what it is: what you have, what is
wrecked, and what is gone. No other screen can say the third thing.*

Merge is a two-selection verb — pick a primary, pick a secondary — so the left list carries a
**primary marker** on the first selection and the footer names both. `EngineerMenu::Draw(bounds,
primary, secondary)` already takes exactly those two ids and no list at all, which is the whole of its
current UI.

##### The verbs

| Selection | Verb | Mechanism | Blocked on |
|---|---|---|---|
| Two modules of one `ModuleKind` | **Merge** | `MergeModulesRequest` | §12.21 for the band; §12.19 for the cost |
| One module | **Deconstruct** | `DeconstructModuleRequest` (§12.13 item 5) | §12.19 |
| One living mount | **Delete** | `DeleteHardpointRequest` — built, unchanged | — |
| One destroyed mount | **Delete** | The same request, **returning nothing** | The one-line `Destroyed` branch |
| One absent mount | **Rebuild** | `RebuildMountRequest { MountId mount; }` | — for the mechanism; §12.19 for the cost |

##### Deconstruction reads the recipe backwards — `deconstructsTo` is superseded

§12.13 item 5 specifies *"a `deconstructsTo` field on `ModuleDef` in `modules.json`."* **§12.19 later
gives every item a `Recipe`**, and §2.4's recovery table is stated as a **percentage** — *"Materials
recovered, 20–45% … 80–100%"* — which is a percentage *of the inputs*. Two authored answers to *"what
is this made of"* would drift the moment either was edited, which is the discipline §12.19 applies to
mass and price and §12.30.3 applies to the cargo hold's two lists.

> **Deconstruction yields the item's own `Recipe` inputs at the facility grade's recovery band.**
> `deconstructsTo` is deleted from §12.13 item 5 before it is authored.

⚠️ **§12.19 adds the clause that makes *"the recipe's inputs"* resolve to matter.** A recipe names
**roles**, not elements (`features.md` §2.10), so read backwards it yields roles. The missing
definition is the **nominal fill** — the roster's lowest-density element scoring ≥ 1 in that role,
which needs no authoring and has three consumers. **The rule and the deletion both stand**; what a
deconstruct returns is deliberately *not* the premium stock the item was built from, and recovery is
clamped so it never returns more **mass** than the instance carried.

*This is the cheapest correction in this section — `modules.json` has no `deconstructsTo` today, so
there is no content to migrate. Fold it into §12.19's content pass.*

##### Types

- **`RebuildMountRequest { MountId mount; }`** — new, in `shared/components/Refactor.h` beside
  `DeleteHardpointRequest`. **A `MountId`, not an `entt::entity`**, because the thing being rebuilt has
  no entity — that is what makes it rebuildable. `RigBlueprint.h` already promises `MountId`
  *"survives saves"*, which is exactly the stability this needs.
- **`DeconstructModuleRequest { ItemId item; }`** — §12.13 item 5, kind-tagged per §12.16 item 21.
  ⚠️ **§12.19 makes it `{ ItemInstance item; }`**: a hold may carry two stacks of one id at two
  grades or two rolls, and deconstructing *the good one* by accident is the failure that makes the
  distinction matter. What it yields is the **nominal fill** of that instance's recipe, clamped by
  the instance's own mass.
- **No new component on the rig.** The missing mounts are a **join between the blueprint and
  `Rig::children`**, computed by a pure function for the list — never a stored "missing" tag, which
  would be a second answer to a question content already answers.
- `refactor_menu::DeletableHardpoints` gains its mirror: **`RebuildableMounts(registry, rigRoot,
  blueprint)`** — pure, headless, and the natural home for the orphan refusal.

##### Systems

`RefactorSystem` gains **rebuild** and the `Destroyed` refund branch. `EngineerSystem` gains
**deconstruct**, per §12.13 item 5's *"a second intent, not a new file"* — it is 167 lines against a
600-line cap.

**Both keep their own gate function, and the duplication should be resolved now rather than tripled.**
The comment in `RefactorSystem.cpp:15` justifies the copy — *"a dozen lines each, and the two systems
have no other reason to depend on one another"* — and that was true with two callers. **With this
screen there are four verbs across two systems behind one gate**, and §12.30.4 adds a third system
asking a structurally identical question about a different kind. `DockedFacility(registry, requester,
kind) -> entt::entity` belongs in `shared/rig/` beside `ModuleAttachment`, which both may include.
*This is Law 11's tie-breaker arriving on schedule: the second consumer was arguable, the fourth is
not.*

##### Tests

- Merge is refused at the band ceiling, and refused when the primary is already there — **the
  assertion `features.md` §2.4 claims is already satisfied and is not.**
- A merge at a **Mythic** bench preserves more than one at a Common bench — which requires
  `AttachModuleComponents` to forward the facility's **grade** (§13.3 K, re-aimed by §12.19: neither
  `level` nor `grade` is parsed today, so fixing it once as `grade` is strictly cheaper than fixing
  it as `level` and then deleting it), and fails today for both reasons.
- Rebuilding a mount the blueprint does not author is refused.
- Rebuilding a mount whose `attachedTo` parent is absent or `Destroyed` is refused; rebuilding the
  parent first then the child succeeds.
- **A rebuilt mount carries no modules**, and delete → rebuild → delete does not increase the number
  of modules in the hold — the duplication regression, asserted directly.
- Scrapping a `Destroyed` hardpoint returns **nothing**; scrapping a living one returns its
  `MountedModules` — the two halves of one verb, asserted side by side.
- Rebuild is refused when the requester is not docked at a living Engineering facility, and when the
  parts cannot be afforded.
- The right-hand list shows a mount the rig lost as **absent**, not as missing from the list — §8.3,
  and the only screen that can fail this test.
- Deconstruction yields the item's recipe inputs within the facility grade's band, never above 100%.
- A merged module's id is bounded in length across repeated merges.
- **The request a screen builds lands on the docked requester and is consumed by its system on the
  following tick** — §12.30's shared shape, four times over.

##### Scheduling

Group **4b**, and it splits by verb rather than shipping whole:

| Verb | Needs | When |
|---|---|---|
| **Delete** | Nothing — built. Plus the `Destroyed` refund branch | **4b, and the branch belongs in group 2** |
| **Rebuild** | The blueprint join · the orphan rule · `MountId` request | **4b.** Mechanism needs nothing; only the *cost* waits on §12.19 |
| **Merge** | §12.21 `Quality` for the band · §12.19 for the material cost · §13.3 K's `level` parse | **After §12.21.** It is routable before that, and it is unbounded and free until then |
| **Deconstruct** | §12.19's `Recipe` | After the item model |

⚠️ **Merge is the one verb that is worse routed than unrouted.** Every other screen in this batch is
inert-but-correct today; merge is *reachable-and-wrong* the moment a gate passes — unbounded, free,
and producing ids that grow exponentially into a map §12.31 needs to serialize. **Ship the Engineering
tab with Delete and Rebuild, and hold Merge behind §12.21**, the same call §12.30.3 makes for holding
the Market behind the item model.

##### What is deliberately not here

- **No shell upgrade in place.** `features.md` §2.4 leaves it ❓ open and calls it *"a new mechanic in
  `RefactorSystem` territory rather than a reuse of an existing one."* Rebuild is not it — rebuild
  restores the grade the blueprint authored, never a higher one.
- **No merging of shells.** §2.4 rules it out on design grounds — a merged cockpit has no answer to
  where it sits or what was mounted in it — and §12.16 item 21 confirms `MergeModulesRequest` stays
  module-only while deconstruction widens.
- **No adding a mount the blueprint never had.** That is the whole of what keeps rebuild cheap, and
  the four validation rules it would otherwise need are named above.
- **No re-equipping from this screen.** Live refit is unrestricted (§2.7) and lives in the §3.10
  loadout overlay; a second equip surface behind a facility gate would re-create the gate §12.30
  deleted.

##### Editing the station's own rig, when it is yours

*Raised by the project owner 2026-08-23; decided the same day.* §12.30.4's Repair screen already
establishes the pattern this needs: *"a station with a repair bay repairs itself"* — one subject
section per valid subject, drawn without new retained "which subject is selected" state, one when the
station is not yours, two when it is. **Engineering gains the same second section**: when the docked
station's `FactionRef` matches the player's, its own rig's mounts become editable through this screen
exactly as the docked vessel's are — same two verbs (Delete, Rebuild; Merge and Deconstruct follow the
same gates as above), same two-list layout, the station's `RigBlueprint` in place of the vessel's.
**No new mechanism** — this is `OwnedVesselAt`'s existing pattern applied to the subject the screen
already has a handle on (`DockedStation`), the same way Repair's dual-subject section works today.

##### Shell items — an open question, distinct from the closed one

**Not what §12.12 item 7 already settled.** That decision withdrew a proposed `ComponentDef` third
authoring tier — *"shell and component are two names for the same thing"* — and remains correct and
final: `ShellDef`/`ModuleDef` stay the complete authored set, and nothing here reopens it.

**What is actually open, raised by the project owner 2026-08-23:** whether a shell, once *acquired* as
an item (`features.md` §2.4 already says *"high-grade shells are found through exploration and
salvage, or obtained by dealing with factions that already hold them"* — shells are already
world-obtainable objects), can be **carried in a `CargoHold` and installed at a mount**, replacing
whatever shell was there — a narrower question than `features.md` §2.4's existing ❓ *"shell upgraded
in place, consuming materials"* (deferred there as *"a new mechanic in `RefactorSystem` territory"*):
that open item is a **grade upgrade of the same shell**; this one is **swapping in a physically
different, already-owned shell**. Related, not identical — both live in `RefactorSystem` territory,
neither is decided. See `features.md` §2.4 for the design-level open question and the two concrete
code gaps it would need (`ItemKind` has no `Shell` case; `MountBlueprint` authors exactly one `ShellId`
per mount with no compatibility model for a different one). Rebuild, as specified above, is unaffected
either way — it restores the mount's own authored shell and stays that operation regardless of how
this question resolves.

#### 12.30.6 Screen 6 — Research

*Settled 2026-08-10. Verified against `src/`. **Taken before Screen 5**, because Manufacturing is
§12.19's primary consumer — §11.9 already records that `features.md` §2.10's whole attribute-
propagation chain is computed there and nowhere else — and specifying a UI over an undefined recipe
model is the confusion this pass exists to avoid. Research's item dependency is one already-settled
line (§12.16 item 21), so it can be written now.*

**Gate:** a living `FacilityKind::Research` hardpoint. **One section**, a queue. **This screen is the
missing producer for the deepest unproduced chain in the codebase**, and that is most of what it is
for.

##### 🐛 Four missing links in one chain, and every link looks built

Verified 2026-08-10 by grepping for every occurrence:

| Link | State |
|---|---|
| `ResearchSystem::Tick` | ✅ Built, scheduled, tested. **Advances** jobs |
| `ResearchJob` | 🐛 **Zero producers.** The symbol appears nowhere in `src/` outside `ResearchSystem` and its own header |
| `StationFacility` — the component holding the jobs | 🐛 **Zero producers** (§13.3 O already records this) |
| `StationFacility::researchTier` | 🐛 **Written only by two lines in `ResearchSystemTests.cpp`** |
| `ctx.knowledge` — the grant target | 🐛 `nullptr` (§12.24 step 6) |
| `NetworkOwner` — the component naming *which* network | 🐛 **Zero producers.** Nothing on any entity points at a knowledge network |

**The loop is closed with no entry point.** `Tick` advances jobs; `CollapseResearchJobs` turns jobs
into records; `PromoteResearchJobs` turns records back into jobs. Every path consumes a job that
already exists, and **nothing creates the first one.** There is no `StartResearchRequest` anywhere.

*This is the audit's dominant defect at its deepest — five links, each individually built, tested and
documented, forming a mechanism that cannot be started. It is worth naming as the strongest possible
case for §2.4's rule: **`ResearchSystem`'s tests all pass**, because every one of them constructs the
job by hand.*

**This screen is the entry point**, and specifying it is most of what closes the chain.

##### 🐛 Three more defects in the built half

**1 — There is no facility gate.** `Tick` views `StationFacility` and never looks for a living
`FacilityKind::Research` hardpoint. **Blowing the lab off a station does not stop the research running
inside it.** Same shape as §12.30.4's repair finding, and `features.md` §3.4 makes it pointed: it
cites *"they blew the engineering bay while you were mid-merge"* as the **good** death — the one that
makes docking a real bargain. Research promises the same thing and does not honour it.

**2 — `researchTier` is a *third* tier system.** §12.19 folds `FacilityStats::level` (1–5) into
`Grade` (7 tiers) and warns that *"two tier systems for one concept would drift the moment either was
tuned."* **There are three.** `StationFacility::researchTier` is a bare `float` on a per-station
component, derived from no facility hardpoint, defaulting to `1.0`, and never written outside tests.

> **`researchTier` is deleted.** Duration derives from the **Research hardpoint's `Grade`**, against
> §2.4's settled table — Common 100% of base time down to Mythic 30% — the same hardpoint the tab
> already names and the same grade §12.30.4 reads for repair and §12.30.5 for merging.

**3 — `ResearchJob::cost` is a duration, not a price.** `progress += dt * tier`, complete at
`progress >= cost`. So `cost` is **seconds**, in a codebase where `BuyItemRequest::cost` and
`RepairRequest::costForFullRepair` are **credits**. Rename it `durationSeconds` in `ResearchJob` and
`ResearchRecord` together. Cheap now — two structs and one ledger, no content — and it is exactly the
kind of thing that produces a real bug the first time someone wires pricing past it.

##### 🐛 A null `ctx.knowledge` completes the job and drops the unlock

```
if (it->progress >= it->cost) {
    if (ctx.knowledge != nullptr) { ctx.knowledge->Grant(...); }
    it = facility.researchJobs.erase(it);          // erased either way
}
```

The header calls this *"a no-op if `ctx.knowledge` is null, the same guard `DiscoverySystem` uses."*
**It is not the same guard, because the two operations are not the same kind.** Discovery is
idempotent and re-derivable — skipping it loses nothing permanently. **Research is destructive**: the
job is erased, and once the sample rule below exists, the sample is consumed. A no-op guard on a
destructive path silently spends the input and produces nothing.

> **The job freezes at completion instead of completing.** `progress` clamps at `durationSeconds`, the
> job stays in the queue, and the grant fires on the first tick `ctx.knowledge` is non-null.

*Worth stating as a general rule while it is in front of us: **a null-pointer guard may skip an effect
only if skipping it is free.** `DiscoverySystem`'s is; this one is not; §12.30.4's `ctx.economy` payer
branch is not either — an NPC that repairs without paying because a pointer was null is the same
mistake pointing the other way.*

##### The sample is the screen's real design content, and none of it exists

`features.md` §2.4 settles four things about the sample, and `ResearchJob` implements none of them —
it carries an item id and nothing else:

| §2.4 says | State |
|---|---|
| Research consumes the sample, on a **survival roll** by facility grade — Common 5% … Mythic 90% | 📋 No roll anywhere |
| *"The sample is **locked** for the job's duration and the roll resolves on completion"* | 📋 Nothing removes it from the hold |
| ⚠️ *"**The survival chance must be shown before the player commits.** Feeding your only Mythic into a Common bench has to be a gamble the player took, not a gotcha the game sprang"* | 📋 There is no commit step |
| *"Research cannot fail"* — the sample roll **is** the failure mechanic; the unlock is granted regardless | 📋 — |

**That disclosure rule is the strongest UI requirement in this batch**, because it is a settled rule
with a stated reason rather than a preference. It decides the screen's shape: **the commit step is a
confirmation, not a button.**

> **Committing a sample shows, before the click: the survival chance, the duration, and which network
> the unlock lands in.** Three numbers, all derived, none authored.

**The lifecycle, with no new type:**

1. **Commit** — the item leaves the `CargoHold` and lives in the `ResearchJob`. The job *is* where the
   sample is held, so it demotes with the job (`ResearchRecord` already carries `item`) and comes back
   with it. No "locked" flag, no second home.
2. **Complete** — the unlock is granted unconditionally (§2.4: research cannot fail). Then the survival
   roll: on success the sample returns to the hold, on failure it is gone.
3. **Cancel** — the sample returns whole. **Nothing was consumed, so nothing is refunded** — what you
   forfeit is the progress, and that is the honest cost, because §2.4 prices research in *time and the
   sample* and nothing else.

⚠️ **The roll is deterministic from `(item id, tick)`** — §2.4's rule, the FNV-1a idiom `MiningSystem`
and `CommsSystem` already share. §12.19 already names this batch as the **third consumer** that
promotes it to `shared/math/`; this is one of the three.

##### Already-known items are refused before the click, not after

§2.4: *"a surviving sample is not reusable **for research** — the unlock is permanent after one
success."* So researching something the target network already holds is pure loss. **The row is
disabled with `ALREADY KNOWN`** (§3.10's degrade-never-remove) rather than being hidden — hiding it
would make the player's own progress invisible, which is §8.3's rule.

**This requires the screen to read the network**, which requires `NetworkOwner` to have a producer.
`KnowledgeStore::Create` exists and returns an id; **nothing holds one.** The player's network is
emplaced at §12.24 step 1 alongside `CargoHold` and `Wallet` (§13.3 P), and a faction's on its
stations — the same gap, in the same factory, as three other components.

##### No credit fee, and that is a decision

`features.md` §2.4 prices research in **time and the sample**, deliberately, and warns in the same
section against stacking brakes: *"Four brakes on one action is the pattern that got upkeep cut."*
Adding a docking fee for using someone's lab would be a third.

**So research at a foreign lab is free, and the reasons to build your own are grade and security** —
a better bench is faster and returns your sample more often, and a station you own is not one you will
find destroyed. Which lands on the risk the ownership model already gives for nothing:

> **A job runs on the host's `StationFacility`, so it dies with the host.** Research at a foreign lab
> is a real decision with a real exposure, and §3.4 already built the exposure. No new mechanism.

##### Duration derives; it is not authored per item

§2.4's table gives the facility grade's **percentage** of research time and never says what it is a
percentage *of*. Filling that with the discipline §12.19 already applies to mass and price:

```
duration(item, facility) = baseDuration(item.grade) * gradeTimeFactor(facility.grade)
```

**One ladder, no per-item authored time.** A Legendary module takes longer than a Common one because
its grade says so, and adding a module never requires guessing a number — the same argument §12.19
makes for its authored element masses. ~~*Flagged as a fill rather than a quote: §2.4 settles the
right-hand factor and is silent on the left.*~~

> ✅ **Confirmed as a quote 2026-08-10 (§12.19).** §2.4 is silent on the left-hand factor;
> **`features.md` §2.8's time curve authors it** — 60s base for a research job, doubling per grade —
> and the two agree to the digit at four points: Common 1m, Mythic 64m, Mythic at a Mythic lab ~19m,
> and (on the manufacturing side, 10s base) a Mythic module in ~192s. Not a fill.

##### `FacilityStats::capacity` gets its second reader, and it is the same meaning

§12.30.2 gave `capacity` its first reader — docking-bay occupancy. §12.30.3 deleted its *"storage
slots"* clause as dead. **Concurrent research jobs is its second reader, and it is the same concept,
not a third meaning:**

> **`FacilityStats::capacity` is how many units of work a facility holds at once.** Vessels for a bay,
> jobs for a lab. `0` remains unlimited.

*That is the confirmation §12.30.3's edit was right: "storage slots" was the odd one out precisely
because a hold's capacity is the hold's own field, while everything else `capacity` names is
concurrent occupancy.* `StationFacility::researchJobs` is a `std::vector` with no cap today — the
header says *"a station running two jobs at once holds both in this one vector"* and nothing stops it
holding two hundred.

##### Layout

| Section | Contents |
|---|---|
| **Header** | Lab name · **grade** · this Research hardpoint's integrity `Gauge` (§3.4) · **slots `2 / 3`** |
| **Sibling selector** | `TabStrip`, one per living Research hardpoint (§12.30.5's generalised rule) |
| **Left `ListView`** | Your `CargoHold` — candidate samples |
| **Right `ListView`** | The queue — one row per running job |
| **Footer** | The commit confirmation, or **Cancel** for a queued row |

Left-hand rows: **`glyph`** the item's monogram, **`label`** its name, **`value`** the survival chance
and duration *at this bench* — so the §2.4 disclosure is present in the list itself, before any
selection. **`RowStyle`** disabled with `ALREADY KNOWN`, or with `NO SLOTS` when the queue is full.

Right-hand rows: the job, its remaining time, and its progress.

##### The one widget-layer addition four screens did not need

The queue needs **per-row progress**, and `Row` is `{ label, value, glyph, style }` with nowhere to
put it. The header `Gauge` shows one thing; a queue of three jobs showing one progress bar is not a
queue.

> **`Row` gains `float fill = -1.0f`** — negative meaning none — and `ListView` draws a fill behind
> the row when it is set.

**Two consumers exist in this batch**: this queue and Manufacturing's (§12.18). That is Law 11's
tie-breaker satisfied, and it is worth recording that **the five widgets of §12.30 covered four
screens with nothing left over and needed exactly one field on the fifth** — which is the evidence
that the set was scoped correctly, and the reason the sixth screen should be checked against it rather
than assumed.

##### Types

- **`StartResearchRequest { ItemId item; KnowledgeNetworkId targetNetwork; }`** — new, in
  `shared/components/Research.h`. **The missing producer**, and the whole reason the chain above is
  dead.
- ⚠️ **Two amendments from §12.30.8**, both found by specifying the sibling screen. **`item` becomes
  an `ItemRef`** — an unlock is keyed on `(ItemId, Grade)`, or one Common sample teaches you to build
  Mythics and §2.7's drop ladder stops meaning anything — and *"at most one job per item"* widens to
  **per `ItemRef`** with it. **`ResearchJob` and `ResearchRecord` gain a `MountId facility`**, because
  `StationFacility` is per *station*: as written, this section's grade-derived duration, its facility
  gate and its `capacity` slot limit are all unevaluable on a station with two labs, and §12.30.5's
  sibling selector puts exactly that station on the screen.
- **`CancelResearchRequest { ItemId item; }`** — named by the item, which works because of one new
  rule: **a station runs at most one job per item.** A second job on the same item is refused. That
  gives cancel a key with no new id type, and it stops a player queuing the same research four times
  for no gain.
- **`ResearchJob`** — `cost` → **`durationSeconds`**; `item` becomes an **`ItemId`** carrying a
  `NetworkEntryKind` (§12.16 item 21, so shells are researchable). `ResearchRecord` follows both.
- **`StationFacility::researchTier`** — **deleted.**
- **`NetworkOwner`** — gains its first producer (§12.24 step 1, and `StationFactory`).
- **No new component.** The queue is `StationFacility`, the gate is `FacilityRef`, the slots are
  `FacilityStats::capacity`, the sample is the job's own `item`.

##### Systems

`ResearchSystem` gains: the **facility gate**, the two request consumers, the **capacity check**, the
**survival roll**, the **grade-derived duration**, and the **freeze-instead-of-drop** guard. It loses
`researchTier`. **No new system**, and no new file — the fourth screen in a row where that holds.

⚠️ **`CollapseResearchJobs` and `PromoteResearchJobs` have no callers either.** `SpaceFlight::
WarpToSystem` carries `CargoHold`, `Wallet` and `wreckLedger_` across a jump and nothing else
(§12.31), so a job on a station in the departing system is destroyed with the world rather than
demoted. **The demotion path is built and correct and is never invoked** — it joins the parked-hull
and crafted-module cases as a fourth thing §12.31's work has to actually call.

##### Tests

- A `StartResearchRequest` at a station with **no living `Research` hardpoint** is refused; one at a
  station with a living lab creates exactly one job.
- Destroying the Research hardpoint mid-job stops progress — the assertion that fails today, in the
  form §3.4 promises.
- A second job on an item already queued at that station is refused; the same item at a *different*
  station is allowed.
- The queue refuses a job beyond `FacilityStats::capacity`; `capacity: 0` never blocks.
- Duration scales with the **facility hardpoint's grade**, not with a per-station float — and
  `researchTier` no longer exists to be set.
- On completion the unlock is granted **exactly once**; the sample returns on a passing roll and is
  gone on a failing one; the roll is identical for the same `(item, tick)` on every platform.
- **With `ctx.knowledge == nullptr` the job does not complete and is not erased**, and grants on the
  first tick the pointer is supplied — the regression test for the drop above.
- Cancelling returns the sample whole and forfeits the progress.
- A row for an item the target network already holds is disabled, not hidden.
- A demote → promote cycle resumes at caught-up progress **and carries the sample** — the existing
  §12.1 test, now with something in the job that can be lost.
- **The request a screen builds lands on the docked requester and is consumed by its system on the
  following tick** — §12.30's shared shape.

##### Scheduling

Group **4b**, and it is the one screen where §12.24 **step 6 is a real prerequisite** rather than a
degradation: with `ctx.knowledge` null there is no network to grant into and none to check
`ALREADY KNOWN` against. The freeze fix makes that safe rather than destructive, but the screen is not
meaningfully playable without it.

| Needs | Status |
|---|---|
| 4a's widgets **plus `Row::fill`** · the router · `PlayerLocation` | Group 4b |
| **§12.24 step 6 — `ctx.knowledge`** | Group 3, independently startable today |
| `NetworkOwner`'s producer · `StationFacility`'s producer | With §13.3 O/P's factory pass — the same factories, the same commit |
| A `Research` facility authored in `modules.json` | Content |
| §12.19 for `ItemId` and the grade ladder | Only for **shells** as samples and for derived duration. Modules work with `ModuleId` today |

**The three defects in the built half belong in group 2**, like §12.30.4's: the facility gate, the
`researchTier` deletion, and the `cost` → `durationSeconds` rename are each a few lines against built
code, and the freeze fix is one more.

##### What is deliberately not here

- **No research failure roll.** §2.4 settles this at length — the sample roll *is* the failure
  mechanic, and a second probability would give four outcomes, two of which read identically.
- **No quality-capped unlock at a low-grade bench.** §2.4 considered it, called it *"genuinely
  interesting"*, and set it aside because it needs a per-unlock quality cap nothing reads — §2.4's own
  dead-abstraction rule applied to itself.
- **No research queue reordering or priority.** Jobs are concurrent up to `capacity`, not sequential,
  so there is no order to change.
- **No espionage or network theft.** §5.10's defection path copies a network with
  `KnowledgeStore::Copy` and belongs to that feature, not to a bench.

##### The Codex — browsing what is already unlocked

*Raised by the project owner 2026-08-23; decided the same day.* Distinct from the queue above, which
is what to research *next*; the Codex is a read-only browse of everything the player's `NetworkOwner`
already grants, reached by a button on this screen (not a new tab — it has no facility gate of its
own, the same shape §12.30.7's overlays have, but opened from Research rather than available
everywhere). **Three sections, by item kind** — Modules, Shells, Materials — each row tagged
by faction and grade/tier, with faction and grade filter chips and a search field over the combined
set. Purely a read of existing state (`KnowledgeNetwork`, the same source `ALREADY KNOWN` already
checks against) plus the `ContentLibrary` defs each unlocked id resolves to for its display fields —
no new component, no new request type, no system it needs to call. **Deliberately not a tech-tree
view**: `features.md` §9's tech-tree structure is still 📋 *"agreed in principle, not yet specified"* —
there is no shape to draw yet, so the Codex stays a flat, filterable list rather than a graph.

#### 12.30.7 The two flight overlays — inventory and loadout

*Settled 2026-08-10. §12.30 moved `StorageMenu` and `ModulesMenu` out of the docked router and into
`features.md` §3.10's overlay set, and warned they **"do not become trivial by moving."** They do not:
the loadout overlay's two pure functions are **both inverted**, and following that through finds a
module-duplication bug and a module-destruction bug reachable from the same click. Verified against
`src/` 2026-08-10.*

**Gate:** none. Neither is facility-gated — that is the whole point of the supersession, and
`features.md` §2.7's **live refit is unrestricted** is the rule behind it. **Keyed on a HUD button or
its §3.6 key**, per §3.10.

##### Where they live, now that a docked screen is full screen

§12.30's frame decision creates a question neither surface had: §3.10 describes overlays as
*"semi-transparent and offset from centre"* over the viewport, and a docked screen has no viewport.

> **An overlay is defined by being over something, not by what it is over.** Both open in flight over
> the world, and both open while docked over the docked screen. Same widgets, same rows, same
> selection state — only the background differs.

Two reasons this is not a special case:

- **The loadout overlay is the *only* refit surface.** §12.30 removed `ModulesMenu` from the router,
  so if it did not open while docked, refitting at a station — the least controversial place to refit
  — would be impossible.
- **The inventory overlay is not redundant with the Market.** A station with neither a `CargoHold` nor
  a `Trade` hardpoint has no Storage tab at all (§12.30.3's fourth ownership case), and the player
  still needs to read their own manifest there.

**Neither pauses** (§3.4), and opening one in flight is the real tactical cost §3.10 already prices.

---

##### The inventory overlay

**It is your own `CargoHold`, everywhere.** `storage_menu::Rows(const CargoHold&) ->
std::vector<std::string>` becomes `ListView` over `Row`, per §12.30 — which is most of the change, and
§12.30.3's one-`ItemStack`-list unification is the rest.

###### It owns exactly one verb, and that verb has a job

**Everything else you might do to an item belongs to a screen that already exists**: buy and sell to
the Market, deposit and withdraw to Storage, merge and deconstruct to Engineering, equip to the
loadout overlay beside it, research to the Research lab. `StorageMenu`'s own header says it *"only
reads `CargoHold`, it never mutates it."* **That remains true but for one addition, and the addition
is not a convenience.**

🐛 **A full hold has no exit, and §12.30.3 is what closes the trap.** Once `CargoHoldHasRoomFor` gains
its other four callers, a full hold refuses loot, refuses a `RefactorSystem` refund, and refuses a
buy. And **every operation that could make room also fills it**: unmounting a module pushes to the
hold, scrapping a hardpoint pushes to the hold, deconstruction pushes to the hold. The only drain in
the entire game is **selling, at a station with a living `Trade` hardpoint.**

> **Jettison.** One verb on the inventory overlay: drop the selected stack into space as a
> `LootDrop` / `MaterialDrop` at the rig's position.

It is worth more than the corner it closes. **§13.3 T records that `LootDrop` and `MaterialDrop` have
zero producers** — `LootSystem` collects drops nothing creates. **Jettison is that producer**, and it
is the shape §13.5's deferral warning points *toward*: a UI-emitted intent consumed by a built system,
not a producer invented inside the system.

*It also makes the hold's capacity a real constraint rather than a wall. A cap you cannot act against
is a bug report; a cap you resolve by choosing what to throw away is the constraints puzzle §2.2 is
built on.*

###### Layout

| Section | Contents |
|---|---|
| **Header** | Capacity `34 / 50` · total mass — the number §2.2's puzzle is actually about |
| **List** | `ListView` over the hold, grouped by `ItemKind` then Material family (§12.30.3) |
| **Footer** | **Jettison**, with a quantity, and nothing else |

Rows are §12.30.3's exactly — periodic abbreviation or monogram in `glyph`, quantity in `value`. **The
two screens share the row builder**, since it is the same hold rendered the same way; the Market
simply draws a second one beside it.

⚠️ **Mass in the header is not decoration.** §2.2's constraints puzzle is mass against thrust, §12.23's
`RecomputeRigTotals` makes cargo mass affect handling, and `features.md` §2.10 authors a mass per
element for exactly this. Without it the player carries fifty units of tungsten and discovers the cost
by flying badly.

###### Types and systems

⚠️ **`JettisonRequest`'s payload widens with the rest** (§12.19): `{ ItemInstance item; int
quantity; }`, and `LootDrop`/`MaterialDrop`/`DeathWreck` all become `ItemStack` in the same pass, so
a jettisoned Mythic roll is the same object when it is picked back up. The loadout overlay's
`MountedModules::ids` becomes `::items` of `ItemInstance` for the same reason — a mounted module has
a quality and a composition, and unmounting must return *that* module rather than its def.

**`JettisonRequest { ItemId item; int quantity; }`** in `shared/components/Loot.h`, consumed by
`LootSystem` — which already owns both drop types and their lifetimes. **No new system, no new
component**, and one dead component pair gains a producer.

---

##### The loadout overlay

###### 🐛 Both of its pure functions are inverted, and the bug is real

`modules_menu::EquippableMounts` returns every hardpoint that *"carries `ShellRole`, does not already
carry `EquippedModule`."* `EquippedMounts` returns those that do. And `Equip.h` states the premise
that breaks both:

> *"`EquippedModule` is only present on a hardpoint while `ModuleEquipSystem` is the one that put a
> module there — a rig's original, `RigFactory`-instantiated loadout is **not** retroactively tagged."*

**So on a freshly spawned ship, every occupied hardpoint reports as an empty slot, and the list of
things you can unmount is empty.** The overlay's two lists are exactly backwards, and each is wrong in
the direction that causes damage.

**Following the first one through, against `ModuleEquipSystem`:**

1. `ProcessMountRequests` refuses an occupied mount by testing `all_of<EquippedModule>`
   (`ModuleEquipSystem.cpp:33`) — *"Already occupied — unmount first."* **A blueprint-mounted
   hardpoint has no `EquippedModule`, so it passes.**
2. `AttachModuleComponents` runs `emplace_or_replace<Weapon>` — **silently overwriting the original
   module's live components** — while `MountedModules.ids` still names the original.
3. The hardpoint now holds three disagreeing answers: `MountedModules` = the original, `EquippedModule`
   = the new one, live components = the new one.

**Two different bugs fall out of that state, from one click each:**

| Action | Result |
|---|---|
| **Unmount** | Returns the **new** module to the hold and `DetachModuleComponents` **removes** `Weapon` outright. The original is now gone from the ship *and* absent from the hold — **destroyed** |
| **Scrap the hardpoint** (§12.30.5) | `RefactorSystem` refunds `MountedModules` — the **original**. The player receives a module they no longer had and loses the one they just fitted — **duplicated** |

*This is finding §13.3 C's abstract warning made concrete, and it is the argument §13.4 decision 2 has
been waiting for.* That decision — *"keep `MountedModules`, delete `EquippedModule`"* — currently reads
as tidiness. **It is a dupe-and-destroy fix**, and this overlay is the surface that reaches it.

> **After decision 2 there is one list.** `EquippableMounts` becomes *"`MountedModules` is empty"*,
> `EquippedMounts` becomes *"it is not"*, and both are correct for blueprint-mounted and
> runtime-mounted modules alike, because there is only one kind.

###### 🐛 Mounting does not check `Destroyed` — and that is the third system that does not

`ProcessMountRequests` validates ownership, occupancy, `ShellRole`, mountability and possession, and
**never tests `Destroyed`.** You can fit a module to a hardpoint that was shot off; it is consumed
from the hold, its components are attached, and every system skips the mount because of the tag.

*Three systems now write to hardpoints without checking the tag* — `StationServicesSystem`'s repair
(§12.30.4), `RefactorSystem`'s refund (§12.30.5), and this. **`DockingSystem` and `DamageSystem` are
the only two that do.** It should be swept once, as a group-2 pass over every hardpoint writer, rather
than three times in three screens' worth of issues.

###### Two more things live refit needs, both already recorded

| Gap | Recorded as |
|---|---|
| `AttachModuleComponents(registry, mount, *module, **0.0f**)` — every runtime-mounted weapon gets a **zero-width firing arc** and never fires | §13.3 D · `MountTraverse`, §13.5 group 2 |
| Nothing recomputes `BodyMass` or `Propulsion` on mount or unmount, so **a swap does not change how the hull flies** | §12.23's `RecomputeRigTotals` · §11.9 |

**§11.9 already makes the second one a hard dependency of shipping this overlay**, and the reason is
worth repeating because it is the whole mechanic: *"live refit is now sanctioned combat play, so a
swap that does not change how the hull flies is the mechanic broken."*

⚠️ `IsMountable(module->kind, shellRole->kind)` is the hardcoded table §12.22 replaces with
`ShellDef::acceptsKinds`. The overlay greys an illegal target either way; the only question is whether
the rule is content or code, and §12.22 settled it as content.

###### Drag the module to the mount — a reversal, kept below for the reasoning trail

§12.10's promotion note names legacy StarReach2's *"slot-rendering and trash-can widgets"*, which were
a drag-and-drop grid. **Not carried forward at launch**, and the reason is §12.30's own widget
decision: a drag is **retained state spanning frames** — what is held, where the cursor took it, what
it is over — and *"no retained tree, no widget-owned state"* is the one thing that decision forbids.

> **Click the module, then click the mount.** Two clicks, both stateless; the pending selection is one
> more field on the UI-state singleton §12.24 already settled, exactly like a selected row index.

⚠️ **Corrected 2026-08-23 — the combat-friction argument was inverted.** An earlier version of this
section cited §4.4's no-pause rule as a *reason to reject* drag-and-drop, reasoning that sustained
precise input during combat is bad UX. **That reads backwards.** Live refit is sanctioned combat play
(§2.7) precisely so that skill and speed matter under fire, and a drag that is genuinely risky to
pull off mid-fight is exactly the kind of friction that *should* gate it — the intent is that
switching loadouts mid-fight demands something from the player, not that it happens for free. §4.4
plays no part in why click-then-target ships first; **the widget-layer's statelessness rule above is
the entire reason**, and it is the only one.

⚠️ **Settled 2026-08-23, later the same day — not left open after all.** The project owner wants
drag-and-drop for this screen, and it **replaces click-then-target entirely** rather than sitting
beside it as an alternative. The "sanctioned exception" this section asked for turns out to be
smaller than it looked: the retained drag state (which module is lifted, where it started) is
**screen-owned state on the same `FlightOverlayState` singleton `pendingModule` already lives on**
— not widget-owned state. `ListView` stays exactly as pure as it is today; it is only ever asked
"what row is under this point," the same question it already answers for click-then-target. The
widget-layer rule this section opened with is not being broken, and never needed to be — it was
never a rule against a *screen* holding a pending drag, only against `ListView`/`Button`/etc.
holding one themselves.

> **Mouse down on a held module, drag it over the mount list, release on a target.** A release over
> a compatible, unoccupied mount emits `MountModuleRequest`; a release over an *occupied* mount,
> dragged back out and released over the hold list, is the unmount gesture — it emits
> `UnmountModuleRequest`. A release anywhere else — an incompatible mount, empty space, back where
> it started — cancels with no request emitted, exactly as a mis-click refuses today.
> `FlightOverlayState` gains `draggedModule` and `draggedFromMount` (mutually exclusive — whichever
> list the drag started in) in place of `pendingModule`: one screen-state swap, not a new category
> of state.

This still needs a **ghost** — something drawn at the cursor while a drag is live, since the module
being carried has to stay visible mid-drag. That is the one genuinely new draw call this adds: a
`Row`-shaped label following the cursor, styled exactly like the row it was picked up from.
`ListView` and `Button` gain nothing.

###### Layout, and the schematic it becomes

| Section | Contents |
|---|---|
| **Header** | Rig name · aggregate integrity · **mass and power**, both live against the pending swap |
| **Left `ListView`** | Your hold, filtered to modules mountable *somewhere* on this rig |
| **Right `ListView`** | Every hardpoint on the rig — living, empty, and `Destroyed` |
| **Footer** | Removed — the drop itself performs the mount/unmount, there is no separate confirm step |

The right-hand rows are three states again, and §3.10's degrade-never-remove decides all three:
occupied (its module, integrity gradient), **empty** (outline, `EMPTY`), and **destroyed** (disabled,
`DESTROYED — REBUILD AT ENGINEERING`, §12.30.5). A `Destroyed` mount also refuses as a drop target,
same as it refused a click.

⚠️ **The header numbers change while the drag is live and hovering a valid target, before the
drop.** §2.2's puzzle is mass against thrust and draw against generation, and a refit screen that
shows the consequence only afterwards is asking the player to guess at the one decision the game is
built around. This costs nothing — `RecomputeRigTotals` is the same function, run against a
hypothetical.

> **When §3.9's status projection lands (§13.5 group 2e), it becomes this overlay's second selector.**
> §3.9 promises *"the same object for the player's own ship, for the current target, and — degraded —
> for a map marker. Not three designs."* **A loadout screen is the fourth use and the most obviously
> spatial one**: which mount, and where on the hull. The hit test is the same pure function
> `ListView` uses — `HardpointAtPoint(projection, cursor) -> index` — now answering "what am I
> hovering while dragging" instead of "what did I click," over the **same drag state**, so the list
> does not go away and nothing is re-modelled.

**Ship the list in 4b; the projection is an addition, not a prerequisite.** Recorded here so 2e knows
it has a consumer waiting, and so nobody builds a second hardpoint-selection model when it lands.

###### Types and systems

- **No new component.** `MountModuleRequest` and `UnmountModuleRequest` are built and correct; what is
  wrong is the predicate that decides which mounts to offer.
- **`FlightOverlayState.pendingModule` is replaced by `draggedModule`/`draggedFromMount`** — screen-
  owned singleton state, same as the field it replaces; no widget-layer change (see the drag note
  above).
- **`EquippedModule` is deleted** (§13.4 decision 2), and `EquippableMounts`/`EquippedMounts` are
  rewritten against `MountedModules`.
- `ModuleEquipSystem` gains the **`Destroyed` refusal**, a real **`MountTraverse`** (§13.3 D), and a
  **`RecomputeRigTotals`** call on both paths (§12.23). **No new system.**

---

##### Tests

**Inventory:**

- Jettison removes exactly the requested quantity and spawns a matching `LootDrop`/`MaterialDrop` at
  the rig's position; jettisoning more than is held is refused whole.
- A jettisoned stack is collectable again by `LootSystem` — the round trip, which is the first time
  either drop type has had one.
- A full hold that refuses a `RefactorSystem` refund accepts it after a jettison — **the trap, asserted
  end to end.**
- The header's mass equals the sum of the hold's item masses (once §12.19 authors them).

**Loadout:**

- On a **freshly spawned** rig, the mount list shows its blueprint modules as *occupied* and its empty
  hardpoints as *empty* — **the assertion that fails today in both directions.**
- Mounting onto an occupied hardpoint is refused whether the module got there from the blueprint or
  from a previous mount.
- Mount → unmount → scrap the hardpoint yields **exactly one** of each module involved, and never a
  module the player did not have — the duplication and destruction regressions, asserted together.
- Mounting onto a `Destroyed` hardpoint is refused and consumes nothing.
- A runtime-mounted weapon **fires** — the `MountTraverse` regression (§13.3 D).
- Mounting a heavier engine changes `BodyMass` and `Propulsion` the same tick (§12.23), and the
  header's pending numbers match what the rig has after the drop.
- A drag released outside any mount, or back where it started, cancels: no request is emitted and
  the module is still in the hold.
- An illegal module/shell pair is greyed in the list and refused by the system — the UI is not
  authority.

##### Scheduling

**Group 4a and 4b, and they are the last two surfaces in the batch.** Neither needs a facility, a
station, or a docked state.

| | Needs | When |
|---|---|---|
| **Inventory** | 4a's widgets · §12.30.3's `ItemStack` unification for grouping · `JettisonRequest` | **4b.** Jettison alone could land with group 2 — it is one intent and one existing system |
| **Loadout** | **§13.4 decision 2** · §13.3 D's `MountTraverse` · §12.23's `RecomputeRigTotals` · the `Destroyed` refusal | **After decision 2**, which is now a dupe-and-destroy fix rather than a cleanup |

⚠️ **The loadout overlay joins Engineering's merge as *worse routed than unrouted*.** Every other
surface in this batch is inert-but-correct; these two are reachable-and-wrong. **Do not wire the
loadout button before decision 2 lands** — the first player to refit a fresh ship destroys a module,
and the failure looks like a UI bug.

##### What is deliberately not here

- **No loadout presets or saved fits.** That is `CustomizeMenu`'s Template (§12.9) — a *design*, not a
  live rig, and §12.31 is emphatic about not letting the two types converge.
- **No repair or rebuild from the loadout list.** They are §12.30.4 and §12.30.5, both facility-gated;
  this overlay is not, and putting a gated verb on an ungated surface is how the gate stops meaning
  anything.
- **No cargo transfer between vessels.** Deposit and withdraw are §12.30.3's, against a station's hold
  and its ownership rule.
#### 12.30.8 Screen 5 — Manufacturing

*Settled 2026-08-10, after §12.19. **Taken last of the seven**, and deliberately: §11.9 records that
`features.md` §2.10's whole attribute-propagation chain is computed in `ManufacturingSystem` and
nowhere else, so this screen is a UI over the item model rather than over a facility, and specifying
it before §12.19 existed would have been writing a parts list against an undefined idea of a part.
Verified against `src/` by grepping for readers and callers. **It amends §12.30.6 in two places** and
completes §12.30's supersession of step 5a's drafting facility.*

**Gate:** a living `FacilityKind::Manufacturing` hardpoint **and** the design present in the actor's
knowledge network — `features.md` §2.8's *"gated by facility and by knowledge, both."* **Two
sections, Queue and Draft**, and they are not two views of one thing: one consumes a network category
and the other produces a different one.

##### The screen is the missing producer for two chains, and they are different chains

| Section | Produces | Read by | State |
|---|---|---|---|
| **Queue** | `ManufacturingJob` | `ManufacturingSystem` | Neither exists. §12.18 specified the system; **nothing is built** |
| **Draft** | A Template — a `ShipBlueprint` in a network | `ConstructionSystem` | 🐛 **Built at both ends and severed in the middle** |

##### 🐛 A saved Template can never be built, and the body is discarded one line from being kept

*Verified 2026-08-10 by following the chain end to end. This is the same five-link shape §12.30.6
found in research, with one difference that makes it worse: **the missing datum is already in the
intent.***

| Link | State |
|---|---|
| `customize_menu::NewDraft` / `AddMount` / `EquipModule` assemble a draft `ShipBlueprint` in local UI state | ✅ Built |
| `CanSave` runs `Validation.h`'s full rule set and returns a `ValidationResult` | ✅ Built |
| `BuildSaveRequest` builds a `SaveTemplateIntent` **carrying the whole blueprint** — `intent.blueprint = draft` | ✅ Built |
| `ConsumeSaveTemplateRequests` re-validates, then calls `Grant(network, SavedTemplate, request.blueprint.id.str())` | 🐛 **Grants the id and drops the body** |
| `KnowledgeNetwork::savedTemplates` is `std::unordered_set<std::string>`, and its comment says why: *"never a blueprint body (Law 10 — the content library already owns those)"* | 🐛 **The content library does not own this one** |
| `ContentLibrary::FindShip` searches `ships_`, the JSON-loaded set. **There is no runtime overlay for ships** — the overlay that exists is `craftedModules_`, and it is for modules | 🐛 |
| `ConstructionSystem`'s `BuildStationRequest` / `PlaceShipRequest` resolve through `rig_factory::Spawn(ctx.world, ctx.content, params)` → `FindShip` → **`nullptr`**, and `Spawn` returns not-ok | 🐛 |

> **A player designs a ship, it validates, it is saved, it appears in their network — and it cannot
> be built, ever, with nothing logged anywhere.**

**And §12.19's retirement is the fix, because the overlay is on the wrong type.** §12.19 deletes
`RegisterCraftedModule` on the ground that a rolled module instance is a **value** that travels in a
hold, not a definition. **A drafted Template is a definition** — a body that has to be resolvable by
id from anywhere, which is precisely what an overlay is for.

> **`ContentLibrary::RegisterCraftedModule` becomes `RegisterDraftedTemplate(ShipBlueprint)`.** Same
> map, same *"runtime wins"* tie-break, same survives-`LoadFromDirectory` rule, and the same one save
> section §12.31 already scoped for the module version — pointed at the type that needs it.

*Law 3 decides which is which and needs no judgement call: a merge produces a **live form**, drafting
produces a **blueprint form**, and an overlay on `ContentLibrary` holds blueprint forms. The two were
swapped.*

##### 🐛 `ConstructionSystem` has no knowledge gate at all

*Verified 2026-08-10.* `ProcessStationRequests` and `ProcessShipRequests` read a `Wallet`, compare
`wallet->credits < request.cost`, and spawn. **There is no `ctx.knowledge`, no network membership
check, and no facility check** — §12.26 gates *build mode*, not the request, and carries no knowledge
rule either.

So `features.md` §2.8's *"a faction that bought your Template can manufacture it forever precisely
because the design sits in their network — that is the same gate, applied to them"* is enforced
against nobody. **Any requester can build any blueprint in the library, for a price they supplied
themselves** — which is `BuyItemRequest::cost`'s Law 9 defect (§12.30.3) in a second system, and
§12.7's royalty model has nothing to hang on, since building someone's design is not an event anyone
can observe.

**Both belong in one edit:** `ConstructionSystem` reads `ctx.knowledge` and refuses a blueprint the
requester's network does not hold, and `cost` is **deleted** from both requests rather than filled in,
priced by `Pricing.h` from the blueprint's parts (§12.19) exactly as the other four requests now are.

##### 🐛 An unlock has no grade, and that decides whether the rarity ladder means anything

`KnowledgeNetwork::unlockedBlueprints` is a `std::unordered_set<std::string>`, and `ResearchJob::item`
is a bare `ModuleId`. **An unlock is therefore keyed on a design, not on a design at a grade.**

If finding one Common cannon unlocks *the cannon*, a player manufactures **Mythic** cannons off it,
gated only by cost. That deletes `features.md` §2.7's drop ladder outright, and it contradicts the
stated reason that ladder is safe to make steep: *"**One recovered Mythic** can be reverse-engineered
into something manufacturable forever, so a drop rate gates first acquisition."* First acquisition of
**that Mythic** — not of the design, which the Common already gave you.

> **An unlock is keyed on `(ItemId, Grade)`. You manufacture the grade you researched, and the grade
> you researched is the grade you found.**

**This is not the facility-tier cap §2.4 rejected.** That cap said *"you may not research this at this
bench"* — a gate on the action. This is the drop ladder doing the job §2.7 already assigns it, and
research stays uncapped in exactly the way §2.4 means: any grade you can find, you can research, at
any bench, and the bench only changes the odds and the clock.

*Three small consequences: `NetworkEntryKind::UnlockedBlueprint` entries carry a grade;
§12.30.6's `StartResearchRequest { ItemId item; }` becomes `{ ItemRef item; … }`; and that section's
"a station runs at most one job per item" widens to **per `ItemRef`**, so two grades of one cannon are
two legitimate concurrent jobs.*

##### 🐛 A job cannot name its own bench — and §12.30.6 settled three rules that require it

*This is an amendment to §12.30.6, found by specifying its sibling. `StationFacility` is **per
station**: `{ std::vector<ResearchJob> researchJobs; float researchTier; }`.*

Three rules settled the same day evaluate against a hardpoint that the job does not identify:

| Rule | Needs |
|---|---|
| §12.30.6 — duration derives from *"the **Research hardpoint's** `Grade`"* | Which hardpoint |
| §12.30.6 — the queue stops when the lab is destroyed (§3.4's bargain) | Which hardpoint |
| §12.30.6 — slots are `FacilityStats::capacity`, **a per-module field** | Which hardpoint |
| §12.30.5 — the sibling selector generalises: *n* benches of different grade behind one tab | Which hardpoint |

**On a station with two labs of different grade, all four are unevaluable**, and this screen doubles
the problem by adding a second queue on the same component.

> **`ResearchJob` and `ManufacturingJob` each gain a `MountId facility`.** Not an `entt::entity` —
> Law 2, and both demote into `core/galaxy/` records that outlive the registry. `RigBlueprint.h`
> already promises `MountId` *"survives saves"*, `SystemWorld.h:42` already addresses a hardpoint as
> `(rig NetworkId, MountId)`, and `rig_factory::FindHardpoint(registry, root, mount)` is built.

The gate becomes *"is the mount with this id alive"*, the grade and the slot count are read off that
hardpoint, and **destroying one bench stops only its own jobs** — which is the version §3.4 actually
promises.

##### Why Draft is a section here rather than a facility — the argument, corrected

§12.30 supersedes step 5a's drafting facility on the ground that *"`ManufacturingSystem` is the only
system that reads designs out of a knowledge network."* ⚠️ **That premise is wrong** — building a
vessel reads a design out of a network too, or would if `ConstructionSystem` had the gate above.
**The conclusion is right and its real reason is the other one §12.30 gives:**

> **A seventh `FacilityKind`, authored by no content and gating one screen, is §2.4's dead
> abstraction in its purest form** — and a design and the things a design is made of belong on one
> surface, because the parts list is the same list.

##### Layout

| Section | Contents |
|---|---|
| **Header** | Facility name · **grade** · this Manufacturing hardpoint's integrity `Gauge` (§3.4) · **slots `1 / 3`** · your credits |
| **Sibling selector** | `TabStrip`, one per living Manufacturing hardpoint (§12.30.5's generalised rule) |
| **Section selector** | `TabStrip` — **Queue** · **Draft** |
| **Queue · left `ListView`** | What this network can make: one row per unlocked `(ItemId, Grade)` |
| **Queue · right `ListView`** | The queue — one row per job, with `Row::fill` |
| **Draft · left `ListView`** | Your unlocked shells and modules, as parts |
| **Draft · right `ListView`** | The draft's mounts, one row each, carrying the `Validation` rule each fails |
| **Footer** | The commit confirmation, or **Cancel** for a queued row, or **Save** for a valid draft |

Queue left-hand rows: **`glyph`** the monogram; **`label`** the item at its grade; **`value`** the
duration *at this bench* and the recipe's demand. **`RowStyle`** disabled with `NO SLOTS` when the
queue is full, or with the missing **role** when the hold cannot fill a slot.

⚠️ **The row cannot say *"missing 5 × Neodymium"*, and must not try.** §2.10's recipes name roles, so
the refusal is per role — **`NO MAGNETIC ELEMENT`** — and the confirmation shows **what the fill would
actually be** out of the hold before the click. *This is the one screen where §2.10's substitution
model is either legible or invisible: a parts list of named elements would present a role-based recipe
as a named-element one and teach the player the wrong model on the first click.* It is the same
disclosure discipline §2.4 forces on research, applied to composition instead of to odds.

Draft's right-hand list is the second contribution. `CanSave` returns a full `ValidationResult` today
and `CustomizeMenu::Draw` renders it as the single word `INVALID`. **One rule per row** — §8.3's
*absence must never look like emptiness*, applied to a failure that already knows its own name.

##### The verbs

| Selection | Verb | Mechanism | Blocked on |
|---|---|---|---|
| An unlocked `(ItemId, Grade)` | **Queue** | `StartManufacturingRequest { ItemRef item; MountId facility; int quantity; }` | §12.19 · §12.18 |
| A running job | **Cancel** | `CancelManufacturingRequest { ItemRef item; MountId facility; }` | ditto |
| A valid draft | **Save** | `SaveTemplateIntent` — **built, and it already carries the body** | The one-line fix above |
| An invalid draft | **none** — disabled, naming the failing rule | — | — |

##### Inputs are consumed per unit, never per job — which is §12.30.4's argument again

*A job of ten is ten units, not one large one, because `features.md` §2.8 rolls quality **per unit**
and §12.19 stacks by indistinguishability: ten units are up to ten stacks.*

> **The job produces one unit per `duration`, decrementing `remaining`, and consumes that unit's
> inputs at the moment it starts it.**

This is §12.30.4's continuous-billing reasoning transplanted, and it earns the same thing — **no
refund logic anywhere:**

| Event | Result |
|---|---|
| A unit completes | It lands in the requester's hold with a fresh quality roll (§2.8) |
| The hold cannot fill the next unit's recipe | The job **stalls at the unit boundary** and resumes when it can. Nothing owed |
| The destination hold is **full** | The job stalls at delivery, holding the finished unit in the job — §12.30.3's *refused whole, never partially applied*, and §12.30.6's *"the job is where the sample is held"* |
| The player cancels | Only the **in-progress** unit's inputs are at stake |
| The Manufacturing hardpoint is destroyed | The job stops. §3.4's bargain, and the mandatory `Gauge` was showing it |

⚠️ **And cancel is not free the way §12.30.6's is.** Research consumes nothing until completion, so
cancelling a research job returns the sample whole. Manufacturing has already put matter into the
unit on the bench. **A cancelled unit returns its inputs at the facility grade's deconstruction
recovery band** (§2.4's 20–45% … 80–100%) — reuse rather than a new rule, and it gives that table its
third consumer after deconstruction itself and §12.19's nominal fill. *Two screens, one word, two
behaviours: say so on the confirmation, or the second one reads as a bug.*

##### Manufacturing at someone else's factory is free, for §12.30.6's reason

The materials are yours, the design is in your network, the bench is theirs. **No fee** — §2.4 warns
that *"four brakes on one action is the pattern that got upkeep cut"*, and this action already carries
three (materials, time, and the knowledge gate).

> **The job runs on the host's `StationFacility`, so it dies with the host** — and unlike research, a
> stalled or cancelled job here has your **materials** in it. Building at a foreign yard is a real
> exposure and §3.4 already built it. No new mechanism.

##### The output lands on the vessel, not on the station

§12.30.3's trap applies unchanged and is worth one assertion here too: the request goes on **the
vessel root the player arrived in**, which is the entity carrying `Docked`, and the finished unit
lands in *that* hold. A screen that places its request on `PlayerControlled` builds nothing, silently
(§12.30.1).

##### Types

- **`ManufacturingJob { ItemRef item; MountId facility; int remaining; float progress; float durationSeconds; ItemStack pendingOutput; }`** — on `StationFacility::manufacturingJobs`, **a separate queue on the same component** (`features.md` §2.8: *"they share a shape, not a resource"*).
- **`StartManufacturingRequest` / `CancelManufacturingRequest`** — new, `shared/components/Manufacturing.h`.
- **`ResearchJob` and `ResearchRecord` gain `MountId facility`** — the amendment above.
- **`KnowledgeNetwork`'s unlock entries carry a `Grade`** — the amendment above; `KnowledgeSerialization` bumps with it.
- **`ContentLibrary::RegisterDraftedTemplate(ShipBlueprint)`**, replacing `RegisterCraftedModule`; `SystemContext::craftedModules` is deleted (§12.19) and the store is reached through `ctx.content` like every other definition.
- **`BuildStationRequest::cost` / `PlaceShipRequest::cost`** — **deleted**, not filled in (Law 9), joining `BuyItemRequest::cost`, `SellItemRequest::value` and `RepairRequest::costForFullRepair`.
- **`core/galaxy/ManufacturingRecord`** — exactly parallel to the built `ResearchRecord` (§12.18).
- **No new component.** The queue is `StationFacility`, the gate is `FacilityRef` plus `ctx.knowledge`, the slots are `FacilityStats::capacity`, the rate is `FacilityStats::ratePerSecond`, the inputs are the requester's `CargoHold`. §2.4's test, passed — and `ratePerSecond` gets its **second** reader after §12.30.4's repair.

##### Systems

**`ManufacturingSystem` is new, and it is the only screen in this batch that needs a new system** —
the other six added verbs to systems that already existed. §12.18 settles its home and why it is
neither `ConstructionSystem` (which spawns entities and carries the layering exemption) nor
`EngineerSystem` (which does not share the gate). It is also, per §11.9, **the only place
`features.md` §2.10's attribute propagation is computed**, which is most of why the item model had to
land first.

`ConstructionSystem` gains the knowledge gate and loses its two caller-supplied costs.
`customize_menu::ConsumeSaveTemplateRequests` keeps the body it currently discards, registering it
through `RegisterDraftedTemplate` before granting the id. *(It also stays where §12.24 🐛 put it —
that finding is about **where** the consumer lives, not about what it does.)*

##### Tests

- A `StartManufacturingRequest` at a station with **no living `Manufacturing` hardpoint** is refused;
  one at a living factory creates exactly one job, naming that hardpoint's `MountId`.
- A request for a design **absent from the requester's network** is refused — and a request for the
  right design at the **wrong grade** is refused, which is the unlock-key assertion.
- Destroying the Manufacturing hardpoint mid-job stops **that** job and leaves a job on a sibling
  bench running — the two halves of the `MountId` fix, asserted together.
- Duration scales with the **facility hardpoint's grade** and with the item's grade, and matches
  §2.8's table: a Mythic module at a Mythic bench is ~192s.
- Two units of one job roll **independent qualities** and land as two stacks; two units that roll
  identically land as one stack of two (§12.19's stacking rule, both directions).
- A unit consumes its inputs at its own start: a job of ten against inputs for three produces three
  units and then **stalls**, with the remaining inputs untouched and nothing owed.
- A full destination hold stalls delivery rather than dropping the unit, and the unit is delivered
  once room exists.
- Cancelling returns the in-progress unit's inputs **within** the facility grade's recovery band and
  never above 100%; a completed unit's inputs are never returned.
- A demote → promote cycle resumes at caught-up progress and carries `remaining` and
  `pendingOutput` — §12.18's rule, with something in the job that can be lost.
- **A saved Template round-trips into the network and is then successfully built by
  `ConstructionSystem`** — the regression test for the severed chain, and the assertion that fails
  today at the last link.
- A `PlaceShipRequest` for a blueprint the requester's network does not hold is refused.
- An invalid draft cannot be saved and the screen names **which** rule failed, not that one did.
- **The request a screen builds lands on the docked requester and is consumed by its system on the
  following tick** — §12.30's shared per-screen shape.

##### Scheduling

Group **4b**, and it is the **most blocked of the seven** — which is why it was specified last rather
than built last.

| Needs | Status |
|---|---|
| 4a's widgets **plus `Row::fill`** · the router · `PlayerLocation` | Group 4b |
| **§12.19 in full**, including `Pricing.h` and the Element/Material content set | Its own issue |
| **§12.18 — `ManufacturingSystem`** | Unstarted; §12.19 is its prerequisite |
| **§12.24 step 6 — `ctx.knowledge`** | Group 3. A hard prerequisite, exactly as for Research: with a null pointer there is no network to check and the Queue list is empty by construction |
| A `Manufacturing` facility authored in `modules.json` | Content |

⚠️ **Ship the tab with Draft alone.** The Template chain's fix is one retained field, one overlay
rename, and one gate on `ConstructionSystem`; it needs neither §12.19 nor `ManufacturingSystem`, and
it turns a feature that is built, tested and unreachable into one that works. **That is the third
screen in this batch to split by verb** — Storage before Market (§12.30.3), Delete and Rebuild before
Merge (§12.30.5), Draft before Queue — and at three occurrences it is the batch's shape rather than
three coincidences: **a screen is a tab plus a set of verbs, and the verbs do not unblock together.**

*The four defects above are **not** 4b work.* The `MountId` on a job and the grade on an unlock are
schema changes against built, tested code and belong with §13.5 group 2's research pass; the
`SaveTemplateIntent` body and `ConstructionSystem`'s gate are group 2 by the same test — *"one-line
and one-view corrections, independently startable today."*

##### What is deliberately not here

- **No vessel manufacturing.** `features.md` §2.8's table puts vessels on `ConstructionSystem`
  because building one *is* entity assembly; this screen makes shells, modules and Materials, which
  land as inventory. The Draft section designs a vessel and does not build one — `B` and §12.26 do.
- **No queue reordering or priority.** Jobs are concurrent up to `capacity`, not sequential, so there
  is no order to change — §12.30.6's reasoning, unchanged.
- **No recipe editing.** A Material's recipe is generated from §2.10's grade table and a module's is
  authored on its def; a player choosing slots by hand would be authoring content at runtime, and
  §2.10's substitution rule already gives them the only choice that matters — what they feed it.
- **No Template marketplace.** Selling a design is §12.7's `TemplateMarketSystem`, and it is a comms
  surface rather than a factory one.
- **No blueprint versioning.** §12.9's ❓ *(does saving over a name overwrite, version, or refuse)*
  is still open and this screen does not close it. It is now reachable, which is a reason to close
  it, and the cheapest answer is **refuse and say so** — a name collision on a surface with a
  `ListView` is visible before the click.


### 12.31 The Rig Snapshot — `RigState`, and why it is not a blueprint

*Settled 2026-08-10, from §13.3 AC. `SpaceFlight.h:62` calls the missing capability a
"live-rig-to-blueprint snapshot," and `BlueprintSerialization.h` defers it as "a factory-adjacent
concern that lives wherever that conversion is first needed." **The name is the design error.** Most
of this section is separating two operations that phrase has been holding together.*

#### One phrase, two operations

| | Live rig → **Template** | Live rig → **state** |
|---|---|---|
| Produces | `ShipBlueprint` | `RigState` — new |
| Lossy? | **Deliberately.** A Template is a *design* | **Never.** A save that loses damage is a bug |
| Carries | Composition: shells, offsets, module ids | Damage · destruction · per-instance quality · position |
| Wanted by | §12.9 "save this ship as a Template", §2.2–2.3 | Warp · parked hulls (§12.30.2) · the world save (§13.3 Y) |
| Transferable to another actor? | **Yes** — sold, manufactured, pitched | **No.** Nobody buys your hull damage |

Both exist. Both are wanted. **They are not the same function returning the same type**, and building
the Template version first — which is the easier one — and then extending it into a save format is
how one type ends up meaning two things. This document has caught that pattern three times already
(`MountedModules`/`EquippedModule`, `Element`/`Material`, `Storage`-the-kind vs. `CargoHold`).

#### Why the state form cannot be a `ShipBlueprint`

Three reasons, in weight order. The third is the one that would have shipped and then hurt.

**1 — §12.21 makes it impossible, not merely awkward.** `MountBlueprint::modules` is
`std::vector<ModuleId>`. §12.21 settles that a `Quality` roll is *"stored per instance"*, so two
`pulse_cannon_i` on the same hull can have different rolled stats and **collapse to identical bytes**
in blueprint form. A blueprint round-trip would silently re-roll or flatten every module on the ship.
That is a data-loss bug of exactly the shape §12.21 warns about for `Direction` — one that *"would
pass every test that only checks 'the number changed.'"*

**2 — It puts state inside content.** A blueprint is authored content (Law 10), and §12.21's own
persistence rule already forbids the inverse mistake for the same reason: *"Storing the derived block
would let a content edit and a save disagree forever."* A hull's damage is a session fact; a shell's
geometry is content. `check_content_pipeline.py` polices how blueprints are constructed precisely
because they are the content boundary.

**3 — `ShipBlueprint` has four uses and adding a fifth breaks the other four.** Its own comment:
*"This is what a player Template is, what a save file stores, what gets sold to a faction, and what a
faction manufactures from… which is what makes all four of those uses the same code path."* Add
`health` to `MountBlueprint` and **`TemplateMarketSystem` sells a damaged design and
`ManufacturingSystem` builds pre-damaged ships.** The four uses are unified because they are all
*designs*; a state record is not one, and the comment's own justification is the test it fails.

> ⚠️ **`ShipBlueprint`'s comment claims "what a save file stores." That claim is now wrong** and
> should be corrected in the same commit: a save stores blueprints **and** `RigState`, and confusing
> the two is what this section exists to prevent.

#### `RigState` is a delta against a `BlueprintId`, not a copy of the rig

This is the whole economy of the design. Shells, local offsets, `attachedTo`, and `traverseRadians`
are **content** and come back from the `BlueprintId`. `RigState` stores only what content cannot
know.

```
shared/rig/RigState.h        POD, no entt::entity anywhere (Law 2)

struct ModuleInstance { ModuleId id; Quality quality; };        // §12.21's per-instance roll

struct MountState {
    MountId  id;            // stable, and RigBlueprint.h already promises it "survives saves"
    float    health;        // absolute, clamped to the recomputed max on restore
    bool     destroyed;
    std::vector<ModuleInstance> modules;
};

struct RigState {
    BlueprintId blueprint;  // content is authoritative; this is a delta against it
    FactionId   faction;
    Vec2        position;   float rotation;
    std::vector<MountState> mounts;
    CargoHold   cargo;      Wallet wallet;   // a parked hull keeps what is in it
};
```

**Two deltas fall out with no extra fields**, which is the sign the shape is right:

- **A hardpoint `RefactorSystem` deleted** is simply absent from `mounts`. No tombstone.
- **A module `ModuleEquipSystem` mounted at runtime** is just an entry in `MountState::modules` that
  the blueprint does not have. No "was this original?" flag.

⚠️ **This requires §13.4 decision 2 to have landed.** `MountState::modules` is *one* list, and the
codebase currently has two unreconciled answers to "what is in this mount" (`MountedModules` vs.
`EquippedModule`, finding §13.3 C). A snapshot cannot be written against both. **This is a third
independent argument for keeping `MountedModules` and deleting `EquippedModule`**, and it is the
first one that makes the choice load-bearing rather than tidy.

#### Where it lives, and the layer rule that forces the split

Three files, and the homes are not a preference — `check_layers.py` decides them:

| File | Why there |
|---|---|
| `shared/rig/RigState.h` | POD, handle-free. **`core/` may include `shared/`** but never `modes/`, so the type has to be here for a save to reach it. Sits beside the existing `shared/rig/ModuleAttachment.h` |
| `core/serialization/RigStateSerialization.{h,cpp}` | Exactly parallel to `BlueprintSerialization` — same `ByteWriter`/`ByteReader`, same *"one Encode/Decode pair, not four"* argument |
| `modes/space/factories/RigSnapshot.{h,cpp}` | `Capture(registry, root) -> RigState` and `Restore(world, content, RigState) -> SpawnResult`. **Law 3: "A factory is the only bridge… nothing else converts between the two forms."** It is the only file here that touches a registry |

**That split is what keeps Law 2's "entity handles never cross a save file or the wire" true by
construction** rather than by review: `core/serialization/` is handed POD and could not serialize a
handle if it tried, and the one file that sees handles cannot be reached from `core/`.

`SpaceFlight::WarpToSystem` is the first caller, and it already has the shape — it captures
`CargoHold`/`Wallet` before `world_ = SystemWorld(...)` and re-applies them after. **It captures a
`RigState` instead**, and the `wreckLedger_` demote/promote pair beside it is the pattern a parked-hull
ledger copies.

#### 🐛 A crafted module does not survive its own process

*Found while scoping this, verified 2026-08-10.* `ContentLibrary::craftedModules_` is a plain
`std::unordered_map<std::string, ModuleDef>` member, written only by
`EngineerSystem`'s `RegisterCraftedModule` and **serialized by nothing.** `FindModule` checks it
before the JSON set, so a merged module resolves perfectly — until the process ends.

**Any `RigState` or save referencing a merged module's id would restore a hull with a module that no
longer exists.** §12.12 sanctions merging as *"genuinely new content, generated at runtime"*; content
generated at runtime has to be persisted somewhere, and no one decided where. **`RigState`'s restore
path is the first consumer that makes this fail loudly**, so the crafted-module overlay gets a save
section in the same work — it is one more `Encode`/`Decode` pair over a type that is already POD.

*Left alone, the failure mode is a player merging two modules, warping, and finding the hardpoint
empty — with nothing in the log to say why.*

> ✅ **Dissolved by §12.19, and the save section is not written.** A merge produces a new `Quality` on
> the primary `ItemInstance`, not a new `ModuleDef`, so `craftedModules_` is deleted rather than
> serialized and the merged module is already inside the hold this snapshot carries. **What `RigState`
> must carry instead** is each mount's `ShellInstance` and `MountedModules::items` — the attributes,
> mass and `Quality` of every instance on the rig, which are stored precisely because their inputs
> were consumed and the derivation is not repeatable. §12.30.8 re-points the overlay at the type that
> does need one: a drafted Template, whose body `ConsumeSaveTemplateRequests` currently discards.

#### What this retires

| | Today | After |
|---|---|---|
| §13.3 AC — warp fully repairs the hull | Free full repair at every system boundary | Damage crosses the jump |
| `features.md` §2.7 — live refit across systems | Silently rolled back at the boundary | Refits persist |
| §12.30.2 — parked hulls | Destroyed by `world_ = SystemWorld(...)` | A `core/galaxy/` ledger, same shape as `WreckRecord` |
| §13.3 Y — the world save | *"no registry serialization, no player rig state"* | The player-rig half exists |

**It does not deliver the world save.** §13.3 Y also needs the seed, `FactionEconomy`,
`DiplomacyMatrix`, `DiscoveryState` and `WreckLedger`. `RigState` is the piece that has four callers
instead of one, which is why it is worth building first and alone.

#### Tests

All headless — `RigState` is POD and `Capture`/`Restore` need no window:

- Capture → Restore round-trips a damaged rig: per-mount health, destroyed flags, and mount count all
  survive.
- A hardpoint deleted by `RefactorSystem` does not come back.
- A module mounted at runtime does come back, and one the blueprint authored is not duplicated.
- Health above a *reduced* max after a content edit clamps down; it never silently heals.
- **A restored rig re-derives its stat block from `def + quality`**, not from stored derived values —
  §12.21's rule, asserted at the snapshot boundary where it is easiest to violate.
- `Encode`/`Decode` round-trips `RigState` bytes, and a truncated buffer fails rather than
  half-decoding — the assertion `BlueprintSerialization`'s tests already make.
- A rig carrying a crafted module restores it, given the crafted-module section (the regression test
  for the 🐛 above).
- **No `entt::entity` value appears anywhere in an encoded buffer** — Law 2's rule, made a test rather
  than a review note.

#### Scope

**Its own issue, and it is not group 1.** It has no dependency on the player existing and can be
verified by tests alone, like §12.28 — but it *does* depend on §13.4 decision 2 (`MountedModules`
unification) and reads better after §12.21's `Quality` type exists. Sequence: **decision 2 → §12.21 →
this → the world save.**

**Nothing in §12.30's docked screens waits on it.** The Bay screen ships with Board and Launch against
hulls in the current system; parked hulls across systems are the part that waits.

---

### 12.32 The Reapers — Universal Hostile-Default and Structural-Density Targeting — `features.md` §5.7

Two genuinely separate pieces of work hide inside one `features.md` subsection, and only one of
them is Reapers-specific.

#### A prerequisite this pass decided to fix rather than route around

`Relation` (`core/diplomacy/Relation.h`) was a **three-state enum** — `Hostile`, `Neutral`,
`Friendly` — not the six-band signed range `features.md` §5.3 describes (Allied / Friendly /
Neutral / Distrustful / Hostile / War). This sharpens §12.30.3's already-recorded finding
(*"three of those six rows are unimplementable against the built code"*) from an inference into a
direct read of the type: there was no code-level distinction between Allied and Friendly, or among
Distrustful / Hostile / War, at all. An earlier draft of this section mapped Ally onto
`Relation::Friendly` and Rival onto `Relation::Hostile` to route around the gap rather than close
it — reversed below, because two other places already assume the fuller range exists and get more
correct, not more complicated, once it does:

- **`Reputation`** (`core/diplomacy/Reputation.h`) already tracks a continuous **−100..100** score
  per faction and only throws away the resolution at the last step — `ThresholdRelation` collapses
  it to 3 states at a flat ±25. The score itself was always six-band-shaped.
- **`TemplateMarketSystem.cpp:10-13`**'s own comment names the mismatch directly: *"DiplomacyMatrix
  has no native numeric value — Relation is a 3-state enum — but `architecture.md` 12.7's roll
  formula wants '...-100..100.' This is the discrete-to-numeric bridge."* A bridge function
  apologizing for its own type is a defect wearing a comment, not a design.

`Relation` becomes:

```cpp
// Six-state diplomatic stance, features.md §5.3. Ordered War < Hostile < Distrustful < Neutral <
// Friendly < Allied so callers can threshold a continuous score into this enum with a plain
// comparison -- the same property the old three-state version documented, extended rather than
// dropped.
enum class Relation : std::uint8_t {
    War,
    Hostile,
    Distrustful,
    Neutral,
    Friendly,
    Allied,
};
```

`DiplomacyMatrix::Get`'s unset-pair default stays `Relation::Neutral` (`DiplomacyMatrix.cpp:14`) —
§5.9 already settles that for every faction but the Reapers, and nothing about widening the enum
changes what an *unwritten* pair means, only what a *written* one can say.

`Reputation::ThresholdRelation` gains three more thresholds, matching §5.3's own band boundaries
instead of inventing new ones: `kAlliedThreshold = 50.0f`, `kFriendlyThreshold = 15.0f` (unchanged
in spirit, moved from the old ±25), `kDistrustfulThreshold = -15.0f`, `kHostileThreshold = -50.0f`,
`kWarThreshold = -85.0f`. `TemplateMarketSystem::RelationValue` stops bridging and starts mapping —
the exact per-band numeric value is a tuning question for whoever wires the roll for real (§6.3
already flags this class of constant as `tools/economy_sim` territory), not a decision this section
pins down.

`DiplomacyMatrix::Get` already established the matrix has zero writers anywhere (finding N, §13.3).
Neither §12 nor §13.5 specifies how the twenty pairs in §5.4's baseline actually get written — group
3 unblocks the *pointer* (`ctx.diplomacy` non-null), not the *contents*. This section specifies both
the general seeder that gap needs regardless of the Reapers, and the one exception §5.7 layers on
top of it.

#### Types

No new component beyond the widened enum above. Two free functions in `core/diplomacy/`:

```cpp
// Writes features.md §5.4's twenty pairs: five Relation::Allied (the alliances), fifteen
// Relation::Hostile (the rivalries -- the standard severity; the Reapers' own three are escalated
// below). Idempotent -- safe to call once at startup.
void SeedBaselineRelations(DiplomacyMatrix& matrix);

// features.md §5.7's exception, called immediately after SeedBaselineRelations. Two effects:
// (1) escalates the Reapers' three seeded rivals -- Aegis Directorate, The Forgotten, AI
// Concordance -- from Relation::Hostile to Relation::War, the priority-target tier that "absorbs
// the most pressure"; (2) every faction not already covered by the Reapers' one ally and three
// rivals (five factions -- Meridian Star Corps, Kore Industries, Voidwalkers, Zenith Collective,
// Edenian Pact) gets an explicit Relation::Hostile against FactionId("reapers"), overriding Get()'s
// Neutral default the same way every other baseline entry does.
void SeedReaperHostility(DiplomacyMatrix& matrix);
```

Deliberately two functions, not one Reapers-aware branch inside a single seeder:
`SeedBaselineRelations` stays a pure transcription of §5.4's table with no named-faction logic in
it, so it can be generated from data later — §14.2 already records that no `factions.json` exists
yet, and when one does, this is the function that gets replaced first. `SeedReaperHostility` is the
one place in the codebase permitted to say `"reapers"` by name outside content, because it is not
expressing a generic relation, it is expressing a rule §5.7 states only the Reapers have. Folding
it into the generic seeder would make the *next* faction with a special case reach for the same
branch instead of writing its own function. It is also now the **only** place `Relation::War` gets
written at seed time — every other faction's worst standing is `Hostile` unless play drifts it
further, which is exactly §5.7's point about what makes the Reapers different.

#### Where it's called

Both are Law 8 galaxy-wide state, like `DiplomacyMatrix` itself — not per-registry, so they run
once, not on a schedule. The natural call site is wherever `ctx.diplomacy` stops being `nullptr`
(§13.5 group 3), since a matrix with a live pointer and zero seeded relations is just a different
flavor of the same null-object failure. Call both immediately after construction, before any
system's first tick.

#### Systems

No scheduled system changes. `TargetingSystem` and `DockingSystem` can now be widened to a real
relation-band test (findings N and §12.30.3's docking gate) once `ctx.diplomacy` is non-null —
the six-state enum this section adds is the type that gate needs to test against; wiring
`DockingSystem`/`TargetingSystem` themselves is still §13.5 group 3's work, not this section's.

#### The targeting half — blocked on a prerequisite this section did not create

§5.7's second half — *"target selection is driven by structural density, not politics"* — is not a
relation-matrix question at all. It is a `core/ai/FactionDecisionEngine` question, and it is
blocked on a gap `RaidDispatchDirective`'s own doc comment already names: *"picking [a target]
needs a border/adjacency model `core/diplomacy/Territory.h` does not carry — it records ownership,
not neighbors."* `EvaluateColonization` sidesteps the same hole by taking `candidateSystemId` from
the caller instead of discovering it. Nothing about the Reapers makes this gap bigger or smaller —
every faction's raid/colonization target selection is equally blocked on "which systems exist and
are reachable" not existing anywhere in `core/galaxy/` yet.

What *is* Reapers-specific, and buildable the moment that prerequisite lands, is the scoring
function candidates get ranked by. This follows `EvaluateRaidDispatch`/`EvaluateColonization`'s
established idiom exactly — `core/` has no registry, so it cannot compute a system's station count
or grade itself; the caller supplies it:

```cpp
// A candidate system and the caller-computed density score features.md §5.7 targets by -- station
// count and grade, once core/galaxy/ has a per-system roster to compute it from. Not owned here
// for the same reason EvaluateColonization takes candidateSystemId instead of discovering it.
struct ReaperTargetCandidate {
    std::string systemId;
    float structuralDensity;  // Caller's units; only relative order matters here.
};

// Highest-density candidate wins, full stop -- no relation check, no roll. Isolated or derelict
// sectors (features.md §5.7) score at or near zero and simply never win. nullopt only for an
// empty list.
std::optional<std::string> SelectReaperTarget(const std::vector<ReaperTargetCandidate>& candidates);
```

Deliberately the smallest possible function — a max-by-key over caller-supplied data — because
everything upstream of it (what counts as "structural density," how a candidate list gets built)
belongs to the same future work that gives every other faction's `RaidDispatchDirective` a target.
Duplicating that design here for the Reapers alone would repeat the two-systems-for-one-concept
mistake §13.3 C already caught once (`MountedModules` / `EquippedModule`).

#### Tests

`SeedBaselineRelations` / `SeedReaperHostility`: the four invariants §5.4 already states as
unit-testable (symmetry, degree, disjointness, closure) plus what this section adds — after both
seeders run, Pyre reads `Relation::Allied`; Aegis, The Forgotten, and AI Concordance each read
`Relation::War`; Meridian, Kore, Voidwalkers, Zenith, and Edenian each read `Relation::Hostile`; and
every other faction's fifteen rivalries (Reapers' excluded) read plain `Relation::Hostile`, not
`War` — the escalation is exclusive to `SeedReaperHostility`'s caller, not a property of "being
someone's rival" in general. `Reputation::ThresholdRelation`: one assertion per new threshold
boundary, the same shape as the existing ±25 tests. `SelectReaperTarget`: empty list, single
candidate, and a documented tie-break rule (state whichever behavior the implementation produces
rather than leaving it unspecified — the `ProjectileSystem::FindHit` iteration-order lesson §12.16
#17 already paid for once).

#### Scope

**Buildable today, independent of everything else:** the widened `Relation` enum,
`SeedBaselineRelations`, `SeedReaperHostility`, `Reputation`'s new thresholds,
`TemplateMarketSystem::RelationValue`'s real mapping, and `SelectReaperTarget`. **Blocked:** calling
either seeder from `SpaceFlight`/`main.cpp` (needs §13.5 group 3's `ctx.diplomacy` wiring); widening
`DockingSystem`/`TargetingSystem` to a real band test (same dependency, §12.30.3); and ever calling
`SelectReaperTarget` for real (needs the system-adjacency/roster model `RaidDispatchDirective` has
been waiting on since before this section existed, plus §1.1's coarse-tick loop to have anywhere to
call it from).

---

### 12.33 The Damage-Type Effect Table, and Ramming as Kinetic Damage — `features.md` §3.1, §3.7

*Answers §15.1 findings 1 and 5 together — they're the same fix. Finding 1: ram damage is tagged
`DamageType::Kinetic` but isn't run through shield absorption, so a Kinetic shield currently does
nothing against a ram. Finding 5: `DamageType` has no `Ion` value anywhere, so §3.1's "absorbed by
every shield, never bypasses" rule has nothing to implement it. Both trace to the same root: today's
absorb-or-bypass rule is a single hardcoded shape (`Shield::absorbs` matches `PendingDamage::type`
or it doesn't) with no way for a type to say "I'm different."*

#### Types

```cpp
// shared/blueprints/Taxonomy.h -- one more enumerator. Shield-matching stays exactly {Kinetic,
// Energy} (features.md §3.1's "it stays two" is unchanged); Ion is a weapon type, not a shield type.
enum class DamageType : std::uint8_t { Kinetic, Energy, Ion };

// core/registries/DamageTypeEffects.h -- one row per DamageType, authored in
// data/base_game/damage_types.json (Law 10). Kinetic and Energy take the all-default row implicitly
// -- only a type that needs to diverge from "absorb if matching shield, full hull damage otherwise"
// needs an entry at all.
struct DamageTypeEffect {
    DamageType type;
    bool alwaysAbsorbedByAnyShield = false;   // Ion: true -- never bypasses, regardless of Shield::absorbs
    bool bypassStillDrainsShieldCharge = false; // Ion: true -- "strips shields quickly"
    float hullDamageFraction = 1.0f;          // Ion: 0.0 -- "deals no hull damage at all once through"
    float powerDrainFraction = 0.0f;          // Ion: 1.0 -- redirected to PowerSource, features.md §2.9's shed path
};
```

`Shield::absorbs` is unchanged — still a single `DamageType`, still exactly `Kinetic` or `Energy`.
This table does not reopen that decision; it only changes what happens once the match check runs.

#### Systems

`DamageSystem`'s per-hit path becomes one generic lookup instead of per-type branches:

1. Look up `PendingDamage::type`'s `DamageTypeEffect` (default row if none authored).
2. **Absorbed** if the hardpoint's `Shield::absorbs == type`, **or** `effect.alwaysAbsorbedByAnyShield`.
   If absorbed: deplete the shield pool, reset `rechargeCooldown` — unchanged from today — and if
   `effect.bypassStillDrainsShieldCharge` is also set, this is where that extra drain applies (Ion's
   case: absorbed *and* draining, not mutually exclusive).
3. **Not absorbed** (no shield present, or the type doesn't match and isn't always-absorbed): the
   surviving amount splits by `hullDamageFraction` (→ ordinary hardpoint health subtraction,
   unchanged) and `powerDrainFraction` (→ `PowerSource`, via the existing generation-drop shed path
   §2.9 already specifies for Ion — no new machinery there).

**`CollisionSystem::ApplyRamDamage` needs no change beyond what already exists** — it already tags
`DamageType::Kinetic` (§15.1 finding 1 confirmed this). The fix is entirely inside `DamageSystem`
correctly running that tag through step 2 above instead of the absorption check silently not
applying to ram-sourced `PendingDamage`. Concretely: whatever currently causes ram damage to skip
absorption (§15.1 didn't name the exact branch — likely `ApplyToHealthAndShield` never being
reached for `CollisionSystem`'s writes, or a source-agnostic pathway existing but not being taken) is
what group 2d's issue closes; the *rule* it should close to is exactly step 2/3 above, unmodified for
ramming.

#### Content

`data/base_game/damage_types.json` — new file (§14.2 already records this content category doesn't
exist). One row: Ion, per the table above. Kinetic and Energy need no entries.

#### Tests

Ion vs. a Kinetic shield: absorbed, shield charge drains, zero hull damage, power drops by the hit's
value. Ion vs. an Energy shield: same outcome — `alwaysAbsorbedByAnyShield` ignores the match check
entirely. Kinetic ram vs. a Kinetic shield: absorbed, shield charge drains, zero hull damage (the
new behavior finding 1 exists to produce). Kinetic ram vs. an Energy shield: full hull damage,
unchanged from today. A rig with no shield: `effect`'s absorption fields never evaluated, straight to
hull/power split — confirms the default path still matches pre-this-section behavior exactly for
Kinetic/Energy against an unshielded target.

#### Scope

Buildable today, independent of everything else in §12 — no dependency on the game loop, `ctx.diplomacy`,
or the coarse tick. Lands with group 2d (the damage-model pass), since it changes the same file
(`DamageSystem.cpp`) group 2d's other items already touch.

---

### 12.34 Docked Invulnerability and Cascading Destruction — `features.md` §3.4

*Answers §15.1 findings 2 and 3. Both halves of §3.4's headline rule — "cannot be shot," "dies with
its host" — are unimplemented; only the auto-lock half of the first one exists.*

#### Systems

**Closing "cannot be shot":** add `exclude<Docked>` to `ProjectileSystem::FindHit`'s view and to
`CollisionSystem`'s ramming-candidate view, alongside the `Targetable` removal `DockingSystem`
already performs on dock. Three call sites, one tag, no new component — the same shape as the
`NpcAiSystem`/`WeaponSystem` `Docked` exclusion finding H already fixed elsewhere in group 2.

**Closing "dies with its host," and the loot mechanism `features.md` §3.4 already called for but
never specified:** when `DamageSystem` tags a rig `Destroyed` (the existing `HasLivingHardpoint`
check, unchanged), a new step queries every rig with `Docked.station == thisEntity` and, for each,
calls the **same** `DeathWreck`-creation path `LootSystem` already runs for an ordinary combat kill
— not a parallel mechanism, the identical function, invoked with a different cause. If the dying
rig carries a `CargoHold`, its contents spawn as `MaterialDrop`/`LootDrop` entities at the wreck
site via `LootSystem`'s existing drop-spawn convention. A partial-survival fraction on the cargo
spill (not everything survives) is a tuning constant for later, not a design decision this section
needs to pin down.

**Ordering:** this step must run in the same tick as the `Destroyed` tag is applied, before anything
downstream treats the now-orphaned `Docked.station` reference as valid — the natural position is
immediately after `DamageSystem`'s existing rig-death branch, same system, no new schedule entry.

#### Tests

A docked rig survives a direct projectile hit and a ramming collision aimed at it (regression for
the exclusion). A rig docked to a host that dies is itself tagged `Destroyed` the same tick and
produces a `DeathWreck`. A host's `CargoHold` contents appear as pickups at the wreck position.
Multiple docked rigs on one dying host all resolve, not just the first found. A host with an empty
bay dies cleanly (no phantom wreck from a bay with nothing in it).

#### Scope

Buildable today, independent of the game loop and `ctx.diplomacy`. The `exclude<Docked>` half is a
group-2-shaped one-liner; the cascade-destroy half changes `DamageSystem.cpp`, so it belongs beside
§12.33 in group 2d rather than as a separate issue.

---

### 12.35 Multi-Scale Territory Navigation — `features.md` §8.1

*Answers §15.1 finding 4 (`NavigationMap` shows individual objects at the Galaxy zoom level, which
`features.md` §8.1 forbids there by name) by specifying the fuller design that motivated the fix,
per `features.md` §8.1's new "Level 1 is a continuous zoom" subsection.*

#### Types

No new component for the zoom mechanism itself — this is a rendering and query concern over state
that mostly already exists:

- **`core::diplomacy::Territory`** gets its first real reader. It currently has zero consumers
  anywhere in `src/` (§13.2). `NavigationMap` at every Level-1 scale queries it to build the
  aggregate blobs the design requires, rather than iterating individual `DiscoveredSystemIds` the
  way today's code does at every level indiscriminately.
- **A new, small aggregation step** — grouping systems into regional clusters, clusters into
  galactic territory, and (pending §9's open scope question) galaxies into groups — has no existing
  analog and is the one genuinely new piece of work here. It is a pure function over `Territory` +
  `Discovery` data, not a system: no per-frame state, callable on demand the same way
  `IconRenderer`'s existing draw functions are.

#### Systems

`NavigationMap::Draw` gains real scale-dependent branching instead of the current
`level == Galaxy || level == Region` single case: each Level-1 sub-scale renders the aggregation
step's output as territory shapes, not points. Hover/click info at every scale is gated through
`DiscoveryState`/`KnowledgeNetwork` exactly as §8.3's fog-of-war model already specifies — no new
gating rule, extended one level further out than it currently reaches. Selecting a target checks
`WarpSystem`'s hyperdrive-range gate (§13.1: currently unbuilt — "no fuel, no module, no charge
time") to decide available-vs-unavailable rendering; `NavigationMap` reads that gate, it does not
reimplement range-checking itself.

#### Content / Data

None required to build the mechanism. Aggregation works over however many systems/factions are
already authored; it degrades gracefully with the sparse content set §14.2 already records.

#### Tests

A cluster of undiscovered systems renders as present-but-unknown, not absent (§8.3's rule, asserted
at this new scale). A territory blob's aggregate composition updates when a member system's owner
changes (`Territory` write → blob recomputation, not a cached stale picture). A target outside
hyperdrive range renders unavailable and cannot be confirmed; one inside range can.

#### Scope

**Blocked on two prerequisites, neither of which this section creates:** `WarpSystem`'s range gate
(§13.1, unbuilt) for the click-to-warp half, and — for hover info to mean anything — the same
`ctx.diplomacy`/`ctx.discovery` wiring group 3 already tracks. The territory-blob rendering itself
(no individual objects at Level-1 scale) is the part finding 4 actually flagged as broken *today*,
and is buildable now, independent of both blockers — ship that part first.

---

### 12.36 Player Spawn And Respawn Placement — Exit The Nearest Friendly Docking Bay

*Settled 2026-08-18, filed as the issue tracked below. Found during #133's M1 verification pass:
`SpaceFlight::OnEnter()` places the player's first spawn at `Vec2{0.0f, 0.0f}` — the sun's exact
position (`WorldGen.cpp`'s `SpawnSun`) — inside `kCoronaRange`, where `HazardSystem::ApplyCorona`
computes full-strength falloff at distance 0 and starts burning the rig before the player can react.
`SpawnSystem::FindSafePlacement`, which #159's respawn producer will call once built, has the same
class of gap: it ring-searches outward from the nearest `SpawnAnchor` (the station's root) with no
regard for what a safe position near a station should actually look like, when a much better answer
— the station's own docking bay — already exists next to it.*

**The rule:** whenever the player needs to appear in a system — the first `OnEnter()`, or a death
respawn — placement resolves in this order:

1. **Exit the nearest friendly `DockingBay`.** The same same-faction test as
   `DockingSystem::FindEligibleBay` (`FactionRef` equality against the bay's `ParentRig` root), but
   unranged — there is no "already near it" precondition for a fresh spawn. The position offsets
   outward from the bay hardpoint's `WorldTransform` along the vector from the station root to the
   bay (the same radial-outward idea `FindSafePlacement`'s ring already uses, just anchored to the
   bay instead of the station center), so the ship reads as having just launched from the bay rather
   than materializing on top of the station's hull. If that exact point is contested, fall back to
   `FindSafePlacement`'s existing ring search — centered on the bay-exit point, not the station root.
2. **No friendly `DockingBay` in the system: fall back to the nearest `SpawnAnchor`'s ring search**
   (the pre-existing behavior). **If neither a friendly bay nor any `SpawnAnchor` exists at all,**
   a respawn is left pending and retried the following tick — the same "no anchor yet" behavior
   the code already had, now also covering "no bay yet" — while `OnEnter()`, which has no tick to
   retry on, falls back to its own reference position at the call site. Showing the player warping
   in for this case has no visual anywhere today — `WarpToSystem` swaps `SystemWorld`s
   instantaneously, with no animation anywhere in `render/`. **Named here as an explicit gap**
   rather than silently shipping a teleport and calling the requirement met (§2.4);
   `playable_roadmap.md`'s **P9-10** now owns building it.

**Why this replaces the ring-search wholesale, not just for `OnEnter`:** a respawning player and a
first-time player are the identical event from the placement algorithm's point of view — "the player
needs to exist somewhere in this system, safely" — and #159's respawn producer was always going to
call the same placement `OnEnter` should have been calling from the start. One algorithm, two call
sites (`SpaceFlight::OnEnter()` and `SpawnSystem::ResolveRespawns`), the same shape `SpawnPlayerInto`
already gives `OnEnter`/`WarpToSystem`'s shared spawn body (§12.24 step 1).

#### Types

No new component. `DockingBay` (`shared/components/Docking.h`) and `SpawnAnchor`
(`shared/components/Spawn.h`) are unchanged — `SpawnAnchor` keeps its separate job of giving
`CullFarRigs` something to measure distance against; it stops being the *placement* target but stays
the *cull* reference.

#### Systems

- `modes/space/systems/SpawnSystem.cpp`: `FindSafePlacement` gains the `DockingBay`-first resolution
  above; its existing ring search becomes the two fallback tiers (a contested bay-exit point, then no
  bay at all) rather than the only tier.
- `modes/space/SpaceFlight.cpp`: `OnEnter()` stops hardcoding `Vec2{0.0f, 0.0f}` and instead resolves
  a placement the same way — populate the system first, then resolve before calling
  `SpawnPlayerInto`, mirroring how `WarpToSystem` already receives a `spawnPosition` computed by its
  caller rather than assuming one.

#### Tests

A fresh `OnEnter()` places the player outside `kCoronaRange`, at a friendly `DockingBay`'s exit
point, never at the sun's position. A respawn resolves the same way. A bay whose exit point is
contested resolves to the nearest clear ring position around that bay, not around the station root.
A system with no station, or no friendly one, falls back to a hazard-clear placement and does not
corona-burn the player on arrival.

#### Scope

**This section does not build a warp-in cinematic.** The no-bay fallback is placement-only; the
visual `WarpSystem`/`SpaceFlight::Draw` would need to actually *show* an arrival is unscoped here.
Flagged rather than left implicit, per §2.4, and now owned: `playable_roadmap.md`'s **P9-10** covers
a shared arrival effect for a docking-bay exit, a no-bay respawn, and a system warp alike.

---

*Compiled 2026-08-09, by grepping for readers and callers rather than by reading schemas. Every
row below was verified against `src/`; nothing here is inferred from a header comment, and where a
header comment and the code disagree, the disagreement is recorded as a finding.*

**Why this section exists.** §0's 🚨 block establishes that there is no game loop. This section
answers the next question: *of the thirty scheduled systems ticking over that empty world, which
ones would actually do anything once it is populated?* The answer is **eight of thirty**, and the
remaining twenty-two fail for reasons that fall into four repeating shapes. The audit is durable
here rather than in a chat log because a row with a gap in any column **is a task**, and §13.5
collects them.

### 13.0 How to read a row

| Column | Question it answers |
|---|---|
| **Producer?** | Does anything in `src/` create the input this system consumes? *(tests do not count — a test-only producer is the defect)* |
| **Consumer?** | Does anything read what this system writes? |
| **UI surface?** | Can a player see or trigger it? |
| **Content?** | Does `data/base_game/` author what it needs? |
| **Matches docs?** | Does the behaviour match `features.md`/this document? |

✅ wired · ⚠️ partial · ❌ missing · — not applicable

> **The dominant defect is not absence of code.** Twenty-two systems are complete, tested, and
> scheduled. They fail on one missing `emplace`, one hardcoded literal, or one component the
> factory never attaches. Almost every row below is a small fix in a system that already works.

### 13.1 The thirty scheduled systems

Listed in `TickSchedule()` order, so the ordering findings in §13.3 read against the same sequence.

| System | Producer? | Consumer? | UI surface? | Content? | Matches docs? |
|---|:---:|:---:|:---:|:---:|:---:|
| **WarpSystem** | ❌ `WarpRequest` test-only; `SystemWarpRequest` has no producer either | ✅ `SpaceFlight::Update` drains `SystemWarpRequest` | ❌ | — | ⚠️ no fuel, no module, no charge time (§2.11) |
| **HierarchySystem** | ✅ `RigFactory` | ✅ everything reads `WorldTransform` | — | ✅ | ❌ **runs before `PhysicsSystem`** — every hardpoint is one tick stale (§13.3 G) |
| **ConstructionSystem** | ❌ `BuildStationRequest`/`PlaceShipRequest` test-only | ✅ spawns rigs | ⚠️ `BuildMenu` builds the request; nothing places it | ✅ | ❌ no facility gate, no range gate (§12.26) |
| **ModuleEquipSystem** | ❌ `MountModuleRequest`/`UnmountModuleRequest` test-only | ✅ writes `EquippedModule` | ⚠️ `ModulesMenu`, unrouted | ✅ | ❌ **traverse hardcoded `0.0f`**; no `BodyMass`/`Propulsion` recompute; `EquippedModule` ≠ `MountedModules` (§13.3 C, D) |
| **PowerSystem** | ✅ `RigFactory` → `PowerSource`/`PowerLoad` | ⚠️ `satisfaction` read by Physics/Weapon; **`PowerShed` read by nobody** | ❌ no category switches, no priority menu | ❌ no level multipliers authored | ❌ **implements roughly a third of §2.9 and none of the commanded half** (§13.3 X) |
| **SpawnSystem** | ❌ **`SpawnAnchor` and `RespawnPending` both have zero producers** | ✅ | ❌ | ❌ nothing authors an anchor | ⚠️ fully inert: no respawn, and the 20 000-unit cull never runs |
| **OrbitSystem** | ✅ `WorldGen` → `OrbitBody`/`GravityWell` | ✅ writes `WorldTransform`/`Velocity` | ❌ planets are never drawn | ⚠️ every planet mechanically identical | ⚠️ gravity also pulls projectiles; stations have no damping (§13.3 J) |
| **PhysicsSystem** | ✅ | ✅ | ⚠️ | ✅ | ⚠️ `ThrustInput` has no player producer (§0 ③) |
| **DockingSystem** | ✅ `AvionicsMenu` — **the only gameplay input in `src/`** | ✅ | ✅ prompt + key | ✅ `docking_bay_i` | ❌ **free unlimited repair**, undercutting `StationServicesSystem` (§13.3 I); faction-equality gate (§5.3) |
| **EngineerSystem** | ❌ `MergeModulesRequest` test-only | ✅ | ⚠️ `EngineerMenu`, unrouted | ❌ no Engineering facility authored | ❌ `ctx.craftedModules` is `nullptr`; **merge level always 1** (§13.3 K) |
| **RefactorSystem** | ❌ `DeleteHardpointRequest` test-only | ✅ | ⚠️ `RefactorMenu`, unrouted | ❌ no Engineering facility | ⚠️ no aggregate recompute; can delete the last hardpoint and kill the rig |
| **StationServicesSystem** | ❌ all three requests test-only | ✅ | ⚠️ `StationServicesMenu`, unrouted | ❌ no Repair/Storage facility | ❌ **no station carries `CargoHold`** — buy/sell can never succeed (§13.3 P); repair gated on `Docked` alone |
| **TargetingSystem** | ✅ `RigFactory` | ✅ `WeaponSystem`, `NpcAiSystem`, `IconRenderer` | ✅ reticle | ✅ | ❌ auto-locks for the player (§3.2 forbids); `IsHostile` = faction inequality, `DiplomacyMatrix` never consulted (§13.3 O) |
| **NpcAiSystem** | ✅ | ✅ | ✅ | ✅ | ❌ **ignores `Docked`** — a docked NPC thrusts and fires (§13.3 H) |
| **WeaponSystem** | ⚠️ `FireIntent` from `NpcAiSystem` only | ✅ spawns projectiles | ✅ | ⚠️ one NPC ship's gun has a zero-width arc (§13.3 W) | ❌ `FiringArc::currentOffset` never steers the shot; `turnRatePerSecond` hardcoded `kPi`; `PowerShed` unchecked; fires while docked |
| **CollisionSystem** | ✅ `RigFactory` | ✅ `PendingDamage` | ⚠️ no visual feedback | ✅ | ⚠️ rig-vs-rig only: **asteroids, planets and the sun have no collision at all** |
| **ProjectileSystem** | ✅ `WeaponSystem` | ✅ `PendingDamage` | ✅ tracers | ✅ | ⚠️ `FindHit` requires `ParentRig` — projectiles pass through asteroids (§13.3 B); iteration-order hit, not nearest (§9.1 flags this as required) |
| **PartySystem** | ❌ **`PartyLeader`/`PartyMember` have zero producers** | ✅ | ❌ | — | ✅ correct once §12.27 supplies membership |
| **DamageSystem** | ✅ | ✅ | ⚠️ hull bar only | ✅ | ⚠️ zeroes `Propulsion` destructively — repairing an engine never restores thrust |
| **TutorialSystem** | ❌ **`Tutorial` has zero producers** | ✅ | ❌ nothing draws a step | — | ⚠️ `DestroyAsteroid` unreachable regardless (§13.3 B) |
| **MiningSystem** | ❌ **nothing can damage an asteroid** (§13.3 B) | ✅ `MaterialDrop` | ❌ drops are invisible | ❌ untyped string ids against no registry — and `"silica"` **is not an element** (silicon is; silica is SiO₂) | ❌ no gathering module, no range stat; needs the `Element` rename (§13.5 group 2b) |
| **ContractSystem** | ❌ `AcceptContractRequest` test-only | ✅ `Wallet` | ❌ no contract board | ❌ | ⚠️ Courier/reputation deferred by its own header |
| **DistressSystem** | ❌ both requests test-only | ✅ `Wallet` | ❌ | — | ✅ |
| **LootSystem** | ⚠️ `MaterialDrop` (unreachable), `DeathWreck` (warp only); **`LootDrop` and `DerelictWreck` have zero producers** | ✅ `CargoHold`/`Wallet` | ❌ **every drop type is invisible** (§13.3 A) | — | ⚠️ `extraRadius` only ever `0.0f`; NPCs cannot loot |
| **CommsSystem** | ❌ `HailRequest` test-only | ❌ **`CommsLog` is write-only — nothing draws it** | ❌ | ✅ | ⚠️ gated on `SensorRange`; §12.27 moves it to `commsRange` |
| **FactionEconomySystem** | ❌ `DepositRequest`/`SpendRequest` test-only | ✅ `SpendResult` — also read by nobody | ❌ | — | ✅ |
| **DiscoverySystem** | ✅ `PlayerControlled` | ✅ `DiscoveryState` | ❌ `NavigationMap` is uncalled | — | ❌ `ctx.discovery` is `nullptr` |
| **CommanderSystem** | ❌ **`Commander` has zero producers** | ❌ **nothing reads `Commander::orders`** | ❌ | — | ⚠️ dead in both directions until §12.27 |
| **ResearchSystem** | ❌ **`StationFacility` has zero producers** — no station carries one | ✅ `KnowledgeStore` | ❌ no menu at all | ❌ no Research facility | ❌ `ctx.knowledge` is `nullptr`; `researchTier` never authored |
| **TemplateMarketSystem** | ❌ `PitchTemplateIntent` has zero producers | ✅ | ⚠️ no longer `CustomizeMenu` — `features.md` §2.6 (2026-08-11, corrected same day) moves the producer to the Trade facility's sell popup, triggered by selling a Template Chip item. `CustomizeMenu` still only saves the network entry (§12.9, unchanged); the chip is burned by a Manufacturing **Queue** job (§12.30.8), not by `CustomizeMenu` or Draft | — | ❌ `ctx.diplomacy` is `nullptr`, so `PassesGate` returns `false` unconditionally — **a no-op even if an intent arrived** |

**Eight systems are wired end to end today:** `HierarchySystem`, `PowerSystem`, `OrbitSystem`,
`PhysicsSystem`, `TargetingSystem`, `NpcAiSystem`, `CollisionSystem`, `ProjectileSystem` — plus
`DamageSystem` and `WeaponSystem` on the NPC path only. Every one of them is in the combat/motion
core, which is exactly the set §10's vertical slice exercised. **Nothing outside that slice has a
producer.**

### 13.2 Everything else that ticks, draws, or was expected to

| Unit | Producer? | Consumer? | UI surface? | Content? | Matches docs? |
|---|:---:|:---:|:---:|:---:|:---:|
| **`WorldGen`** | ❌ **called only from `WarpToSystem`**, never `OnEnter` | ✅ | ❌ **none of its output is drawable** (§13.3 A) | ⚠️ 3 ships, 7 modules, 7 shells, 1 facility | ❌ spawns no station (§12.24 step 4) |
| **`RigFactory`** | ✅ | ✅ | — | ✅ | ⚠️ emplaces no `CargoHold`, no `Wallet`, no `ActorRef`; `SensorRange` hardcoded `2000.0f`; `mobile` gates `Propulsion` (§12.25) |
| **`StationFactory`** | ❌ reachable only from `ConstructionSystem` | ✅ | — | ✅ `aegis_outpost` | ❌ six-line pass-through: adds **no** station-specific component — no `StationFacility`, no `CargoHold`, no `SpawnAnchor` |
| **`NpcFactory`** | ✅ `WorldGen` | ✅ | — | ✅ | ✅ |
| **`WorldRenderer`** | ✅ | — | ✅ | ⚠️ placeholder shapes, no art | ❌ **draws only rig roots, hardpoints and projectiles** — no sun, planets, asteroids, drops or wrecks |
| **`LightingPass`** | ✅ `WorldGen`'s `LightSource` | ✅ `WorldRenderer` | ✅ | ✅ | ✅ |
| **`IconRenderer`** | ✅ | ⚠️ `DrawMapMarker` called only by the uncalled `NavigationMap` | ✅ reticle | — | ❌ **renders the auto-lock §3.2 forbids** (§13.3 Q); no camera-AABB cull or icon substitution (§9.1 requires both) |
| **`FactionDecisionEngine`** | — | ❌ **zero callers in `src/`** — not scheduled, not called from any mode | ❌ | — | ❌ §0 lists it as built beside the thirty scheduled systems; §4 names it `FactionDecisionSystem`. It is a tested library with no invocation path |
| **`core/serialization/`** | — | ❌ **zero callers outside its own directory and tests** — `SaveFile`, `SaveMigrator`, `BlueprintSerialization`, `KnowledgeSerialization` | ❌ no save, no load, no menu entry | — | ❌ **and it could not save a game if called** (§13.3 Y) |
| **`main.cpp`'s mode loop** | — | ✅ | ❌ **the transition to `SpaceFlight` is one-way** — `QuitRequested()` is only checked while the menu is active, so the only exit is closing the window | — | ❌ `features.md` §3.6 already specifies a pause menu on `Esc` (§12.29) |
| **`core::diplomacy::Territory`** | — | ❌ zero users outside `core/` | ❌ | — | ❌ |
| **`core::diplomacy::DiplomacyMatrix`** | ❌ zero writers | ⚠️ read only by the unreachable `TemplateMarketSystem` | ❌ | — | ❌ the exact failure §5.3 was written to prevent |
| **`TickCoarse`** | — | ❌ **three definitions, zero callers** — `RunTick` calls only `tick` | — | — | ❌ §1.1's LOD tiers have an interface and no driver |
| **The nine menus** | ❌ none places the request it builds | — | ❌ **none is referenced outside its own TU and tests**; none handles input | ⚠️ | ❌ §12.24 step 5, settled in §12.30 |
| **`InventoryGrid`** | ✅ `StorageMenu`, `ModulesMenu` | — | ❌ | — | ❌ **the shared row widget has two verbatim clones** — `StationServicesMenu.cpp` and `RefactorMenu.cpp` each re-implement its `kRowHeight = 20.0f` loop, and `EngineerMenu.cpp` hardcodes offsets. Four copies of one widget across six files nothing calls (§12.30) |
| **`FacilityStats::kind`** | ✅ `ParseFacilityStats` | ✅ `FacilityRef` | — | ⚠️ one facility authored | ❌ **`OptionalEnum`, not `Require`** — a `"facility": {}` block silently becomes the enum's default. Finding W's class, applied to the facility's *identity* (§12.30 🐛) |
| **`CockpitHud` · `AvionicsMenu` · `BridgeView`** | ✅ called from `SpaceFlight::Draw` | ✅ | ✅ | ✅ | ⚠️ `BridgeView::kAllKinds` omits `Engineering`; no tab selection |

### 13.3 What this audit found that §0's list did not

Twenty-two new findings, verified. They are ordered by how much they collapse when fixed — the
first four each unblock several systems at once.

#### A · `WorldGen`'s entire output is invisible, and half of it is inert

`WorldRenderer::DrawShips` views `WorldTransform + PreviousTransform + CollisionRadius`;
`DrawHardpoints` views `ShellRole + HitRadius`. `WorldGen` gives its sun `WorldTransform`,
`GravityWell`, `LightSource`; its planets `OrbitBody + WorldTransform`; its asteroids
`Asteroid, Health, WorldTransform, Velocity, AsteroidComposition`. **Not one of them satisfies
either view.** The sun, every planet, and every asteroid are drawn by nothing. So are
`MaterialDrop`, `DeathWreck` and every other loot entity, all of which are created with a bare
`WorldTransform`.

Worse for asteroids: `PhysicsSystem` views `WorldTransform + Velocity + BodyMass`, and an asteroid
has no `BodyMass`. **The drift velocity `WorldGen` rolls for each one is never integrated** — and
neither is the gravity `OrbitSystem` adds to it every tick. Asteroids are stationary, invisible,
and accumulating velocity nobody reads.

> This is why §12.24 step 1 is not sufficient on its own. Calling `PopulateSystem` from `OnEnter`
> produces a world the player cannot see. **A renderable/collidable component set for
> non-rig world bodies is a prerequisite for step 1 being judgeable**, not a follow-up — and it is
> the cheapest possible version of §3.5's scale work, which needs a `hullRadius` concept that does
> not exist anywhere in the codebase yet.

#### B · Mining is unreachable — not "unstatted", unreachable

`MiningSystem` acts on `Asteroid + AsteroidComposition + WorldTransform + Destroyed`. The only two
producers of `PendingDamage` — the only path to `Destroyed` — are:

- `ProjectileSystem::FindHit`, whose view requires **`HitRadius, WorldTransform, ParentRig`**
- `CollisionSystem`, whose view requires **`Rig, CollisionRadius, BodyMass, Velocity, RamCooldown`**

An asteroid carries none of those. **Nothing in this codebase can damage an asteroid.** Four
things die behind that one gap: `MiningSystem` entirely, `TutorialSystem`'s `DestroyAsteroid` step,
every `MaterialDrop`, and `LootSystem`'s material path. Giving asteroids `HitRadius` (and a
`ParentRig` self-reference, or relaxing `FindHit`'s view) fixes all four at once. §0's note that
*"MiningSystem reads no module stat"* is true and understates it by an order of magnitude.

#### C · `MountedModules` and `EquippedModule` are two unreconciled answers to one question

`RigFactory` writes **`MountedModules { ids }`** on every hardpoint at spawn. `ModuleEquipSystem`
writes and reads **`EquippedModule { id }`**, and treats *absence* of it as "this mount is empty."
`ModulesMenu::EquippableMounts` uses the same predicate. Consequences, all live:

- **Every factory-authored hardpoint reads as an empty mount** in `ModulesMenu`.
- A `MountModuleRequest` aimed at an occupied hardpoint **passes the occupancy check**, then
  `AttachModuleComponents` `emplace_or_replace`s over the authored module's `Weapon`/`Shield`/
  `PowerLoad` — silently destroying it without returning it to cargo.
- `RefactorSystem` returns `MountedModules::ids` to cargo on delete and never looks at
  `EquippedModule`, so a runtime-mounted module is destroyed with the hardpoint.

One of the two must go. `MountedModules` is the more general shape (a mount may hold several) and
already has the `RefactorSystem` consumer; `EquippedModule` is the one with the equip/unequip
lifecycle. **Recommendation: keep `MountedModules` as the single record and delete
`EquippedModule`**, since a hardpoint's contents is a list either way and the factory path is the
one that must never diverge from what `RefactorSystem` refunds.

#### D · Every runtime-mounted weapon gets a zero-width firing arc

`ModuleEquipSystem.cpp:50` calls `AttachModuleComponents(registry, mount, *module, 0.0f)` — the
fourth argument is `mountTraverseRadians`, hardcoded. `RigFactory` passes the authored
`mount.traverseRadians` there. So a weapon mounted at runtime gets `FiringArc{halfWidth = 0}`, and
`AimAt`'s `withinArc` test (`|rawOffset| <= 0.0f`) is a float equality that will essentially never
hold. **A live-refitted weapon never fires.**

This cannot be fixed inside `ModuleEquipSystem` as written: the mount's authored traverse lives on
`MountBlueprint`, which is not stored on the hardpoint entity. It needs either a
`MountTraverse { float radians; }` component written by `RigFactory`, or a blueprint lookup through
`BlueprintRef` + `MountRef`. **The component is the right answer** — `RigFactory` already has the
value in hand, and it makes the hardpoint self-describing the way every other rig attribute is.

> This is load-bearing for `features.md` §2.7's live refit, which is now sanctioned combat play.
> It joins §12.23's `RecomputeRigTotals` and §12.25's `Propulsion` fix in the same issue.

#### E · `FiringArc::currentOffset` is a dead output

`AimAt` integrates `currentOffset` toward the target every tick and uses it to decide whether the
mount *may* fire. `SpawnProjectiles` then computes the shot direction as
`ToAngle(aimPoint - mountXf.position)` — the exact bearing to the target, ignoring `currentOffset`
entirely. Nothing renders it either. **Turret traverse gates whether a mount fires but never where
the shot goes**, so a turret that is 179° off aim and one perfectly on aim produce identical
projectiles the moment the gate opens. Same class as `PowerShed` below: simulated, then discarded.

#### F · `PowerShed` has zero readers

`PowerSystem`'s severe-overdraw branch sorts loads by shedding priority and emplaces `PowerShed` on
the losers. **Nothing anywhere reads that component.** `WeaponSystem` reads
`PowerBudget.satisfaction` (to slow cooldowns) and never checks whether the hardpoint was shed;
`DamageSystem`'s shield regen checks neither. So the entire load-shedding half of `features.md`
§2.9 is computed and thrown away, and a browning-out rig loses fire *rate* but never loses
*hardpoints* — which is the mechanic §2.9 is actually describing.

#### G · `HierarchySystem` runs before `PhysicsSystem`, so every hardpoint is one tick stale

The schedule places `HierarchySystem` second, justified as *"must be first of the rest; everything
below reads `WorldTransform`."* **Availability is not freshness.** `PhysicsSystem` moves the rig
root at position 8; hardpoint world transforms were computed at position 2 from the *previous*
tick's root pose. Everything downstream therefore reads stale hardpoint positions:

| Reader | What it gets wrong |
|---|---|
| `CollisionSystem::BuildWorldHull` | The convex hull trails the `CollisionRadius` broad-phase circle it was rejected against |
| `ProjectileSystem::FindHit` | Hit tests resolve against last tick's hardpoint positions |
| `WeaponSystem` | Muzzle position, range check and aim bearing all use the stale pose |
| `WorldRenderer` | Hardpoints render one tick behind the hull triangle they sit on |

At 60 Hz and a few hundred units/second this is single-digit units — invisible in a screenshot,
and systematically wrong in every hit test. **The fix is to move `HierarchySystem` after
`PhysicsSystem` and before `DockingSystem`**, which satisfies every constraint the current comment
block states. Nothing between positions 3 and 8 reads a hardpoint `WorldTransform`; the only
caveat is `WarpSystem`'s stated reason for running first, which is preserved.

#### H · `NpcAiSystem` ignores `Docked`

Its view is `Target + WorldTransform + ThrustInput`, `exclude<PlayerControlled>` — no `Docked`
exclusion. It runs at position 14; `DockingSystem` zeroes a docked rig's `ThrustInput` at position
9. **So a docked NPC has its throttle rewritten and `FireIntent` re-emplaced every tick**, and
flies out of the bay under thrust. `DockingSystem` removes `Targetable` from the docked rig, which
stops others targeting *it*, but leaves its own `Target` intact.

This is the same failure §12.24 step 2's ordering table predicts for the player — *"running input
after `DockingSystem` would let a docked player fly away… this is the constraint that would
silently break."* **It is already broken, for NPCs, today.** Excluding `Docked` in `NpcAiSystem`
and `WeaponSystem` is more robust than relying on order, and it is what makes §3.4's "a docked
vessel cannot be shot" symmetrical — right now a docked vessel cannot be shot *at* but can still
shoot.

#### I · Docking already grants free, unlimited, unconditional repair

`DockingSystem::HealAndImmobilize` restores **15% of max hull per second to every hardpoint of
every docked rig** — no facility check, no cost, no cap, ~6.7 seconds to full. `StationServices
System::ProcessRepairRequests` charges credits, scaled by a requested fraction, for the same
outcome.

§12.24 step 5a proposes gating the paid path on `FacilityKind::Repair`. **That leaves the free path
running**, which makes the gate meaningless and the paid service unsellable. This is a design
decision, not a mechanical fix: either docking-heals is the intended baseline and
`StationServicesSystem`'s repair is redundant, or repair is a facility service and
`DockingSystem`'s heal must be deleted. **Recommendation: delete the automatic heal.** It is a
verbatim port of legacy `DockRepair.h`'s `kDockHealPerSecond`, it predates the facility model
entirely, and free full repair at any bay removes the entire economic pressure §2.7 cites as the
credit sink that justifies deferring upkeep.

> ⚠️ **§12.30.4 supplies a third answer this framing excluded, and two more defects in the same
> path.** Deleting the heal is right; **deleting the *rate* with it is not.** `kDockHealPerSecond` is
> the only rate in the codebase that `FacilityStats::ratePerSecond` (*"Repair HP/s"* — parsed, merged
> by `EngineerSystem`, **read by nothing**) and `features.md` §2.7's Repair crew role (listed twice as
> ✅ *"Buildable now"*, consumer named as *"`DockingSystem`'s dock-repair **rate**"*) were both
> authored for. **The rate moves to the facility path rather than dying with the free heal.**
>
> The paid path is also worse than "charges credits for the same outcome": it has **no facility gate
> at all** — `DockedStation`'s result is used only as a docked-ness check — and it has **no `Destroyed`
> check**, so it heals a permanently dead hardpoint to full while the tag remains, which makes
> `features.md` §3.9's colour-is-condition schematic draw a destroyed mount green. **And deleting the
> free heal removes NPC repair entirely**, since `HealAndImmobilize` views `<Docked, Rig>` with no
> player filter and NPCs carry no `Wallet`. See §12.30.4.

#### J · Stations sit in the physics view with no damping

`RigFactory` emplaces `BodyMass` and `Velocity` on **every** root, and `LinearDamping` only when
`mobile`. `OrbitSystem`'s gravity loop excludes `OrbitBody`, not `Propulsion`. So a station inside
`kSunGravityRange` accumulates velocity every tick with nothing to bleed it off, and
`PhysicsSystem` integrates it. §12.24 step 4 describes this as gravity that *"drags a station that
has no engines"*; it is unbounded acceleration, not drag.

§12.25's fix — always emplace `Propulsion` **and `LinearDamping`** — resolves it as a side effect.
Worth recording because it moves §12.25 from "conceptual tidiness" to "prerequisite for step 4
placing a station anywhere near the star."

#### K · `FacilityStats::level` is never parsed and never copied

Two independent halves of one path are missing:

1. `BlueprintJson::ParseFacilityStats` reads `kind`, `ratePerSecond`, `capacity` — **not `level`.**
2. `AttachModuleComponents` constructs `FacilityRef` with **one** argument (`module.facility.kind`),
   so `level` takes its in-struct default of `1`.

`Facility.h` states *"Copied from `ModuleDef::facility.level` at attach time."* It is not, on either
side. `EngineerSystem`'s merge formula is therefore permanently `primary + secondary * 0.1` — a
flat 10% carryover regardless of authoring. §12.12 and `features.md` §2.4 both settle that merge
scales with **facility level, not engineer skill**; that decision is unimplemented while appearing
implemented, which is the most expensive kind of gap in this repository.

`FacilityStats::capacity` is parsed and read by nobody — docking bays have unlimited capacity.

#### L · `FactionDecisionEngine` has no invocation path at all

Not in `TickSchedule()` (it takes no `SystemContext`, by design), not called from `main.cpp`, not
called from `SpaceFlight`, not called from any system. Its six free functions are exercised only by
`FactionDecisionEngineTests.cpp`. §0 lists it among the built inventory and §4 names it
`FactionDecisionSystem`; **there is no macro tick for it to live in.** `core::diplomacy::Territory`
is in the same position — zero users outside `core/`.

This is the one finding that needs a *new* thing rather than a wiring fix: §1.1's Tier 2/3 coarse
loop. Which is the same hole as:

#### M · `TickCoarse` has three definitions and no caller

`CommanderSystem`, `DiscoverySystem` and `FactionEconomySystem` each expose the second entry point
`System.h` prescribes for LOD-spanning systems. **`RunTick` calls only `system.tick`**, and no
coarse schedule exists. §1.1's time-sliced LOD model has an interface, a convention, three
implementations — and no driver. Per §2.4 this is three dead abstractions, and the honest options
are to build the coarse loop or to delete the three functions until §1.1 is real.

#### N · `TargetingSystem` has no concept of neutrality

`IsHostile` is `!(seeker.id == other.id)` — **faction inequality**. Every rig not of your own
faction is a valid auto-acquired target, and `DiplomacyMatrix` is never consulted. `features.md`
§5.3–§5.6 specify a three-state relation model with fifteen named rivalries and five alliances;
none of it reaches combat. `DockingSystem` makes the same simplification and *documents* it as
predating `core/diplomacy/`; `TargetingSystem` does not.

Fixing this is blocked on §12.24 step 6 (`ctx.diplomacy` is `nullptr`), so it belongs in the same
issue as the step-6 pointer work rather than with the targeting changes. §12.32 specifies the
seeder that same pointer work needs regardless of the Reapers, plus their §5.7 exception.

#### O · A station cannot trade, because nothing gives a station a `CargoHold`

`StationServicesSystem`'s buy and sell paths both `try_get<CargoHold>(station)` and bail on
`nullptr`. `RigFactory` emplaces no `CargoHold` on any rig; `StationFactory` adds nothing at all.
So two of the three station services are gated behind a **missing component**, not merely a missing
request producer — wiring `StationServicesMenu` into the router would still trade nothing.

`StationFactory::Spawn` is a six-line pass-through to `rig_factory::Spawn` that only rejects
`mobile: true` blueprints. Every station-specific component the codebase expects — `StationFacility`
(`ResearchSystem`), `CargoHold` (`StationServicesSystem`), `SpawnAnchor` (`SpawnSystem`) — has
**zero producers anywhere**, and this factory is where all three belong.

#### P · `RigFactory` gives the player neither `CargoHold` nor `Wallet`

Only `WarpToSystem` emplaces them, on the rig it just spawned. A player created by §12.24 step 1's
`OnEnter` will have neither, and `ModuleEquipSystem`, `StationServicesSystem`, `EngineerSystem` and
`RefactorSystem` all bail on a null `CargoHold`. **Step 1 must emplace both**, or every
cargo-touching system silently no-ops for a fresh player and the failure looks like a UI bug.

#### Q · The forbidden target lock already has a rendered UI

`IconRenderer::DrawTargetReticle` reads the player's `Target` and draws a bracket on it, called
every frame from `SpaceFlight::Draw`. §12.24's fix (excluding `PlayerControlled` from
`TargetingSystem`) makes this draw nothing — which is correct, but the function should be
deliberately repurposed to the cursor aim point or deleted, not left to silently no-op.

#### R · `SpawnSystem` is wholly inert, in both halves

`SpawnAnchor` and `RespawnPending` both have zero producers. No respawn ever resolves, and
`CullFarRigs` never culls anything, because `FindNearestAnchor` returns `false` when no anchor
exists and the cull is skipped entirely. The 20 000-unit registry bound §1.1 relies on does not
exist at runtime.

#### S · `CommsLog` is write-only

`CommsSystem` maintains an 8-entry log on a singleton entity — the pattern §12.24 correctly cites
as precedent for UI state. **Nothing draws it.** The hail feature is complete from request to
formatted response string, and the response is unreadable.

#### T · `LootDrop` and `DerelictWreck` have zero producers

No module ever drops from anything. `LootSystem`'s module-pickup path and credit-reward path both
exist with no source. Only `MaterialDrop` (via the unreachable `MiningSystem`) and `DeathWreck`
(via `WarpToSystem`'s wreck-promotion) can appear at all — and both are invisible per finding A.

#### U · `SpendResult` and `KillCredited` join the write-only set

`FactionEconomySystem` emplaces `SpendResult` and nothing reads it. `KillCredited` is correctly
consumed by its own producer. Listed for completeness — `SpendResult` is a genuine dead output; a
faction spend that fails is indistinguishable from one that succeeded.

#### V · `RefactorSystem` can delete a rig out of existence

`ProcessDeleteRequests` refuses a hardpoint with a dependent child, but nothing prevents deleting
the *last* hardpoint. `DamageSystem`'s `HasLivingHardpoint` check then tags the root `Destroyed` on
the same tick. The legacy project's "minimum one hardpoint must remain" rule is not ported. It also
recomputes no rig aggregate — `BodyMass` and `CollisionRadius` keep the deleted hardpoint's
contribution forever, same gap as §12.23 records for `ModuleEquipSystem`.

#### W · One of the two authored NPC ships cannot shoot

`MountBlueprint::traverseRadians` defaults to `0.0f`. `forgotten_scrapper`'s only weapon mount
(`gun_nose`) omits the field, so it gets a zero-width firing arc and — by the same float-equality
path as finding D — essentially never fires. `aegis_vanguard`'s two wing mounts author `0.35`.

`aegis_outpost` authors **no weapon mount at all**, so the only station in the content set cannot
defend itself. Both are content fixes, but the zero-default is the underlying hazard: an omitted
`traverseRadians` should be a `Validation` error on a weapon-capable shell, not a silently
unusable gun.

#### X · `PowerSystem` implements a third of §2.9, and none of the half the player touches

*Recorded 2026-08-09, correcting this audit's own first pass, which scored this as the single
missing `PowerShed` reader in finding F. That understated it.*

| `features.md` §2.9 specifies | State |
|---|---|
| Four levels per **category** — Offline / Reduced / Normal / Boosted | ❌ nothing |
| **Draw and effect multipliers authored per module** (Law 10) | ❌ `modules.json` authors none; `ModuleDef` has no field |
| Player-configurable **power priority list** | ⚠️ `PowerLoad::priority` exists and is authored, but nothing reorders it |
| **Boost refuses without headroom** rather than browning out the ship | ❌ nothing |
| The afterburner as `Ctrl`-held engine boost | ❌ nothing |
| Four category keys `F`/`G`/`H`/`J` (§3.6) | ❌ nothing — one gameplay key exists in `src/` |
| Damage-driven throttle and shed | ✅ built — **and its shed output is read by nobody** |

So the built third is the part that reacts to *damage*, and the missing two-thirds is the part the
player *commands* — which is the entire point of the section, since §2.9 exists as the
**counterplay to hardpoint fragility**. Finding F's missing reader is real and still worth fixing on
its own, but repairing it does not deliver §2.9; it makes the fallback behave.

#### Z · The crew shell is designed in four sections and exists in none of the code

`ModuleKind` is `{Weapon, ShieldGenerator, PowerCell, Engine, Armor, Facility}`. `ShellKind` has no
crew value. **There are zero occurrences of "crew" anywhere in `src/`.**

Four settled designs depend on it, and all four are therefore unimplemented:

| Section | Depends on |
|---|---|
| `features.md` §2.7 | `ModuleKind::Crew` with rollable `operation` and `command` stats |
| `features.md` §3.2 | Destroying the crew shell disables a hull rather than destroying it |
| `features.md` §3.4 | *"The player is always associated with exactly one shell… if that shell dies, the player dies"* |
| §12.27 | Command requires *"a living `Crew` module with a non-zero `command` roll"* |

`NpcAiSystem` also has no crew check, so an "uncrewed" hull would keep flying and firing. **The
uncrewed hull is the capture path's precondition**, so §3.2's capture state cannot be reached at all
until this lands.

#### AA · Two of three damage types render identically

`WorldRenderer::ColorForProjectile` is `type == DamageType::Energy ? SKYBLUE : YELLOW`. §3.1 settled
**three** weapon types — Kinetic, Energy, Ion — so **Ion and Kinetic are visually the same shot.**

That is the worst pair to conflate: Ion is absorbed by neither shield type (`DamageSystem` absorbs
only on exact type match), so mistaking it for Energy means misreading whether your shields are doing
anything at all. `features.md` §3.9 sets the palette — Energy blue, Kinetic purple, Ion electric
white-blue — and requires it to be identical across projectiles, in-world shield shimmer, and the
status display.

#### AB · The drawn silhouette is larger than the hittable shape

`DrawShips` sizes its triangle from `CollisionRadius` — the rig's **maximum reach**, 27 on
`aegis_vanguard` — while `ProjectileSystem` tests hardpoint circles reaching 22 except behind. **Shots
pass through the drawn nose and flanks.** It reads as a hit-detection bug and is a placeholder-art
defect; `features.md` §3.5 now requires drawing the tested shape until real art exists.

Related and *not* a defect, but undocumented until now: `ProjectileSystem` tests hardpoint circles
individually while `CollisionSystem` convex-hulls them, so **a projectile can pass through a gap a
hull cannot**. That is physically correct — a shot is smaller than a ship — and is now recorded as
deliberate so it stops looking like an inconsistency.

#### Y · The save system has no caller — and could not save a game if it had one

`SaveFile`, `SaveMigrator`, `BlueprintSerialization` and `KnowledgeSerialization` are built, tested
and **invoked from nowhere.** §0 lists both "unified serialization" and "save schema migration"
among the ✅ inventory. The second half is worse than the first:

> **`SaveFile`'s entire API is `SaveShipBlueprint` / `LoadShipBlueprint` / `SaveKnowledgeStore` /
> `LoadKnowledgeStore`.** There is no world save. No registry serialization, no player rig state,
> no `Wallet`/`CargoHold`, no `FactionEconomy`, no `DiplomacyMatrix`, no `DiscoveryState`, no
> `WreckLedger`, no world seed.

`features.md` §3.3 **settled** the save model — free/manual saves plus a coarse autosave that never
fires on death, chosen specifically so Tier 2's recovery run survives. That decision has neither a
caller nor the capability underneath it.

**The practical consequence is a scoping one, and it is good news:** returning to the main menu is
*not* blocked on any of this (§12.29). Quit-to-menu needs a two-way mode transition and nothing
else. Save/Load is a separate, larger piece of work that has to define what a world save even
contains — and it should not be allowed to hold up the pause menu.

#### AC · Warping fully repairs your hull and undoes every refit

*Added 2026-08-10 by §12.30.2's parked-hull work. **This audit missed it on the first pass**, and the
reason is worth recording: §13's method is to catch places where a header comment and the code
disagree. Here they agree — `SpaceFlight.h:62` states the gap accurately — and neither this document
nor `features.md` ever learned what the comment says. **A truthful comment about a missing capability
is still a missing capability**, and a method tuned to find lies will not find it.*

`WarpToSystem` does `world_ = SystemWorld(targetSystemId)` — Law 2's clean handoff, *"nothing survives
this line except what was captured above"* — then re-generates the destination and **re-spawns the
player from their `BlueprintRef`**. Only `CargoHold`, `Wallet` and demoted `DeathWreck`s cross.

| Consequence | |
|---|---|
| **Every jump is a free, complete repair** | Finding I by a second route. That finding deletes free repair at a docking bay; this one hands out the same thing at every system boundary — on a `WarpSystem` §13.1 already scores as having no fuel cost, no module gate and no charge time |
| **Every jump rolls back the loadout** | `features.md` §2.7's live refit is silently undone at the boundary, so a refit only matters within one system |
| **Anything else parked in the system is destroyed** | Which is what §12.30.2's parked hulls need solved |

**✅ Specified 2026-08-10 in §12.31** — and the header comment's name for it turned out to be the
design error: the state form **must not be a `ShipBlueprint`**, because §12.21's per-instance
`Quality` cannot survive one and because adding health to `MountBlueprint` would have
`ManufacturingSystem` building pre-damaged ships. `RigState` is a delta against a `BlueprintId`.

**The missing capability is a live-rig snapshot**, and it is the same one §13.3 Y needs:
*"no registry serialization, no player rig state."* Four things wait on it — parked hulls, warp damage
persistence, cross-system refit, and the world save. **Scope it once**, rather than special-casing a
parked hull; the demote/promote half already has three working precedents in `core/galaxy/`
(`WreckRecord`, `ResearchRecord`, and §12.18's planned `ManufacturingRecord`).

### 13.4 Five decisions this raised — all now resolved by the project owner, 2026-08-11

Everything above except these has a correct answer that falls out of the existing design. All five
recommendations below are now decided, not merely proposed — this table is a build spec for §13.5's
task list, not an open question anymore.

| # | Question | Decision |
|:---:|---|---|
| **1** | ✅ **DECIDED 2026-08-11 — accepted as recommended.** Does docking heal for free? (finding I) A facility-gated repair service and an unconditional 15%/s heal cannot both exist | **Delete the automatic heal.** It predates the facility model and removes the credit sink §2.7 relies on. The **rate** moves to `FacilityStats::ratePerSecond` rather than being deleted with it — otherwise a dead field stays dead and `features.md` §2.7's Repair crew role loses its only named consumer. Deleting the heal also removes **NPC** repair, which re-homes onto `ctx.economy` |
| **2** | ✅ **DECIDED 2026-08-11 — accepted as recommended.** `MountedModules` or `EquippedModule`? (finding C) Two representations, three inconsistent readers | **Keep `MountedModules`, delete `EquippedModule`.** A mount holds a list either way, and the factory path must never diverge from what `RefactorSystem` refunds. **This is not a cleanup, it is a dupe-and-destroy fix.** `EquippedModule` is absent on blueprint-mounted hardpoints, so `modules_menu::EquippableMounts` offers **every occupied mount on a fresh ship as an empty slot**. Mounting there overwrites the original's live components; unmounting then **destroys** the original, and scrapping the hardpoint **duplicates** it. Both reachable from one click on a surface that ships in this batch — treat as a priority fix, not routine cleanup |
| **3** | ✅ **DECIDED 2026-08-11 — accepted as recommended.** Build the coarse loop, or delete `TickCoarse`? (findings L, M) §1.1's LOD tiers have three implementations and no driver, and `FactionDecisionEngine` has no home without one | **Delete the three `TickCoarse` functions now**, per §2.4, and reinstate them with the loop. `FactionDecisionEngine` is pure and loses nothing by waiting. Keeping them is the same "scaffolded, never adopted" pattern §0 opens with |
| **4** | ✅ **RESOLVED 2026-08-09 — see §12.28.** How do non-rig world bodies render and collide? (finding A) | `WorldBody { radius; BodyKind }` for drawing; **hittability needed no new component at all** — narrowing `FindHit`'s view is the whole fix. Asteroids orbit rather than fall, the belt moves to 1,800–2,800, and a star's `Corona` burns rather than killing at a line. Planets and stars stay non-colliding, deliberately |
| **5** | ✅ **DECIDED 2026-08-11 — accepted as recommended.** Is `traverseRadians = 0` legal? (finding W) It currently means "welded forward and unable to fire" | **Make it a `Validation` error** on a weapon-capable shell. A fixed-forward gun is a real design, but it should be authored as such, not reached by omission |

### 13.5 The task list

Grouped by what unblocks what. **Everything in group 1 is a prerequisite for judging anything
else**, which is §11.9's existing verdict re-derived from the wiring rather than from §0.

**Group 1 — the micro loop (§12.24 steps 1–4), amended by this audit.** One issue.
- `OnEnter`: `PopulateSystem` + `rig_factory::Spawn` + `PlayerControlled` + **`CargoHold` + `Wallet`** (P)
- `PlayerInputSystem` + `FlightControls` + `ActorRef` + the `ActorId` relocation (§12.24 step 2)
- `TargetingSystem` excludes `PlayerControlled`; `AimPoint`; **repurpose or delete `DrawTargetReticle`** (Q)
- Camera assignment; delete the duplicate `DrawWorld` at `SpaceFlight.cpp:145`
- `WorldGen` spawns a station; `StationFactory` gains `StationFacility` + `CargoHold` + `SpawnAnchor` (O, R)
- **§12.28 in full** — `WorldBody`/`BodyKind`, the narrowed `FindHit` view, asteroids on `OrbitBody`, the belt move, `HazardSystem` + `Corona`, `OrbitSystem`'s `PreviousTransform` write. Without it step 1 produces a world nothing can see and nothing can shoot (A, B)
- `kDockKey` moves off `E`

*§12.28 is large enough to split off as its own issue if group 1 gets unwieldy — it has no
dependency on the player existing, so it can land first and be verified by tests alone.*

**Group 2 — one-line and one-view corrections, independently startable today.**
- Move `HierarchySystem` after `PhysicsSystem` (G)
- `NpcAiSystem` and `WeaponSystem` exclude `Docked` (H)
- **`ProjectileSystem::FindHit` and `CollisionSystem`'s ramming-candidate view exclude `Docked`**
  (§15.1 finding 2, §12.34) — the same shape as the bullet above, closing the half of
  `features.md` §3.4's "cannot be shot" rule that `Targetable` removal alone doesn't cover
- `BridgeView::kAllKinds` gains `Engineering` + a `static_assert` on the enumerator count
- `ModuleEquipSystem` passes a real traverse via a new `MountTraverse` component (D)
- `WeaponSystem` reads `PowerShed`; `SpawnProjectiles` uses `currentOffset` (E, F)
- ~~`ParseFacilityStats` reads `level`; `AttachModuleComponents` forwards it (K)~~ →
  **`ParseFacilityStats` reads `grade`; `AttachModuleComponents` forwards it as `FacilityRef::grade`**
  (K, re-aimed by §12.19). Neither field is parsed today, so the two fixes cost the same and only one
  of them then has to be deleted — `FacilityStats::level` folds into `Grade`, as does
  `StationFacility::researchTier`
- `RefactorSystem` refuses the last hardpoint (V)
- **`RefactorSystem::ProcessDeleteRequests` refuses deletion of a hardpoint that still holds
  modules, instead of refunding them** (§15.1 finding 8) — `features.md` §2.2's settled reversal,
  tracked as a pending decision in §12.13 but never given its exact line until this audit. Lands
  beside the last-hardpoint refusal above — both are refusal conditions on the same function.
- **`RefactorSystem`'s storage-room check stops going through `CargoHoldHasRoomFor`'s
  count-not-mass bug** (§15.1 finding 9) — same underlying bug group 4b's `CargoHoldHasRoomFor`
  bullet already tracks; this names the previously-unlisted `RefactorSystem.cpp:78` call site.
- **`BuildMenu::CanAfford` reads `CargoHold`, not just `Wallet`** (§15.1 finding 15) — the file's
  own header already claims this; the code doesn't do it
- **`CustomizeMenu::Draw` iterates `ValidationResult::errors` and names which rule failed**,
  instead of rendering the literal string `"INVALID"` (§15.1 finding 21) — the data already exists,
  `§12.9`'s Tests bullet already requires this
- **Citation fix: `StationServicesMenu.h`'s promotion-note reference moves from §12.11 to §12.10**
  (§15.1 finding 23), matching `InventoryGrid.h`'s correct citation for the same note
- Content: `traverseRadians` on `gun_nose`; weapon mounts on `aegis_outpost` (W)
- **One `Destroyed` sweep across every hardpoint writer (§12.30.7), not three separate fixes.**
  `StationServicesSystem`'s repair heals a destroyed hardpoint, `RefactorSystem`'s scrap refunds its
  modules, and `ModuleEquipSystem`'s mount fits a module to it. **`DockingSystem` and `DamageSystem`
  are the only two writers that test the tag.** Sweep it once as a group-2 pass rather than three
  times in three screens' worth of issues.
- **The four research defects (§12.30.6), all against built and tested code.** `ResearchSystem::Tick`
  gains a **`FacilityKind::Research` gate** (it has none, so blowing the lab off a station does not
  stop the jobs in it); **`StationFacility::researchTier` is deleted** — it is a *third* tier system
  beside `FacilityStats::level` and `Grade`, written only by tests; `ResearchJob::cost` and
  `ResearchRecord::cost` are renamed **`durationSeconds`**, since they hold seconds in a codebase
  where every other `cost` is credits; and a null `ctx.knowledge` **freezes the job instead of
  erasing it**, because the current guard silently spends the input and produces nothing.
- **`RefactorSystem` returns nothing when scrapping a `Destroyed` hardpoint** (§12.30.5). One branch
  at `RefactorSystem.cpp:76`, which has no `Destroyed` check at all — so losing a hardpoint in combat
  currently costs a shell and nothing else, and every module it carried comes back intact.
- **The three repair defects (§12.30.4), all against built and tested code.** `ProcessRepairRequests`
  gains a **`FacilityKind::Repair` gate** (it has none — the station handle is used only as a
  docked-ness check) and a **`Destroyed` exclusion** (it has none — it heals a permanently dead
  hardpoint to full, which makes `features.md` §3.9's colour-is-condition schematic draw it green);
  `DockingSystem::HealAndImmobilize` **loses its heal loop** per §13.4 decision 1 and is renamed.
  *Do not hold these for group 4b — the `Destroyed` one is actively producing a display that lies.*

*(Finding B's asteroid fix moved into group 1 as part of §12.28 — it shares the `WorldBody` work.)*

> 🐛 **`Validation`'s message says "a mobile craft needs an engine."** `features.md` §2 retired *craft*
> and rules that **a vessel is a vessel, never a craft**. The docs were swept 2026-08-09; this string
> in `shared/blueprints/Validation.cpp` was not, because it is code. Fix it with group 2b.

**Group 2b — the `Element`/`Material` rename, and it must land in one pass.**
`features.md` §2 renamed the supply tiers on 2026-08-09: **`Element` is now the raw tier and
`Material` is the manufactured one**, which *flips the meaning* of names already in the code.
`CargoHold::materials`, `MaterialStack`, `MaterialDrop`, `MaterialChance`,
`AsteroidComposition::materials` and `MiningSystem`'s spawn path all hold what are now **Elements**.
A half-migrated tree where one word means both things is worse than either name alone, so this is one
commit or none. **Cheapest it will ever be** — no `materials.json` exists yet, so there is no content
to migrate. Fold in §13's finding that these are untyped `std::string` ids (`"iron"`, and a `"silica"`
that is not even an element) against no registry.

**Group 2c — new content-facing work the materials pass created.** None blocks the loop.
- **`tools/element_check`** — the roster validator (`features.md` §2.10): pairwise dominance, Pareto
  validity, role coverage, density spread. Runs beside the four existing structural checks, and is
  the first time §2.2–2.4's enforcement principle reaches **content** rather than code
- **A gathering module kind** — §13 records that `MiningSystem` reads no module stat; skimming and
  harvesting need the same one, so all three activities land together
- **A planet `type` flag** — `WorldGen.cpp` concedes *"every planet is mechanically identical aside
  from its orbit."* Gas giants must be distinguishable before skimming can target one
- **Nebulae** — a **system-level** hazard property (`features.md` §3.8), not a gathering site. Three
  parts, all cheap: a `Corona`-shaped volume `HazardSystem` already handles; an **`Inert` threshold**
  on the rig deciding zero-damage vs. burning; and a **semi-transparent haze render pass** placed
  *after* `DrawProjectiles` — **the case §12.28 predicted by name** when it ruled that a body wanting
  to draw in front of rigs is a new pass rather than a `BodyKind` value. Ordinary asteroids and gas
  giants sit inside; what is scarce is the ability to be there. **Content for built systems**
- **`core/economy/Pricing.h`** — free functions beside `FactionEconomy`, computing base value from a
  recipe and modulating it by local stock (`features.md` §2.10). **There is no pricing logic anywhere
  today**: `StationServicesMenu::BuildBuyRequest(module, cost)` takes cost as a *parameter*, so
  `BuyItemRequest::cost` is invented by whoever calls it — the same producer gap as the request itself.
  **All three signatures take an `ItemInstance`, not an `ItemId`** (§12.19): `BaseValue`, `LocalPrice`
  and `RepairCostPerHp`, the last of which reads §2.10's **Inert** attribute and is its only named reader
- ✅ **`tools/economy_sim` — built 2026-08-11, and it did its job.** §2.10's cost check counted one of
  **three** compounding knobs — quantity per grade, the 2→8 breadth of the grade table, and §2.8's
  input-grade chain — so the naive multiplier estimate was ~10⁴ rather than ×64. Running the actual
  tool settled the quantity-per-grade knob at **~1×** (not the ~2× working value), which keeps
  Common→Mythic module cost at a real but sane 14×, against ~28,672× at the old 2× value. `features.md`
  §2.10 has the full table. **What's still open:** the tool models the curve's *shape* parametrically,
  since no `elements.json`/`materials.json` content exists yet (§14.2) — it has no real content set to
  run against. `core/economy/Pricing.h` (below) and the content pass are what let it graduate from
  modeling the shape to pricing the actual authored set.
- **`FactionEconomy` widens from one scalar per faction to `(FactionId, StationId) → ItemId,
  quantity`** (§15.1 finding 11), per `features.md` §5.0/§12.20's already-specified shape. Not a
  wiring fix — the current math (`Deposit`/`Spend` on a bare `int`) would give wrong answers about
  blockades or Material Security even fully wired, since it has no notion of *what* is held or
  *where*. Lands with §12.20 whenever that work is picked up, not standalone.
- **`MiningSystem` gains a `core/galaxy/` depletion record**, the same shape as `WreckRecord` (§12.5),
  so a depleted asteroid stays depleted across a demotion/promotion cycle (§15.1 finding 13). A
  second, independent blocker behind mining beyond finding B's unreachability — §7.2's own boundary
  rule (*"a system that regenerates mutable state from a seed will silently undo player actions"*)
  is what this closes. Lands with whichever issue makes asteroids hittable (group 1, §12.28).

**Group 2d — the damage-model pass (`features.md` §3.2, §3.5, §3.9). One issue; it changes built code.**
- **Structural integrity** — derived aggregate, structural failure at ~30%, normalised display so the
  bar reaches a true zero. `CockpitHud::AggregateHullFraction` gains a second reader
- **Most-specific-wins hit resolution** in `ProjectileSystem::FindHit`, replacing first-in-iteration
  order — which §9.1 and §12.16 already list as requiring a nearest-hit fix
- **Structural coverage validation** — chassis plus armour must cover the hull envelope (§3.5 rule 12)
- **§12.22's cascade**, already designed and startable, lands with this rather than separately
- **Draw the tested shape** (§13.3 AB), and `ShellDef`'s optional baked collision polygon
- **The §3.9 palette**, fixing Ion and Kinetic rendering identically (§13.3 AA)
- **§12.33 in full: the `DamageType` enum gains `Ion`, the `DamageTypeEffect` table, and
  `DamageSystem`'s generic absorb/hull/power-split path** (§15.1 findings 1, 5) — this is what makes
  ram damage actually interact with shields (today it's tagged `Kinetic` but skips absorption
  entirely) and gives Ion somewhere to exist as a value at all. Same file (`DamageSystem.cpp`) as
  the structural-integrity and hit-resolution bullets above, so it lands in this pass rather than
  as a separate issue.
- **§12.34's cascade-destroy half: a dying rig's docked occupants route through `LootSystem`'s
  existing `DeathWreck` path, and its `CargoHold` spills as pickups** (§15.1 finding 3) — reuses the
  combat-death pipeline with a different trigger, not a new mechanic. The `exclude<Docked>`
  hit-testing half of the same finding is a group-2 one-liner, already listed there.

**Group 2e — the status display and flight HUD (`features.md` §3.9, §3.10).** Colour-is-condition /
glyph-is-identity, outline-encloses-coverage with dash-density charge, and fit-based LOD driven by
`StructuralAttachment`. Needs §3.1's shield coverage modes (startable today per §11.9) to have anything
but Personal to draw.

The flight HUD (§3.10) adds a rule worth naming here because it is `BridgeView`'s pattern one layer
out: **a HUD surface exists exactly when a living module provides it** — sensor, comms, crew-with-
command, construction, hyperdrive — with **fixed slots that disable rather than disappear**, since a
control vanishing mid-fight is §8.3's *"absence must never look like emptiness"* in miniature. It also
settles that there is **no radar**: sensor contacts are one data source rendered as screen-edge
indicators in combat and as the navigation map out of it. **No `RadarSystem`.**

**Blocked, and worth knowing why:** **§3.2's capture state** cannot be reached until the crew shell
exists (§13.3 Z). Ownership transfer itself is no longer an open design question — settled 2026-08-11
as boarding-in-place, corrected above at items 8 & 9's neighbor — but it still can't be *built* until
the crew shell (item 15/16, this section) lands. Disabling is complete without either.

**Group 2f — multi-scale territory navigation (`features.md` §8.1, §12.35). Splits by what's
actually blocked.**
- **Buildable now, and it's the part that's actively broken today:** `NavigationMap` renders
  territory aggregates instead of individual system markers at every Level-1 sub-scale (§15.1
  finding 4) — `features.md` §8.1 already forbids individual objects there, and today's code fakes
  the zoom difference with layout spacing instead of actually changing what it draws. Gives
  `core::diplomacy::Territory` its first consumer anywhere in `src/`.
- **`IconRenderer::DrawMapMarker` gains a camera/zoom parameter** (§15.1 finding 19) — not a
  one-line addition, the current signature has nowhere to put a cull or a `BodyKind` dispatch, so
  this is a real signature change every caller needs updating for.
- **`IconRenderer`'s markers become cached per-kind shapes instead of one immediate-mode circle**
  (§15.1 finding 20), closing the second, independent way §8.2's icon model is unmet.
- **Blocked on two prerequisites:** hover/click info gated by sensors needs group 3's
  `ctx.diplomacy`/`ctx.discovery` wiring — this is also where `NavigationMap`'s own relation-check
  gap belongs (§15.1 finding 17: `VisibleHostileRigs` never consults `DiscoveryState` or
  `DiplomacyMatrix`, the same simplification finding N already records for `TargetingSystem`, on a
  file that finding didn't cover). Click-to-warp needs `WarpSystem`'s hyperdrive-range gate, which
  §13.1 already records as entirely unbuilt (no fuel, no module, no charge time) — a prerequisite
  this group doesn't create and shouldn't duplicate.
- ✅ **`features.md` §9's scope question (one galaxy vs. literal multiverse) is decided 2026-08-11**:
  both — one galaxy for the base game, a literal (not relabeled) multiverse as future expansion scope.
  Never blocked anything mechanical here, and now the outermost tiers' labels are settled too.

**Group 3 — the five null pointers (§12.24 step 6), independently startable today.**
Unblocks `DiscoverySystem`, `EngineerSystem`, `ResearchSystem`, `TemplateMarketSystem`, and
`TargetingSystem`'s relation check (N). `TemplateMarketSystem` is a guaranteed no-op until
`ctx.diplomacy` is non-null, regardless of any producer work. **§12.32 rides with this group**,
not as a sixth null pointer but as what the fifth one needs to be correct rather than merely
non-null: the widened six-state `Relation` enum, `SeedBaselineRelations` / `SeedReaperHostility`
called at the same startup point that stops `ctx.diplomacy` being `nullptr`, and `Reputation`'s
matching threshold widening. Land it in the same issue — a live pointer into an unseeded matrix is
the same null-object failure one layer down.

Three more items ride with this group, none of them new null pointers, all of them "the pointer
alone isn't enough" the same way §12.32 already is:

- **`DiscoverySystem` writes `KnowledgeNetwork`, not `DiscoveryState`** (§15.1 finding 12) —
  `features.md` §8.3 already settled this (*"knowledge networks win... `DiscoveryState` cannot
  implement this section at all"*) and named the cost (`DiscoverySystem` rewrites, `SaveFile` drops
  its `DiscoveryState` section). This group is where a system reachable through `ctx.discovery`
  should already be writing the right store, not the one §8.3 retired.
- **`TemplateMarketSystem::PassesAccept` needs a real `archetypeFits` computation**, not a
  pre-set boolean nothing ever sets true (§15.1 finding 10) — becoming reachable via `ctx.diplomacy`
  doesn't fix this; it's a second, independent gap in the same system.
- **`ContractSystem`, `CommsSystem`, `FactionEconomySystem` gain the relation-writing trigger logic
  `features.md` §5.3 already names them for** (contract complete/fail, successful diplomacy,
  trade/blockade drift) **— §15.1 finding 14 found none of the three contain it at all**, which is
  stronger than "blocked on the null pointer": there's no dormant trigger waiting behind it. Scope
  this as its own line within the group, since it's new logic in three files, not a pointer fix.

**Group 4 — the docked screens (§12.24 step 5, completed in §12.30). Splits in two.**

- **4a — the shared widget layer. Startable today, nothing blocking.** `shared/ui/` gains `UiInput`,
  `Row`, and five widgets (`PanelFrame`, `ListView`, `Button`, `TabStrip`, `Gauge`); `sr_shared_ui`
  becomes `STATIC`; **`InventoryGrid` is deleted** and the four hand-rolled row loops in
  `StationServicesMenu`, `RefactorMenu`, `EngineerMenu` and `InventoryGrid` itself fold into one.
  Pure presentation over POD — **no player, no world, no station**, so like §12.28 it can land first
  and be verified headless. *Landing it first is what stops seven screens being written in parallel
  against no shared widget, which is how `InventoryGrid` came to have two verbatim copies before any
  consumer could be run.*
- **4b — the router and the screens. After group 1.** `PlayerLocation`; `AvailableTabs` returning a
  **`ScreenId` and the hardpoint entity**, not just the kind (§12.30.3); the seven screens; the
  request placement; the `Storage` → `Trade` enum swap and its six sites; **`ParseFacilityStats`'s
  `kind` becoming `Require`** (§12.30 🐛, and it belongs with §13.5 group 2's `level` fix in one
  parser pass); the facility content set — `Trade`, `Repair`, `Engineering`, `Manufacturing`,
  `Research` authored in `modules.json`, which today holds exactly one facility.
- **4b splits a third time at Manufacturing (§12.30.8).** The **Draft** half needs one retained
  field (`ConsumeSaveTemplateRequests` discards a blueprint body the intent already carries), one
  overlay rename (`RegisterCraftedModule` → `RegisterDraftedTemplate`, since §12.19 retires the
  module version) and one gate on `ConstructionSystem`. **The Queue half needs §12.19 in full plus
  §12.18's `ManufacturingSystem`** — the only new system in the whole batch. **Ship the tab with
  Draft alone**, which turns a built, tested and unreachable feature into a working one. *Three
  screens now split by verb, which makes it the batch's shape rather than three coincidences.*
- **4b splits again at the Market (§12.30.3).** The **Storage** half needs only §13.3 O's
  `CargoHold` on `StationFactory` plus the capacity fix, and ships with 4b. The **Market** half also
  needs §12.19's `ItemId`, `core/economy/Pricing.h` (group 2c), and group 3's `ctx.diplomacy` — for
  the relation-band docking gate *and* `features.md` §5.3's reputation price modifier. **Ship the tab
  with Storage alone** rather than holding it: a warehouse that does not deal is a complete screen and
  exercises the row model, the capacity check and the transfer path before a price exists.
- **Two group-2-shaped corrections this screen forces**, both one-liners with real consequences:
  `CargoHoldHasRoomFor` has **exactly one caller** (`RefactorSystem.cpp:78`) while four other systems
  write a `CargoHold` unchecked; and `BuyItemRequest`/`SellItemRequest` must **lose** their
  caller-supplied `cost`/`value` rather than have them filled in — a menu that names its own price is
  a client-authoritative wallet write (Law 9).
- **`BuildMenu` needs actual build content, beyond the `CargoHold` affordability fix already in
  group 2** (§15.1 finding 16) — today's `Draw` renders one hardcoded-cost, affordability-colored
  "BUILD" label and never calls the `BuildStationBuildRequest`/`BuildPlaceShipRequest` functions the
  file itself exports. A blueprint list, and the placement-mode hookup §12.12 already names as this
  menu's home, are what turn the label into a construction UI. Rides with 4b since it's a docked/
  command surface at the same maturity as the other screens here, not group 1's minimal loop.

*`StorageMenu` and `ModulesMenu` leave this group for `features.md` §3.10's overlay set (§12.30), and
ship on 4a's widgets rather than on a facility gate. **Both are specified in §12.30.7**, and the
loadout half carries a hard prerequisite: §13.4 decision 2, without which the overlay duplicates and
destroys modules on a fresh ship.*

**Two more one-liners land with 4b's `RefactorMenu`/`ModulesMenu` work**, found by §15's behavioral
pass rather than by wiring:
- `RefactorMenu::Draw` iterates *all* leaf hardpoints and renders the non-deletable ones as disabled
  rows, instead of omitting them from the list entirely (§15.1 finding 18) — the file's own header
  already promises "greyed out," and the omission is the exact §8.3 failure ("absence must never
  look like emptiness") inside the one file whose comment says it does the opposite. Each row also
  gains its real name/`ShellRole` glyph instead of the literal string `"hardpoint"` (finding 22).
- `ModulesMenu::Draw` calls `EquippableMounts` as well as `EquippedMounts` (§15.1 finding 24) — the
  function is already exported and already correct, `Draw` just never calls it, covering roughly a
  third of §12.30.7's required surface today.

**Group 5 — §13.4's decisions, all now settled 2026-08-11, so this group is buildable as-is rather
than blocked on a call.**
Docking heal (1) · module-record unification (2) · `TickCoarse` (3) · world-body model (4, folded
into group 1) · traverse validation (5).

**§6's Simulation Decision Engine has two gaps waiting on the same `TickCoarse` decision, found by
§15 rather than by §13's wiring pass**, worth scoping alongside decision 3 rather than rediscovering
later: **§6.2's entire archetype-weighting table has no code at all** — three of `ComputeFacets`'
four operational facets are permanently `50.0f`, so no faction ever branches on its archetype
(§15.1 finding 6); and **§6.4's border-skirmish mechanic has no function to call**, not merely no
caller — `RaidDispatchChance` exists and matches its threshold exactly, but there is nothing
named `Border`/`Skirmish` anywhere in `src/` for the second of the section's two tuned constants
(finding 7). Both are scope for whichever issue builds out the facet engine once `TickCoarse` has
a driver, not one-line fixes.

**Deferred by this audit, deliberately:** `PartySystem`, `CommanderSystem`, `ContractSystem`,
`DistressSystem`, `CommsSystem`, `TutorialSystem` and `SpawnSystem` all wait on producers that
§12.26/§12.27 or a contract-board feature will supply. They are correctly built and correctly
inert; **do not "fix" them by inventing a producer in the system itself**, which is how
`CustomizeMenu::ConsumeSaveTemplateRequests` became a Law 9 violation.

**§15's findings 25 and 26 are deliberately not given a task-list line.** Finding 25
(`EvaluateColonization`'s threshold has no `features.md` backing) is the audit running in reverse —
undocumented code, not incorrect code — and the fix, if any, is a documentation one, not an
implementation one. Finding 26 (`WorldRenderer::DrawHardpoints` has no draw-layer sort) confirms
`features.md` §3.5's five-layer model is exactly as unbuilt as its 📋 status already says, with
nothing partial to reconcile — whoever eventually builds that feature has an accurate starting
description waiting, which is what §15 exists to leave behind, not a task to schedule twice.

---

## 14. Coverage Beyond §13

*Compiled 2026-08-10. §13 audits the thirty scheduled systems against `src/`. It does not ask
whether the document describing them is itself still accurate, whether `data/base_game/` can
support what the code already expects, or what sits outside `modes/space/` entirely. This section
checks those three things, the same way — against the repository, not against a summary.*

### 14.1 §4's system inventory has five stale 📋 rows

`SystemSchedule.cpp:90-123` registers exactly thirty systems. Five of them —
`ResearchSystem`, `CommanderSystem`, `TemplateMarketSystem`, `StationServicesSystem`,
`ModuleEquipSystem` — are `#include`d, scheduled, and scored with real producer/consumer detail in
§13.1. §4's table still marks all five 📋 **Planned**, which the Status Legend defines as
*"designed, agreed, not yet written."* They are written. `ManufacturingSystem` and `HazardSystem`
are correctly 📋 — neither appears in `SystemSchedule.cpp` — so the table isn't wrong throughout,
only on the five rows §12's first two batches landed.

This is the exact class of defect §0 says this document watches for: *"where a header comment and
the code disagree, the disagreement is recorded as a finding."* §4 predates §13 and was never swept
after it. **Fixed above, in the same pass that found it** — the five rows now read ✅ *(scheduled;
see §13.1 for wiring gaps)*, since ✅ here means *scheduled*, not *wired*, and §13.1 remains the sole
authority on whether each one does anything.

### 14.2 The content pipeline has three files, and a fourth category doesn't exist

`data/base_game/` holds exactly `ships.json`, `shells.json` (74 lines), and `modules.json`
(87 lines) — no `facilities.json`, no `materials.json`, no `elements.json`. Individual findings
already note this piecemeal (§13.1's `ResearchSystem` row: *"no Research facility"*; §13.5 group 2b:
*"no `materials.json` exists yet"*; group 4b's facility content set), but nowhere is it inventoried
as one fact with one blast radius:

**Every facility-gated system in §13.1 is content-blocked, independently of its code-wiring gap.**
`ResearchSystem`, `EngineerSystem`, `RefactorSystem`, `StationServicesSystem` all gate on a living
`FacilityKind` hardpoint, and `modules.json` authors exactly one facility, of an unstated kind. Even
after every producer/consumer fix in §13.5 lands, none of these four can be exercised in a generated
world until `modules.json` grows `Trade`/`Repair`/`Engineering`/`Research` facility entries — the
same content debt group 4b already schedules, restated here as a standalone blocker rather than a
line inside four other groups.

**`Element`/`Material` (§13.5 group 2b) has no content to migrate yet, which the group's own text
already calls "cheapest it will ever be."** That stays true: this pass found nothing to revise it.

### 14.3 `MainMenu` — confirmed intentional, not an unaudited unknown

`src/modes/main_menu/` (`MainMenu.h:16-28`) is outside §13's scope — it audits `modes/space/`'s
schedule, not `modes/main_menu/`. Read directly: it is a 3-file, self-documented stub. Its own
header comment states the legacy `MainMenu.cpp` (1,249 lines) carried a ship showcase, a
multiplayer connect flow, a lore-backed faction picker, and a save picker, and that this port
deliberately keeps only *"a title screen the player can actually get past"* because none of the
other four have a supporting system yet (no showcase renderer, `net/` is 🧊, no faction lore
content, no working save path per §13.3 Y). **This is not a gap this audit is revealing** — it is a
gap the code already names accurately, which is the one class of defect §13.3 AC found this
method structurally cannot catch on its own (*"a truthful comment about a missing capability is
still a missing capability, and a method tuned to find lies will not find it"*). Recorded here so
`modes/main_menu/` has an explicit line in this document instead of simply never being mentioned.

### 14.4 A stale figure in §0's own "current reality" snapshot — ✅ fixed 2026-08-11

Line 153's **"281 tests pass (690 assertions)"** was written 2026-07-29 (`git blame`), the same
commit that stamped line 21's *"Current reality (updated 2026-07-29)."* At least ten feature PRs
merged to `main` since — `#95` Wreck/Recovery, `#97` WarpSystem, `#104` CommanderSystem, `#99`
CustomizeMenu, `#105` Faction Survival, `#100` NavigationMap, `#98` StationServicesMenu, `#102`
StorageMenu/ModulesMenu/ModuleEquipSystem, `#103` BuildMenu/ConstructionSystem, `#101`
TemplateMarketSystem, `#106` ResearchSystem — each adding a test file. This branch's own commits
(§0's `git log`) never touch `tests/`, so the drift predates them — the four documentation passes
that produced §§9–13 rewrote the surrounding paragraphs repeatedly without revisiting this one
number. **A `grep -rc TEST_CASE tests/` count alone was not trusted as the fix**, per this section's
own closing instruction below — the binary was rebuilt from a clean `cmake --build` and actually run
(`out/build/x64-Debug/bin/sr_tests.exe`) on 2026-08-11: **424 tests pass, 1097 assertions**, which
also cross-checks the grep count (424) exactly. Line 153 now carries the correct figure.

Not a functional defect — the tests themselves are fine. Flagged because this document's entire
method is *"verified against `src/`, not inferred from a header comment,"* and §0 is the one
paragraph that method was never re-applied to until now.

### 14.6 A systematic design-coverage sweep, and one confirmed gap

*The question this answers: does every `features.md` section have an architectural translation
somewhere in this document — not "is it wired" (§13's question), but "was it ever given a plan at
all"? Extracted all 64 numbered sections/subsections from `features.md`, then checked each against
`architecture.md` for a citation. The first pass (`features.md §X` literal citations) missed several
sections that turned out to be covered anyway — by a bare `§X` reference, by "features.md section X"
spelled out instead of using `§`, or by discussing the mechanism without citing the number at all
(§12.30.2 covers §4.1's boarding/bay mechanic in full without once writing "§4.1"). **Number-citation
matching alone produces false negatives as often as §14's first draft's title-only matching produced
false positives** — every apparent gap below needed a manual read of the actual `features.md`
content and a content-level (not citation-level) search of `architecture.md` before it could be
trusted.

Sections that looked uncovered by citation search and were **not** actual gaps, once read:

| Section | Looked uncovered because | Actually covered by |
|---|---|---|
| §4.1 Boarding, the bay | No `§4.1` citation anywhere | §12.30.2 ("Screen 1 — the Bay") covers the mechanic in full under a different name |
| §5.2 Canonical Faction Registry | No faction id string anywhere in `src/` | Correctly content-only per Law 10 — `FactionRef` (`Identity.h:24`) documents *"resolves against features.md section 5.2"*, and `ships.json` already authors two of the ten keys (`aegis_directorate`, `the_forgotten`) |
| §5.9 Unlisted Pairings | No dedicated section | One sentence, entirely downstream of §5.3's relation-writer gap (already finding N) — not a distinct gap |
| §6.5 Boss encounters | No dedicated architecture section | Explicitly designed as an emergent consequence of §2.5/§2.7/§4.5/§5.1, all already covered elsewhere — the section's own text says so |
| §7.3 On-Demand Streaming | No dedicated section | Resolves into §1.1's Tier 3 boundary rule (*"no registry exists at all,"* §12.2) — the same `TickCoarse` gap findings L/M already record, not a second one |
| §5.8 Changes From Earlier Drafts | No dedicated section | A changelog, not a mechanic — correctly needs no architecture home |

**One section is a confirmed, standalone gap: §5.7, "The Reapers."** `features.md` §5.7 specifies a
named-faction special case that a generic diplomacy fix will not produce by itself: universal
hostility to every faction not explicitly listed (rather than the neutral default §5.9 gives
everyone else), a mutual alliance with Pyre that both `DiplomacyMatrix` and `FactionDecisionEngine`
would need to special-case, and **target selection by structural density of a system rather than by
relation standing** — a distinct AI-targeting axis nothing else in the design uses. `grep -i
"reaper\|structural density" docs/architecture.md` returns nothing. This is not the same gap as
finding N (`TargetingSystem` ignores `DiplomacyMatrix` entirely) or finding L (`FactionDecisionEngine`
has no invocation path) — fixing both of those wires the *general* relation model into combat and
faction AI, and the Reapers' behavior still would not exist, because it isn't expressible as a
relation-matrix lookup at all.

> ✅ **Given its own design-to-architecture pass, §12.32 (2026-08-10).** The relation half
> (`SeedBaselineRelations`, `SeedReaperHostility`) is buildable today, independent of the coarse
> loop — and widened `Relation` itself from three states to the full six `features.md` §5.3
> specifies, once checking `Reputation` and `TemplateMarketSystem` showed both already assumed the
> richer range and only got more correct for it. That widening is what lets the Reapers' three
> priority rivals actually be stored as `War`, distinct from the other five factions' `Hostile` —
> the earlier draft of §12.32 had collapsed both onto one value and pushed the distinction into the
> targeting layer instead, which this revision retired. The structural-density targeting half still
> turned out not to need new design at all — it's blocked on the same missing system-adjacency/
> roster model `RaidDispatchDirective` already names as unbuilt for *every* faction, not a
> Reapers-specific gap, so §12.32 gives it the smallest possible function (`SelectReaperTarget`, a
> max-by-key over caller-supplied density scores) rather than a parallel design.

### 14.7 What this section does not change

§13's "eight of thirty wired end to end" and its task list stand as written. §14.1's five systems
being *scheduled* is orthogonal to whether they're *wired* — §13.1 already scores all five ⚠️/❌ on
producer, content, or both, and none of those rows move. §14.6's sweep found the design-to-
architecture link is in materially better shape than a first, title-only pass suggested — one real
gap in 64 sections, not the handful a keyword search first implied. This section's net additions:
one internal consistency defect (§4 vs. reality), one content-inventory fact previously scattered
across four findings, one confirmed-intentional stub, one stale number in the document's own status
line, and one confirmed design gap (§5.7) with no architectural home yet.

---

## 15. System & UI Behavior Audit — Design vs. Implementation

*Compiled 2026-08-10. §13 asked whether a system is **connected** — producer, consumer, UI surface.
This section asks a different question of the same code: for a system or UI file that already
exists, does its **actual logic** do what the specific `features.md`/`architecture.md` section
naming it says it should? Nine passes, one per subsystem area, each re-verifying the specific
design section's checkable claims (numeric rules, specific algorithms, specific type-matching)
against the code directly — not against §13's own summary of it. Findings already recorded in §13.3
or elsewhere are cited, not restated.*

### 15.1 Findings that invert or contradict an explicit design rule

**1. Ramming bypasses shields — inverted.** `features.md` §3.1 settles *"shields are projectile-only
… ramming bypasses shields entirely,"* stated as ramming's whole tactical identity. `CollisionSystem
::QueueDamage` tags every ram hit `DamageType::Kinetic` and writes it into the same `PendingDamage`
component projectiles use; `DamageSystem::ApplyToHealthAndShield` makes no distinction by source. A
rig with a charged Kinetic shield **absorbs ram damage exactly as it would gunfire** — the opposite
of the documented mechanic. Not the same question as §12.22's "Recorded as rejected" table, which
only covers *physical* hull-blocking shields.

**2. A docked vessel can still be shot and rammed.** `features.md` §3.4's table states plainly:
*"Direct fire, ramming, targeting — **No**, the vessel is not a target."* `architecture.md` line 1869
scores this row **"Yes."** `DockingSystem::Tick` only strips `Targetable` from the docked rig, which
stops new auto-lock acquisition (`TargetingSystem::IsValidTarget` checks it) — but
`ProjectileSystem::FindHit`'s view (`HitRadius, WorldTransform, ParentRig`) and `CollisionSystem`'s
candidate view never check `Docked` at all. A stray or pre-aimed shot, or a ramming rig, still lands
on a docked ship. "Cannot be shot" holds only for the auto-targeted-fire path.

**3. Destroying a docking facility does not destroy the vessels docked to it.** Same §3.4 table,
second row: *"Destruction of the docking facility — **Yes, total.** Every vessel inside is destroyed
with it."* Also scored "Yes" at line 1869. No file in `src/` implements this — `DamageSystem`'s
rig-death check only looks at the dying rig's own hardpoints, never at who is `Docked` to it, and
`DockingSystem` never reads its host's `Destroyed` state. A destroyed station's docked ships persist
indefinitely, `Docked` to a now-dead entity — a dangling reference, not merely a missing feature.

**4. `NavigationMap` shows individual objects at the Galaxy zoom level, which the design forbids
there by name.** `features.md` §8.1: Level 1 (Galaxy) shows *"Faction territory, trade vectors,
military weight. **No individual objects.**"* `NavigationMap::Draw` branches
`level == Galaxy || level == Region` as one case and renders every `DiscoveredSystemIds` entry as an
individual marker at both levels, differing only by layout radius (260 vs. 140). There is no
aggregate/territory rendering anywhere in the file — Level 1 fakes its zoom-out by spacing dots
farther apart, not by changing what it draws.

**5. `DamageType` has no Ion value at all — sharper than finding AA.** Finding AA (§13.3) records
Ion and Kinetic rendering identically; the actual gap is structural, not cosmetic. `Taxonomy.h`
defines `enum class DamageType : std::uint8_t { Kinetic, Energy };` — **Ion does not exist as an
enum value**, so `Weapon::damageType`, `Shield::absorbs`, and `PendingDamage::type` cannot hold it
under any circumstance. `features.md` §3.1's 2026-08-08 Ion redesign (*"absorbed by every shield
type… suppresses power, not hull"*) has zero representation anywhere in the type system. Finding AA's
own text (*"Ion is absorbed by neither shield type"*) describes the **pre-redesign** model and is
itself stale relative to the current `features.md` text — noted as a minor doc-currency gap, not a
second code bug.

**6. §6.2's entire Task-Weighting-by-Archetype table has zero corresponding code.** `features.md`
§6.1 presents four operational facets a faction evaluates every macro tick. `ComputeFacets`
computes only Material Security (from `FactionEconomy::Stock`); Market Dominance, Ideological
Doctrine, and Tech Superiority are permanently `50.0f` — disclosed in the struct's own comment, but
never surfaced in `architecture.md`'s narrative (there is no §12.x section for the facet engine
itself, only §12.3 for the separate Survival predicate). Direct consequence: **no function in
`FactionDecisionEngine` reads or branches on faction archetype at all**, so §6.2's Corporate /
Military / Scientific / Anomalous / Opportunist / Ecological weighting table is unbuilt in full, not
partially.

**7. §6.4's border-skirmish mechanic has no callee, not merely no caller.** §6.4 names two tuned
thresholds; `RaidDispatchChance`'s (Material Security < 30 → 45% chance) matches the code exactly.
The second — *"two rival factions sharing a border with hostile Doctrine → border skirmish, moderated
by relative fleet strength"* — has **no function anywhere in `src/`** (`grep -i "border\|skirmish"`
returns nothing). This is a stronger gap than finding L's "no invocation path": there is nothing to
invoke.

### 15.2 Findings that are real, internal, and lower severity

**8. `RefactorSystem` still refunds a shell's modules on deletion — a settled reversal it never
picked up.** `features.md` §2.2 (2026-08-10): *"A shell cannot be removed while it still holds
modules… this reverses existing behaviour: hardpoint deletion currently returns a shell's modules to
cargo automatically. The new rule refuses the deletion instead."* `architecture.md` §12.13 already
tracks this as a pending decision, but the exact unimplemented line was never named:
`RefactorSystem::ProcessDeleteRequests` still refunds every mounted module unconditionally before
destroying the hardpoint, with no "does it still hold modules" check anywhere in the function.

**9. `RefactorSystem` inherits the count-not-mass `CargoHoldHasRoomFor` bug at a previously unnamed
call site.** The already-documented bug (`CargoHoldEntryCount`, a row count, compared against a
`capacity` meant to be a mass budget) gates `RefactorSystem`'s "storage full, deletion refused"
decision too — every such refusal is by row count, not mass, and no existing writeup names
`RefactorSystem` as a consumer.

**10. `TemplateMarketSystem`'s Accept step checks a predicate nothing ever sets.** The 3-step
Gate→Accept→Roll shape and the negotiation-roll formula both match `architecture.md` §12.7's
pseudocode constant-for-constant — confirmed clean. But `PassesAccept` reads
`pitch.archetypeFits`/`pitch.beatsCurrentManufacture` as pre-set booleans on the intent; nothing in
`src/` ever sets `archetypeFits` true, and no content type carries an archetype tag to compute it
from. This is a logic gap, not just a missing producer — even a caller would have to invent the
archetype-fit computation from nothing.

**11. `FactionEconomy`'s stock is one scalar per faction, not per-item/per-station as §5.0/§12.20
require.** `features.md` §5.0: *"Stock is held per **station**, not per faction and not per
system."* `core/economy/FactionEconomy.h`'s `Deposit`/`Spend` operate on a single
`int` per `FactionId` — no `ItemId`, no station key, no notion of *what* is held. This is a shape
gap in the math itself, not merely a missing producer; it would give wrong answers about blockades
or Material Security even once wired to something.

**12. `DiscoverySystem` writes the store the design says must be abandoned.** `features.md` §8.3
states outright that `DiscoveryState` (faction-keyed) *"cannot implement this section at all"* —
fog-of-war requires per-viewing-entity divergence `KnowledgeNetwork` provides and `DiscoveryState`
structurally can't. `DiscoverySystem::Tick` still calls `ctx.discovery->Discover(...)`, writing
exactly the store the design already retired.

**13. `MiningSystem` has no persistence path for depletion — a second, independent blocker behind
mining beyond finding B's unreachability.** §7.2's boundary rule: *"if a player or faction could
have changed it, it is not seed-derived… a system that regenerates mutable state from a seed will
silently undo player actions on every revisit."* `MiningSystem` destroys a depleted asteroid entity
outright with no `core/galaxy/` record analogous to `WreckRecord` — a revisited system would
regenerate the asteroid from its seed, intact, exactly the failure §7.2 warns about by name.

**14. Four of §5.3's named `DiplomacyMatrix` writers contain no relation-writing logic at all.**
`ContractSystem`, `CommsSystem`, `FactionEconomySystem`, and `DiscoverySystem` are named with
specific triggers (contract complete/fail, successful diplomacy, trade volume, trespass). None of
the four references `Relation`/`DiplomacyMatrix` anywhere — this is stronger than "blocked on
`ctx.diplomacy == nullptr`" (§13.5 group 3); there's no dormant trigger logic waiting behind the
null pointer the way `PartySystem`'s formation logic waits for a producer.

**15. `BuildMenu` cannot gate a build on material cost — its own header overclaims.** `BuildMenu.h`:
*"Reads the requester's own CargoHold/Wallet for affordability,"* matching §12.12. `CanAfford`
checks only `wallet.credits`; no function in the file reads a `CargoHold`, and `Draw` never takes
one as a parameter.

**16. `BuildMenu` has no build content — one affordability-colored label, not a construction UI.**
Beyond finding 15: `Draw` takes one hardcoded `cost` and renders a single static "BUILD" label; it
never calls the `BuildStationBuildRequest`/`BuildPlaceShipRequest` functions the file itself
exports, and has no blueprint list, position/rotation selection, or input handling.

**17. `NavigationMap`'s hostile coloring at System level never consults `DiscoveryState` and ignores
`DiplomacyMatrix` — the same simplification finding N records for `TargetingSystem`, on a different,
previously undocumented file.** `VisibleHostileRigs` treats any different `FactionId` as hostile and
always colors `kStatusCritical`; `features.md` §3.10 requires three-state relation coloring
("colour is relation only"), and `architecture.md` §12.6 requires the `DiscoveryState` check this
function skips.

**18. `RefactorMenu`'s own header contradicts its own body.** The header claims non-deletable
hardpoints are shown "greyed out"; `Draw` iterates only `DeletableHardpoints(...)`, so non-leaf,
`Destroyed`, and absent hardpoints are never drawn at all — not greyed, simply missing. This is the
exact failure §8.3 names (*"absence must never look like emptiness"*), inside the one file whose own
comment says it does the opposite.

**19. `IconRenderer::DrawMapMarker` has no camera or zoom parameter — the missing cull (already
recorded, finding Q) needs a signature change, not a one-line addition.** `DrawMapMarker(Vec2, Color,
std::string)` has nowhere to receive zoom level or camera bounds; the `BodyKind`-driven icon
substitution §12.28 assigns this function also has nowhere to dispatch from.

**20. `IconRenderer`'s markers don't match §8.2's cached-per-kind-shape model.** §8.2 credits
`IconRenderer` with already solving this for the reticle; `DrawMapMarker` draws one immediate-mode
filled circle every call, uncached, with no shape variation — a fleet, a ship, and a wreck would all
render as the identical dot.

**21. `CustomizeMenu` never reports which validation rule failed, though the data already exists.**
§12.9's Tests bullet requires an invalid draft to *"report which rule failed."*
`ValidationResult::errors` (a `std::vector<ValidationError>`) exists specifically for this;
`Draw` renders only the literal string `"INVALID"` and never iterates it.

### 15.3 Low-severity and informational

**22.** `RefactorMenu` renders the literal string `"hardpoint"` for every row — no per-row name or
`ShellRole` glyph, unlike `StationServicesMenu`'s comparable stub, which at least renders real
per-row content.

**23.** Citation mismatch: `StationServicesMenu.h` cites §12.11 for a promotion note that actually
lives in §12.10; `InventoryGrid.h`'s comment cites the correct section for the same note.

**24.** `ModulesMenu::Draw` calls only `EquippedMounts`; `EquippableMounts` is exported but never
called by `Draw` — roughly a third of §12.30.7's required surface is exercised.

**25.** `FactionDecisionEngine::EvaluateColonization`'s stock threshold has no `features.md` backing
at all (`grep -i coloniz` over `features.md` returns nothing) — an undocumented invention, the
opposite direction from most findings here.

**26.** `WorldRenderer::DrawHardpoints` has no draw-layer sort of any kind — consistent with §3.5's
five-layer model being 📋, but recorded precisely: no field, no sort call, no layer enum reference
exists to be *partially* right or wrong.

### 15.4 Verified clean — confirmations worth keeping

Several checks resolved *in the code's favor*, including two open questions this document had
flagged without an answer:

- **`CollisionSystem::BuildWorldHull` excludes `Destroyed` hardpoints** — resolves §12.22's own
  flagged "⚠️ worth confirming" uncertainty: a stripped capital does not still collide at full size.
- **`CollisionSystem::ApplyRamDamage`'s mass-share split matches §3.7's worked example exactly**
  (mass-100 vs. mass-10 → 9.09%/90.9%, to the decimal).
- **`TemplateMarketSystem`'s negotiation-roll formula and 3-step Gate→Accept→Roll shape** match
  `architecture.md` §12.7's pseudocode constant-for-constant.
- **`FactionDecisionEngine::RaidDispatchChance`'s threshold** (Material Security < 30 → 45%) matches
  §6.4 and §12.16 exactly.
- **`LootSystem`'s wreck demote/promote cycle** matches §12.5 almost verbatim, componentwise.
- **`ModuleEquipSystem`, `ConstructionSystem`, and `EngineerSystem`'s merge/refusal logic** all match
  their cited sections precisely, including the subtle "two distinct owned copies" merge-with-self
  refusal §12.12 calls out by name.
- **`BridgeView` carries no vestige of the withdrawn two-player-mode model** (§12.16 item 23).
- **`CockpitHud` matches its own documented minimal state exactly** (one aggregate hull bar), and
  correctly has nothing radar-like given it has no sensor-contact rendering yet.
- **`PhysicsSystem`, `HierarchySystem`, `OrbitSystem`, `SpawnSystem`, and `ProjectileSystem`** were
  checked against every applicable numeric/algorithmic claim in scope and diverge nowhere beyond
  what §13.3/§12.14/§12.22/§12.28 already record.
- **`DamageSystem`'s shield absorption/bypass/recharge logic** matches §3.1/§12.22 exactly, apart
  from finding 1's ramming-bypass gap.

### 15.5 Where these land

✅ **Folded into §13.5, 2026-08-10 — this section's earlier claim that the fold-in was still
pending is now stale and is corrected here.** All twenty-six findings have a citation inside §13.5
or the design sections it points at: 1 and 5 together in group 2d (the damage-model pass); 2 in
group 2 and §12.34; 3 in group 2d; 4 in group 2f; 6 and 7 in the addendum under group 5; 8, 9, 15,
21, and 23 in group 2; 10, 12, and 14 in group 3; 11 and 13 in group 2c; 16, 18, 22, and 24 in
group 4b; 17, 19, and 20 in group 2f. 25 and 26 were deliberately left off the task list, with the
reasoning stated where they're discussed above (25 is a documentation gap, not a code one; 26
confirms an already-accurate 📋). None of the original groupings this section predicted (*"15–21
belong in group 4b"*, *"11–14 belong beside §12.20"*) survived exactly as guessed — several
findings ended up in group 2f or 2c instead — which is the expected shape of a prediction made
before the placements were written, not a discrepancy worth chasing further.
