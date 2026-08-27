#pragma once

#include <raylib.h>

#include <optional>
#include <vector>

#include "modes/space/ui/EngineeringScreen.h"

// Private to EngineeringScreen.cpp (Update()'s gesture handling) and EngineeringScreenDraw.cpp
// (Draw()'s rendering) -- NOT part of this screen's public API (EngineeringScreen.h), which stays
// raylib-free like every other docked screen's own header. architecture.md 2.2's 600-line file cap
// left the drag-and-drop rewrite needing a THIRD file rather than the usual Update+Draw/Model
// split RepairScreen.cpp and friends establish, since Update() and Draw() now share a nontrivial
// amount of Rectangle/Vector2-typed layout and hit-testing that EngineeringScreenModel.cpp's own
// "no raylib" rule (its own header comment) rules out putting there. Everything here has ordinary
// external linkage within sr::space::ui::engineering_screen -- not an unnamed namespace, since an
// unnamed namespace is TU-local and would defeat the point of a shared header -- but nothing
// outside this screen's own two .cpp files includes this header, so it is exactly as private in
// practice.
namespace sr::space::ui::engineering_screen {

constexpr float kHeaderHeight = 54.0f;
constexpr float kSectionGap = 10.0f;
constexpr float kSiblingStripHeight = 28.0f;
constexpr float kColumnGap = 16.0f;
constexpr float kPanelLabelHeight = 22.0f;
constexpr float kLegendHeight = 20.0f;
constexpr float kMinPanelHeight = 324.0f;  // Floor for CARGO HOLD/SHIP RIG on a very short window
                                           // -- ordinarily ComputeLayout stretches both panels
                                           // well past this to fill the screen.
// AvionicsMenu.cpp's own "[R] LAUNCH"/"[R] UNDOCK" prompt draws centered at
// screenHeight - kPromptMarginBottom(56) at font size 20, and is live on every docked screen
// (ResolveOwnedDockedHull doesn't care which facility tab PlayerLocation names) -- duplicated
// locally per that file's own established precedent, since neither file may depend on the other.
// This is how far the panels stop short of the bottom of the window so they never sit under it.
constexpr float kLaunchPromptClearance = 92.0f;
constexpr float kIconBoxSize = 30.0f;
constexpr float kRowHeight = 48.0f;
constexpr float kNodeRadius = 22.0f;
constexpr float kRigToggleWidth = 92.0f;
constexpr float kRigToggleHeight = 24.0f;
constexpr float kRigToggleGap = 6.0f;
constexpr float kCargoToggleHeight = 22.0f;
constexpr float kCargoToggleGap = 6.0f;
// A release within this many pixels of where the drag started is a click, not a drag -- what lets
// a cargo row keep #230's plain-click Deconstruct even though picking one up now always begins a
// potential drag.
constexpr float kDragThreshold = 6.0f;
// The rig canvas's own scroll-to-zoom range and per-notch step -- a busy rig can cram its nodes'
// labels together tightly enough to be unreadable, which is what EngineeringScreenState::rigZoom
// is for.
constexpr float kMinRigZoom = 0.4f;
constexpr float kMaxRigZoom = 4.0f;
constexpr float kRigZoomStep = 1.15f;  // Multiplier per GetMouseWheelMove() unit.

// features.md 3.9's status triad, the same three-stop thresholds Bay's/Storage's/Repair's own
// IntegrityStatusColor use -- duplicated locally per screen file on purpose, the established
// precedent those files already set.
Color IntegrityStatusColor(float fraction);

// Two side-by-side chamfered buttons -- "YOUR VESSEL" / "THIS STATION" -- reused for both the
// rig-subject selector (header, fixed width) and the cargo-subject selector (panel-width split,
// ComputeLayout sizes each use differently). Zero-sized (both rects) whenever there is no station
// subject at all, which ButtonClicked/CheckCollisionPointRec already treat as an impossible hit.
struct ToggleButtons {
    Rectangle vessel{};
    Rectangle station{};
};

struct Layout {
    Rectangle content{};
    Rectangle header{};
    ToggleButtons rigToggle;    // Which rig the SHIP RIG panel shows. Top-right, under the header.
    Rectangle siblingStrip{};   // Zero height when there is only one bench.
    Rectangle cargoPanel{};     // The bracket panel wrapping the CargoHold list -- 1/3 width.
    ToggleButtons cargoToggle;  // Which hull's CargoHold the panel shows.
    Rectangle cargoList{};      // Update()'s hit-test rect for the CargoHold rows.
    Rectangle rigPanel{};       // The bracket panel wrapping the rig schematic -- 2/3 width.
    Rectangle rigCanvas{};      // Update()'s hit-test rect for the hardpoint nodes.
};

// `content` is bridge_view::FrameContentRect() -- already inset by the router's one bezel, so
// this lays sections out inside it directly rather than re-insetting via sr::ui::PanelContentRect,
// except for each panel's own interior, which gets exactly one nested inset. CARGO HOLD takes the
// first third of `content`'s width, SHIP RIG the remaining two-thirds -- the
// schematic needs the room a 50/50 split couldn't spare it; the toggle rows below only reserve
// space when `showStationSection` is true, the same conditional-space precedent the sibling strip
// already sets. Both panels stretch vertically to fill the rest of the window (kMinPanelHeight
// floors it on a very short one), stopping kLaunchPromptClearance short of the bottom so neither
// ever sits under AvionicsMenu's own "[R] LAUNCH" prompt.
Layout ComputeLayout(Rectangle content, bool showSiblingStrip, bool showStationSection);

// Pure -- the bordered-icon-box row analog of sr::ui::ListViewRowAt, using kRowHeight. No scroll
// offset, matching Repair's/Research's own row-at functions: the panel is sized for the CargoHold
// counts this pass targets and scrolling past that is out of scope.
std::optional<int> CargoRowAt(Rectangle bounds, int rowCount, Vector2 cursor);

// The half-extent (world units) the schematic normalises every mount's `localOffset` against --
// the largest distance any authored mount sits from the rig root, so a fighter's tight rig and a
// station's sprawling one both fill the same canvas. 1.0f guards a single-mount rig (offset always
// {0,0}) from a division by zero.
float SchematicRadius(const RigBlueprint& blueprint);

// The rig canvas's live camera on top of the auto-fit layout SchematicRadius/SchematicNodeCenter
// already compute: `zoom` multiplies the auto-fit scale, `pan` offsets the result by this many
// screen pixels. Bundled into one struct purely so the geometry functions below don't each grow a
// pair of extra parameters; read straight off EngineeringScreenState::rigZoom/rigPanOffset at
// every call site.
struct SchematicView {
    float zoom = 1.0f;
    Vec2 pan;
};

// The centre point `SchematicNodeCenter` builds every node position from -- the one place both it
// and the zoom-to-cursor math in EngineeringScreen.cpp's own UpdateRigCamera compute this, so the
// two never drift apart.
Vector2 SchematicCanvasCenter(Rectangle canvas);

// One mount's screen-space node centre inside `canvas`. `localOffset` is root-relative, in world
// units (ShipBlueprint.h's own comment); this remaps it so the rig's authored +x axis (Vec2.h:
// "zero pointing along +x") reads as screen "up" and +y reads as screen right -- nose-up, the
// usual orientation for a top-down ship schematic (data/base_game/ships.json's own emitter/
// thruster offsets read correctly under it: the emitter sits at the most positive x, the thruster
// at the most negative). No chassis image sits behind this -- see EngineeringScreen.h's own header
// comment on why. `view` applies the live zoom/pan camera on top of the auto-fit scale.
Vector2 SchematicNodeCenter(Vec2 localOffset, Rectangle canvas, float schematicRadius,
                            const SchematicView& view);

// Pure -- which of `blueprint`'s mounts (by index, matching MountRows' own order) `cursor` is
// over inside `canvas` under the live `view` camera, or nullopt. Iterated back-to-front so a node
// drawn last (on top, were any ever to overlap) also wins the hit test.
std::optional<int> SchematicNodeAt(Rectangle canvas, const RigBlueprint& blueprint, Vector2 cursor,
                                   const SchematicView& view);

entt::entity EnsureStateSingleton(entt::registry& registry);
entt::entity FindStateSingleton(const entt::registry& registry);

// One rig-mount subject the right-hand section(s) edit: the requester's own vessel, or the
// station itself (architecture.md 12.30.5's station section). `blueprint` resolves the same way
// for either -- both are spawned from a ShipBlueprint (modes/space/factories/RigFactory.cpp), a
// station is not a distinct authoring type.
struct Subject {
    entt::entity rigRoot = entt::null;
    const RigBlueprint* blueprint = nullptr;
    bool isStation = false;
};

// Shared by Update and Draw: PlayerLocation's living Engineering facility, the station it
// belongs to, the requester's own vessel docked there, and its blueprint's RigBlueprint. Any
// stage failing returns entt::null/nullptr for the rest -- both callers bail the same way BayView
// and RepairScreen already do on their own equivalent chains.
struct ResolvedContext {
    entt::entity facility = entt::null;
    entt::entity station = entt::null;
    entt::entity requester = entt::null;
    const RigBlueprint* blueprint = nullptr;
};

ResolvedContext Resolve(const entt::registry& registry, const FactionId& playerFaction,
                        const core::ContentLibrary& content);

// `ctx.requester` first, then `ctx.station` when it is a second valid subject
// (StationIsSubject) and its own BlueprintRef resolves -- a station with no authored rig simply
// omits the section rather than showing an empty one.
std::vector<Subject> Subjects(const entt::registry& registry, const ResolvedContext& ctx,
                              const FactionId& playerFaction, const core::ContentLibrary& content);

// `subjects[1]` when `showsStation` names a real second subject, else `subjects[0]` -- the one
// place both panels' independent toggles resolve to an actual rig, so Update()/Draw() never
// duplicate the "is there really a station subject" check.
const Subject& SelectedSubject(const std::vector<Subject>& subjects, bool showsStation);

}  // namespace sr::space::ui::engineering_screen
