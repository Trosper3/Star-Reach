find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(sr_tests
    unit/MathTests.cpp
    unit/TextureCacheTests.cpp
    unit/HudThemeTests.cpp
    unit/IconRendererTests.cpp
    unit/LightingPassTests.cpp
    unit/CockpitHudTests.cpp
    unit/BridgeViewTests.cpp
    unit/ValidationTests.cpp
    unit/ByteStreamTests.cpp
    unit/BlueprintSerializationTests.cpp
    unit/SaveMigratorTests.cpp
    unit/SaveFileTests.cpp
    unit/FixedTimestepTests.cpp
    unit/IntentQueueTests.cpp
    unit/SystemWorldTests.cpp
    unit/CollisionSystemTests.cpp
    unit/HierarchySystemTests.cpp
    unit/OrbitSystemTests.cpp
    unit/PartySystemTests.cpp
    unit/PhysicsSystemTests.cpp
    unit/PowerSystemTests.cpp
    unit/TargetingSystemTests.cpp
    unit/WeaponSystemTests.cpp
    unit/ProjectileSystemTests.cpp
    unit/SpawnSystemTests.cpp
    unit/DamageSystemTests.cpp
    unit/DiscoveryTests.cpp
    unit/DiscoverySystemTests.cpp
    unit/KnowledgeNetworkTests.cpp
    unit/KnowledgeSerializationTests.cpp
    unit/LootSystemTests.cpp
    unit/MiningSystemTests.cpp
    unit/NpcAiSystemTests.cpp
    unit/DockingSystemTests.cpp
    unit/WarpSystemTests.cpp
    unit/CommsSystemTests.cpp
    unit/ContractSystemTests.cpp
    unit/DistressSystemTests.cpp
    unit/TutorialSystemTests.cpp
    unit/FactionEconomyTests.cpp
    unit/FactionEconomySystemTests.cpp
    unit/DiplomacyMatrixTests.cpp
    unit/ReputationTests.cpp
    unit/TerritoryTests.cpp
    unit/FactionDecisionEngineTests.cpp
    integration/ContentTests.cpp
    integration/RigFactoryTests.cpp
    integration/NpcFactoryTests.cpp
    integration/StationFactoryTests.cpp
    integration/WorldGenTests.cpp
)

# Tests link sr_space, which pulls the whole graph. The genuinely interesting property is that
# unit/ValidationTests and unit/MathTests would link against sr_shared ALONE -- no renderer, no
# registry, no mode. If that ever stops being true, something has leaked downward.
target_link_libraries(sr_tests PRIVATE sr_space sr_shared_ui Catch2::Catch2WithMain)

# Precompiled header: sr_tests is the largest target in the project (50 files), every one of
# which pulls in Catch2's macro-heavy headers, and most also touch entt::registry directly
# against a bare SystemWorld. Biggest single PCH win here by file count.
target_precompile_headers(sr_tests PRIVATE <catch2/catch_test_macros.hpp>
                                            <entt/entity/registry.hpp>)

# The integration test validates the real shipped content, not a fixture. Authored data that
# cannot be instantiated should fail CI, not a player's session.
target_compile_definitions(sr_tests PRIVATE
    SR_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/base_game"
)

catch_discover_tests(sr_tests)
