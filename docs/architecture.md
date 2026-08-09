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

**Current reality (updated 2026-07-29):** the enforcement infrastructure, the First Vertical Slice
(§10), and the **twenty-two ✅ rows of the §4 system inventory** are built. What remains is what
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
- ✅ **All twenty-two of the §4 systems** registered in `SystemSchedule.cpp`, plus
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
- ✅ **281 tests pass** (690 assertions), including the coverage the previous snapshot listed plus
  docking/warp/comms/contract/distress/tutorial/mining/loot lifecycle coverage, faction
  economy/decision-engine coverage, diplomacy/reputation/territory coverage, discovery coverage,
  and blueprint-serialization/save-migration coverage.

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
batch are now startable, none blocking. Two ❓s remain, and neither blocks anything: §12.1's network
raiding and §12.2's sub-commander recruitment/loyalty are both scoped as "build the mechanism, leave
the roster policy open."

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
│   └── economy_sim/         # 🧊 Headless supply/demand balancing (links sr_core only)
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
│       │   ├── systems/            # ✅ System.h (the contract) + all 22 systems in §4,
│       │   │                       #    registered in SystemSchedule.cpp
│       │   │                       # 📋 ResearchSystem, CommanderSystem, TemplateMarketSystem
│       │   │                       #    (§12.1-12.2, 12.7); StationServicesSystem,
│       │   │                       #    ModuleEquipSystem, ConstructionSystem (§12.10-12.12)
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
| `ResearchSystem` | Reverse-engineering jobs: cost, progress, unlock into a network | 1–2 | 📋 |
| `CommanderSystem` | AI sub-commander standing orders, fleet dispatch, death | 1 / 2–3 | 📋 |
| `TemplateMarketSystem` | Template pitch, valuation, negotiation roll, royalty accrual | 2–3 | 📋 |
| `StationServicesSystem` | Buy/sell modules & materials, hull repair, module merge (§12.10) | 1 | 📋 |
| `ModuleEquipSystem` | Mount/unmount modules onto an already-live rig (§12.11) | 1 | 📋 |
| `ConstructionSystem` | Player-initiated station/ship build via `StationFactory`/`RigFactory` (§12.12) | 1 | ✅ |
| `EngineerSystem` | Merge two owned same-kind modules into one, level-scaled loss (§12.12) | 1 | ✅ |
| `RefactorSystem` | Delete a live hardpoint, returning its modules to storage (§12.12) | 1 | ✅ |
| `ManufacturingSystem` | Queued module/shell/craft production; the consumer research has lacked (§12.18) | 1–2 | 📋 |

All twenty-two ✅ rows above are built. Each landed as its own GitHub issue (§11.9 tracked the few
cross-issue dependencies among them).

**The six 📋 rows are new.** The first three (`ResearchSystem`, `CommanderSystem`,
`TemplateMarketSystem`) were added by the §12 pass that gave `features.md`'s design sections
architecture homes. The last three (`StationServicesSystem`, `ModuleEquipSystem`,
`ConstructionSystem`, §12.10–§12.12) were added by a second pass surveying `../StarReach2`'s menu
files directly — they back UI that was never a numbered `features.md` section but existed as
working code and as components already built anticipating them (`CargoHold`'s doc comment names
`StorageMenu` as a future consumer by name). They are listed here rather than left to be discovered
because that is this section's entire purpose — §4 exists because naming only three systems is how
12,947 lines ended up in an orchestrator. Two further design sections deliberately do **not** appear
as new systems:

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
| `ManufacturingSystem` (§12.18) | §12.19 Item Model **and** the materials/crafts content set | Needs `ItemId`/`ItemKind`, and needs something to consume. `data/base_game/` has no `materials.json` or `crafts.json` |
| Per-item faction stock (§12.20) | §12.19 Item Model | The ledger is keyed on `ItemId`, which does not exist |
| Stat pools (§12.21) | §12.19 Item Model | A `Quality` roll is stored per instance alongside the item's identity; both land together |
| §12.19 Item Model | **Materials and crafts content** *(authoring, not an issue)* | Mass and price derive from recipes. With no material set there is nothing to derive from |
| Navigation map levels 1–2 (§12.6) | §12.17 Galaxy Topology | Systems carry no galactic coordinate, so there is nothing to lay out |
| The `std::hash` warp fix (§12.15 🐛) | §12.17 Galaxy Topology | The placeholder exists *because* systems have no coordinate to seed from |
| ECM module (§2.11) | Fog of war migration (`features.md` §8.3) | Suppressing sensor coverage means nothing until coverage is per-viewer |
| Cloak / stealth (§2.11) | **A signature/detection model** *(does not exist)* | Sensors carry only a range; there is nothing to detect *against*. This is a system, not a module |

