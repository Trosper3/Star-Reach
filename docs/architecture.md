Star Reach: Architecture & Engineering Guidelines (v1.2)

Target Audience: Core Programmers, AI Agents, & Open-Source Contributors Tech Stack: C++20, CMake, Raylib, ENet, vcpkg, GitHub Actions
1. Core Architectural Philosophy (The 8 Laws)
To maintain a scalable, multi-year C++ codebase, all contributors and AI agents must strictly adhere to a Data-Driven ECS (Entity-Component-System) architecture. This enforces a rigid boundary between pure data, simulation logic, rendering, and multiplayer authority.
The ECS + Data-Driven Hybrid
Data-Driven (JSON Blueprints): External JSON configurations dictate what objects are (e.g., base stats, hardpoint slots, faction rules). These live exclusively in core/registries/.
ECS (Live Memory): The active simulation utilizes contiguous arrays of plain-old-data components (Transform, Health, HardpointRig) attached to generic Entity IDs. Systems run logic over these components at a fixed 60 Hz tick rate. These live in shared/entities/ and modes/[mode]/systems/.
ECS Memory Management: Document the use of Sparse Sets with Packed Arrays (utilizing swap-and-pop deletion) to handle dynamic component attachment and removal (critical for localized hardpoint destruction) with zero heap fragmentation. 
Factories First
Object assembly logic (e.g., NpcFactory, StationBuilder) must be completely decoupled from frame-by-frame simulation systems to avoid circular dependencies.
The Unified Rig Law
Starships, orbital stations, and ground structures share the exact same underlying mount hierarchy: Shell -> Component -> Module. This foundational struct is defined centrally in shared/entities/Hardpoint.h.
The Diplomacy/Economy Bridge Law
Galaxy-wide state—including the diplomacy matrix, reputation scores, and faction resource stocks—lives in core/. It is completely mode-agnostic and serves both Space Flight and future Planet modes.
The Multiplayer Authority Law (Client/Server)
UI actions and player inputs generate Intents (e.g., PlaceShipRequest) sent to the host via the net/ layer. The host validates the intent against core/economy/, simulates the result, and synchronizes the new ECS state back to clients. UI never mutates game state directly.
Scope & Network Foundation: Note the single-player-first development strategy, while maintaining a clean Data/Logic command-pattern decoupling to ensure a smooth transition to ENet multiplayer later. 
The Orchestrator Exclusivity Law
Mode files (e.g., SpaceFlight.cpp) are orchestrators. They manage lifecycle events (Init, Update, Draw) and system routing. Math Rule: Presentation math (camera interpolation, viewport scaling, UI positioning) naturally belongs here. However, simulation math (AI logic, combat calculations, physics) is strictly forbidden.
The "No Dump Folders" Law
Generic directories named utility/, helpers/, or misc/ are banned. Domain-agnostic helpers are acceptable, but they must be strictly scoped by their technical purpose (e.g., core/math/Random.h or core/string/Format.h). This gives flexibility while preventing massive junk-drawer files.
The Anti-Spaghetti Event Law
Systems may listen to the Event Bus, but a system cannot emit a new event in direct response to receiving one. If an event must trigger a multi-step chain reaction, that sequence must be explicitly managed by an orchestrator, not by bouncing blindly through the bus.
2. Master Directory Blueprint
All new files must be placed according to this strict domain hierarchy.
Plaintext
StarReach/
├── .github/                # CI/CD & DevOps Automation
│   └── workflows/          
│       ├── build.yml  # Cross-platform compilation, tests, format checks, ASan
│       └── deploy.yml      # Apple notarization & Steam deployment on release tags
│
├── data/                   # External Data & Asset Blueprints
│   ├── base_game/          # Canonical JSON registries (ships, modules, factions)
│   └── mods/               # User-generated overrides loaded at runtime
|
├── docs/		            # Markdown files / guides for architecture / game design and lore
│
├── tools/                  # Internal Development Tooling (Standalone Executables)
│   ├── asset_viewer/       # Visualizer for testing modular ship/component assembly
│   └── economy_sim/        # Headless simulation tool for supply/demand balancing
│
├── tests/                  # Automated Test Suites (Excluded from production builds)
│   ├── unit/               # Fast, isolated tests for math, logic, and state transitions
│   ├── integration/        # Factory outputs, system interactions, & JSON schema validation
│   └── mocks/              # Mock datasets for test feeds
│
├── src/                    # Primary C++ Source Code
│   ├── main.cpp            # Application entry point
│   │
│   ├── engine/             # The Hardware Abstraction Layer (Mode-Agnostic)
│   │   ├── platform/       # Window lifecycle, OS APIs, & crash handlers
│   │   ├── graphics/       # Low-level rendering wrappers & shader compilation
│   │   ├── input/          # Action mapping layer (Keyboard/Gamepad/Steam Input)
│   │   ├── memory/         # Object pools and contiguous allocators
│   │   └── assets/         # Asset streaming, UUID lookup, and sprite compositing
│   │
│   ├── core/               # Mode-Agnostic Universe Truths & Rules
│   │   ├── algorithms/     # Pure mathematical models (graph theory, pathfinding)
│   │   ├── diplomacy/      # Global faction relation matrix & reputation tracking
│   │   ├── economy/        # Galaxy-wide command economy, trade routes, & stock logic
│   │   ├── events/         # Global Event Bus (decouples audio, combat, UI, & quests)
│   │   ├── registries/     # JSON parsers loading data into memory
│   │   └── serialization/  # Unified byte packing for saves and network snapshots
│   │
│   ├── shared/             # Unified Data Models & Cross-Mode UI
│   │   ├── entities/       # Pure ECS Components (`Transform`, `Health`, `HardpointRig`)
│   │   ├── math/           # Domain-specific math tied to data (`OrbitMath`)
│   │   └── ui/             # Cross-mode UI primitives (HUD themes, chamfered rects)
│   │
│   ├── net/                # Multiplayer Networking Authority
│   │   ├── NetSession      # ENet connection lifecycle (Host vs Client)
│   │   └── Protocol        # Binary wire protocol & packet serialization
│   │
│   └── modes/              # Mode-Specific Orchestrators
│       ├── IGameMode.h     # Standard interface (`OnEnter`, `Update`, `Draw`, `OnExit`)
│       ├── main_menu/      # Main menu and transition loading screens
│       │
│       ├── space/   # Space Gameplay Mode
│       │   ├── SpaceFlight.cpp/.h # Lifecycle, Camera Math, & Routing ONLY
│       │   ├── data/       # Space ECS state (`SystemWorld`, `Asteroid`)
│       │   ├── factories/  # Object construction (`WorldGen`, `NpcFactory`)
│       │   ├── systems/    # Active game loops (`CombatSystem`, `NpcAiSystem`)
│       │   ├── render/     # 3D World rendering (`WorldRenderer`, `LightingPass`)
│       │   └── ui/         # 2D Screen-space HUD & modals (`CockpitHud`, `AvionicsMenu`)
│       │
│       └── planet/         # YAGNI BOUNDARY - DO NOT BUILD YET
│                           # Exists purely to force `core/` to remain mode-agnostic.
│                           # Zero active development until Space Flight is complete.
│
├── CMakeLists.txt          # Master CMake build script linking src, tools, and tests
├── vcpkg.json              # Dependency manifest (Raylib, ENet, nlohmann_json, Catch2)
└── .clang-format           # Universal C++ formatting rules for all contributors

