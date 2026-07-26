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

---

## 2. Engineering, Customization & Reverse Engineering 📋

Engineering is the heart of progression. Objects are modular, built from a hierarchy of **Shells**
(graphical hardpoint housings), **Modules** (functional stats), **Crafts** (intermediary
components), and **Raw Materials**.

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
| 5 | ≥ 1 Engine shell with an attached module — **mobile craft only** | Stations are exempt |
| 6 | No orphaned mounts | Every declared mount references a shell that exists |
| 7 | Adjacency valid | Every shell connects to the rig graph; no floating islands |
| 8 | All module and shell IDs resolve in the registry | Blocks typos and missing mod content |
| 9 | Unique blueprint ID, present `schemaVersion` | Required for saves and migration |

Rules 3 and 4 are the design-facing ones — they are the constraints puzzle. Rules 6–9 are integrity
checks and should produce distinct, specific error messages in the Engineering UI, never a generic
"invalid template."

### 2.4 Reverse Engineering & Research 📋

**From loot to blueprint** — rare high-tier modules, weapons, and alien components found via
exploration, quests, or salvage are inputs, not trophies.

**The research loop** — bringing a rare item to a station with a Research/Engineering Facility
module lets the player spend resources and time to reverse-engineer it. Success makes the item
permanently manufacturable and available for Template integration.

**Faction gates** — access to higher tiers of reverse engineering is influenced by faction alignment
and reputation tier. High standing with a tech-focused faction (Zenith Collective, AI Concordance)
unlocks advanced blueprints faster.

**Unlocks are account-level meta-progression.** They survive ship loss, and they survive a Hard Game
Over (see §3.3). This is the one thread of permanence in a game otherwise built on loss.

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

Baseline types are **Kinetic** and **Energy**. ❓ *Whether the roster expands (Ion, Thermal,
Corrosive) is open — see §7.*

### 3.2 Localized Hardpoint Destruction

Ships and stations are physical collections of hardpoints — **each one its own entity** — not a
single health bar.

**Targeted systems** — stripping or bypassing shields lets fire strike a specific hardpoint.
Destroy a thruster shell and the ship stalls. Destroy a weapon battery and that firing arc is gone
permanently.

**Functional degradation** — capitals and stations lose capabilities dynamically as hardpoints are
blown apart: repair bays stop healing, manufacturing bays stop building, shield generators stop
regenerating.

**Uniformity is the point.** A fighter wing, a station battery, and a capital's dorsal turret are
the same kind of thing to the damage system. There is no per-craft-type special case — see
`architecture.md` Law 4 for why this is stated so emphatically.

### 3.3 The Cost of Failure

If the ship the player is piloting — or commanding from the Bridge — is destroyed, **the player
dies.** That vessel and all its equipped modules are permanently lost.

**Respawn & backup** — the player respawns at an allied station. Stashed backup ships or an existing
fleet can be transitioned into immediately, backed by stored wealth and unlocked blueprints.

**The Starter Ship safety net** — with no stored ships remaining, the player may take a basic,
low-stat Starter Ship and claw back up.

**Hard Game Over** — if the player dies with zero remaining craft, backup fleets, and stations, the
run ends. The player may restart a fresh single ship *within the same galaxy* (which has continued
evolving without them) or return to the main menu.

**📋 Decision: what survives a Hard Game Over.**

| Wiped | Preserved |
|---|---|
| Credits and stored materials | Unlocked blueprints and reverse-engineering progress |
| All ships, stations, and fleet assets | Saved Templates |
| Faction reputation (reset to baseline) | Galaxy state — territory, faction standings, your sold designs still in service |
| Active contracts | |

*Rationale: the Meso loop asks for hours of investment in research and design. If a Hard Game Over
wiped that, the rational response would be to avoid the Macro loop entirely and never risk the
assets. Preserving knowledge while wiping assets keeps the loss severe but makes the restart
meaningfully different from a new game — you re-enter a galaxy that remembers you, flying designs
you still know how to build. This is reversible if playtesting shows it blunts the stakes too much.*