**Startable today, nothing blocking:** **§12.24 steps 1–4, the micro loop — the one to take first**
· §12.24 step 6's five `SystemContext` pointers · the `BridgeView::kAllKinds` fix · §12.17 Galaxy
Topology (and it unblocks the most) · §12.22's shield coverage fix · §12.22's collision type B and
structural cascade · §12.23's aggregation rule and `RecomputeRigTotals` · the
`Sensor`/`CargoBay`/`FireControl` kinds · the fog-of-war migration (`DiscoveryState` →
`KnowledgeNetwork`) · the one-line duplicate `DrawWorld` call in `SpaceFlight::Draw`.

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

❓ **Open (raised, deliberately not answered here):** whether a network has a capturable physical
host, so a faction can *steal* designs rather than only destroy the holder. `features.md` §2.5
raises it. It changes the type — a raidable network needs a location and an owner entity — so it is
cheaper to decide before this is built than after.

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

❓ **Open:** recruitment, competence/personality traits, and whether a rival can turn one
(`features.md` §4.5). None of the three block the component or the system skeleton — build the
mechanism, leave the roster policy open.

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
3. **Rate roll — the only step that is actually a roll.** Runs only if step 2 accepted. It can raise
   or lower the payout; it can never undo an acceptance.

   ```cpp
   float rateBonus =
       0.5f * reputation.Score(faction)              // core/diplomacy/Reputation, -100..100
     + 0.4f * relation.Value(faction, sellerFaction)  // core/diplomacy/DiplomacyMatrix, -100..100
     + (archetypeFits ? 20.0f : -10.0f);
   rateBonus = std::clamp(rateBonus, -100.0f, 100.0f);
   float payoutMultiplier = 1.0f + rateBonus / 200.0f;  // 0.5x .. 1.5x the base lump sum / royalty rate
   ```

**These weights are a placeholder, not a balance pass.** `0.5f`, `0.4f`, `20.0f`, and the
`payoutMultiplier` curve are provisional in the same sense §6.4 flags its two tuned numbers as the
*only* tuned numbers in the design — they make `TemplateMarketSystem` buildable and testable now,
and get tuned later against the headless `tools/economy_sim` (`architecture.md` §3) once there is a
game to playtest against. Do not treat the specific constants as final; treat the three-step shape
as final.

❓ **Still open, and no longer blocking:** the base royalty rate scale itself (what a royalty *unit*
is worth before `payoutMultiplier` is applied) and whether a royalty stream survives the seller's
death — `features.md` §9 still lists both as undecided. Step 3's formula multiplies whatever that
base rate turns out to be; it does not depend on knowing it in advance.

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
- **Content:** the module → materials yield is authored data (Law 10) — a `deconstructsTo` field on
  `ModuleDef` in `modules.json`. Never a C++ table.
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
| 11 | Docked craft cannot be shot, but die with their host (§3.4) | **Yes** | `DockingSystem` / `DamageSystem` |
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

#### 19 — Shell sizing, placement bounds, z-layers, and penetration (`features.md` §3.5)