3. "Where Does It Go?" (Utility & Helper Guide)
Because generic utility/ folders are not recommended, use this cheat sheet to properly categorize common helper functions:
String formatting, parsing, & casting: core/string/
Random Number Generation: core/math/Random.h
Procedural Name Generation (Factions/Ships): core/generation/NameGenerator.h
Domain-Agnostic Algorithms (A Pathfinding, Graphs):* core/algorithms/
Game-Specific Math (Orbit calculations, Line-of-sight): shared/math/
UI primitives & color palettes: shared/ui/
4. Subsystem Engineering Standards
When implementing new features, contributors must utilize the established subsystems rather than writing ad-hoc solutions.
Memory Pooling (engine/memory/): Use object pools for high-frequency, short-lived entities (projectiles, particles) to prevent heap fragmentation and frame stutters. Avoid raw new/delete mid-frame.
Unified Serialization (core/serialization/): Use a single serialization pipeline to handle both writing .sav files to disk and byte-packing packets for ENet propagation.
Fixed Timestep Simulation: Game logic ticks execute at a fixed rate (e.g., 60 Hz) using a time accumulator, ensuring fully deterministic physics regardless of monitor refresh rates or FPS spikes.
Save Schema Migration: Save files must include a schemaVersion header. This is processed through SaveMigrator.h to convert older save formats to current memory layouts, preventing version-mismatch crashes.
5. Asset Pipeline & Management
Pushing raw assets directly to the GPU limits scale. The asset pipeline must strictly separate authoring formats from runtime formats.
Texture Atlases & Compositing: Raw separated PNGs (chassis, weapon layers) are for authoring only. At runtime, the engine/assets/ compositor bakes static layers into single dynamic spritesheets. Dynamic components (turrets, exhaust plumes) remain separate draw calls.
Audio Banks: Do not load hundreds of individual .wav files into memory simultaneously. Audio must be streamed or loaded via chunked banks based on the active scene context.
Asset Naming & Lookups: Avoid hardcoded string paths in C++ gameplay code (e.g., LoadTexture("ship.png")). The Asset Manager assigns UUIDs or hashed IDs to all assets upon loading. Live entities and registries query assets strictly via these IDs.
Hot-Reloading: During development, the asset system watches the data/ directory for filesystem modifications. Changes to JSON blueprints or shaders trigger hot-reloads without requiring the application to restart.
6. Build System & Dependency Toolchain
The project rejects manual binary dependency management in favor of a declarative pipeline.
Package Manager: Dependencies are managed via vcpkg operating in Manifest Mode (vcpkg.json). This ensures identical compilation environments across Windows, macOS, and Linux.
Build System: CMake is the single source of truth, utilizing the Ninja generator for parallel compilation and Precompiled Headers (PCH) to drastically reduce compile times for heavy libraries (like <vector> and JSON).
Dependency Automation: Renovate Bot is scheduled to monitor vcpkg.json, enforcing rigid version pinning and automating upgrade pull requests.
7. Cross-Platform CI/CD & Apple/Steam Automation
Code must remain cross-platform natively. The CI/CD pipeline enforces code quality and automates release mechanics.
Continuous Integration (GitHub Actions)
Matrix Builds: Every push and pull request compiles and tests against windows-latest (MSVC), ubuntu-latest (GCC/Clang), and macos-latest (Clang).
Quality Assurance Pipeline:
Code Formatting: CI enforces .clang-format and .clang-tidy. Unformatted PRs will automatically fail the build.
Automated Testing: Driven by Catch2/gtest via the tests/ directory.
JSON Schema Validation: CI automatically validates all data/base_game/ .json configurations against schemas to block typo-induced runtime crashes.
Memory Safety: A dedicated CI job builds with AddressSanitizer (ASan) to detect memory leaks and illegal pointers automatically.
macOS Porting Requirements (Automated in CI)
Universal Binaries: CMake utilizes Apple arguments (-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64") to compile natively for both Intel Macs and Apple Silicon.
App Bundle Packaging: CMake packages the .exe and data/ directories into a strictly formatted macOS .app bundle at build time.
Apple Code Signing & Notarization: An automated GitHub Action utilizes repository secrets to execute codesign and notarytool, satisfying Apple's Gatekeeper security requirements before shipping.
Steam Deployment Operations
Pushing a Git release tag (e.g., v1.0.0-beta) triggers the deployment pipeline.
The pipeline utilizes steamcmd (via game-ci/steam-deploy) to ingest release artifacts.
Builds are pushed directly to the project's internal testing branch on Steamworks, pending manual promotion to the public branch.

Building
Configure the project with CMake:
cmake -B build -S .


Build the executable:
cmake --build build


Run the binary:
Windows: .\build\bin\StarReach.exe
Linux/macOS: ./build/bin/StarReach
