#include "modes/space/ui/EngineeringScreen.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "core/registries/ContentLibrary.h"
#include "modes/space/ui/BridgeView.h"
#include "modes/space/ui/EngineeringScreenInternal.h"
#include "shared/components/Engineer.h"
#include "shared/components/Equip.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Refactor.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::engineering_screen {

Color IntegrityStatusColor(float fraction) {
    return fraction > 0.5f   ? sr::ui::kStatusGood
           : fraction > 0.2f ? sr::ui::kStatusCaution
                             : sr::ui::kStatusCritical;
}

Layout ComputeLayout(Rectangle content, bool showSiblingStrip, bool showStationSection) {
    Layout layout;
    layout.content = content;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    float y = content.y + kHeaderHeight;

    if (showStationSection) {
        const Rectangle vessel{content.x + content.width - (kRigToggleWidth * 2.0f + kRigToggleGap),
                               y, kRigToggleWidth, kRigToggleHeight};
        const Rectangle station{vessel.x + kRigToggleWidth + kRigToggleGap, y, kRigToggleWidth,
                                kRigToggleHeight};
        layout.rigToggle = {vessel, station};
        y += kRigToggleHeight;
    }
    y += kSectionGap;

    if (showSiblingStrip) {
        layout.siblingStrip = {content.x, y, content.width, kSiblingStripHeight};
        y += kSiblingStripHeight + kSectionGap;
    }

    const float cargoWidth = (content.width - kColumnGap) / 3.0f;
    const float rigWidth = content.width - kColumnGap - cargoWidth;
    const float cargoTitleHeight =
        kPanelLabelHeight + (showStationSection ? (kCargoToggleHeight + kCargoToggleGap) : 0.0f);
    // Stretch both panels down to just above AvionicsMenu's own "[R] LAUNCH" prompt (this file's
    // own kLaunchPromptClearance comment) rather than a fixed content height -- kMinPanelHeight
    // only floors it on a window too short for that to leave any room at all.
    const float bottomLimit = static_cast<float>(GetScreenHeight()) - kLaunchPromptClearance;
    const float panelHeight = std::max(kMinPanelHeight, bottomLimit - y);

    layout.cargoPanel = {content.x, y, cargoWidth, panelHeight};
    const Rectangle cargoInner = sr::ui::PanelContentRect(layout.cargoPanel);
    if (showStationSection) {
        const float buttonWidth = (cargoInner.width - kCargoToggleGap) * 0.5f;
        const Rectangle vessel{cargoInner.x, cargoInner.y + kPanelLabelHeight + kCargoToggleGap,
                               buttonWidth, kCargoToggleHeight};
        const Rectangle station{vessel.x + buttonWidth + kCargoToggleGap, vessel.y, buttonWidth,
                                kCargoToggleHeight};
        layout.cargoToggle = {vessel, station};
    }
    layout.cargoList = {cargoInner.x, cargoInner.y + cargoTitleHeight, cargoInner.width,
                        cargoInner.height - cargoTitleHeight};

    layout.rigPanel = {content.x + cargoWidth + kColumnGap, y, rigWidth, panelHeight};
    const Rectangle rigInner = sr::ui::PanelContentRect(layout.rigPanel);
    layout.rigCanvas = {rigInner.x, rigInner.y + kPanelLabelHeight, rigInner.width,
                        rigInner.height - kPanelLabelHeight - kLegendHeight};
    return layout;
}

std::optional<int> CargoRowAt(Rectangle bounds, int rowCount, Vector2 cursor) {
    if (rowCount <= 0 || !CheckCollisionPointRec(cursor, bounds)) {
        return std::nullopt;
    }
    const int index = static_cast<int>((cursor.y - bounds.y) / kRowHeight);
    if (index < 0 || index >= rowCount) {
        return std::nullopt;
    }
    return index;
}

