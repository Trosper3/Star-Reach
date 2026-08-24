# StarReach: A New Contributor's Guide

*This is the "where do I even start" document. `architecture.md` is the law book — read it before
your first non-trivial PR. `features.md` is the design bible. This document exists to get you from
"I cloned the repo" to "I shipped a new ship/module/system" without reading either cover to cover
first.*

**One thing to know before anything else: there are two repositories.** `StarReach/` (this one) is
a from-scratch rewrite started 2026-07-26. `../StarReach2` is the previous 39,000-line game — it is
playable and it is where every piece of *behavior* was originally worked out, but it is not the
codebase you're contributing to. Mine it as a reference for how something should behave; never copy
its file structure or its C++ patterns. The whole point of this rewrite is that the old structure
grew a 12,947-line file despite a design document that told it not to (`architecture.md` §0, §9).

---

## 1. The one-paragraph mental model

A game object — a fighter, a station, a turret — is authored as **JSON** (a *blueprint*): a shell
(a housing) with modules bolted onto it, and shells bolted onto other shells. At runtime, a
**factory** reads that JSON and builds an **entity graph**: one parent entity for the root shell,
one child entity per mounted shell, each carrying whatever components its modules imply (`Health`,
`Weapon`, `Shield`, `PowerCell`, …). Every frame, ~30 **systems** — free functions, not classes —
each do one job across every entity that has the right components, in a fixed order. UI and input
never touch game state directly; they push an **Intent** onto a queue that systems drain. That's
the whole architecture. Everything below is detail on top of that paragraph.

---

## 2. Folder-by-folder tour

```
StarReach/
├── data/base_game/       Content. shells.json, modules.json, ships.json, elements.json.
│                          THE ONLY place new ships/weapons/materials get defined. No C++ tables.
├── docs/                  architecture.md (laws + conventions), features.md (design), lore.md
├── tools/ci/              4 Python scripts that fail your build for structural violations
├── tests/unit/            One system, one file, real content, no window
├── tests/integration/     Loads and validates the actual data/base_game/ files
└── src/
    ├── main.cpp           Load content -> open window -> fixed-timestep loop. That's it.
    ├── engine/platform/   raylib lives ONLY here (Window.h/.cpp). Zero game knowledge.
    ├── core/              Galaxy-wide truths: diplomacy, economy, knowledge, registries,
    │                      serialization, galaxy/seeding. No raylib. No modes. No registry
    │                      instance (that's a system's, not the galaxy's).
    ├── shared/             THE BOTTOM LAYER. blueprints/, components/, math/, ui/.
    │                      No raylib, no core/. This is the shared vocabulary every layer speaks.
    ├── net/                Deferred (multiplayer), but see §5 below — the discipline isn't.
    └── modes/
        ├── main_menu/      MainMenu — the title screen mode
        └── space/          The flight mode. This is where almost all new work happens:
            ├── SpaceFlight.cpp/.h   Lifecycle + routing ONLY (Law 6 — see §4)
            ├── data/                SystemWorld — owns the entt::registry
            ├── systems/             ~30 free-function systems + SystemSchedule.cpp
            ├── factories/           RigFactory, NpcFactory, StationFactory, WorldGen
            ├── render/              WorldRenderer, LightingPass, IconRenderer
            └── ui/                  CockpitHud, menus — pushes Intents, never mutates state
```

**Dependency direction is one-way and enforced by the linker, not a comment:**
`shared` → `core` → `engine`/`modes`. `sr_core` physically cannot link raylib — that's not a style
rule, it's why `tools/economy_sim` can run headless and why the faction sim is unit-testable in CI.
`tools/ci/check_layers.py` fails your PR if an include crosses the wrong way.

---

## 3. The four content files, and how they relate

Everything a designer authors lives in `data/base_game/`. Four files, in dependency order:

| File | Defines | Referenced by |
|---|---|---|
| `elements.json` | Raw materials (iron, carbon, silica, titanium — real elements, not fantasy ore) | Mining/crafting yields, module costs |
| `modules.json` | The *thing that does something*: a weapon, an engine, an armor plate, a power cell, a shield emitter | `shells.json` mounts, `ships.json` mount lists |
| `shells.json` | The *housing a module mounts into* — also the unit of localized damage. A "hardpoint" in casual conversation is a live instance of a shell. | `ships.json` mounts |
| `ships.json` | A full craft: a tree of mounts, each `{ id, shell, modules[], attachedTo, localOffset }` | `RigFactory::Spawn` at runtime |

