#include "modes/space/systems/OrbitSystem.h"

#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

namespace sr::space::orbit_system {
namespace {

// Recurses through `center` rather than reading a center entity's already-written
// WorldTransform, so a moon's position does not depend on iteration order visiting its planet
// first within the same tick -- every body's position is an independent pure function of
// `elapsed`, per OrbitBody's contract.
Vec2 ResolvePosition(const entt::registry& registry, entt::entity entity, float elapsed) {
    const auto* orbit = registry.try_get<OrbitBody>(entity);
    if (orbit == nullptr) {
        const auto* transform = registry.try_get<WorldTransform>(entity);
        return transform != nullptr ? transform->position : Vec2{0.0f, 0.0f};
    }

    const Vec2 center = orbit->center != entt::null
                            ? ResolvePosition(registry, orbit->center, elapsed)
                            : Vec2{0.0f, 0.0f};
    const float angle = orbit->phase + orbit->angularSpeed * elapsed;
    return center + FromAngle(angle) * orbit->radius;
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    const float elapsed = static_cast<float>(ctx.tick) * ctx.dt;

    for (auto [entity, orbit, transform] : registry.view<OrbitBody, WorldTransform>().each()) {
        if (auto* prev = registry.try_get<PreviousTransform>(entity)) {
            *prev = PreviousTransform{transform.position, transform.rotation};
        }
        transform.position = ResolvePosition(registry, entity, elapsed);
    }

    for (auto [wellEntity, well, wellTransform] :
         registry.view<GravityWell, WorldTransform>().each()) {
        for (auto [entity, velocity, transform] :
             registry.view<Velocity, WorldTransform>(entt::exclude<OrbitBody>).each()) {
            if (entity == wellEntity) {
                continue;
            }

            const Vec2 toWell = wellTransform.position - transform.position;
            const float distance = Length(toWell);
            if (distance <= 0.01f || distance >= well.range) {
                continue;
            }

            const float falloff = 1.0f - (distance / well.range);
            const float accel = well.strength * falloff * falloff;
            velocity.linear += (toWell / distance) * accel * ctx.dt;
        }
    }
}

}  // namespace sr::space::orbit_system
