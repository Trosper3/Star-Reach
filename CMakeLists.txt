cmake_minimum_required(VERSION 3.21)
project(StarReach VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

option(SR_BUILD_TESTS "Build the Catch2 test suite" ON)
option(SR_ENABLE_ASAN "Build with AddressSanitizer" OFF)
option(SR_WERROR "Treat warnings as errors" OFF)

find_package(EnTT CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)

# vcpkg always builds raylib with USE_EXTERNAL_GLFW=ON on desktop, but raylib's exported target
# never puts glfw in INTERFACE_LINK_LIBRARIES. When raylib is a SHARED library the omission is
# harmless -- the DLL/so resolves glfw* internally and glfw shows up only as a runtime hint in
# IMPORTED_LINK_DEPENDENT_LIBRARIES. When it is STATIC, libraylib.a ships ~90 undefined glfw*
# symbols that nothing on the link line supplies, and every executable reaching rcore.c.o fails.
#
# That is the axis, NOT the operating system: CI was green on x64-windows (dynamic triplet, raylib
# .dll) and red on x64-linux and arm64-osx (static triplets, libraylib.a). Gating on WIN32 would
# get the right answer for those three by accident and then break on x64-windows-static, so gate
# on the property that actually matters. On macOS linking glfw also restores the Cocoa/IOKit
# frameworks, which went missing through the same hole.
#
# Do NOT gate on the target's TYPE property alone. raylib's vcpkg config reports
# TYPE == SHARED_LIBRARY on x64-linux even though what it actually points at is libraylib.a, so a
# TYPE-only gate evaluated false on precisely the triplets that need glfw and CI failed with ~90
# undefined glfw* symbols while cheerfully logging "raylib is a SHARED_LIBRARY". The imported
# location is the trustworthy signal: it is the file that will actually be put on the link line.
#
# The suffix test is correct on dynamic Windows too, where IMPORTED_LOCATION is raylib.dll (the
# .lib import library hangs off IMPORTED_IMPLIB), so it does not match and glfw stays off.
set(SR_LINK_GLFW OFF)
get_target_property(SR_RAYLIB_TYPE raylib TYPE)
get_target_property(SR_RAYLIB_LOC raylib IMPORTED_LOCATION_RELEASE)
if(NOT SR_RAYLIB_LOC)
    get_target_property(SR_RAYLIB_LOC raylib IMPORTED_LOCATION)
endif()
if(SR_RAYLIB_TYPE STREQUAL "STATIC_LIBRARY"
   OR SR_RAYLIB_LOC MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    find_package(glfw3 CONFIG REQUIRED)
    set(SR_LINK_GLFW ON)
endif()
# Printed because the two paths fail very differently and only one of them is visible locally: a
# gate that silently evaluated false on a static triplet looks exactly like a correct skip on a
# dynamic one, right up until the link fails on someone else's machine. The location is printed
# alongside the type precisely because the type turned out to be the untrustworthy half.
message(STATUS "raylib TYPE=${SR_RAYLIB_TYPE} location=${SR_RAYLIB_LOC}; "
               "linking glfw explicitly: ${SR_LINK_GLFW}")

# ---------------------------------------------------------------------------------------------
# Shared compile settings, carried by an INTERFACE target rather than repeated per layer.
# ---------------------------------------------------------------------------------------------
add_library(sr_options INTERFACE)
target_include_directories(sr_options INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(sr_options INTERFACE /W4 /permissive-)
    if(SR_WERROR)
        target_compile_options(sr_options INTERFACE /WX)
    endif()
else()
    target_compile_options(sr_options INTERFACE -Wall -Wextra -Wpedantic)
    if(SR_WERROR)
        target_compile_options(sr_options INTERFACE -Werror)
    endif()
endif()

if(SR_ENABLE_ASAN)
    if(MSVC)
        target_compile_options(sr_options INTERFACE /fsanitize=address)
        # MSVC's ASan instruments std::string/std::vector with container-overflow annotations
        # that every linked translation unit must agree on. vcpkg's prebuilt libraries (Catch2,
        # raylib) are never built with /fsanitize=address, so without this the link fails with
        # LNK2038 "mismatch detected for annotate_string/annotate_vector" -- Microsoft's own
        # documented fix for mixing ASan code with non-instrumented static libraries.
        target_compile_definitions(sr_options INTERFACE _DISABLE_VECTOR_ANNOTATION
                                                        _DISABLE_STRING_ANNOTATION)
    else()
        target_compile_options(sr_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(sr_options INTERFACE -fsanitize=address)
    endif()
endif()

# =============================================================================================
# THE LAYER GRAPH -- architecture.md section 2.1
#
# This is the single highest-value structural mechanism in the project. Each layer is a real
# library with explicit dependencies, so an illegal dependency is a LINK ERROR rather than a
# code-review opinion. Laws 8 and 9 then enforce themselves, permanently and for free.
#
#   sr_shared  ->  EnTT, nlohmann_json            NO raylib. NO core. NO modes.
#   sr_core    ->  sr_shared                      NO raylib. NO engine. NO modes.
#   sr_engine  ->  raylib                         NO game knowledge at all.
#   sr_space   ->  sr_core, sr_shared, sr_engine
#   StarReach  ->  sr_space
#
# DEVIATION FROM THE ORIGINAL section 2.1 GRAPH, and why:
# it specified `sr_shared -> sr_core, sr_engine`. That cannot link. core/registries/ parses JSON
# INTO the blueprint structs that live in shared/blueprints/, so sr_core depends on sr_shared;
# making sr_shared depend on sr_core as well is a cycle. Section 2.3 already pointed at the fix
# by forbidding core/ only from `shared/ui/` rather than from all of shared/. So sr_shared is
# the bottom layer -- POD blueprints, components, pure math, no raylib -- and the one part that
# genuinely needs a renderer, shared/ui/, becomes sr_shared_ui above sr_engine when it is built.
#
# Note what the graph does NOT do: every target shares one include root (src/), so a bad
# #include still COMPILES. It fails at link only if a symbol is actually used, which header-only
# violations avoid. That gap is exactly what tools/ci/check_layers.py covers. The two mechanisms
# are complementary; neither is sufficient alone.
#
# Not yet declared, deliberately (section 2.4 -- no dead abstractions):
#   sr_net       net/ is deferred with ENet (Law 9). Add with the first wire type.
# =============================================================================================

# --- sr_shared : the bottom layer -------------------------------------------------------------
# Blueprint form (Law 3), POD components (Laws 1 and 4), and pure math. Links no renderer, which
# is what lets every component be constructed in a headless unit test and in tools/economy_sim.
add_library(sr_shared STATIC
    src/shared/blueprints/Taxonomy.cpp
    src/shared/blueprints/Validation.cpp
    src/shared/rig/CargoView.cpp
    src/shared/rig/ModuleAttachment.cpp
)
target_link_libraries(sr_shared PUBLIC sr_options EnTT::EnTT)

# Precompiled header: EnTT is header-only and heavily templated, and every one of sr_shared's
# ten files touching entt::registry re-parses it from scratch without this. PRIVATE -- a PCH is
# a build-time speedup, not part of the public interface, so it is never forced onto a consumer.
target_precompile_headers(sr_shared PRIVATE <entt/entity/registry.hpp>)

# SYSTEM: re-declares EnTT's already-linked include dir as a system include for sr_shared's own
# compiles. CMake normally infers this automatically for an IMPORTED target's consumers (and it
# does, verified locally against MSVC's -external:I), but CI's clang-tidy job showed it was NOT
# taking effect for clang-tidy-18's compile-database-driven invocation on Linux -- 4.2 million
# diagnostics generated (then discarded) from EnTT/raylib/nlohmann_json/Catch2 internals across
# 106 files, the dominant cost in that job. Declaring it explicitly here does not depend on
# inferring why the default did not carry through; it just makes the intended behavior certain.
target_include_directories(sr_shared SYSTEM PRIVATE
    $<TARGET_PROPERTY:EnTT::EnTT,INTERFACE_INCLUDE_DIRECTORIES>
)

# --- sr_core : universe truths ----------------------------------------------------------------
# Registries, intents, time. Mode-agnostic, registry-agnostic, and render-agnostic (Law 8).
# It CANNOT link raylib, and that single constraint is what makes the headless tools possible
# and keeps the faction simulation unit-testable. A link error is a better guardian than a
# paragraph.
add_library(sr_core STATIC
    src/core/ai/FactionDecisionEngine.cpp
    src/core/diplomacy/DiplomacyMatrix.cpp
    src/core/diplomacy/RelationSeeding.cpp
    src/core/diplomacy/Reputation.cpp
    src/core/diplomacy/Territory.cpp
    src/core/economy/FactionEconomy.cpp
    src/core/galaxy/ResearchRecord.cpp
    src/core/galaxy/Seeding.cpp
    src/core/galaxy/Topology.cpp
    src/core/galaxy/WreckRecord.cpp
    src/core/knowledge/KnowledgeNetwork.cpp
    src/core/knowledge/KnowledgeSeeding.cpp
    src/core/registries/JsonReader.cpp
    src/core/registries/BlueprintJson.cpp
    src/core/registries/ContentLibrary.cpp
    src/core/registries/DamageTypeEffects.cpp
    src/core/serialization/BlueprintSerialization.cpp
    src/core/serialization/KnowledgeSerialization.cpp
    src/core/serialization/SaveFile.cpp
    src/core/serialization/SaveMigrator.cpp
)
target_link_libraries(sr_core PUBLIC sr_options sr_shared nlohmann_json::nlohmann_json)

# Precompiled header: nlohmann_json is the one heavy header sr_core's registries/ files actually
# use. Deliberately NOT entt::registry or raylib.h here -- sr_core uses neither (Law 8), and a PCH
# that included them would be silent scope creep past what check_layers.py's #include-grep would
# ever flag, since a PCH-injected header never appears as an #include line in the source itself.
target_precompile_headers(sr_core PRIVATE <nlohmann/json.hpp>)

# SYSTEM: see sr_shared's identical declaration above for why this is explicit rather than relied
# on as a default.
target_include_directories(sr_core SYSTEM PRIVATE
    $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
)

# --- sr_engine : hardware abstraction ---------------------------------------------------------
# Zero game knowledge. Note the absence of sr_core and sr_shared below: that omission is the
# enforcement. engine/ is the layer a second game could be built on.
add_library(sr_engine STATIC
    src/engine/assets/TextureCache.cpp
    src/engine/assets/FontCache.cpp
    src/engine/platform/Window.cpp
)
target_link_libraries(sr_engine PUBLIC sr_options raylib)
if(SR_LINK_GLFW)
    # PUBLIC, not PRIVATE: the undefined symbols live in raylib's static archive, so glfw has to
    # stay on the link line of every executable that transitively reaches sr_engine, not just of
    # sr_engine itself.
    target_link_libraries(sr_engine PUBLIC glfw)
endif()

# Precompiled header: raylib.h is sr_engine's entire reason to exist.
target_precompile_headers(sr_engine PRIVATE <raylib.h>)

# SYSTEM: see sr_shared's identical declaration above for why this is explicit rather than relied
# on as a default.
target_include_directories(sr_engine SYSTEM PRIVATE
    $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>
)

# --- sr_shared_ui : the HUD theme and the shared widget layer ----------------------------------
# The one exception to shared/'s "no raylib, no engine/" rule (section 2.1's note; section 2.3's
# layer table). STATIC, not INTERFACE, since architecture.md 12.30 -- hit-testing has a .cpp now,
# not just palette constants and draw-time math.
add_library(sr_shared_ui STATIC
    src/shared/ui/HudTheme.h
    src/shared/ui/UiInput.h
    src/shared/ui/Row.h
    src/shared/ui/Widgets.h
    src/shared/ui/Widgets.cpp
)
target_link_libraries(sr_shared_ui PUBLIC sr_options sr_shared sr_engine)
target_precompile_headers(sr_shared_ui PRIVATE <raylib.h>)

# SYSTEM: see sr_shared's identical declaration above for why this is explicit rather than relied
# on as a default.
target_include_directories(sr_shared_ui SYSTEM PRIVATE
    $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>
)

# --- sr_menu : the main menu mode -------------------------------------------------------------
add_library(sr_menu STATIC
    src/modes/main_menu/MainMenu.cpp
)
target_link_libraries(sr_menu PUBLIC sr_options sr_core sr_shared sr_engine sr_shared_ui)
target_precompile_headers(sr_menu PRIVATE <raylib.h>)

# SYSTEM: see sr_shared's identical declaration above for why this is explicit rather than relied
# on as a default.
target_include_directories(sr_menu SYSTEM PRIVATE
    $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>
)

# --- sr_space : the space flight mode ---------------------------------------------------------
add_library(sr_space STATIC
    src/modes/space/SpaceFlight.cpp
    src/modes/space/data/SystemWorld.cpp
    src/modes/space/factories/RigFactory.cpp
    src/modes/space/factories/NpcFactory.cpp
    src/modes/space/factories/StationFactory.cpp
    src/modes/space/factories/WorldGen.cpp
    src/modes/space/render/IconRenderer.cpp
    src/modes/space/render/LightingPass.cpp
    src/modes/space/render/WorldRenderer.cpp
    src/modes/space/ui/AvionicsMenu.cpp
    src/modes/space/ui/FlightControls.cpp
    src/modes/space/ui/SystemMenu.cpp
    src/modes/space/ui/BridgeView.cpp
    src/modes/space/ui/BayView.cpp
    src/modes/space/ui/CockpitHud.cpp
    src/modes/space/ui/BuildMenu.cpp
    src/modes/space/ui/EngineerMenu.cpp
    src/modes/space/ui/RefactorMenu.cpp
    src/modes/space/ui/StorageMenu.cpp
    src/modes/space/ui/ModulesMenu.cpp
    src/modes/space/ui/StationServicesMenu.cpp
    src/modes/space/ui/StorageScreen.cpp
    src/modes/space/ui/NavigationMap.cpp
    src/modes/space/ui/CustomizeMenu.cpp
    src/modes/space/ui/ResearchScreen.cpp
    src/modes/space/systems/CaptureSystem.cpp
    src/modes/space/systems/CollisionSystem.cpp
    src/modes/space/systems/ConstructionSystem.cpp
    src/modes/space/systems/EngineerSystem.cpp
    src/modes/space/systems/RefactorSystem.cpp
    src/modes/space/systems/CommanderSystem.cpp
    src/modes/space/systems/CommsSystem.cpp
    src/modes/space/systems/ContractSystem.cpp
    src/modes/space/systems/DamageSystem.cpp
    src/modes/space/systems/DiscoverySystem.cpp
    src/modes/space/systems/DistressSystem.cpp
    src/modes/space/systems/DockingSystem.cpp
    src/modes/space/systems/FactionEconomySystem.cpp
    src/modes/space/systems/HazardSystem.cpp
    src/modes/space/systems/HierarchySystem.cpp
    src/modes/space/systems/LootSystem.cpp
    src/modes/space/systems/MiningSystem.cpp
    src/modes/space/systems/ModuleEquipSystem.cpp
    src/modes/space/systems/NpcAiSystem.cpp
    src/modes/space/systems/OrbitSystem.cpp
    src/modes/space/systems/PartySystem.cpp
    src/modes/space/systems/PhysicsSystem.cpp
    src/modes/space/systems/PlayerInputSystem.cpp
    src/modes/space/systems/PlayerLocationSystem.cpp
    src/modes/space/systems/PlayerRecordSystem.cpp
    src/modes/space/systems/PowerSystem.cpp
    src/modes/space/systems/ProjectileSystem.cpp
    src/modes/space/systems/ResearchSystem.cpp
    src/modes/space/systems/SpawnSystem.cpp
    src/modes/space/systems/StationServicesSystem.cpp
    src/modes/space/systems/TargetingSystem.cpp
    src/modes/space/systems/TemplateMarketSystem.cpp
    src/modes/space/systems/TutorialSystem.cpp
    src/modes/space/systems/WarpSystem.cpp
    src/modes/space/systems/WeaponSystem.cpp
    src/modes/space/systems/SystemSchedule.cpp
)
target_link_libraries(sr_space PUBLIC sr_options sr_core sr_shared sr_engine sr_shared_ui)

# Precompiled header: sr_space is the largest target by file count (34 .cpp) and the only one
# that genuinely needs both entt::registry (systems/, factories/) and raylib.h (render/, ui/).
target_precompile_headers(sr_space PRIVATE <entt/entity/registry.hpp> <raylib.h>)

# SYSTEM: see sr_shared's identical declaration above for why this is explicit rather than relied
# on as a default.
target_include_directories(sr_space SYSTEM PRIVATE
    $<TARGET_PROPERTY:EnTT::EnTT,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>
)

# --- StarReach : the executable ---------------------------------------------------------------
add_executable(StarReach src/main.cpp)
target_link_libraries(StarReach PRIVATE sr_space sr_menu)

# --- sr_economy_sim / economy_sim : the headless pricing-curve tool ---------------------------
# tools/economy_sim/EconomyModel.h -- deliberately linked against nothing but sr_options (no
# EnTT, no raylib, no nlohmann_json). features.md 2.10 and architecture.md 13.5 group 2c both
# describe this as a headless derivation tool, and the model doesn't touch content JSON yet
# (architecture.md 14.2 -- no elements.json/materials.json exists to read).
add_library(sr_economy_sim STATIC
    tools/economy_sim/EconomyModel.cpp
)
target_link_libraries(sr_economy_sim PUBLIC sr_options)
target_include_directories(sr_economy_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tools)

add_executable(economy_sim tools/economy_sim/main.cpp)
target_link_libraries(economy_sim PRIVATE sr_economy_sim)

if(SR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