float SchematicRadius(const RigBlueprint& blueprint) {
    float maxDistance = 0.0f;
    for (const MountBlueprint& mount : blueprint.mounts) {
        maxDistance = std::max(maxDistance, Length(mount.localOffset));
    }
    return maxDistance > 0.0f ? maxDistance : 1.0f;
}

Vector2 SchematicCanvasCenter(Rectangle canvas) {
    return {canvas.x + canvas.width * 0.5f, canvas.y + canvas.height * 0.5f};
}

Vector2 SchematicNodeCenter(Vec2 localOffset, Rectangle canvas, float schematicRadius,
                            const SchematicView& view) {
    constexpr float kMargin = 0.78f;  // Leaves room for each node's own label past its ring.
    const float scale =
        (std::min(canvas.width, canvas.height) * 0.5f * kMargin) / schematicRadius * view.zoom;
    const Vector2 center = SchematicCanvasCenter(canvas);
    return {center.x + view.pan.x + localOffset.y * scale,
            center.y + view.pan.y - localOffset.x * scale};
}

std::optional<int> SchematicNodeAt(Rectangle canvas, const RigBlueprint& blueprint, Vector2 cursor,
                                   const SchematicView& view) {
    if (!CheckCollisionPointRec(cursor, canvas)) {
        return std::nullopt;
    }
    const float schematicRadius = SchematicRadius(blueprint);
    for (int i = static_cast<int>(blueprint.mounts.size()) - 1; i >= 0; --i) {
        const Vector2 center =
            SchematicNodeCenter(blueprint.mounts[static_cast<std::size_t>(i)].localOffset, canvas,
                                schematicRadius, view);
        if (CheckCollisionPointCircle(cursor, center, kNodeRadius * view.zoom)) {
            return i;
        }
    }
    return std::nullopt;
}

entt::entity EnsureStateSingleton(entt::registry& registry) {
    for (auto [entity] : registry.view<EngineeringScreenStateSingleton>().each()) {
        return entity;
    }
    const entt::entity singleton = registry.create();
    registry.emplace<EngineeringScreenStateSingleton>(singleton);
    registry.emplace<EngineeringScreenState>(singleton);
    return singleton;
}