**A shell is not a third type alongside "component."** Earlier design drafts implied a
`Shell -> Component -> Module` chain; that's wrong. It's two tiers: `Shell -> Module`. "Component"
in conversation just means "shell" (`architecture.md` Law 4). Stations and capital ships use the
exact same two-tier model as fighters — there's no separate station-hardpoint type.

### Concrete example (`data/base_game/ships.json`, the starter fighter):

```json
{
  "id": "aegis_vanguard",
  "faction": "aegis_directorate",
  "mobile": true,
  "structuralMassLimit": 260.0,
  "mounts": [
    { "id": "core", "shell": "shell_fighter_chassis", "modules": ["armor_plate_i"],
      "localOffset": { "x": 0.0, "y": 0.0 } },
    { "id": "reactor", "shell": "shell_power_bay", "attachedTo": "core",
      "modules": ["power_cell_i"], "localOffset": { "x": -6.0, "y": 0.0 } },
    { "id": "wing_port", "shell": "shell_wing_hardpoint", "attachedTo": "core",
      "modules": ["pulse_cannon_i"], "localOffset": { "x": 6.0, "y": -14.0 },
      "traverseRadians": 0.35 }
  ]
}
```

Every mount but the root has `attachedTo`, naming the mount id it's parented to — that's the tree.
`localOffset` is position relative to the parent, in the parent's local frame. `traverseRadians`
lets a weapon mount slew instead of firing dead-ahead.

---

## 4. Blueprint form vs. live form (the idea everything else hangs on)

Every composite object exists in **two representations**, and confusing them is the single most
common category of bug in a project shaped like this one (`architecture.md` Law 3):

| | Blueprint form | Live form |
|---|---|---|
| What it is | Plain struct, JSON-shaped | Entities + components |
| Identity | Stable string id (`"aegis_vanguard"`) | `entt::entity` handle — **volatile, per-session** |
| Lives in | `data/base_game/*.json`, save files, wire packets | The `entt::registry` inside a `SystemWorld` |
| Who reads it | Registries, factories, validation, serialization | Systems |

**A factory is the only bridge.** `rig_factory::Spawn(world, content, params)` takes a blueprint id
and returns a live entity graph: one parent entity carrying `Rig{ children }`, one child entity per
mount carrying `ParentRig`, `LocalTransform`, `WorldTransform`, `Health`, and whatever components its
modules imply. Nothing else converts between the two forms — a system never builds a rig, and
`modes/space/systems/` is physically forbidden (CI-checked) from including `factories/`.

Why entities, not a nested `std::vector<Hardpoint>` struct? Because that's exactly what StarReach2
did, and the struct grew to 1,146 lines, needed four separate wire-format structs for
fighters/stations/capitals, and made "destroy just this turret" impossible to express generically.
As live entities, `registry.view<Health, ParentRig>()` handles a fighter's wing gun and a station's
dorsal battery identically — no per-craft-type branching anywhere.

**Practical consequence for you:** never store an `entt::entity` in a component that outlives one
tick's worth of certainty about it, and never put one in a save file, a Template, or a network
packet. Persistent references use `NetworkId` or a mount's stable string id
(`rig_factory::FindHardpoint(registry, root, mountId)` resolves a mount id back to a live entity
when you need one).

---

## 5. How a frame actually runs

`main.cpp` opens the window, loads `data/base_game/` into a `ContentLibrary`, and runs a fixed
60 Hz timestep loop (`core/time/FixedTimestep.h`) — game logic is deterministic regardless of frame
rate, which matters because Law 2's "fast-forward a system you're not currently in" depends on it.

