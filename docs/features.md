Star Reach: Game Design Document (v1.4)

Core Concept: A systemic, top-down space sandbox that merges twitch-based tactical combat, deep engineering, and macro-level fleet command within a living, autonomous economy.
1. The Core Gameplay Loops
The game is driven by an infinite progression loop that scales the player from a solo pilot to a faction-level engineer and commander.
Micro Loop (Minute-to-Minute): Fly, manually mine resources, engage in tactical combat to target specific enemy hardpoints, and salvage destroyed vessels or explore derelicts/anomalies for rare tech.
Meso Loop (Hour-to-Hour): Dock at vessels equipped with Facility or Research modules. Deconstruct salvage, reverse-engineer rare items into repeatable blueprints, and engineer better modules. Balance ship Mass and Power constraints to create a highly optimized personal vessel, saving the configuration as a Template.
Macro Loop (Session-to-Session): Sell optimized Templates to AI factions. Watch the faction manufacture your design to conquer territory. Accumulate enough wealth to equip a Command Module, transition to the Bridge, and command automated fleets to establish or expand your own faction.
Time-Sliced LOD Simulation: Document the 3-Tier macro-simulation model (Real-Time Active Sector, Medium-Tick Local Neighborhood, and Low-Tick Galaxy Background) to solve the simulation performance overhead cleanly. 
2. Engineering, Customization & Reverse Engineering
Engineering is the heart of player progression. Ships are fully modular, built using a hierarchy of Shells (the graphical hardpoint housings), Modules (functional stats), Crafts (intermediary components), and Raw Materials.
Physical Customization & Templates
Modular Assembly: Players snap shells and modules onto a base chassis. Because assets use separated top-down texture layers, custom designs retain a distinct, recognizable visual silhouette.
The Constraints Puzzle: Custom objects are bound by physics. Every module demands Power (dictated by the ship's Power Cell) and adds Mass, which directly degrades turn rates and top speeds.
Template Creation: Once a custom ship or station configuration is finalized, the player saves it as a Template. This template can be kept for personal use or sold to factions.
Template Validation Rules: Formally document the baseline structural requirements for any ship or station template:
Must contain at least one active Armor/Chassis shell with an attached module.
Must contain at least one active Power Cell shell with an attached module.
Net power balance must be $\ge 0$, and total mass must fall within structural thresholds to pass validation.
Reverse Engineering & Research
From Loot to Blueprint: When players find rare, high-tier modules, weapons, or alien components via exploration, quests, or salvage, they don’t have to hoard them.
The Research Loop: By bringing a rare item to a station equipped with a specialized Research/Engineering Facility Module, the player can spend resources and time to reverse-engineer it.
Faction Gates: Access to higher-tier reverse engineering and complex research projects is influenced by the player’s current faction alignment and reputation tier (e.g., a high standing with a tech-focused faction unlocks advanced blueprints faster). Once successfully reverse-engineered, the item becomes permanently unlockable for mass manufacturing and template integration.
3. Combat & Localized Damage
Combat is tactical, surgical, and rewards careful loadout planning and precise execution.
Shield Dynamics & Weapon Typing
Shields are not a universal blanket defense; they interact dynamically with incoming damage types:
Shield Absorption: Shields absorb incoming weapon fire if the weapon matches the shield's type, depleting the shield pool progressively.
Shield Bypass: Mismatched weapon types can pass right through active shields to inflict immediate localized damage on underlying hardpoints and hulls.
Recharge & Vulnerability: Shields continuously regenerate over time—unless the specific shield generator hardpoint is targeted and destroyed, permanently disabling shield restoration.
Localized Hardpoint Destruction
Ships and stations are physical collections of components mapped as ECS entities rather than a single health bar.
Targeted Systems: Stripping or bypassing shields allows targeted fire to strike specific hardpoints. Destroying a thruster shell stalls a ship; destroying a weapon battery permanently disables that firing arc.
Functional Degradation: Capital ships and stations lose capabilities (repairing, building, firing) dynamically as their specific hardpoints are blown apart.
The Cost of Failure (Permadeath & Game Over)
If the ship the player is piloting (or commanding from the Bridge) is destroyed, the player dies. The active vessel and all its equipped modules are permanently lost.
Respawn & Backup: The player respawns at an allied station. If they have stashed backup ships or a fleet, they can transition straight to them, relying on stored wealth and unlocked blueprints.
The Starter Ship Safety Net: If the player has no stored ships remaining, they can opt to receive a basic, low-stats "Starter Ship" to claw their way back up.
Hard Game Over: If the player dies and all current craft, backup fleets, and stations are entirely destroyed (leaving zero assets or ships remaining), it triggers a hard Game Over. The player can then choose to restart a fresh single ship within the same game world or return to the main menu to launch an entirely new game.
4. The Bridge & Fleet Command
The game features a dual-layer control scheme, transitioning from a top-down shooter to an RTS interface.
Physical Docking: Transition to command by flying a fighter to a Capital Ship or Station and pressing the interact key.
Component-Driven Menus: The Bridge UI dynamically generates based on physical modules. A ManufacturingHardpoint enables ship construction, while a lack of a RepairHardpoint disables healing for docked craft.
RTS Directives & AI Autonomy: In Bridge mode, players issue macro-commands (Move, Attack, Defend, Build). Uncommanded AI defaults to self-preservation, patrols, and role-based tasks.
5. The Living Universe & Factions
The galaxy operates independently through a background simulation. Factions have global market awareness but localized physical inventories, creating opportunities for strategic blockades.
Nomadic & Stationary Factions
Factions can be stationary empire-builders or nomadic survivors (like The Forgotten scavengers or Voidwalkers who migrate to avoid conflict). If a faction loses its final vessel/command module, it is permanently eliminated from the galaxy.
The Canonical Faction Matrix
Players inherit parent faction relations upon alignment, and retain baseline relations when branching off to form a rogue faction.

Faction
Primary Ally
Active Rivalries

Aegis Directorate
Meridian Star Corps
Reapers, The Forgotten, Pyre Ascendancy

Meridian Star Corps
Aegis Directorate
Kore Industries, Reapers, Voidwalkers

Kore Industries
The Forgotten
Meridian Star Corps, Pyre Ascendancy, Edenian Pact

The Forgotten
Kore Industries
Aegis Directorate, Pyre Ascendancy, Reapers

Zenith Collective
Concordance
Pyre Ascendancy, Voidwalkers, Reapers

Pyre Ascendancy
Reapers
Zenith Collective, Aegis Directorate, The Forgotten

Concordance
Zenith Collective
Voidwalkers, Reapers, Edenian Pact

Voidwalkers
Edenian Pact
Zenith Collective, Concordance, Meridian Star Corps

Reapers
Pyre Ascendancy
Aegis Directorate, The Forgotten, Voidwalkers

Edenian Pact
Voidwalkers
Kore Industries, Pyre Ascendancy, Reapers

(Note: Unlisted pairings default to Neutral relations. Relations shift dynamically via natural events or rare "out-of-character" AI decisions.)

