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

**Current reality (updated):** the enforcement infrastructure, the First Vertical Slice (§10), and
a first wave of §4 systems beyond it are built.

- ✅ **CI is running.** `build.yaml` moved to `.github/workflows/`. Four structural jobs gate a
  three-platform matrix build.
- ✅ **Layer targets** — `sr_shared`, `sr_core`, `sr_engine`, `sr_space`, `StarReach` (§2.1).
- ✅ **Enforcement scripts** — size caps, layer includes, content pipeline, empty scaffolding
  (§2.2–2.4), in `tools/ci/`. All four verified to fail on planted violations.
- ✅ **`CMakePresets.json`**, raylib + Catch2 in `vcpkg.json`, a real `.clang-format`
  (it was previously an empty file, which clang-format errors on).
- ✅ **Blueprint schema** (`shared/blueprints/`), **POD components** (`shared/components/`),
  **JSON registries** (`core/registries/`), **intent queue** (`core/events/`), **fixed timestep**
  (`core/time/`), **`SystemWorld`** and the **system contract** (`modes/space/`).
- ✅ **Factories** — `RigFactory`, `NpcFactory` (`modes/space/factories/`).

- ✅ **Eleven of the §4 systems** — `HierarchySystem`, `PowerSystem`, `OrbitSystem`,
  `PhysicsSystem`, `TargetingSystem`, `NpcAiSystem`, `WeaponSystem`, `CollisionSystem`,
  `ProjectileSystem`, `SpawnSystem`, `PartySystem`, `DamageSystem` — registered in `SystemSchedule.cpp`.
- ✅ **Minimal renderer** — `WorldRenderer` (`modes/space/render/`).
- ✅ **119 tests pass**, including validation of the real `data/base_game/` content set and
  damage/shield-bypass, hierarchy-propagation, targeting, power-budget/load-shedding, mass/momentum
  ramming-collision, party formation/retaliation coverage, and spawn-culling/safe-placement coverage

Not built: the other twelve entries in §4's system inventory (tracked as individual GitHub
issues), `modes/space/ui/`, `shared/ui/`, `modes/main_menu/`, unified serialization, and everything
marked 🧊.

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

## 2. Enforcement 📋

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
│   │   ├── events/          # ✅ Intent.h, IntentQueue.h (Law 9)
│   │   ├── registries/      # ✅ JsonReader, BlueprintJson, ContentLibrary
│   │   ├── time/            # ✅ FixedTimestep.h
│   │   │                    # 📋 diplomacy/ economy/ galaxy/ algorithms/ serialization/
│   │
│   ├── shared/              # THE BOTTOM LAYER (§2.1). No raylib, no core/.
│   │   ├── blueprints/      # ✅ Ids, Taxonomy, ModuleDef, ShellDef, RigBlueprint,
│   │   │                    #    ShipBlueprint, DefLibrary, Validation
│   │   ├── components/      # ✅ Identity, Transform, Rig, Health, Physics, Power,
│   │   │                    #    Combat, Targeting  — all POD
│   │   ├── math/            # ✅ Vec2.h, Angle.h
│   │   └── ui/              # 📋 HUD theme — becomes sr_shared_ui, the only lib above
│   │                        #    both sr_shared and sr_engine
│   │
│   ├── net/                 # 🧊 Deferred; Law 9's intent discipline is NOT deferred
│   │
│   └── modes/
│       ├── IGameMode.h      # 📋 Lands with the second mode, not before
│       ├── main_menu/       # 📋
│       │
│       ├── space/
│       │   ├── SpaceFlight.cpp/.h   # ✅ Lifecycle + routing ONLY. 6 members, cap 25 (Law 6)
│       │   ├── data/               # ✅ SystemWorld — owns the registry + NetworkId mapping
│       │   ├── systems/            # ✅ System.h (the contract) + SystemSchedule (empty)
│       │   │                       # 📋 the ~23 systems in §4
│       │   ├── factories/          # 📋 WorldGen, NpcFactory, StationFactory, RigFactory
│       │   ├── render/             # 📋 WorldRenderer, LightingPass, IconRenderer
│       │   └── ui/                 # 📋 CockpitHud, AvionicsMenu, BridgeView
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

## 4. The System Inventory 📋

Previous versions of this document named three systems (`CombatSystem`, `NpcAiSystem`, `WorldGen`).
StarReach2 actually needed about twenty. **The gap between those numbers is where 12,947 lines
went** — work with nowhere named to go lands in the orchestrator.

So the systems are enumerated up front. This list is derived from what the legacy game actually
does, not from guesswork.

