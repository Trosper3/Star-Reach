find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(sr_tests
    unit/MathTests.cpp
    unit/TextureCacheTests.cpp
    unit/HudThemeTests.cpp
    unit/WidgetsTests.cpp
    unit/IconRendererTests.cpp
    unit/LightingPassTests.cpp
    unit/CockpitHudTests.cpp
    unit/BridgeViewTests.cpp
    unit/NavigationMapTests.cpp
    unit/CustomizeMenuTests.cpp
    unit/ValidationTests.cpp
    unit/ByteStreamTests.cpp
    unit/BlueprintSerializationTests.cpp
    unit/SaveMigratorTests.cpp
    unit/SaveFileTests.cpp
    unit/FixedTimestepTests.cpp
    unit/IntentQueueTests.cpp
    unit/SystemWorldTests.cpp
    unit/SystemScheduleTests.cpp
    unit/CollisionSystemTests.cpp
    unit/HazardSystemTests.cpp
    unit/HierarchySystemTests.cpp
    unit/OrbitSystemTests.cpp
    unit/WorldRendererTests.cpp
    unit/PartySystemTests.cpp
    unit/PhysicsSystemTests.cpp
    unit/PlayerInputSystemTests.cpp
    unit/PowerSystemTests.cpp
    unit/TargetingSystemTests.cpp
    unit/WeaponSystemTests.cpp
    unit/ProjectileSystemTests.cpp
    unit/SpawnSystemTests.cpp
    unit/DamageSystemTests.cpp
    unit/DiscoveryTests.cpp
    unit/DiscoverySystemTests.cpp
    unit/WreckRecordTests.cpp
    unit/SeedingTests.cpp
    unit/TopologyTests.cpp
    unit/KnowledgeNetworkTests.cpp
    unit/KnowledgeSerializationTests.cpp
    unit/LootSystemTests.cpp
    unit/MiningSystemTests.cpp
    unit/ResearchRecordTests.cpp
    unit/ResearchSystemTests.cpp
    unit/TemplateMarketSystemTests.cpp
    unit/ModuleAttachmentTests.cpp
    unit/CargoViewTests.cpp
    unit/ModuleEquipSystemTests.cpp
    unit/StorageMenuTests.cpp
    unit/ModulesMenuTests.cpp
    unit/CommanderSystemTests.cpp
    unit/NpcAiSystemTests.cpp
    unit/DockingSystemTests.cpp
    unit/BuildMenuTests.cpp
    unit/RefactorSystemTests.cpp
    unit/EngineerMenuTests.cpp
    unit/RefactorMenuTests.cpp
    unit/StationServicesMenuTests.cpp
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
    integration/ConstructionSystemTests.cpp
    integration/EngineerSystemTests.cpp
    integration/ModuleEquipSystemTests.cpp
    integration/NpcFactoryTests.cpp
    integration/StationFactoryTests.cpp
    integration/StationServicesSystemTests.cpp
    integration/WorldGenTests.cpp
    integration/SpaceFlightSystemWarpTests.cpp
    integration/SpaceFlightOnEnterTests.cpp
    unit/EconomyModelTests.cpp
)

# Tests link sr_space, which pulls the whole graph. The genuinely interesting property is that
# unit/ValidationTests and unit/MathTests would link against sr_shared ALONE -- no renderer, no
# registry, no mode. If that ever stops being true, something has leaked downward.
#
# sr_economy_sim is a separate, deliberately unrelated addition to that graph: EconomyModelTests
# is the only file that needs it, and it links against nothing sr_space already provides.
target_link_libraries(sr_tests PRIVATE sr_space sr_shared_ui sr_economy_sim Catch2::Catch2WithMain)

# Precompiled header: sr_tests is the largest target in the project (50 files), every one of
# which pulls in Catch2's macro-heavy headers, and most also touch entt::registry directly
# against a bare SystemWorld. Biggest single PCH win here by file count.
target_precompile_headers(sr_tests PRIVATE <catch2/catch_test_macros.hpp>
                                            <entt/entity/registry.hpp>)

# SYSTEM: see CMakeLists.txt's sr_shared declaration for why this is explicit rather than relied
# on as a default. sr_tests is the job clang-tidy spends the most time on (50 of 106 files).
target_include_directories(sr_tests SYSTEM PRIVATE
    $<TARGET_PROPERTY:Catch2::Catch2WithMain,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:EnTT::EnTT,INTERFACE_INCLUDE_DIRECTORIES>
)

# The integration test validates the real shipped content, not a fixture. Authored data that
# cannot be instantiated should fail CI, not a player's session.
target_compile_definitions(sr_tests PRIVATE
    SR_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/base_game"
)

catch_discover_tests(sr_tests)
