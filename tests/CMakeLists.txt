find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(sr_tests
    unit/MathTests.cpp
    unit/ValidationTests.cpp
    unit/FixedTimestepTests.cpp
    unit/IntentQueueTests.cpp
    unit/SystemWorldTests.cpp
    unit/CollisionSystemTests.cpp
    unit/HierarchySystemTests.cpp
    unit/OrbitSystemTests.cpp
    unit/PhysicsSystemTests.cpp
    unit/TargetingSystemTests.cpp
    unit/WeaponSystemTests.cpp
    unit/ProjectileSystemTests.cpp
    unit/DamageSystemTests.cpp
    unit/NpcAiSystemTests.cpp
    integration/ContentTests.cpp
    integration/RigFactoryTests.cpp
    integration/NpcFactoryTests.cpp
)

# Tests link sr_space, which pulls the whole graph. The genuinely interesting property is that
# unit/ValidationTests and unit/MathTests would link against sr_shared ALONE -- no renderer, no
# registry, no mode. If that ever stops being true, something has leaked downward.
target_link_libraries(sr_tests PRIVATE sr_space Catch2::Catch2WithMain)

# The integration test validates the real shipped content, not a fixture. Authored data that
# cannot be instantiated should fail CI, not a player's session.
target_compile_definitions(sr_tests PRIVATE
    SR_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/base_game"
)

catch_discover_tests(sr_tests)
