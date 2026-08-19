#include "modes/space/systems/PhysicsSystem.h"

#include <algorithm>

#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

namespace sr::space::physics_system {

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    for (auto [entity, transform, velocity, mass] :
         registry.view<WorldTransform, Velocity, BodyMass>().each()) {
        const Vec2 heading = FromAngle(transform.rotation);
        const Vec2 right = Rotated(heading, kPi / 2.0f);

        if (const auto* propulsion = registry.try_get<Propulsion>(entity)) {
            if (const auto* thrust = registry.try_get<ThrustInput>(entity)) {
                // Can exceed 1.0 when engines are commanded Boosted (features.md 2.9's
                // afterburner) -- PowerBudget::engines is not clamped to [0, 1] the way a single
                // rig-wide satisfaction float used to be.
                float engineOutput = 1.0f;
                if (const auto* budget = registry.try_get<PowerBudget>(entity)) {
                    engineOutput = budget->engines;
                }

                // Forward/strafe is a joystick, not two independent throttles: clamp the combined
                // input to unit length so a diagonal push cannot exceed the rig's rated thrust.
                Vec2 inputDir{thrust->forward, thrust->strafe};
                const float inputLength = Length(inputDir);
                if (inputLength > 1.0f) {
                    inputDir = inputDir / inputLength;
                }

                const Vec2 force = (heading * inputDir.x + right * inputDir.y) *
                                   (propulsion->thrustNewtons * engineOutput);
                velocity.linear += (force / mass.kilograms) * ctx.dt;

                const float angularAccel =
                    (propulsion->turnTorque * thrust->turn * engineOutput) / mass.kilograms;
                velocity.angular += angularAccel * ctx.dt;
            }
        }

        if (const auto* damping = registry.try_get<LinearDamping>(entity)) {
            velocity.linear -= velocity.linear * std::min(damping->perSecond * ctx.dt, 1.0f);
            velocity.angular -=
                velocity.angular * std::min(damping->angularPerSecond * ctx.dt, 1.0f);
        }

        if (const auto* propulsion = registry.try_get<Propulsion>(entity)) {
            const float speed = Length(velocity.linear);
            if (propulsion->maxSpeed > 0.0f && speed > propulsion->maxSpeed) {
                velocity.linear = velocity.linear * (propulsion->maxSpeed / speed);
            }
        }

        if (auto* prev = registry.try_get<PreviousTransform>(entity)) {
            *prev = PreviousTransform{transform.position, transform.rotation};
        }

        transform.position += velocity.linear * ctx.dt;
        transform.rotation = WrapAngle(transform.rotation + velocity.angular * ctx.dt);
    }
}

}  // namespace sr::space::physics_system