| System | Responsibility | Tier | Status |
|---|---|:---:|:---:|
| `HierarchySystem` | One pass: parent → child `WorldTransform` propagation (Law 4) | 1 | ✅ |
| `PhysicsSystem` | Thrust, mass, momentum, drag, angular acceleration | 1 | ✅ |
| `OrbitSystem` | Planet/moon orbits, sun gravity wells, deterministic fast-forward | 1–2 | 📋 |
| `CollisionSystem` | Broad-phase spatial grid + narrow-phase convex hull | 1 | ✅ |
| `ProjectileSystem` | Advance, expire, cull | 1 | ✅ |
| `DamageSystem` | Shield typing, bypass, localized hardpoint destruction | 1 | ✅ |
| `WeaponSystem` | Fire control, cooldowns, charge/burst/spread modes | 1 | ✅ |
| `TargetingSystem` | Target acquisition, aim-point selection per rig type | 1 | ✅ |
| `PowerSystem` | Power budget, load shedding, throttle gating | 1 | ✅ |
| `NpcAiSystem` | Steering, state machine (Patrol/Chase/Attack/Flee/Escort) | 1 | ✅ |
| `SpawnSystem` | Safe spawn placement, culling, respawn-around-anchor | 1 | ✅ |
| `PartySystem` | Escort formations, retaliation propagation, party warp | 1 | 🚧 |
| `LootSystem` | Drops, material salvage, derelict wrecks, pickup radius | 1 | 📋 |
| `MiningSystem` | Asteroid depletion, station mining ticks | 1–2 | 📋 |
| `DockingSystem` | Proximity prompts, dock/undock, seated turrets, capture | 1 | 📋 |
| `WarpSystem` | Local warp, system warp, galaxy warp, registry handoff | 1 | 📋 |
| `CommsSystem` | Hail, dialogue lines, message log | 1 | 📋 |
| `ContractSystem` | Contract lifecycle: accept, tick, complete, fail | 1–2 | 📋 |
| `DistressSystem` | Distress call generation and response | 1–2 | 📋 |
| `TutorialSystem` | Step gating and advancement | 1 | 📋 |
| `FactionEconomySystem` | Production, stock, spending, station rebuild/upgrade | 2–3 | 📋 |
| `FactionDecisionSystem` | The 4-facet engine, colonization, raid dispatch | 3 | 📋 |
| `DiscoverySystem` | Sensor intel, system discovery, shared knowledge | 2–3 | 📋 |

Every 📋 row above is tracked as its own GitHub issue. Most are independently startable; the few
that are not are listed in §11.9, not here — check there before picking one up.

**Tier** refers to the LOD model in `features.md` §1. Tier 1 runs in the active system's registry
at 60 Hz; Tier 2 in neighbor registries at ~5 Hz; Tier 3 against `core/galaxy/` on the macro tick.
A system spanning tiers exposes separate entry points (`Tick`, `TickCoarse`) — never one function
branching on tier.

---

## 5. Subsystem Standards

**Fixed Timestep Simulation** ✅ — gameplay ticks at a fixed 60 Hz via a time accumulator
(`core/time/FixedTimestep.h`). Rendering interpolates. Determinism must hold regardless of refresh
rate or frame spikes, because Law 2's coarse-tick catch-up depends on it.

**Unified Serialization** 📋 — one pipeline writes `.sav` files and packs wire packets. It operates
on **blueprint form only** (Law 3). Entity handles are never serialized. StarReach2's `ByteWriter`
and `ByteReader` port directly.

**Save Schema Migration** 📋 — saves carry a `schemaVersion` header, processed through
`SaveMigrator` to convert older layouts forward.

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
| **`.clang-tidy`** | 📋 | Not yet configured |
| **ASan** | 📋 | The `asan` preset exists; no CI job runs it yet |

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
(`ctest --preset release`: 97/97). The "slice succeeds when" behavioral claim above is backed by
those unit tests but has not yet been confirmed in an actual play session — that verification is
still open.

The remaining §4 systems, `modes/space/ui/`, `shared/ui/`, `modes/main_menu/`, and unified
serialization are tracked as individual GitHub issues rather than as a slice checklist.

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

Most 📋 work in §3/§4/§5 is independent — pick an issue, build it standalone (a legacy port or a
fresh system against a bare `SystemWorld`), no other issue needs to land first. A few issues say
otherwise directly in their body. This table is the one place those are collected, so a contributor
picking a task does not have to re-read every open issue to notice one:

| Task | Depends on | Why |
|---|---|---|
| `WarpSystem` (#25) | Unified Serialization (#33) — conditional | Only if the registry-to-registry handoff needs wire/save-shaped serialization. Check at the start of #25 whether a lighter handoff suffices; if so this is not actually blocking. |
| Save Schema Migration (#34) | Unified Serialization (#33) | Saves are one of that pipeline's two outputs (§5) — there is no schema to migrate until it exists. |
| `modes/space/ui/` (#36) | `shared/ui/` HUD theme (#35) | `CockpitHud`/`AvionicsMenu`/`BridgeView` are built against `sr_shared_ui`'s primitives, which do not exist on disk until #35 lands. |
| `FactionDecisionSystem` (#31) | `FactionEconomySystem` (#30), `core/diplomacy/` (#42) | The 4-facet decision engine reads and writes `core/economy/` and `core/diplomacy/` state that #30 and #42 are what create it. |

**Before starting a task, check this table.** If it names a dependency, confirm that dependency has
actually **merged to `main`** — an open PR is not enough, since the branch you cut in §11.8 step 1
still would not have the code. If the dependency has not merged, stop and pick a different task
rather than implementing against code that does not exist yet; the alternative is a branch that
cannot build, or one that silently re-implements what the dependency was going to supply.

These are prose notes in each issue body today, not GitHub's native tracked-dependency links (an
issue's `blocked_by`/`blocking` count is 0 for all of them as of this writing) — this table is what
makes them enforceable in practice. Maintain it by hand: add a row in the same PR that notices a new
dependency, and delete the row (not mark it done) once its target merges — an empty table is the
signal that everything open is independently startable again.
