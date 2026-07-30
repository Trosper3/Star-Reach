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
| Reverse-engineering unlocks — what you know how to manufacture | Physical modules, weapons, raw materials |
| Saved Templates (§2.2) | Ships, stations, fleet assets |
| Discovered systems and sensor intel | Credits and stockpiled goods |
| Contract and diplomatic standing history | Anything in a cargo hold |

**Networks are owned, and there is more than one.** They are not a single global player inventory:

| Network | Held by | Notes |
|---|---|---|
| **Player network** | The player directly | Follows the player across ship loss and respawn |
| **Sub-commander networks** | Each AI sub-commander (§4.1) | Independent. A commander can hold designs the player does not, and vice versa |
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

❓ *Open: whether a network has a physical location that can be raided — stealing a rival's designs
by capturing the station hosting their network. Thematically excellent (it gives Zenith Collective
and AI Concordance a reason to exist beyond flavor) and a significant scope addition. Not required
by anything above.*

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

❓ *Open: royalty rate scale and whether royalties survive the seller's death. Under §2.5 the design
sits in the buyer's network and keeps being manufactured regardless — the open part is only whether
the payment stream has anywhere to go.*

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
Corrosive) is open — see §9.*

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

❓ *Open: the recovery window's duration, whether it is wall-clock or in-game time, and whether the
wreck is visible to the player on the navigation map (§8) or must be navigated to from memory.*

#### Tier 3 — Hard Game Over 📋

**The player's faction is eliminated by the same rule as any other faction.** This is not a special
case bolted onto the player — it is §5.1's Three Pillars applied to them. A Hard Game Over is what
it looks like when the player's side loses all three at once:

| Pillar | Lost when |
|---|---|
| Command Structure | No surviving station or capital carrying a command module |
| Recognized Leadership Entity | The player is dead **and** every AI sub-commander (§4.1) is destroyed |
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

❓ *Open, and it decides how hard the rest of this design can lean on loss: whether the player may
simply reload a save instead. If saves are freely loadable, Hard Game Over is a death screen with
extra steps and every stake above is theatre. That is a legitimate stance — many sandboxes let
players set their own harshness — but it needs to be stated deliberately rather than left to fall
out of the save system's design. Decide the save model (free/manual vs. checkpoint vs. single
persistent slot) and this resolves itself.*

### 3.4 No Pause, No Safe Zones 📋

**The simulation never stops while the player is alive.** Opening the navigation map (§8), the
Engineering view, station services, or the Bridge interface does **not** pause the game. Ships keep
flying, weapons keep firing, and the player's vessel remains a physical, targetable object the entire
time.

There are no safe zones. Docking at a friendly station is protection by *circumstance* — the
station's guns and its owner's disposition — never by rule. A station that is losing a fight is not
a refuge, and §3.3 still applies to a player sitting inside one.

This is what gives the engineering and command layers real weight: time spent in a menu is time the
galaxy spends without the player watching it, and choosing *when* to open one is a tactical decision.

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

### 4.1 AI Sub-Commanders 📋

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

❓ *Open: how sub-commanders are recruited or created, whether they have individual competence or
personality traits that affect their autonomous decisions, and whether a rival faction can turn one.*

---

## 5. The Living Universe & Factions 📋

The galaxy runs independently through the Tier 2/3 background simulation (§1.1). Factions have
**global market awareness but localized physical inventories** — they know the price of ore
everywhere and can only spend the ore they physically hold, which is what makes blockades a
strategy rather than an inconvenience.

### 5.1 Faction Survival & Elimination — The Three Pillars 📋

Factions are stationary empire-builders or nomadic survivors — The Forgotten scavengers, the
Voidwalkers migrating away from conflict. Both kinds live or die by the same test.

**A faction remains active as long as it holds at least one of three pillars:**

| Pillar | Held while the faction has… |
|---|---|
| **Command Structure** | At least one station or capital carrying a command module |
| **Recognized Leadership Entity** | At least one surviving leader — a faction head, or an AI sub-commander (§4.1) |
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