Each tick, `SpaceFlight` (the mode orchestrator) calls `RunTick(ctx)` in
`modes/space/systems/SystemSchedule.cpp`, which runs every scheduled system in a fixed, documented
order — currently 30 of them, from `WarpSystem` first down through `TemplateMarketSystem` last. A
31st system, `FactionDecisionSystem`, is built (as `core/ai/FactionDecisionEngine`) but has its own
separate entry point rather than a slot in this schedule, and nothing calls it yet (architecture.md
§13.2/§13.3 L) — it does not tick here. The
order is not arbitrary; `SystemSchedule.cpp`'s header comment is ~90 lines explaining exactly why
`HierarchySystem` must run after `PhysicsSystem` (a hardpoint's world position is derived from its
root's this-tick position), why `DamageSystem` must run last among combat systems (destruction is
the tick's final word), and so on. **If you add a system, read that comment before deciding where
it goes** — most ordering mistakes are already documented there as "why not."

Every system receives exactly one thing: a `SystemContext` (`modes/space/systems/System.h`) — the
world, this tick's intents, read-only content, `dt`, the tick count, and nullable pointers to the
handful of galaxy-wide stores (`economy`, `discovery`, `knowledge`, `diplomacy`, `reputation`) that
some systems need. **It deliberately does not give you a pointer back to the mode class.** That's
the mechanism, not a convention: there's nothing to reach for, so nothing accumulates in one file.

**UI never mutates state directly.** A click on "fire" pushes a `FireWeaponsIntent` onto the
`IntentQueue`; `PlayerInputSystem` drains it and writes `FireIntent`/`ThrustInput` components that
`WeaponSystem`/`PhysicsSystem` read later the same tick. This is deliberately the same shape
multiplayer will use later (an intent from the network looks identical to one from local input) —
`net/` is deferred, but this discipline is not.

---

## 6. Recipes — the four things you'll actually do

### Add a new raw material (element)

Edit `data/base_game/elements.json`. Add `{ "id", "displayName", "mass" }`. That's it — no C++.

### Add a new module (weapon / engine / armor / power cell / shield / …)

Edit `data/base_game/modules.json`. Every module has `id`, `displayName`, `kind`, `mass`, and then
a kind-specific block (`weapon{ damage, damageType, fireIntervalSeconds, projectileSpeed,
rangeUnits, spreadRadians, projectilesPerShot }`, `engine{ thrustNewtons, turnTorque, maxSpeed }`,
`hullBonus`, `powerGeneration`/`powerDraw`, …). Copy the closest existing entry of the same `kind`
and change numbers — the schema per kind is entirely inferable from the existing file, and that's
deliberate (Law 10: JSON is the *only* content pipeline; there is no C++ struct literal anywhere
that CI will let you add instead).

### Add a new shell (a mountable housing / hardpoint type)

Edit `data/base_game/shells.json`. `{ id, displayName, kind, hull, mass, moduleSlots, radius,
spriteLayer }`. `kind` must match what modules of the intended `kind` can legally mount into it —
`ModuleCompatibility` validation (`shared/blueprints/Validation.h`) rejects a mismatch before any
entity is ever built, so an invalid ship fails at content-load / CI time, never mid-game.

### Add a new ship, or a new hardpoint on an existing ship

Edit `data/base_game/ships.json`. A ship is a `mounts` tree: pick a `shell`, list the `modules` it
carries, and (for every mount but the root) name `attachedTo` and a `localOffset`. Adding a new
hardpoint to an existing ship is the same operation on one more `mounts` entry — nothing in C++
needs to change; `RigFactory::Spawn` builds whatever tree the JSON describes.

**Validate before you assume it works:** `tests/integration/ContentTests.cpp` loads and validates
every file in `data/base_game/` — the same validation `main()` runs at startup — so a malformed or
structurally invalid entry (an `attachedTo` that doesn't resolve, two roots, a module mounted into
a shell `kind` that doesn't accept it, …) fails `ctest` with a specific message rather than
crashing the game later. Run the test suite after any content edit, not just after a C++ change.

### Add a new system (new simulation behavior)

This is the "I need actual new gameplay logic" case — the recipe is spelled out completely in
`architecture.md` §11.3, condensed here:

1. `modes/space/systems/YourSystem.h/.cpp`. Free functions in a snake_case namespace, not a class:
   ```cpp
   namespace sr::space::your_system {
   void Tick(const SystemContext& ctx);
   }
   ```
   A class invites member variables, and per-frame state that lives in a member instead of a
   component is invisible to every other system — which is exactly how the old project's god
   object grew. The one legitimate exception (an expensive cache like a broad-phase grid) lives as
   a component on a singleton entity, not a private field.
2. Take only `SystemContext`. Nothing else is available to you, by design (§5 above).
3. Add it to `TickSchedule()` in `SystemSchedule.cpp` **in the same commit**. Read the ordering
   comment above the schedule first — decide where your system goes relative to
   `HierarchySystem`/`PowerSystem`/`DamageSystem` using the reasoning already written there, don't
   guess. A system with no schedule entry is a dead abstraction and gets deleted at review.
4. Add a unit test in `tests/unit/`. A `SystemContext` can be built against a bare `SystemWorld`
   with no window and no content file, so this is cheap.
5. Run `python tools/ci/check_all.py`.

**Need a new kind of per-entity state to go with it?** Add a POD struct (no methods, no owning
pointers, no `std::vector` where a child entity would do — see §4) to `shared/components/`, and
write a one-line comment naming which system is allowed to write it. That comment is the cheapest
defense against a component that everything writes and nothing owns.

**Need galaxy-wide state instead** (something that isn't per-system-registry — diplomacy, faction
stock, discovered systems)? That's `core/`, not a component and not a mode member (Law 8) — and if
a system needs to read/write it, thread it through `SystemContext` as one more nullable pointer,
following the existing `economy`/`discovery`/`knowledge` pattern in `System.h`.

---

## 7. Build, test, and the checks that gate a PR

```powershell
cmake --preset release          # first time, or after CMakeLists.txt changes
cmake --build --preset release
ctest --preset release --output-on-failure
python tools/ci/check_all.py    # the 4 structural checks CI runs (sizes/layers/pipeline/dead-dirs)
```

Windows must be configured from a Developer Command Prompt (or with `CC=cl`/`CXX=cl` set) — the
Ninja generator otherwise picks up MSYS2's `g++` if it's on `PATH` and produces an ABI mismatch
against vcpkg's MSVC triplet. `CMakeLists.txt`/`tests/CMakeLists.txt` list source files explicitly
(no glob) — a new `.cpp` needs both a list edit and a reconfigure, not just a rebuild.

What actually fails a build, beyond a compile error: a file over 600 lines, a function over 80, or a
mode class over 25 members (`check_sizes.py`) — that last one is Law 6's enforcement, the mechanism
that caps a mode orchestrator's state to prevent another `StarReach2`-style god object (`SpaceFlight.h`
had grown to 466 members there); an include crossing a layer boundary backward (`check_layers.py`);
constructing a `ModuleDef`/`ShellDef`/`ShipBlueprint` anywhere outside `core/registries/`, `tests/`,
or `tools/` (`check_content_pipeline.py`); an empty `src/` subdirectory (`check_dead_dirs.py`);
unformatted code (`clang-format --Werror`); and any authored blueprint that fails `Validate()`
(`ctest`).

---

## 8. Rules of thumb (the five-minute version, `architecture.md` §11.1)

| You want to… | Do this |
|---|---|
| Add a stat, weapon, ship, module, element | Edit `data/base_game/*.json`. **Never** write a C++ definition. |
| Add a simulation behavior | New file in `modes/space/systems/`, free functions, register in `SystemSchedule.cpp` — same commit. |
| Add per-entity state | New POD struct in `shared/components/`. No methods, no owning pointers. |
| Add galaxy-wide state | `core/`. Not a component, not a mode member. |
| Build a composite object | `modes/space/factories/`. Systems never construct rigs. |
| Respond to a click | Push an Intent. UI never mutates state. |
| Add a helper | Put it in the mode you're working in. Promote to `shared/` only when a **second** consumer actually appears — not preemptively. |

And the two rules that are easy to forget because nothing enforces them mechanically:

- **No `entt::entity` in anything persistent** (saves, Templates, wire packets, long-lived
  components). Use a stable string/int id.
- **A system may not emit a new event/intent in direct response to consuming one.** Chain reactions
  are sequenced explicitly by the orchestrator, not by bouncing through the event bus — this is the
  rule that keeps a debugging session from turning into an archaeology dig (Law 12).

---

## 9. Where to go next

- **`docs/architecture.md`** — the full law book. §1 (Twelve Laws) explains *why* each rule above
  exists, in more depth than this document repeats. §4 is the full system inventory (~30 systems,
  what each owns). §11 is this document's source material, with more edge cases. §12 is a long
  running log of specific design decisions and the code changes they implied — worth searching
  (not reading start to end) when you're about to touch something that sounds already-designed.
  §13 is a wiring-gap audit: components with a reader but no writer, menus with no caller — good to
  check before assuming a feature that *looks* built is actually reachable in play.
- **`docs/features.md`** — the design document: what each system is *for*, numeric ranges, the
  economy/crafting/faction model. Read the relevant section before implementing a §12 item.
- **`../StarReach2`** — the legacy game. Treat it as an executable spec for *behavior* (numbers,
  edge cases already debugged) — never as a structural reference. If an old memory or comment names
  a file like `SpaceFlight.cpp` or `Hardpoint.h`, it means the StarReach2 copy, not this repo.
- **`tools/ci/check_all.py`** — run it before every push; it's what CI runs first and it's seconds,
  not minutes.
