#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/render/LightingPass.h"
#include "shared/components/Lighting.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::LightSource;
using sr::WorldTransform;
using sr::space::render::ComputeBrightness;
using sr::space::render::LightForObject;

TEST_CASE("ComputeBrightness reaches its floor at and beyond maxRange", "[lighting]") {
    const float atEdge = ComputeBrightness(1000.0f, 1000.0f);
    const float beyondEdge = ComputeBrightness(5000.0f, 1000.0f);
    CHECK(atEdge == Approx(beyondEdge));
}

TEST_CASE("ComputeBrightness is strictly brighter closer to the source", "[lighting]") {
    const float near = ComputeBrightness(100.0f, 1000.0f);
    const float mid = ComputeBrightness(500.0f, 1000.0f);
    const float far = ComputeBrightness(999.0f, 1000.0f);
    CHECK(near > mid);
    CHECK(mid > far);
}

TEST_CASE("ComputeBrightness overexposes well past 1.0 right at the source", "[lighting]") {
    CHECK(ComputeBrightness(0.0f, 1000.0f) > 1.0f);
}

TEST_CASE("ComputeBrightness treats a non-positive maxRange as ambient-only", "[lighting]") {
    const float ambient = ComputeBrightness(5000.0f, 1000.0f);  // far beyond range == ambient floor
    CHECK(ComputeBrightness(0.0f, 0.0f) == Approx(ambient));
    CHECK(ComputeBrightness(500.0f, -10.0f) == Approx(ambient));
}

TEST_CASE("LightForObject returns the ambient floor with no LightSource in the registry",
          "[lighting]") {
    entt::registry registry;
    const float ambient = ComputeBrightness(1e6f, 1000.0f);  // effectively unreachable distance
    CHECK(LightForObject(registry, sr::Vec2{100.0f, 100.0f}) == Approx(ambient));
}

TEST_CASE("LightForObject brightens near a LightSource and dims far from it", "[lighting]") {
    entt::registry registry;
    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, sr::Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<LightSource>(sun, 1000.0f);

    const float near = LightForObject(registry, sr::Vec2{50.0f, 0.0f});
    const float far = LightForObject(registry, sr::Vec2{20000.0f, 0.0f});
    CHECK(near > far);
}

TEST_CASE("LightForObject takes the max across overlapping sources, not a sum", "[lighting]") {
    entt::registry registry;
    const entt::entity a = registry.create();
    registry.emplace<WorldTransform>(a, sr::Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<LightSource>(a, 1000.0f);

    const entt::entity b = registry.create();
    registry.emplace<WorldTransform>(b, sr::Vec2{5.0f, 0.0f}, 0.0f);  // sits right at the query
    registry.emplace<LightSource>(b, 1000.0f);

    const float combined = LightForObject(registry, sr::Vec2{5.0f, 0.0f});
    const float fromClosestAlone = ComputeBrightness(0.0f, 1000.0f);
    const float fromFartherAlone = ComputeBrightness(5.0f, 1000.0f);
    CHECK(combined == Approx(fromClosestAlone));
    CHECK(combined < fromClosestAlone + fromFartherAlone);  // max, not a sum
}