❓ *Open: whether a rogue operator can be formally invited to* join *an existing faction as a member
rather than founding their own, and what that does to asset ownership.*

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

**AI craft are bound by exactly the same mechanics as the player.** This is a hard design rule, not
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
thresholds need a balancing pass — which is what the headless `tools/economy_sim` exists for
(`architecture.md` §3). Building that tool early is cheap, because `sr_core` links no renderer.*

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
military-weight indicators; individual craft simply do not exist at that scale.

❓ *Open: whether Level 3 shows only assets the player has sensor coverage of, or everything in the
system. Fog-of-war here would give `DiscoverySystem` and sensor intel real teeth, at the cost of
making the RTS surface partially blind.*

---

## 9. Open Design Questions ❓

Genuinely undecided items, listed so they are not mistaken for oversights.

**Performance budget — needed soonest.** `architecture.md` prescribes LOD tiers, spatial grids, and
pooling with no stated target. Without numbers we cannot tell which optimizations are essential and
which are premature. Needed: systems per galaxy, craft per active system, peak entity count, target
frame rate, and reference hardware.

**Economy and progression pacing.** No currency scale, no tier definitions, no time-to-milestone
targets. "Infinite progression" needs at least a rough curve — how long to a first custom Template,
to a first capital, to a first owned system.

**The save model — now blocking a design decision, not just an implementation one.** §3.3 Tier 3
depends on it. If saves are freely loadable at any time, Hard Game Over is a death screen with extra
steps and the entire three-tier failure structure is theatre. Decide: free/manual saves, checkpoint
saves, or a single persistent slot. This is the highest-leverage open question in the document
because several others resolve automatically once it is settled.

**UI/UX specification — the largest missing document.** The Bridge, component-driven menus, HUD
theme, Engineering view, and station services are referenced throughout both documents and specified
nowhere as screens or flows. Much of this game is menus. StarReach2 accumulated roughly 5,000 lines
across a dozen menu files with no unifying spec, which is why they are the messiest port target in
`architecture.md` §9. *§8 specifies the navigation map, and `architecture.md` §12.9–§12.12 now give
Template creation, station services, cargo/hardpoint equip, and construction/refit/grafting the same
treatment — the remaining unspecified pieces are the HUD theme's full screen inventory and whichever
of §12.12's `EngineerMenu` question resolves to a genuinely new mechanic.*

**Damage type roster.** Kinetic and Energy are the baseline. Does the roster expand? Each added type
multiplies the shield-matching matrix and the loadout puzzle — and also the content burden.

**Fighter persistence in Bridge mode.** §4 — does the fighter remain a destroyable entity while its
pilot commands from the Bridge?

**Reverse-engineering costs.** Time, material, and facility-tier requirements per item grade.

**Recovery-run parameters.** §3.3 Tier 2 — the window's duration, wall-clock vs. in-game time, and
whether the death wreck is marked on the navigation map.

**Sub-commander recruitment and loyalty.** §4.1 — how they are acquired, whether they have
individual competence or personality, and whether a rival can turn one.

**Network raiding.** §2.5 — whether a knowledge network has a capturable physical host, letting a
faction steal designs rather than merely destroy the holder.

**Royalty scale and posthumous payment.** §2.6 — the per-unit rate, and whether a royalty stream
survives the seller's death.

**Level 3 fog of war.** §8.2 — does the orbital view show everything in the system, or only what the
player has sensor coverage of?

*Resolved since earlier drafts:* **Template economics** (§2.6 — player chooses lump sum or
royalties, negotiated against faction disposition) and **research permanence** (§2.5 — knowledge
lives in networks, which die with their hosts).

**Mod load order.** `data/mods/` exists in the directory plan. Override vs. merge semantics, load
order, and what is moddable at all are unspecified (🧊 deferred, but worth deciding before the
registry format calcifies).