---

## 4. The Bridge & Fleet Command 📋

A dual-layer control scheme, transitioning from top-down shooter to RTS interface.

**Physical docking** — transition to command by flying a fighter to a Capital Ship or Station and
pressing interact. The transition is diegetic; there is no menu-only path to fleet command.

**Component-driven menus** — the Bridge UI generates from physical modules. A Manufacturing
hardpoint enables ship construction; absence of a Repair hardpoint disables healing for docked
craft. Destroying a hardpoint removes its tab mid-session.

**RTS directives & AI autonomy** — in Bridge mode the player issues macro-commands (Move, Attack,
Defend, Build). Uncommanded AI defaults to self-preservation, patrols, and role-based tasks.

**Command inherits the death rule** — §3.3 applies to the vessel being commanded from. Taking the
Bridge of a capital does not make the player safe; it makes them a larger target.

❓ *Open: whether the player's fighter persists as a separate entity while its pilot is on the
Bridge, and whether it can be destroyed independently.*

---

## 5. The Living Universe & Factions 📋

The galaxy runs independently through the Tier 2/3 background simulation (§1.1). Factions have
**global market awareness but localized physical inventories** — they know the price of ore
everywhere and can only spend the ore they physically hold, which is what makes blockades a
strategy rather than an inconvenience.

### 5.1 Nomadic & Stationary Factions

Factions are stationary empire-builders or nomadic survivors — The Forgotten scavengers, the
Voidwalkers migrating away from conflict. **If a faction loses its final vessel and command module,
it is permanently eliminated from the galaxy.** Elimination is real and irreversible; a ten-faction
galaxy can become a six-faction galaxy through play.

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

### 5.9 Unlisted Pairings

Everything not named above starts **Neutral** and drifts through play via the writers in §5.3.

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

### 6.3 Probability Thresholds

- Material Security below 30% → **+45%** chance to launch a resource-raiding fleet.
- Two rival factions sharing a border with hostile Doctrine → border skirmish rolled every macro
  tick, moderated by relative fleet strength.

❓ *These two are the only tuned numbers in the design. The remaining weights, drift rates, and
thresholds need a balancing pass — which is what the headless `tools/economy_sim` exists for
(`architecture.md` §3). Building that tool early is cheap, because `sr_core` links no renderer.*

---

## 7. Open Design Questions ❓

Genuinely undecided items, listed so they are not mistaken for oversights.

**Performance budget — needed soonest.** `architecture.md` prescribes LOD tiers, spatial grids, and
pooling with no stated target. Without numbers we cannot tell which optimizations are essential and
which are premature. Needed: systems per galaxy, craft per active system, peak entity count, target
frame rate, and reference hardware.

**Economy and progression pacing.** No currency scale, no tier definitions, no time-to-milestone
targets. "Infinite progression" needs at least a rough curve — how long to a first custom Template,
to a first capital, to a first owned system.

**UI/UX specification — the largest missing document.** The Bridge, component-driven menus, HUD
theme, Engineering view, Galaxy Map, and station services are referenced throughout both documents
and specified nowhere as screens or flows. Much of this game is menus. StarReach2 accumulated
roughly 5,000 lines across a dozen menu files with no unifying spec, which is why they are the
messiest port target in `architecture.md` §9.

**Damage type roster.** Kinetic and Energy are the baseline. Does the roster expand? Each added type
multiplies the shield-matching matrix and the loadout puzzle — and also the content burden.

**Template economics.** Does selling a design pay once, or accrue royalties per unit manufactured?
Royalties reinforce the Macro loop far more strongly and are also far harder to balance.

**Fighter persistence in Bridge mode.** §4 — does the fighter remain a destroyable entity while its
pilot commands from the Bridge?

**Reverse-engineering costs.** Time, material, and facility-tier requirements per item grade.

**Mod load order.** `data/mods/` exists in the directory plan. Override vs. merge semantics, load
order, and what is moddable at all are unspecified (🧊 deferred, but worth deciding before the
registry format calcifies).
