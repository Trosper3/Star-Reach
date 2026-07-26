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
#   sr_menu      modes/main_menu/ has no files yet. Add with the first menu.
#   sr_shared_ui shared/ui/ has no files yet. Add with the first HUD primitive; it is the ONLY
#                library allowed to sit above both sr_shared and sr_engine.
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

# --- sr_space : the space flight mode ---------------------------------------------------------
add_library(sr_space STATIC
    src/modes/space/SpaceFlight.cpp
    src/modes/space/data/SystemWorld.cpp
    src/modes/space/factories/RigFactory.cpp
    src/modes/space/systems/SystemSchedule.cpp
)
target_link_libraries(sr_space PUBLIC sr_options sr_core sr_shared sr_engine)

# --- StarReach : the executable ---------------------------------------------------------------
add_executable(StarReach src/main.cpp)
target_link_libraries(StarReach PRIVATE sr_space)

if(SR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
