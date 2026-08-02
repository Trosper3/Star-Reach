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
Shell  ->  Component  ->  Module
```

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
| `ResearchSystem` (§12.1) | `core/knowledge/KnowledgeNetwork` | A completed research job's only output is a grant into a network. Without the store there is nothing to grant to. |
| `CommanderSystem` (§12.2) | `core/knowledge/KnowledgeNetwork` | Each sub-commander owns a network (`features.md` §4.1); the `Commander` component holds a `NetworkId` into it. |
| `TemplateMarketSystem` (§12.7) | `core/knowledge/KnowledgeNetwork` | A sale copies a Template from the seller's network into the buyer's. Both ends are the store. |
| Faction survival (§12.3) | `CommanderSystem` (§12.2) | The Leadership pillar is "player alive **or** any sub-commander alive". The predicate cannot be written, let alone tested, before commanders exist. |
| Recovery run (§12.5) | — *(none)* | Deliberately independent: it extends the existing `LootSystem` and adds one `core/galaxy/` record. Startable today. |
| Seeding (§12.4) | — *(none)* | Pure functions in `sr_core`. Startable today, and the natural first pick. |
| Navigation map (§12.6) | Seeding (§12.4) | Zoom levels 1–2 render systems the player has never visited, which only exist as seed output until instantiated. |
| Template creation (§12.9) | `KnowledgeNetwork` (§12.1) | `SaveTemplateRequest`'s only consumer is `KnowledgeStore::Grant` — no store, nowhere to save to. |
| `MergeModuleRequest` within §12.10 | — *(content schema, not an issue)* | No module tier/grade concept exists in `ModuleDef` yet; buy/sell/repair in the same menu do not depend on it and can land first. |
| §12.11's `ModuleEquipSystem` | — *(design decision, not an issue)* | The `factories/` layering question must be resolved (extract shared attach logic, or amend the rule) before, not during, implementation. |

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

**The apparent contradiction, resolved.** `features.md` §4.1 says each sub-commander *holds* a
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

### 12.2 AI Sub-Commanders — `features.md` §4.1

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
(`features.md` §4.1). None of the three block the component or the system skeleton — build the
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
`SellItemRequest`, `RepairRequest { float fraction }`, `MergeModuleRequest { ModuleId a, b }` —
same POD-Intent shape as `Docking.h`'s `DockRequest`.

**Systems:** `StationServicesSystem::Tick` consumes the four requests above the same tick they're
set, debits/credits `Wallet`, moves entries between `CargoHold` and the station's own stock, and
restores `Health` proportional to `RepairRequest::fraction` × credits spent.

**Tests:** buying debits `Wallet` and adds to `CargoHold`; selling is the exact inverse; repair
spend scales with the requested fraction and refuses when `Wallet.credits` is insufficient; merge
consumes two same-tier modules and yields exactly one higher-tier module.

❓ **Open, and it blocks `MergeModuleRequest` specifically:** no module "tier/grade" concept exists
in `ModuleDef` (`shared/blueprints/`) yet. `features.md` never specifies what a merge upgrades —
stats, rarity, both? This needs a content-schema decision before the merge path can be built; buy/
sell/repair do not depend on it and can land first.

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

❓ **Open, and worth resolving before writing this system — a real layering question, not a
placeholder.** Attaching a module to an *already-instantiated* rig is the same operation
`RigFactory::AttachModule` performs at construction time (`shared/components/Facility.h`'s comment
names it directly). But §2.3's table forbids `modes/space/systems/` from including
`factories/`. Equip-from-storage during play is a case the original layer rule did not anticipate,
because no runtime equip mechanic existed when the rule was written. Two honest resolutions, either
acceptable, neither decided here per §11.7 ("change the rule in the same PR, with the reasoning
written down"):

1. Extract the minimal attach logic `RigFactory::AttachModule` and `ModuleEquipSystem` both need
   into a function `shared/` can hold (it is pure: given a mount entity and a `ModuleId`, attach
   the components), so neither side includes the other.
2. Reclassify runtime equip as assembly and grant `ModuleEquipSystem` a factory-layer exemption,
   documented as a rule amendment here.

Option 1 is recommended — it is Law 5's "factories first" principle applied literally, and it
avoids special-casing the layer rule for one system.

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
are the only two authored tiers), and the settled mechanic is simpler: delete a hardpoint from the
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