**Shell radius stays per-type and authored** — `ShellDef.radius` already works this way and needs no
change. A uniform-radius model was specified on 2026-08-07 and withdrawn on 2026-08-08; ignore any
surviving reference to it. What `shells.json` *does* need is re-authoring against §3.5's scale table
(chassis at ~50% of hull radius, 50-unit fighters), which every existing blueprint fails today.

**Two new validation rules** (`Validation.h`), both cheap and both pure geometry:

- **Rule 10 — separation:** no two mounts closer than `r(A) + r(B)`, so hit circles stay disjoint and
  §3.2's manual aim has something to bite on.
- **Rule 11 — attachment:** every mount within `A extent + B extent` of what it attaches to, so a
  mount cannot satisfy graph connectivity (rule 7) while floating visually detached.

**`ShellDef` needs a z-layer field.** `spriteLayer` is a *string asset key* — which image, not what
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
- Capture itself is unspecified (`features.md` §3.2 ❓) and should not be guessed at. Disabling is
  worth building on its own — a hull that goes dead and adrift when you shoot its cockpit is already
  a complete, readable mechanic without an ownership transfer behind it.

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
craft. `ResearchJob::item`'s `ModuleId` typing has the same problem and should widen in the same pass
(§12.16 item 21).

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

**Depends on:** `core/knowledge/KnowledgeNetwork` (#78) for the gate, and on a materials/crafts content
set that does not exist — `data/base_game/` holds only `modules.json`, `shells.json`, and `ships.json`.
**That content pass is the real blocker**, and `features.md` §9 now names it as such.

**This is what closes the research loop.** `ResearchSystem` grants an unlock into a network today and
nothing consumes it.

### 12.19 The Item Model — `features.md` §2.10

**The content set has three files and needs six.** `data/base_game/` holds `modules.json`,
`shells.json`, and `ships.json`. There is no `materials.json`, no `crafts.json`, no `MaterialId`, and
no `ItemId` — `ResearchJob::item` is typed `ModuleId` with a comment admitting the gap (§12.16 item
21). Everything §2.4 prices in "materials/crafts" has, until now, had nothing to be priced in.

**Home:** `shared/blueprints/` for the types, `core/registries/` for the parsers, `data/base_game/`
for the content.

**Types:**

| Type | Notes |
|---|---|
| `ItemKind` | `Material · Craft · Module · Shell · Vessel`. The tag that lets a cargo hold, a research job, a recipe, and a faction ledger all refer to "a thing" |
| `ItemId` | Stable string id, kind-tagged. Replaces `ModuleId` wherever a mechanic is kind-agnostic |
| `MaterialDef` | id, display name, abbreviation, availability band, **authored mass**, **authored base price**. The only two authored numbers in the whole value chain |
| `CraftDef` | id, display name, per-material weighting. **Recipes are generated** from `features.md` §2.10's grade table, never authored per grade |
| `Recipe` | A field on `ModuleDef`/`ShellDef`/`ShipBlueprint` — one item, one entry, no second file to keep in sync |
| `Grade` | The seven-tier ladder (`features.md` §2.7), shared by shells, modules, crafts, and facilities |

**Systems:** none new beyond `ManufacturingSystem` (§12.18). This is content plus derivation.

**Derivation happens once, at content load, and is then immutable:**

```
mass(item)  = sum(mass(inputs))  * gradeMassMultiplier   // features.md 2.7's 100% -> 70% ladder
price(item) = sum(price(inputs)) * (1 + manufacturingMargin)
```

**Fourteen authored masses and fourteen authored prices; everything else follows.** This is the same
discipline §12.15 applies to system radius and §12.4 to seeds — a derived value that gets cached is a
derived value that will drift. Neither figure is ever stored per instance.

⚠️ **`Validation.h` gains rule 13** — a shell's recipe-derived mass must land within a wide band of
`k · density · radius²`. Not a formula for mass (that comes from the recipe) but a **check on the
recipe**, catching a wing-mount-sized shell whose parts list produces a station-core mass. The kind of
thing that is obvious in a table and invisible in play.

⚠️ **`FacilityStats::level` (1–5) folds into `Grade`.** A facility is a `ModuleKind::Facility` module
and carries a grade like everything else. `EngineerSystem`'s merge scaling, `ResearchSystem`'s
duration and sample-survival roll, and deconstruction recovery all re-derive against seven steps
instead of five. Two tier systems for one concept would drift the moment either was tuned.

⚠️ **Every roll in this batch is deterministic from `(item id, tick)`** — deconstruction recovery,
research sample survival, and quality. The FNV-1a idiom `MiningSystem` and `CommsSystem` already
share, which §12.13 already flags for promotion to `shared/math/` on its third consumer. **This batch
is that third consumer.** Promote it rather than pasting it again.

**Persistence:** `CargoHold` entries become `(ItemId, quantity)` rather than `(ModuleId, quantity)`;
rolled instances additionally carry their quality scalar. `SaveFile` and `SaveMigrator` both bump.

**Tests:** derived mass and price are identical for the same recipe on every platform; a generated
craft recipe honours the grade table's variety and rarity caps at every tier; rule 13 rejects a
planted mass/radius mismatch; deconstruction recovery stays within its facility's band and never
exceeds the item's own mass; `ItemId` round-trips through a save for all five kinds.

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
protected core §3.2 rejects, since the chassis is the most exposed part of a hull (50% of hull radius,
dead centre).

Orphaned children are destroyed and drop salvage through `LootSystem`'s existing death-wreck path.

⚠️ **Content precondition:** chassis hull must dominate peripheral hull, or "shoot the middle" is the
fastest kill and localized damage becomes decorative.

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
| **`CargoBay`** | §2.2 specifies capacity from mounted bays; the kind was never added. `slotCount` × `slotCapacity`, **total derived and never stored** | `ModuleAttachment`; `CargoHold` aggregation |
| **`FireControl`** | Drives `FiringArc::turnRatePerSecond` — read by `WeaponSystem`, **hardcoded to `kPi`**, authored nowhere | `ModuleAttachment`; `WeaponSystem` |
| **`Hyperdrive`** | `WarpSystem` has **no module and no fuel reference at all** | `WarpSystem` gate; new fuel component |

**Do not merge `FireControl` into `Weapon`.** It would undo §12.22's turret decision: with tracking
baked into the gun, a cheap weapon could never be independent and the withdrawn tier gate returns.
Separate modules are what let a cheap turret with a good gunner match an expensive automated one — and
the tier progression re-emerges correctly through `moduleSlots`, since a 2-slot turret fits weapon plus
fire control while a 1-slot one does not.

**`PowerPriorityFor(ModuleKind)` absorbs all four with no fifth category**: FireControl → Weapon
priority; Sensor, CargoBay, Hyperdrive → Facility priority.

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
  `features.md` §9.1's headless combat harness depends on that staying true.

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
| `StationServicesMenu` — buy/sell | `StationServicesSystem` | `Docked` only | **+ a trade/storage facility** |
| `StorageMenu` | *(read-only)* | none | **`FacilityKind::Storage`** |
| `CustomizeMenu` | *(see 🐛 below)* | none | **a drafting facility** (new kind) |

⚠️ **Splitting `StationServicesSystem`'s three requests across two gates is a behaviour change** —
a station with a docking bay but no repair bay stops being able to repair. That is the intent: it
makes station *composition* matter instead of "a station is a station."

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

**Symmetric for NPCs (§6.3).** An enemy commander runs the same gate, so **destroying a hostile's
comms hardpoint degrades their fleet coordination** — a tactical objective that costs nothing extra
to build because it is the same check running for both sides.

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
