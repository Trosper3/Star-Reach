#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <span>
#include <string>
#include <vector>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/render/WorldRenderer.h"
#include "shared/math/Vec2.h"

// modes/space/ui/ -- SensorContacts (issue #234, features.md 3.10's "Sensor contacts -- edge
// indicators, and there is no radar"): "sensor contacts are one data source rendered as
// screen-edge indicators in combat and as the navigation map out of it. No RadarSystem, no
// second detection model." The data source is NavigationMap::VisibleHostileRigs -- this file
// only adds the edge-indicator presentation on top; it does not scan for contacts itself.
//
// features.md 3.10: "on-screen contacts need no module -- they are simply visible. Off-screen
// awareness is the sensor module's entire product," so Draw() below marks only contacts that
// project OUTSIDE the viewport; anything already on screen is left to WorldRenderer's own sprite.
namespace sr::space::ui::sensor_contacts {

struct Contact {
    entt::entity rig = entt::null;
    Vec2 worldPosition;
};

// `player`'s in-range hostile contacts (navigation_map::VisibleHostileRigs), reduced to world
// position -- everything Draw() below needs to project and clamp to the screen edge. Empty
// exactly when VisibleHostileRigs is: no living Sensor module (SensorRange::units <= 0, zeroed
// the instant the last living Sensor hardpoint dies -- shared/rig/ModuleAttachment.cpp's
// RecomputeRigTotals), out of range, wrong relation band, or an undiscovered system. Pure -- no
// raylib -- so unit-testable.
std::vector<Contact> Build(const entt::registry& registry, entt::entity player,
                           const core::diplomacy::DiplomacyMatrix* diplomacy,
                           const core::knowledge::KnowledgeStore* knowledge,
                           const std::string& systemId);

// Pure geometry: clamps `point` to the rectangle [margin, screenWidth - margin] x
// [margin, screenHeight - margin], moving it inward along the ray from `screenCenter` through
// `point` -- the direction an edge indicator points from the middle of the screen toward its
// contact. `pointWasInside` reports whether `point` already fell within that rectangle, which
// Draw() uses to skip drawing an indicator for a contact already visible as a world sprite.
// Unit-testable without a live GL context, unlike Draw() below.
struct EdgeClampResult {
    Vec2 position;
    bool pointWasInside = false;
};
EdgeClampResult ClampToEdge(Vec2 point, Vec2 screenCenter, float screenWidth, float screenHeight,
                            float margin);

// Draws one marker (render::DrawMapMarker, IconRenderer.h) per Build() contact that projects
// outside the viewport under `camera` -- WorldToScreen then ClampToEdge, screen space. Must be
// called outside BeginMode2D/EndMode2D, same as IconRenderer's other screen-space draws.
void Draw(std::span<const Contact> contacts, const render::CameraView& camera);

}  // namespace sr::space::ui::sensor_contacts
