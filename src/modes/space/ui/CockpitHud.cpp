#include "modes/space/ui/CockpitHud.h"

#include <raylib.h>

#include <array>
#include <string>

#include "modes/space/ui/HudGating.h"
#include "modes/space/ui/SensorContacts.h"
#include "modes/space/ui/StatusProjection.h"
#include "shared/components/Identity.h"
#include "shared/rig/ModuleAttachment.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::cockpit_hud {
namespace {

// Placeholder placement/size: features.md 3.10's bottom band (a separate issue) is what actually
// sizes and positions this against the rest of the flight HUD -- "its diameter sets the band
// height." Until that lands, this keeps the projection where the old flat bar sat.
constexpr float kDiameter = 140.0f;
constexpr float kMargin = 24.0f;

// The module-gated button bar's placeholder placement -- features.md 3.10's "right cluster,"
// bottom-right until the full bottom band lands alongside the projection above. Fixed regardless
// of which slots are online (features.md 3.10: "the bar never resizes as hardpoints die").
constexpr float kSlotWidth = 150.0f;
constexpr float kSlotHeight = 26.0f;
constexpr float kSlotGap = 6.0f;
constexpr float kSlotFontSize = 14.0f;

void DrawSlotBar(
    const std::array<hud_gating::SlotStatus, hud_gating::kSurfaceOrder.size()>& slots) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const Font font = GetFontDefault();
    const float barHeight = static_cast<float>(slots.size()) * (kSlotHeight + kSlotGap) - kSlotGap;
    float y = screenHeight - kMargin - barHeight;

    for (const hud_gating::SlotStatus& slot : slots) {
        const Rectangle bounds{screenWidth - kMargin - kSlotWidth, y, kSlotWidth, kSlotHeight};
        std::string label(hud_gating::Label(slot.surface));
        Color textColor = sr::ui::kValueBright;
        if (!slot.online) {
            label += " OFFLINE";
            textColor = sr::ui::kStatusCritical;
        }
        sr::ui::DrawChamferedButton(bounds, label.c_str(), font, kSlotFontSize, sr::ui::kPanelGlass,
                                    sr::ui::kPanelChrome, textColor);
        y += kSlotHeight + kSlotGap;
    }
}

}  // namespace

float AggregateHullFraction(const entt::registry& registry, entt::entity rigRoot) {
    return rig_attachment::AggregateStructuralIntegrity(registry, rigRoot);
}

void Draw(const entt::registry& registry, const core::diplomacy::DiplomacyMatrix& diplomacy,
          const core::knowledge::KnowledgeStore& knowledge, const std::string& systemId,
          const render::CameraView& camera) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        const float screenHeight = static_cast<float>(GetScreenHeight());
        const float radius = kDiameter * 0.5f;
        const Vec2 center{kMargin + radius, screenHeight - kMargin - radius};

        const status_projection::Projection projection =
            status_projection::Build(registry, entity, kDiameter);
        status_projection::Draw(projection, center, kDiameter);

        DrawSlotBar(hud_gating::BuildSlots(registry, entity));

        const std::vector<sensor_contacts::Contact> contacts =
            sensor_contacts::Build(registry, entity, &diplomacy, &knowledge, systemId);
        sensor_contacts::Draw(contacts, camera);
        return;  // Exactly one PlayerControlled entity (shared/components/Identity.h).
    }
}

}  // namespace sr::space::ui::cockpit_hud
