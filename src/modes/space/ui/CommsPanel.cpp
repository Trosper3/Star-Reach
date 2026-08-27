#include "modes/space/ui/CommsPanel.h"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <string>

#include "shared/components/Comms.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::comms_panel {
namespace {

// KEY_O: mnemonic for "open comms," and free -- features.md 3.6's Z/X/C/V/N/T/B/M/K/U/Tab are
// already reserved for the not-yet-built local command system (architecture.md 12.27) and other
// still-📋 features, and O carries no such conflict. Not a settled binding, the same
// placeholder-until-3.6-finishes reasoning StorageMenu.cpp's KEY_I and ModulesMenu.cpp's KEY_L
// document for themselves.
constexpr int kHailKey = KEY_O;

// Mirrors CommsSystem.cpp's own private kCommsLogCap -- CommsLog::entries never holds more than
// this, so the panel can size for it without exporting an internal constant across files.
constexpr std::size_t kMaxLogLines = 8;

constexpr float kPanelLeft = 24.0f;
constexpr float kPanelTop = 24.0f;
constexpr float kPanelWidth = 340.0f;
constexpr float kPadding = 12.0f;
constexpr float kHeaderFontSize = 16.0f;
constexpr float kLineFontSize = 14.0f;
constexpr float kLineHeight = 18.0f;
constexpr float kPromptFontSize = 18.0f;
constexpr float kPromptGap = 8.0f;

entt::entity FindPlayerControlled(const entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        return entity;
    }
    return entt::null;
}

const CommsLog* FindLog(const entt::registry& registry) {
    for (auto [entity, log] : registry.view<CommsLogSingleton, CommsLog>().each()) {
        (void)entity;
        return &log;
    }
    return nullptr;
}

}  // namespace

entt::entity NearestHailable(const entt::registry& registry, entt::entity player) {
    if (player == entt::null || !registry.all_of<WorldTransform>(player)) {
        return entt::null;
    }
    const Vec2 origin = registry.get<WorldTransform>(player).position;

    entt::entity nearest = entt::null;
    float nearestDistanceSq = 0.0f;
    for (auto [entity, xf, name, rig] : registry.view<WorldTransform, DisplayName, Rig>().each()) {
        (void)name;
        (void)rig;
        if (entity == player) {
            continue;
        }
        const float distanceSq = DistanceSquared(origin, xf.position);
        if (nearest == entt::null || distanceSq < nearestDistanceSq) {
            nearest = entity;
            nearestDistanceSq = distanceSq;
        }
    }
    return nearest;
}

void Update(entt::registry& registry) {
    if (!IsKeyPressed(kHailKey)) {
        return;
    }
    const entt::entity player = FindPlayerControlled(registry);
    if (player == entt::null) {
        return;
    }
    const entt::entity target = NearestHailable(registry, player);
    if (target == entt::null) {
        return;
    }
    registry.emplace_or_replace<HailRequest>(player, target);
}

void Draw(const entt::registry& registry) {
    const entt::entity player = FindPlayerControlled(registry);
    if (player == entt::null) {
        return;
    }

    const Font font = GetFontDefault();
    const CommsLog* log = FindLog(registry);
    const std::size_t lineCount =
        std::min(log == nullptr ? std::size_t{0} : log->entries.size(), kMaxLogLines);
    const float bodyHeight = static_cast<float>(std::max<std::size_t>(lineCount, 1)) * kLineHeight;
    const Rectangle bounds{kPanelLeft, kPanelTop, kPanelWidth,
                           kPadding * 2.0f + kHeaderFontSize + 4.0f + bodyHeight};
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome);
    DrawTextEx(font, "COMMS", {bounds.x + kPadding, bounds.y + kPadding}, kHeaderFontSize, 1.0f,
               sr::ui::kLabelDim);

    float y = bounds.y + kPadding + kHeaderFontSize + 4.0f;
    if (log == nullptr || log->entries.empty()) {
        DrawTextEx(font, "No signal.", {bounds.x + kPadding, y}, kLineFontSize, 1.0f,
                   sr::ui::kLabelDim);
    } else {
        for (const CommsEntry& entry : log->entries) {
            const Color color = entry.fromPlayer ? sr::ui::kValueBright : sr::ui::kStatusGood;
            DrawTextEx(font, entry.text.c_str(), {bounds.x + kPadding, y}, kLineFontSize, 1.0f,
                       color);
            y += kLineHeight;
        }
    }

    if (const entt::entity target = NearestHailable(registry, player); target != entt::null) {
        const std::string prompt = "[O] HAIL " + registry.get<DisplayName>(target).value;
        DrawTextEx(font, prompt.c_str(), {bounds.x, bounds.y + bounds.height + kPromptGap},
                   kPromptFontSize, 1.0f, sr::ui::kValueBright);
    }
}

}  // namespace sr::space::ui::comms_panel