entt::entity FindStateSingleton(const entt::registry& registry) {
    for (auto [entity] : registry.view<EngineeringScreenStateSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

namespace {

const RigBlueprint* ResolveBlueprint(const entt::registry& registry, entt::entity rigRoot,
                                     const core::ContentLibrary& content) {
    const BlueprintRef* blueprintRef = registry.try_get<BlueprintRef>(rigRoot);
    if (blueprintRef == nullptr) {
        return nullptr;
    }
    const ShipBlueprint* ship = content.FindShip(blueprintRef->id);
    return ship != nullptr ? &ship->rig : nullptr;
}

}  // namespace

ResolvedContext Resolve(const entt::registry& registry, const FactionId& playerFaction,
                        const core::ContentLibrary& content) {
    ResolvedContext ctx;
    ctx.facility = CurrentFacility(registry, PlayerShell(registry));
    if (ctx.facility == entt::null) {
        return ctx;
    }
    ctx.station = registry.get<ParentRig>(ctx.facility).root;
    ctx.requester = OwnedVesselAt(registry, ctx.station, playerFaction);
    if (ctx.requester == entt::null) {
        return ctx;
    }
    ctx.blueprint = ResolveBlueprint(registry, ctx.requester, content);
    return ctx;
}

std::vector<Subject> Subjects(const entt::registry& registry, const ResolvedContext& ctx,
                              const FactionId& playerFaction, const core::ContentLibrary& content) {
    std::vector<Subject> subjects{Subject{ctx.requester, ctx.blueprint, false}};
    if (StationIsSubject(registry, ctx.station, playerFaction)) {
        if (const RigBlueprint* stationBlueprint = ResolveBlueprint(registry, ctx.station, content);
            stationBlueprint != nullptr) {
            subjects.push_back(Subject{ctx.station, stationBlueprint, true});
        }
    }
    return subjects;
}

const Subject& SelectedSubject(const std::vector<Subject>& subjects, bool showsStation) {
    return (showsStation && subjects.size() > 1) ? subjects[1] : subjects[0];
}

std::optional<bridge_view::GaugeStatus> ActiveGaugeStatus(const entt::registry& registry,
                                                          const FactionId& playerFaction) {
    const entt::entity facility = CurrentFacility(registry, PlayerShell(registry));
    if (facility == entt::null) {
        return std::nullopt;
    }
    const entt::entity station = registry.get<ParentRig>(facility).root;
    if (OwnedVesselAt(registry, station, playerFaction) == entt::null) {
        return std::nullopt;
    }
    const Health* health = registry.try_get<Health>(facility);
    const float fraction =
        health != nullptr && health->max > 0.0f ? health->current / health->max : 1.0f;
    return bridge_view::GaugeStatus{"ENGINEERING BAY", fraction};
}

namespace {

// architecture.md 2.2's function-length cap -- split out of Update() below, one gesture each.

// Clears any drag naming a rig that is no longer a subject (the station stopped being one --
// undocked, lost ownership, or the sibling strip moved PlayerLocation elsewhere) and forces both
// panels back to the vessel, so a stale selection or an orphaned drag never survives losing the
// second subject.
void ResetRigCamera(EngineeringScreenState& state) {
    state.rigZoom = 1.0f;
    state.rigPanOffset = Vec2{};
    state.rigPanning = false;
}

void ResetIfStationGone(EngineeringScreenState& state, bool showStationSection) {
    if (showStationSection) {
        return;
    }
    state.cargoShowsStation = false;
    state.rigShowsStation = false;
    state.draggedModule = ModuleId();
    state.draggedModuleSource = entt::null;
    state.draggedFromMount = entt::null;
    state.draggedFromMountRig = entt::null;
    ResetRigCamera(state);
}

bool Moved(const EngineeringScreenState& state, Vector2 cursor) {
    return Distance(state.dragStartCursor, Vec2{cursor.x, cursor.y}) > kDragThreshold;
}

// A click (not a drag) on a rig node: Delete/Rebuild, unchanged from issue #230 -- fires on the
// same mouse-down frame ButtonClicked-style clicks always have, since neither verb has anything to
// pick up. Returns true once the click is consumed, whether or not a request was issued.
bool TryClickNode(entt::registry& registry, entt::entity requester, const Subject& subject,
                  const MountRow& row) {
    if (row.row.style.disabled) {
        return true;
    }
    if (row.hardpoint == entt::null) {
        registry.emplace_or_replace<RebuildMountRequest>(
            requester, RebuildMountRequest{row.mount, subject.rigRoot});
    } else {
        registry.emplace_or_replace<DeleteHardpointRequest>(
            requester, DeleteHardpointRequest{row.hardpoint, subject.rigRoot});
    }
    return true;
}

// Mouse-down while nothing is being dragged: the sibling strip, both toggle pairs, a cargo row
// (always begins a potential drag now), or a rig node (an occupied one begins a
// potential Unmount drag; anything else is TryClickNode's immediate click).
void BeginGesture(entt::registry& registry, entt::entity requester,
                  const std::vector<Subject>& subjects, const std::vector<entt::entity>& siblings,
                  entt::entity facility, const Layout& layout, bool showStationSection,
                  EngineeringScreenState& state, const sr::ui::UiInput& input) {
    if (siblings.size() > 1) {
        const std::optional<int> hit = sr::ui::TabStripHitTest(
            layout.siblingStrip, static_cast<int>(siblings.size()), input.cursor);
        if (hit.has_value()) {
            const entt::entity target = siblings[static_cast<std::size_t>(*hit)];
            if (target != facility) {
                registry.remove<PlayerLocation>(facility);
                registry.emplace<PlayerLocation>(target, PlayerLocation{target});
            }
            return;
        }
    }

    if (showStationSection) {
        if (sr::ui::ButtonClicked(layout.rigToggle.vessel, input)) {
            state.rigShowsStation = false;
            ResetRigCamera(state);
            return;
        }
        if (sr::ui::ButtonClicked(layout.rigToggle.station, input)) {
            state.rigShowsStation = true;
            ResetRigCamera(state);
            return;
        }
        if (sr::ui::ButtonClicked(layout.cargoToggle.vessel, input)) {
            state.cargoShowsStation = false;
            return;
        }
        if (sr::ui::ButtonClicked(layout.cargoToggle.station, input)) {
            state.cargoShowsStation = true;
            return;
        }
    }

    const Subject& cargoSubject = SelectedSubject(subjects, state.cargoShowsStation);
    const std::vector<ModuleRow> modules = ModuleRows(registry, cargoSubject.rigRoot);
    const std::optional<int> cargoHit =
        CargoRowAt(layout.cargoList, static_cast<int>(modules.size()), input.cursor);
    if (cargoHit.has_value()) {
        state.draggedModule = modules[static_cast<std::size_t>(*cargoHit)].module;
        state.draggedModuleSource = cargoSubject.rigRoot;
        state.dragStartCursor = {input.cursor.x, input.cursor.y};
        return;
    }

    const Subject& rigSubject = SelectedSubject(subjects, state.rigShowsStation);
    const std::optional<int> mountHit = SchematicNodeAt(
        layout.rigCanvas, *rigSubject.blueprint, input.cursor, {state.rigZoom, state.rigPanOffset});
    if (!mountHit.has_value()) {
        return;
    }
    const std::vector<MountRow> mounts =
        MountRows(registry, rigSubject.rigRoot, *rigSubject.blueprint);
    if (*mountHit >= static_cast<int>(mounts.size())) {
        return;
    }
    const MountRow& row = mounts[static_cast<std::size_t>(*mountHit)];
    if (row.holdsModules) {
        state.draggedFromMount = row.hardpoint;
        state.draggedFromMountRig = rigSubject.rigRoot;
        state.dragStartCursor = {input.cursor.x, input.cursor.y};
        return;
    }
    TryClickNode(registry, requester, rigSubject, row);
}

// A cargo-toggle button's own rig, or entt::null if `cursor` is over neither -- the drop target a
// cargo-to-cargo transfer needs, since there is no second cargo panel to drop directly onto (this
// file's own header comment).
entt::entity ToggleTargetAt(const std::vector<Subject>& subjects, const ToggleButtons& toggle,
                            Vector2 cursor) {
    if (CheckCollisionPointRec(cursor, toggle.vessel)) {
        return subjects[0].rigRoot;
    }
    if (subjects.size() > 1 && CheckCollisionPointRec(cursor, toggle.station)) {
        return subjects[1].rigRoot;
    }
    return entt::null;
}

// Release while a cargo-sourced drag is live: an unmoved release is a plain click, not a drag --
// it cancels and leaves the module exactly where it was (no request at all, the same "a mis-click
// refuses" convention ModulesMenu's own hold list already follows; a previous pass fired
// DeconstructModuleRequest here, which is the bug this comment replaces -- a bare click should
// never destroy a module the player never dragged anywhere). A drag onto a rig node Mounts
// (same-hull or cross-hull, ModuleEquipSystem's sourceCargo field carrying the difference); a drag
// onto either cargo-toggle button that names a DIFFERENT rig than the module came from is a
// straight cargo-to-cargo TransferItemRequest; anything else cancels.
void EndCargoDrag(entt::registry& registry, entt::entity requester,
                  const std::vector<Subject>& subjects, const Subject& rigSubject,
                  const Layout& layout, bool showStationSection, EngineeringScreenState& state,
                  const core::ContentLibrary& content, Vector2 cursor) {
    if (!Moved(state, cursor)) {
        return;
    }

    const std::optional<int> mountHit = SchematicNodeAt(
        layout.rigCanvas, *rigSubject.blueprint, cursor, {state.rigZoom, state.rigPanOffset});
    if (mountHit.has_value()) {
        const std::vector<MountRow> mounts =
            MountRows(registry, rigSubject.rigRoot, *rigSubject.blueprint);
        if (*mountHit < static_cast<int>(mounts.size())) {
            const entt::entity mount = mounts[static_cast<std::size_t>(*mountHit)].hardpoint;
            if (CanMountHere(registry, content, rigSubject.rigRoot, mount, state.draggedModule)) {
                MountModuleRequest request;
                request.module = state.draggedModule;
                request.mount = mount;
                if (state.draggedModuleSource != rigSubject.rigRoot) {
                    request.sourceCargo = state.draggedModuleSource;
                }
                registry.emplace_or_replace<MountModuleRequest>(rigSubject.rigRoot, request);
            }
        }
        return;
    }

    if (!showStationSection) {
        return;
    }
    const entt::entity destination = ToggleTargetAt(subjects, layout.cargoToggle, cursor);
    if (destination == entt::null || destination == state.draggedModuleSource) {
        return;
    }
    // TransferItemRequest (shared/components/StationServices.h) is always anchored on the
    // requester's own vessel with a toStation direction bool -- `destination` (an actual rig
    // entity, since ToggleTargetAt names whichever button was dropped on) resolves to that bool
    // here rather than needing its own cross-hull field the way Mount/Unmount did.
    TransferItemRequest request;
    request.kind = ItemKind::Module;
    request.id = state.draggedModule.str();
    request.quantity = 1;
    request.toStation = destination == subjects[1].rigRoot;
    registry.emplace_or_replace<TransferItemRequest>(requester, request);
}

// Release while a mount-sourced (Unmount) drag is live: a drop on the currently-shown cargo panel
// or on either cargo-toggle button sends the module to that hull's cargo -- same-hull if it
// matches where the module was mounted, cross-hull otherwise (UnmountModuleRequest::
// destinationCargo carrying the difference). Anything else -- including an unmoved release, since
// an occupied node has no plain-click action while it still holds a module (MountRows'
// "UNMOUNT FIRST") -- cancels, same as ModulesMenu's own convention.
void EndMountDrag(entt::registry& registry, const std::vector<Subject>& subjects,
                  const Subject& cargoSubject, const Layout& layout, bool showStationSection,
                  EngineeringScreenState& state, Vector2 cursor) {
    if (!Moved(state, cursor)) {
        return;
    }
    entt::entity destination = entt::null;
    if (CheckCollisionPointRec(cursor, layout.cargoList)) {
        destination = cargoSubject.rigRoot;
    } else if (showStationSection) {
        destination = ToggleTargetAt(subjects, layout.cargoToggle, cursor);
    }
    if (destination == entt::null) {
        return;
    }
    UnmountModuleRequest request;
    request.mount = state.draggedFromMount;
    if (destination != state.draggedFromMountRig) {
        request.destinationCargo = destination;
    }
    registry.emplace_or_replace<UnmountModuleRequest>(state.draggedFromMountRig, request);
}

// Shifts `pan` so the point currently under `cursor` stays under it after the schematic's scale
// changes by `ratio` (newScale / oldScale) -- the zoom-to-cursor math a plain "zoom toward the
// canvas centre" would skip. Derivation: screenPos = canvasCenter + pan + worldPos * scale, so the
// world point under the cursor is worldPos = (cursor - canvasCenter - pan) / scale; solving for
// the pan that keeps screenPos == cursor at the new scale gives this.
Vec2 ZoomPanAdjust(Vec2 pan, Vector2 canvasCenter, Vector2 cursor, float ratio) {
    const Vec2 delta{cursor.x - canvasCenter.x - pan.x, cursor.y - canvasCenter.y - pan.y};
    return {pan.x + delta.x * (1.0f - ratio), pan.y + delta.y * (1.0f - ratio)};
}

// The rig canvas's own camera controls: scroll-to-zoom, cursor-anchored so the node being zoomed
// toward is the one that stays under the pointer, and right-drag-to-pan -- a busy rig's nodes can
// sit close enough together that their labels overlap, and this is how the player spreads them
// back out. Runs every frame regardless of the left-button gesture state machine below (panning is
// a held-button drag of its own, not a click-or-release event), gated only on the cursor being
// over the canvas for the gestures that start there; an in-progress pan keeps tracking the mouse
// even if it briefly leaves the canvas, the same "once started, only the release matters" rule
// EndCargoDrag/EndMountDrag already follow for their own drags.
void UpdateRigCamera(Rectangle canvas, EngineeringScreenState& state, Vector2 cursor) {
    const bool overCanvas = CheckCollisionPointRec(cursor, canvas);

    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && overCanvas && !state.rigPanning) {
        const float oldZoom = state.rigZoom;
        state.rigZoom =
            std::clamp(state.rigZoom * std::pow(kRigZoomStep, wheel), kMinRigZoom, kMaxRigZoom);
        state.rigPanOffset = ZoomPanAdjust(state.rigPanOffset, SchematicCanvasCenter(canvas),
                                           cursor, state.rigZoom / oldZoom);
    }

    if (!state.rigPanning && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && overCanvas) {
        state.rigPanning = true;
        state.rigPanDragStart = {cursor.x, cursor.y};
        state.rigPanDragStartOffset = state.rigPanOffset;
    } else if (state.rigPanning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            state.rigPanOffset = {
                state.rigPanDragStartOffset.x + (cursor.x - state.rigPanDragStart.x),
                state.rigPanDragStartOffset.y + (cursor.y - state.rigPanDragStart.y)};
        } else {
            state.rigPanning = false;
        }
    }
}

}  // namespace

