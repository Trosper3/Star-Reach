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
set(SR_LINK_GLFW OFF)
get_target_property(SR_RAYLIB_TYPE raylib TYPE)
if(SR_RAYLIB_TYPE STREQUAL "STATIC_LIBRARY")
    find_package(glfw3 CONFIG REQUIRED)
    set(SR_LINK_GLFW ON)
endif()
# Printed because the two paths fail very differently and only one of them is visible locally: a
# gate that silently evaluated false on a static triplet looks exactly like a correct skip on a
# dynamic one, right up until the link fails on someone else's machine.
message(STATUS "raylib is a ${SR_RAYLIB_TYPE}; linking glfw explicitly: ${SR_LINK_GLFW}")

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
)
target_link_libraries(sr_shared PUBLIC sr_options EnTT::EnTT)

# --- sr_core : universe truths ----------------------------------------------------------------
# Registries, intents, time. Mode-agnostic, registry-agnostic, and render-agnostic (Law 8).
# It CANNOT link raylib, and that single constraint is what makes the headless tools possible
# and keeps the faction simulation unit-testable. A link error is a better guardian than a
# paragraph.
add_library(sr_core STATIC
    src/core/diplomacy/DiplomacyMatrix.cpp
    src/core/diplomacy/Reputation.cpp
    src/core/diplomacy/Territory.cpp
    src/core/economy/FactionEconomy.cpp
    src/core/registries/JsonReader.cpp
    src/core/registries/BlueprintJson.cpp
    src/core/registries/ContentLibrary.cpp
)
target_link_libraries(sr_core PUBLIC sr_options sr_shared nlohmann_json::nlohmann_json)

# --- sr_engine : hardware abstraction ---------------------------------------------------------
# Zero game knowledge. Note the absence of sr_core and sr_shared below: that omission is the
# enforcement. engine/ is the layer a second game could be built on.
add_library(sr_engine STATIC
    src/engine/platform/Window.cpp
)
target_link_libraries(sr_engine PUBLIC sr_options raylib)
if(SR_LINK_GLFW)
    # PUBLIC, not PRIVATE: the undefined symbols live in raylib's static archive, so glfw has to
    # stay on the link line of every executable that transitively reaches sr_engine, not just of
    # sr_engine itself.
    target_link_libraries(sr_engine PUBLIC glfw)
endif()

# --- sr_shared_ui : the HUD theme -------------------------------------------------------------
# The one exception to shared/'s "no raylib, no engine/" rule (section 2.1's note; section 2.3's
# layer table). Header-only, like every previous incarnation of this file -- there is no state
# to own, only palette constants and draw-time math. An INTERFACE library still needs a real
# consumer to be exercised by the build; tests/unit/HudThemeTests.cpp is that consumer until
# modes/space/ui/ (a separate, dependent issue) becomes the real one.
add_library(sr_shared_ui INTERFACE)
target_sources(sr_shared_ui INTERFACE src/shared/ui/HudTheme.h)
target_link_libraries(sr_shared_ui INTERFACE sr_options sr_shared sr_engine)

# --- sr_menu : the main menu mode -------------------------------------------------------------
add_library(sr_menu STATIC
    src/modes/main_menu/MainMenu.cpp
)
target_link_libraries(sr_menu PUBLIC sr_options sr_core sr_shared sr_engine sr_shared_ui)

# --- sr_space : the space flight mode ---------------------------------------------------------
add_library(sr_space STATIC
    src/modes/space/SpaceFlight.cpp
    src/modes/space/data/SystemWorld.cpp
    src/modes/space/factories/RigFactory.cpp
    src/modes/space/factories/NpcFactory.cpp
    src/modes/space/factories/StationFactory.cpp
    src/modes/space/factories/WorldGen.cpp
    src/modes/space/render/WorldRenderer.cpp
    src/modes/space/systems/CollisionSystem.cpp
    src/modes/space/systems/CommsSystem.cpp
    src/modes/space/systems/ContractSystem.cpp
    src/modes/space/systems/DamageSystem.cpp
    src/modes/space/systems/DistressSystem.cpp
    src/modes/space/systems/DockingSystem.cpp
    src/modes/space/systems/FactionEconomySystem.cpp
    src/modes/space/systems/HierarchySystem.cpp
    src/modes/space/systems/LootSystem.cpp
    src/modes/space/systems/MiningSystem.cpp
    src/modes/space/systems/NpcAiSystem.cpp
    src/modes/space/systems/OrbitSystem.cpp
    src/modes/space/systems/PartySystem.cpp
    src/modes/space/systems/PhysicsSystem.cpp
    src/modes/space/systems/PowerSystem.cpp
    src/modes/space/systems/ProjectileSystem.cpp
    src/modes/space/systems/SpawnSystem.cpp
    src/modes/space/systems/TargetingSystem.cpp
    src/modes/space/systems/TutorialSystem.cpp
    src/modes/space/systems/WarpSystem.cpp
    src/modes/space/systems/WeaponSystem.cpp
    src/modes/space/systems/SystemSchedule.cpp
)
target_link_libraries(sr_space PUBLIC sr_options sr_core sr_shared sr_engine)

# --- StarReach : the executable ---------------------------------------------------------------
add_executable(StarReach src/main.cpp)
target_link_libraries(StarReach PRIVATE sr_space sr_menu)

if(SR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
