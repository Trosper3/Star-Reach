// EngineeringScreen.cpp's own rendering half, split out to satisfy architecture.md 2.2's 600-line
// file cap -- the cross-hull drag-and-drop rewrite grew Update() and Draw() each large enough that
// neither the usual two-file (screen + pure Model) split nor a single file could hold both any
// more. Everything here draws; nothing here mutates the registry beyond the lazily-created
// EngineeringScreenState singleton's own read. modes/space/ui/EngineeringScreenInternal.h is the
// private (not part of this screen's public header) seam Update() and Draw() share -- layout math
// and hit-testing that is pure but Rectangle/Vector2-typed, which is why it could not live in
// EngineeringScreenModel.cpp (that file's own header comment: "no raylib").
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
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::engineering_screen {
namespace {

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

// A fixed "ENGINEERING BAY" title (the router's top bar already names the specific station under
// "DOCKED AT" -- Bay's/Storage's/Repair's/Research's own precedent) over a GRADE/CREDITS stat
// line. No integrity readout here any more -- ActiveGaugeStatus feeds that to the router's
// top-bar Gauge instead, the same call Repair's/Research's own headers already made.
void DrawHeader(Rectangle header, const sr::ui::Fonts& fonts, int grade,
                const std::optional<int>& credits) {
    DrawTextEx(fonts.heading, "ENGINEERING BAY", {header.x, header.y}, 24.0f, 1.0f,
               sr::ui::kValueBright);

    float x = header.x;
    const float y = header.y + 30.0f;
    auto DrawStat = [&](const std::string& label, const std::string& value) {
        DrawTextEx(fonts.body, label.c_str(), {x, y}, 14.0f, 1.0f, sr::ui::kLabelDim);
        x += MeasureTextEx(fonts.body, label.c_str(), 14.0f, 1.0f).x;
        DrawTextEx(fonts.body, value.c_str(), {x, y}, 14.0f, 1.0f, sr::ui::kValueBright);
        x += MeasureTextEx(fonts.body, value.c_str(), 14.0f, 1.0f).x;
    };
    DrawStat("GRADE ", std::to_string(grade));
    if (credits.has_value()) {
        DrawTextEx(fonts.body, " -- ", {x, y}, 14.0f, 1.0f, sr::ui::kLabelDim);
        x += MeasureTextEx(fonts.body, " -- ", 14.0f, 1.0f).x;
        DrawStat("CREDITS ", std::to_string(*credits) + " CR");
    }
}

// One "YOUR VESSEL"/"THIS STATION" toggle pair, `selectedStation` highlighting whichever is
// currently showing -- the rig- and cargo-subject selectors share this one drawer.
void DrawToggleButtons(const ToggleButtons& toggle, const sr::ui::Fonts& fonts,
                       bool selectedStation) {
    sr::ui::DrawChamferedButton(toggle.vessel, "YOUR VESSEL", fonts.body, 11.0f,
                                selectedStation ? sr::ui::kPanelGlass : sr::ui::kPanelChrome,
                                sr::ui::kPanelChrome,
                                selectedStation ? sr::ui::kLabelDim : sr::ui::kValueBright);
    sr::ui::DrawChamferedButton(toggle.station, "THIS STATION", fonts.body, 11.0f,
                                selectedStation ? sr::ui::kPanelChrome : sr::ui::kPanelGlass,
                                sr::ui::kPanelChrome,
                                selectedStation ? sr::ui::kValueBright : sr::ui::kLabelDim);
}

// The label row + divider shared by every bracket panel below: a title naming the panel and a
// right-aligned count, over a hairline rule -- mirrors Repair's/Research's own SectionLayout
// label treatment. Returns the interior rect (title row and divider excluded).
Rectangle DrawPanelHeader(Rectangle panel, const sr::ui::Fonts& fonts, const std::string& title,
                          const std::string& count) {
    sr::ui::DrawBracketPanel(panel, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);
    const Rectangle inner = sr::ui::PanelContentRect(panel);
    DrawTextEx(fonts.body, title.c_str(), {inner.x, inner.y}, 14.0f, 1.0f, sr::ui::kValueBright);
    const float countWidth = MeasureTextEx(fonts.body, count.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(fonts.body, count.c_str(), {inner.x + inner.width - countWidth, inner.y}, 14.0f,
               1.0f, sr::ui::kLabelDim);
    const float dividerY = inner.y + kPanelLabelHeight;
    DrawLineEx({inner.x, dividerY}, {inner.x + inner.width, dividerY}, 1.0f, sr::ui::kDivider);
    return inner;
}

// A bordered icon box (border colour dims once disabled, the glyph inside carries identity -- Row
// has none for a held module today, so the box draws empty) plus label/value text -- Bay's/
// Repair's own row treatment, replacing the generic sr::ui::ListView this screen drew before
// (issue #230's visual-chrome pass).
void DrawCargoRow(Rectangle bounds, const sr::ui::Fonts& fonts, const sr::ui::Row& row) {
    const Rectangle iconBox{bounds.x, bounds.y + (bounds.height - kIconBoxSize) / 2.0f,
                            kIconBoxSize, kIconBoxSize};
    const Color chrome = row.style.disabled ? sr::ui::kLabelDim : sr::ui::kPanelChrome;
    DrawRectangleLinesEx(iconBox, 1.0f, chrome);
    if (row.glyph[0] != '\0') {
        const Vector2 glyphSize = MeasureTextEx(fonts.heading, row.glyph, 14.0f, 1.0f);
        DrawTextEx(fonts.heading, row.glyph,
                   {iconBox.x + (iconBox.width - glyphSize.x) / 2.0f,
                    iconBox.y + (iconBox.height - glyphSize.y) / 2.0f},
                   14.0f, 1.0f, chrome);
    }

    const float textX = iconBox.x + iconBox.width + 14.0f;
    const Color labelColor = row.style.disabled ? sr::ui::kLabelDim : sr::ui::kValueBright;
    DrawTextEx(fonts.heading, row.label.c_str(), {textX, bounds.y + bounds.height / 2.0f - 8.0f},
               15.0f, 1.0f, labelColor);

    const float valueWidth = MeasureTextEx(fonts.body, row.value.c_str(), 13.0f, 1.0f).x;
    DrawTextEx(fonts.body, row.value.c_str(),
               {bounds.x + bounds.width - valueWidth, bounds.y + bounds.height / 2.0f - 6.0f},
               13.0f, 1.0f, sr::ui::kLabelDim);
}

// The CargoHold row list, top to bottom inside `bounds`, divider rules between rows -- mirrors
// Repair's/Research's own DrawRepairRows/DrawCandidateRows. `draggedModule` dims the one row
// currently picked up, the same "already being carried" treatment ModulesMenu's own hold list
// uses.
void DrawCargoRows(Rectangle bounds, const sr::ui::Fonts& fonts,
                   const std::vector<ModuleRow>& modules, const ModuleId& draggedModule) {
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                     static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    if (modules.empty()) {
        DrawTextEx(fonts.body, "CARGO HOLD EMPTY", {bounds.x, bounds.y}, 13.0f, 1.0f,
                   sr::ui::kLabelDim);
        EndScissorMode();
        return;
    }
    for (std::size_t i = 0; i < modules.size(); ++i) {
        const float y = bounds.y + static_cast<float>(i) * kRowHeight;
        if (y > bounds.y + bounds.height) {
            break;
        }
        if (i > 0) {
            DrawLineEx({bounds.x, y}, {bounds.x + bounds.width, y}, 1.0f, sr::ui::kDivider);
        }
        sr::ui::Row row = modules[i].row;
        // A plain click does nothing now -- only a drag Mounts, Unmounts, or transfers a module
        // (this file's own EndCargoDrag comment on why a bare click stopped Deconstructing) -- so
        // every row says the same thing regardless of which hull's hold this is.
        row.value = "DRAG TO MOVE";
        row.style.disabled = row.style.disabled || modules[i].module == draggedModule;
        DrawCargoRow({bounds.x, y, bounds.width, kRowHeight}, fonts, row);
    }
    EndScissorMode();
}

// A ring built from short arcs rather than a full circle -- the reference's dashed "open/
// destroyed" node treatment (features.md 3.10's degrade-never-remove: a missing or dead hardpoint
// is drawn, never omitted, but must not read as equally solid/healthy as a living one).
void DrawDashedCircle(Vector2 center, float radius, float thickness, Color color) {
    constexpr int kSegments = 16;  // 8 dashes, 8 gaps.
    constexpr float kTau = 6.28318530717958647692f;
    for (int i = 0; i < kSegments; i += 2) {
        const float a0 = (static_cast<float>(i) / kSegments) * kTau;
        const float a1 = (static_cast<float>(i + 1) / kSegments) * kTau;
        DrawLineEx({center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius},
                   {center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius}, thickness,
                   color);
    }
}

// One hardpoint node: a solid ring for a living hardpoint (colour carries condition -- dim once
// disabled, else IntegrityStatusColor, the same rule Repair's/Research's own rows use) or a
// dashed one for an authored-but-absent mount (green once Rebuild is actually clickable, dim
// otherwise) or a Destroyed one (red, always disabled) -- architecture.md 8.3's three states.
// `hoverValid` overrides the ring colour while a compatible drag hovers this exact node (nullopt
// when nothing is being dragged, or this is not the node under the cursor). The secondary line
// under the node shows what RefactorSystem would actually do (DELETE/REBUILD/UNMOUNT FIRST/
// DESTROYED) unless a module is mounted here, in which case it names that module instead --
// `moduleLabel` is empty otherwise.
void DrawSchematicNode(Rectangle canvas, const sr::ui::Fonts& fonts, Vec2 localOffset,
                       float schematicRadius, const MountRow& entry, const std::string& moduleLabel,
                       std::optional<bool> hoverValid, const SchematicView& view) {
    const Vector2 center = SchematicNodeCenter(localOffset, canvas, schematicRadius, view);
    const float nodeRadius = kNodeRadius * view.zoom;
    const bool missing = entry.hardpoint == entt::null;

    Color color;
    bool dashed;
    if (missing) {
        dashed = true;
        color = entry.row.style.disabled ? sr::ui::kLabelDim : sr::ui::kStatusGood;
    } else if (entry.destroyed) {
        dashed = true;
        color = sr::ui::kStatusCritical;
    } else {
        dashed = false;
        color = entry.row.style.disabled ? sr::ui::kLabelDim
                                         : IntegrityStatusColor(entry.row.style.integrity);
    }
    if (hoverValid.has_value()) {
        color = *hoverValid ? sr::ui::kStatusGood : sr::ui::kStatusCritical;
    }

    if (dashed) {
        DrawDashedCircle(center, nodeRadius, 2.0f, color);
    } else {
        DrawRing(center, nodeRadius - 2.5f, nodeRadius, 0.0f, 360.0f, 32, color);
    }

    // Text scales with zoom too, not just the ring -- a clustered rig's real problem is unreadable
    // labels, and a bigger ring with the same tiny text underneath would not fix that.
    const float glyphSize14 = 14.0f * view.zoom;
    const float labelSize12 = 12.0f * view.zoom;
    const float valueSize11 = 11.0f * view.zoom;

    if (entry.row.glyph[0] != '\0') {
        const Vector2 glyphSize = MeasureTextEx(fonts.heading, entry.row.glyph, glyphSize14, 1.0f);
        DrawTextEx(fonts.heading, entry.row.glyph,
                   {center.x - glyphSize.x * 0.5f, center.y - glyphSize.y * 0.5f}, glyphSize14,
                   1.0f, color);
    }

    const std::string label = FormattedMountLabel(entry.mount.str());
    const float labelWidth = MeasureTextEx(fonts.body, label.c_str(), labelSize12, 1.0f).x;
    DrawTextEx(fonts.body, label.c_str(),
               {center.x - labelWidth * 0.5f, center.y + nodeRadius + 4.0f}, labelSize12, 1.0f,
               sr::ui::kValueBright);

    const std::string& secondary = moduleLabel.empty() ? entry.row.value : moduleLabel;
    const float valueWidth = MeasureTextEx(fonts.body, secondary.c_str(), valueSize11, 1.0f).x;
    DrawTextEx(fonts.body, secondary.c_str(),
               {center.x - valueWidth * 0.5f, center.y + nodeRadius + 4.0f + labelSize12 + 2.0f},
               valueSize11, 1.0f, moduleLabel.empty() ? color : sr::ui::kValueBright);
}

// The reference's colour legend along the schematic's bottom edge -- without it, a fresh player
// has no way to learn that dashed means "not really there" versus solid meaning "installed." A
// right-aligned reminder of the camera controls rides the same row: nothing else on this panel
// hints that scrolling or right-dragging over the schematic does anything.
void DrawSchematicLegend(Rectangle bounds, const sr::ui::Fonts& fonts) {
    struct Entry {
        const char* label;
        Color color;
        bool dashed;
    };
    const Entry entries[] = {
        {"INSTALLED", sr::ui::kStatusGood, false},
        {"DAMAGED", sr::ui::kStatusCaution, false},
        {"OPEN -- REBUILD", sr::ui::kStatusGood, true},
        {"DESTROYED", sr::ui::kStatusCritical, true},
    };
    constexpr float kSwatchRadius = 5.0f;
    float x = bounds.x;
    const float y = bounds.y + bounds.height * 0.5f;
    for (const Entry& entry : entries) {
        if (entry.dashed) {
            DrawDashedCircle({x + kSwatchRadius, y}, kSwatchRadius, 1.5f, entry.color);
        } else {
            DrawCircleV({x + kSwatchRadius, y}, kSwatchRadius, entry.color);
        }
        x += kSwatchRadius * 2.0f + 6.0f;
        DrawTextEx(fonts.body, entry.label, {x, y - 6.0f}, 11.0f, 1.0f, sr::ui::kLabelDim);
        x += MeasureTextEx(fonts.body, entry.label, 11.0f, 1.0f).x + 18.0f;
    }

    const char* hint = "SCROLL: ZOOM -- RIGHT-DRAG: PAN";
    const float hintWidth = MeasureTextEx(fonts.body, hint, 11.0f, 1.0f).x;
    DrawTextEx(fonts.body, hint, {bounds.x + bounds.width - hintWidth, y - 6.0f}, 11.0f, 1.0f,
               sr::ui::kLabelDim);
}

// One subject's full SHIP RIG panel: bracket frame, title + mount count, the schematic node
// layout, and the state legend. `hoveredMount`/`hoverValid` highlight the node a live cargo-drag
// is over, if this is the panel it is over; both empty otherwise. `view` is the live zoom/pan
// camera (EngineeringScreenState::rigZoom/rigPanOffset) -- panning or zooming out can push a node
// past `canvas`'s own edges, so the node loop scissors to it the same way DrawCargoRows already
// does for an overflowing list.
void DrawShipRigPanel(Rectangle panel, Rectangle canvas, const sr::ui::Fonts& fonts,
                      const std::string& title, const RigBlueprint& blueprint,
                      const std::vector<MountRow>& rows, const entt::registry& registry,
                      const core::ContentLibrary& content, std::optional<int> hoveredMount,
                      bool hoverValid, const SchematicView& view) {
    const Rectangle inner =
        DrawPanelHeader(panel, fonts, title, std::to_string(rows.size()) + " MOUNTS");
    if (rows.empty()) {
        DrawTextEx(fonts.body, "NO MOUNTS", {canvas.x, canvas.y}, 13.0f, 1.0f, sr::ui::kLabelDim);
        return;
    }

    const float schematicRadius = SchematicRadius(blueprint);
    const std::size_t count = std::min(rows.size(), blueprint.mounts.size());
    BeginScissorMode(static_cast<int>(canvas.x), static_cast<int>(canvas.y),
                     static_cast<int>(canvas.width), static_cast<int>(canvas.height));
    for (std::size_t i = 0; i < count; ++i) {
        const std::optional<bool> hover =
            hoveredMount.has_value() && *hoveredMount == static_cast<int>(i)
                ? std::optional<bool>(hoverValid)
                : std::nullopt;
        const std::string moduleLabel = MountedModuleLabel(registry, content, rows[i].hardpoint);
        DrawSchematicNode(canvas, fonts, blueprint.mounts[i].localOffset, schematicRadius, rows[i],
                          moduleLabel, hover, view);
    }
    EndScissorMode();

    const Rectangle legend{inner.x, inner.y + inner.height - kLegendHeight, inner.width,
                           kLegendHeight};
    DrawSchematicLegend(legend, fonts);
}

// The module being carried, following the cursor -- the one genuinely new draw call a drag adds
// (mirrors ModulesMenu.cpp's own DrawGhost). `label` is the module id for a cargo-sourced drag, or
// the mounted module's display name for one picked up off a hardpoint.
void DrawGhost(const sr::ui::Fonts& fonts, const std::string& label, Vector2 cursor) {
    constexpr float kOffset = 14.0f;
    const Vector2 textSize = MeasureTextEx(fonts.heading, label.c_str(), 13.0f, 1.0f);
    const Rectangle bounds{cursor.x + kOffset, cursor.y + kOffset, textSize.x + 20.0f, 28.0f};
    DrawRectangleRec(bounds, sr::ui::kPanelGlass);
    DrawRectangleLinesEx(bounds, 1.5f, sr::ui::kPanelChrome);
    DrawTextEx(fonts.heading, label.c_str(), {bounds.x + 10.0f, bounds.y + 7.0f}, 13.0f, 1.0f,
               sr::ui::kValueBright);
}

void DrawHeaderSection(const entt::registry& registry, entt::entity facility,
                       entt::entity requester, const Layout& layout, const sr::ui::Fonts& fonts,
                       const EngineeringScreenState& state, bool showStationSection) {
    const FacilityRef& facilityRef = registry.get<FacilityRef>(facility);
    std::optional<int> credits;
    if (const Wallet* wallet = registry.try_get<Wallet>(requester)) {
        credits = wallet->credits;
    }
    DrawHeader(layout.header, fonts, facilityRef.grade, credits);
    if (showStationSection) {
        DrawToggleButtons(layout.rigToggle, fonts, state.rigShowsStation);
    }
}

void DrawSiblingSection(const Layout& layout, const sr::ui::Fonts& fonts,
                        const std::vector<entt::entity>& siblings, entt::entity facility,
                        bool show) {
    if (!show) {
        return;
    }
    std::vector<std::string> labels;
    labels.reserve(siblings.size());
    int selected = -1;
    for (std::size_t i = 0; i < siblings.size(); ++i) {
        labels.push_back("BENCH " + std::to_string(i + 1));
        if (siblings[i] == facility) {
            selected = static_cast<int>(i);
        }
    }
    sr::ui::DrawTabStrip(layout.siblingStrip, labels, selected, fonts.body);
}

void DrawCargoSection(const entt::registry& registry, const Layout& layout,
                      const sr::ui::Fonts& fonts, const Subject& cargoSubject,
                      bool showStationSection, const EngineeringScreenState& state) {
    const std::vector<ModuleRow> modules = ModuleRows(registry, cargoSubject.rigRoot);
    DrawPanelHeader(layout.cargoPanel, fonts, "CARGO HOLD",
                    std::to_string(modules.size()) + " STACKS");
    if (showStationSection) {
        DrawToggleButtons(layout.cargoToggle, fonts, state.cargoShowsStation);
    }
    DrawCargoRows(layout.cargoList, fonts, modules, state.draggedModule);
}

// Which node (if any) a live cargo-drag is hovering inside the rig canvas, and whether dropping
// there would actually Mount -- empty/false while nothing is being dragged, or the cursor is
// outside the canvas. Mirrors ModulesMenu.cpp's own ComputeHover/HoverState.
struct HoverState {
    std::optional<int> mountIndex;
    bool mountValid = false;
};

HoverState ComputeHover(const entt::registry& registry, const core::ContentLibrary& content,
                        const Subject& rigSubject, const Layout& layout,
                        const EngineeringScreenState& state, Vector2 cursor) {
    HoverState hover;
    if (state.draggedModule.empty() || !CheckCollisionPointRec(cursor, layout.rigCanvas)) {
        return hover;
    }
    hover.mountIndex = SchematicNodeAt(layout.rigCanvas, *rigSubject.blueprint, cursor,
                                       {state.rigZoom, state.rigPanOffset});
    if (!hover.mountIndex.has_value()) {
        return hover;
    }
    const std::vector<MountRow> mounts =
        MountRows(registry, rigSubject.rigRoot, *rigSubject.blueprint);
    if (*hover.mountIndex < static_cast<int>(mounts.size())) {
        hover.mountValid = CanMountHere(
            registry, content, rigSubject.rigRoot,
            mounts[static_cast<std::size_t>(*hover.mountIndex)].hardpoint, state.draggedModule);
    }
    return hover;
}

void DrawRigSection(const entt::registry& registry, const core::ContentLibrary& content,
                    const Layout& layout, const sr::ui::Fonts& fonts, const Subject& rigSubject,
                    const HoverState& hover, const EngineeringScreenState& state) {
    std::string rigTitle = rigSubject.isStation ? "STATION" : "VESSEL";
    if (const DisplayName* name = registry.try_get<DisplayName>(rigSubject.rigRoot)) {
        rigTitle = name->value;
    }
    rigTitle += rigSubject.isStation ? " -- THIS STATION" : " -- YOUR VESSEL";
    DrawShipRigPanel(layout.rigPanel, layout.rigCanvas, fonts, rigTitle, *rigSubject.blueprint,
                     MountRows(registry, rigSubject.rigRoot, *rigSubject.blueprint), registry,
                     content, hover.mountIndex, hover.mountValid,
                     {state.rigZoom, state.rigPanOffset});
}

void DrawDragGhost(const entt::registry& registry, const core::ContentLibrary& content,
                   const sr::ui::Fonts& fonts, const EngineeringScreenState& state,
                   Vector2 cursor) {
    std::string label = state.draggedModule.str();
    if (state.draggedFromMount != entt::null) {
        label = MountedModuleLabel(registry, content, state.draggedFromMount);
    } else if (const ModuleDef* module = content.FindModule(state.draggedModule)) {
        label = module->displayName;
    }
    DrawGhost(fonts, label, cursor);
}

}  // namespace

void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::ContentLibrary& content, const sr::ui::Fonts& fonts) {
    const ResolvedContext ctx = Resolve(registry, playerFaction, content);
    if (ctx.requester == entt::null || ctx.blueprint == nullptr) {
        return;
    }

    const std::vector<Subject> subjects = Subjects(registry, ctx, playerFaction, content);
    const bool showStationSection = subjects.size() > 1;
    const std::vector<entt::entity> siblings = SiblingBenches(registry, ctx.station);
    const bool showSiblingStrip = siblings.size() > 1;
    const Layout layout =
        ComputeLayout(bridge_view::FrameContentRect(), showSiblingStrip, showStationSection);

    const entt::entity stateEntity = FindStateSingleton(registry);
    const EngineeringScreenState state = stateEntity != entt::null
                                             ? registry.get<EngineeringScreenState>(stateEntity)
                                             : EngineeringScreenState{};

    DrawHeaderSection(registry, ctx.facility, ctx.requester, layout, fonts, state,
                      showStationSection);
    DrawSiblingSection(layout, fonts, siblings, ctx.facility, showSiblingStrip);

    const Subject& cargoSubject = SelectedSubject(subjects, state.cargoShowsStation);
    const Subject& rigSubject = SelectedSubject(subjects, state.rigShowsStation);
    DrawCargoSection(registry, layout, fonts, cargoSubject, showStationSection, state);

    const Vector2 cursor = GetMousePosition();
    const HoverState hover = ComputeHover(registry, content, rigSubject, layout, state, cursor);
    DrawRigSection(registry, content, layout, fonts, rigSubject, hover, state);

    if (!state.draggedModule.empty() || state.draggedFromMount != entt::null) {
        DrawDragGhost(registry, content, fonts, state, cursor);
    }
}

}  // namespace sr::space::ui::engineering_screen