void Update(entt::registry& registry, const FactionId& playerFaction,
            const core::ContentLibrary& content) {
    const ResolvedContext ctx = Resolve(registry, playerFaction, content);
    if (ctx.requester == entt::null || ctx.blueprint == nullptr) {
        return;
    }

    const std::vector<Subject> subjects = Subjects(registry, ctx, playerFaction, content);
    const bool showStationSection = subjects.size() > 1;
    const std::vector<entt::entity> siblings = SiblingBenches(registry, ctx.station);
    const Layout layout =
        ComputeLayout(bridge_view::FrameContentRect(), siblings.size() > 1, showStationSection);

    EngineeringScreenState& state =
        registry.get<EngineeringScreenState>(EnsureStateSingleton(registry));
    ResetIfStationGone(state, showStationSection);

    // Independent of the left-button gesture state machine below: a right-drag pan or a wheel
    // zoom can happen on any frame, including one where a module is mid-drag in the other hand.
    const Vector2 cursor = GetMousePosition();
    UpdateRigCamera(layout.rigCanvas, state, cursor);

    const bool dragging = !state.draggedModule.empty() || state.draggedFromMount != entt::null;
    const sr::ui::UiInput input{cursor, IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};

    if (!dragging) {
        if (!input.clicked) {
            return;
        }
        BeginGesture(registry, ctx.requester, subjects, siblings, ctx.facility, layout,
                     showStationSection, state, input);
        return;
    }

    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        return;  // Drag still live -- Draw() shows the ghost; nothing else to do this frame.
    }

    if (!state.draggedModule.empty()) {
        const Subject& rigSubject = SelectedSubject(subjects, state.rigShowsStation);
        EndCargoDrag(registry, ctx.requester, subjects, rigSubject, layout, showStationSection,
                     state, content, cursor);
    } else {
        const Subject& cargoSubject = SelectedSubject(subjects, state.cargoShowsStation);
        EndMountDrag(registry, subjects, cargoSubject, layout, showStationSection, state, cursor);
    }

    state.draggedModule = ModuleId();
    state.draggedModuleSource = entt::null;
    state.draggedFromMount = entt::null;
    state.draggedFromMountRig = entt::null;
}

}  // namespace sr::space::ui::engineering_screen
