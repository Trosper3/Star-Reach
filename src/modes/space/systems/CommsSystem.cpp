#include "modes/space/systems/CommsSystem.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "shared/components/Comms.h"
#include "shared/components/Identity.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

namespace sr::space::comms_system {
namespace {

// Ported verbatim from legacy StarReach2's kCommsLogCap.
constexpr std::size_t kCommsLogCap = 8;

constexpr std::array<const char*, 3> kHailResponses = {
    "Receiving you.",
    "Go ahead.",
    "We read you.",
};

// A pure function of (entity, tick) rather than global RNG state -- Law 2's coarse-tick
// fast-forward needs every time-dependent decision to be reproducible from the same inputs, the
// same reasoning MiningSystem's material roll already documents.
std::uint32_t Hash32(std::uint32_t seed) {
    std::uint32_t hash = 2166136261u;
    for (int byte = 0; byte < 4; ++byte) {
        hash ^= (seed >> (byte * 8)) & 0xFFu;
        hash *= 16777619u;
    }
    return hash;
}

// System.h's "one legitimate cache" exception: a singleton entity rather than mode-held state
// (Law 6). Created the first time anything is logged, not up front.
entt::entity EnsureLog(entt::registry& registry) {
    for (auto [entity] : registry.view<CommsLogSingleton>().each()) {
        return entity;
    }
    const entt::entity log = registry.create();
    registry.emplace<CommsLogSingleton>(log);
    registry.emplace<CommsLog>(log);
    return log;
}

void PushEntry(entt::registry& registry, entt::entity logEntity, std::string text,
               bool fromPlayer) {
    CommsLog& log = registry.get<CommsLog>(logEntity);
    log.entries.push_back(CommsEntry{std::move(text), fromPlayer});
    if (log.entries.size() > kCommsLogCap) {
        log.entries.erase(log.entries.begin());
    }
}

std::string NameOf(const entt::registry& registry, entt::entity entity) {
    if (const auto* name = registry.try_get<DisplayName>(entity)) {
        return name->value;
    }
    return "Unknown contact";
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request, xf, sensor] :
         registry.view<HailRequest, WorldTransform, SensorRange>().each()) {
        consumed.push_back(self);

        if (request.target == entt::null || !registry.valid(request.target)) {
            continue;
        }
        const auto* targetXf = registry.try_get<WorldTransform>(request.target);
        if (targetXf == nullptr) {
            continue;
        }

        const entt::entity log = EnsureLog(registry);
        const std::string name = NameOf(registry, request.target);
        PushEntry(registry, log, "Hailing " + name + "...", true);

        if (Distance(xf.position, targetXf->position) <= sensor.units) {
            const auto seed = static_cast<std::uint32_t>(entt::to_integral(self)) * 2654435761u +
                              static_cast<std::uint32_t>(ctx.tick);
            const std::size_t index = Hash32(seed) % kHailResponses.size();
            PushEntry(registry, log, name + ": " + kHailResponses[index], false);
        } else {
            PushEntry(registry, log, name + " does not respond -- out of range.", false);
        }
    }

    for (const entt::entity self : consumed) {
        registry.remove<HailRequest>(self);
    }
}

}  // namespace sr::space::comms_system
