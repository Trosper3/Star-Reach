#include "modes/space/ui/ModulesMenu.h"

#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>

#include "shared/components/FlightOverlay.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/rig/ModuleAttachment.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/Widgets.h"

// The pure, non-raylib half of this screen -- EquippableMounts/EquippedMounts/DestroyedMounts/
// BuildMountRequest/BuildUnmountRequest/CanMount/IsOpen/IsEmpty -- lives in
// ModulesMenuModel.cpp, split out to satisfy architecture.md 2.2's 600-line file cap (see that
// file's own header comment). This file keeps Update()'s input polling and Draw()'s layout math,
// which stay with the widgets they position.
namespace sr::space::ui::modules_menu {
namespace {

// KEY_L: mnemonic for Loadout, and free -- see StorageMenu.cpp's identical note on KEY_I; the
// same "features.md 3.6 is still 📋 and reserves neither" reasoning applies here.
constexpr int kToggleKey = KEY_L;

constexpr float kPanelWidth = 620.0f;
constexpr float kPanelTop = 80.0f;
constexpr float kHeaderHeight = 56.0f;
constexpr float kColumnLabelHeight = 22.0f;
constexpr float kIconBoxSize = 26.0f;
constexpr float kRowHeight = 44.0f;
constexpr float kVisibleRows = 5.0f;
constexpr float kListHeight = kRowHeight * kVisibleRows;
constexpr float kColumnGap = 18.0f;
constexpr float kGhostOffset = 14.0f;

// features.md 3.9's status triad, the same three-stop thresholds Bay's/Storage's/Repair's own
// IntegrityStatusColor use -- duplicated locally per screen file on purpose, the established
// precedent those files already set.
Color IntegrityStatusColor(float fraction) {
    return fraction > 0.5f   ? sr::ui::kStatusGood
           : fraction > 0.2f ? sr::ui::kStatusCaution
                             : sr::ui::kStatusCritical;
}

// One-letter monogram per ShellKind (features.md 3.9's glyph-carries-identity rule) -- the same
// placeholder RepairScreen.cpp's own ShellGlyph uses; duplicated locally rather than shared,
// since neither file may depend on the other and this is the whole of it.
void ShellGlyph(ShellKind kind, char (&out)[3]) {
    switch (kind) {
        case ShellKind::Chassis: out[0] = 'C'; break;
        case ShellKind::Armor: out[0] = 'A'; break;
        case ShellKind::PowerCell: out[0] = 'P'; break;
        case ShellKind::Engine: out[0] = 'E'; break;
        case ShellKind::Weapon: out[0] = 'W'; break;
        case ShellKind::Shield: out[0] = 'S'; break;
        case ShellKind::Facility: out[0] = 'F'; break;
    }
    out[1] = '\0';
    out[2] = '\0';
}

// One-letter monogram per ModuleKind -- the hold list's own identity glyph, since a held module
// has no ShellRole of its own to read one from.
void ModuleGlyph(ModuleKind kind, char (&out)[3]) {
    switch (kind) {
        case ModuleKind::Weapon: out[0] = 'W'; break;
        case ModuleKind::ShieldGenerator: out[0] = 'S'; break;
        case ModuleKind::PowerCell: out[0] = 'P'; break;
        case ModuleKind::Engine: out[0] = 'E'; break;
        case ModuleKind::Armor: out[0] = 'A'; break;
        case ModuleKind::Facility: out[0] = 'F'; break;
        case ModuleKind::Sensor: out[0] = 'R'; break;
        case ModuleKind::CargoBay: out[0] = 'C'; break;
        case ModuleKind::FireControl: out[0] = 'X'; break;
        case ModuleKind::Hyperdrive: out[0] = 'H'; break;
        case ModuleKind::Crew: out[0] = 'K'; break;
    }
    out[1] = '\0';
    out[2] = '\0';
}

// Duplicated locally per screen file (architecture.md 12.30's established rule) -- see
// StorageMenu.cpp's identical helper for the reasoning. ModulesMenuModel.cpp keeps its own copy
// too (IsOpen needs it there, and that file must stay independently buildable).
entt::entity EnsureSingleton(entt::registry& registry) {
    for (auto [entity] : registry.view<FlightOverlayStateSingleton>().each()) {
        return entity;
    }
    const entt::entity singleton = registry.create();
    registry.emplace<FlightOverlayStateSingleton>(singleton);
    registry.emplace<FlightOverlayState>(singleton);
    return singleton;
}

entt::entity FindSingleton(const entt::registry& registry) {
    for (auto [entity] : registry.view<FlightOverlayStateSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

struct Layout {
    Rectangle header{};
    Rectangle holdLabel{};
    Rectangle holdList{};
    Rectangle mountLabel{};
    Rectangle mountList{};
};

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth,
                     kHeaderHeight + kColumnLabelHeight + kListHeight};
}

Layout ComputeLayout() {
    const Rectangle content = sr::ui::PanelContentRect(PanelBounds());
    const float columnWidth = (content.width - kColumnGap) * 0.5f;
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    const float columnY = content.y + kHeaderHeight;
    layout.holdLabel = {content.x, columnY, columnWidth, kColumnLabelHeight};
    layout.mountLabel = {content.x + columnWidth + kColumnGap, columnY, columnWidth,
                         kColumnLabelHeight};
    const float listY = columnY + kColumnLabelHeight;
    layout.holdList = {content.x, listY, columnWidth, kListHeight};
    layout.mountList = {content.x + columnWidth + kColumnGap, listY, columnWidth, kListHeight};
    return layout;
}

// Pure -- the icon-box row index under `cursor`, using kRowHeight and `scrollOffset` (pixels
// scrolled down, from FlightOverlayState::holdScroll/mountScroll).
std::optional<int> ModulesRowAt(Rectangle bounds, int rowCount, float scrollOffset,
                                Vector2 cursor) {
    if (rowCount <= 0 || !CheckCollisionPointRec(cursor, bounds)) {
        return std::nullopt;
    }
    const int index = static_cast<int>((cursor.y - bounds.y + scrollOffset) / kRowHeight);
    if (index < 0 || index >= rowCount) {
        return std::nullopt;
    }
    return index;
}

// Pure -- clamps a scroll offset into [0, contentHeight - listHeight], zero (nothing to scroll)
// once every row already fits within `listHeight`. Re-run every frame in Update(), not just on
// a wheel tick, so a row list shrinking out from under a stale offset (a hardpoint dying, a
// module being consumed) cannot leave the view scrolled past its own last row.
float ClampScroll(int rowCount, float listHeight, float offset) {
    const float contentHeight = static_cast<float>(std::max(rowCount, 0)) * kRowHeight;
    const float maxScroll = std::max(0.0f, contentHeight - listHeight);
    return std::clamp(offset, 0.0f, maxScroll);
}

// Mouse-wheel-over-list scrolls that list, then both offsets are re-clamped regardless (see
// ClampScroll's own comment on why every frame, not just on a wheel tick).
void ApplyScroll(FlightOverlayState& state, const Layout& layout, int heldCount, int mountCount,
                 Vector2 cursor) {
    const float wheelDelta = GetMouseWheelMove() * kRowHeight;
    if (wheelDelta != 0.0f && CheckCollisionPointRec(cursor, layout.holdList)) {
        state.holdScroll -= wheelDelta;
    } else if (wheelDelta != 0.0f && CheckCollisionPointRec(cursor, layout.mountList)) {
        state.mountScroll -= wheelDelta;
    }
    state.holdScroll = ClampScroll(heldCount, layout.holdList.height, state.holdScroll);
    state.mountScroll = ClampScroll(mountCount, layout.mountList.height, state.mountScroll);
}

}  // namespace

void Update(entt::registry& registry, entt::entity rigRoot, const core::ContentLibrary& content) {
    if (IsKeyPressed(kToggleKey)) {
        FlightOverlayState& state = registry.get<FlightOverlayState>(EnsureSingleton(registry));
        state.loadoutOpen = !state.loadoutOpen;
    }

    if (!IsOpen(registry) || rigRoot == entt::null) {
        return;
    }

    const Layout layout = ComputeLayout();
    FlightOverlayState& state = registry.get<FlightOverlayState>(EnsureSingleton(registry));
    const std::vector<ModuleId> held = HeldModules(registry, rigRoot, content);
    const std::vector<entt::entity> mounts = OrderedMounts(registry, rigRoot);
    const Vector2 cursor = GetMousePosition();
    ApplyScroll(state, layout, static_cast<int>(held.size()), static_cast<int>(mounts.size()),
                cursor);

    const bool dragging = !state.draggedModule.empty() || state.draggedFromMount != entt::null;

    if (!dragging) {
        if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            return;
        }

        const std::optional<int> holdHit =
            ModulesRowAt(layout.holdList, static_cast<int>(held.size()), state.holdScroll, cursor);
        if (holdHit.has_value()) {
            state.draggedModule = held[static_cast<std::size_t>(*holdHit)];
            return;
        }

        const std::optional<int> mountHit = ModulesRowAt(
            layout.mountList, static_cast<int>(mounts.size()), state.mountScroll, cursor);
        if (mountHit.has_value()) {
            const entt::entity mount = mounts[static_cast<std::size_t>(*mountHit)];
            // Only an occupied, living mount has a module to pick up -- an empty or Destroyed
            // row starts no drag, the same as a mis-click refused today.
            if (!registry.all_of<Destroyed>(mount) && !IsEmpty(registry, mount)) {
                state.draggedFromMount = mount;
            }
        }
        return;
    }

    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        return;  // Drag still live -- Draw() shows the ghost; nothing else to do this frame.
    }

    if (!state.draggedModule.empty()) {
        const std::optional<int> mountHit = ModulesRowAt(
            layout.mountList, static_cast<int>(mounts.size()), state.mountScroll, cursor);
        if (mountHit.has_value()) {
            const entt::entity mount = mounts[static_cast<std::size_t>(*mountHit)];
            if (CanMount(registry, content, rigRoot, mount, state.draggedModule)) {
                registry.emplace_or_replace<MountModuleRequest>(
                    rigRoot, BuildMountRequest(state.draggedModule, mount));
            }
        }
        // Anywhere else -- an incompatible or occupied mount, empty space, back over the hold
        // list itself -- cancels: no request emitted, exactly as a mis-click refuses today.
    } else if (state.draggedFromMount != entt::null) {
        if (CheckCollisionPointRec(cursor, layout.holdList)) {
            registry.emplace_or_replace<UnmountModuleRequest>(
                rigRoot, BuildUnmountRequest(state.draggedFromMount));
        }
    }

    state.draggedModule = ModuleId();
    state.draggedFromMount = entt::null;
}

namespace {

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

// Which mount row (if any) a live held-module drag is hovering, and whether it would commit; or
// whether a live mount drag is hovering the hold list at all. nullopt/false fields when nothing
// is being dragged (Draw only calls this while `dragging` is true).
struct HoverState {
    std::optional<int> mountIndex;
    bool mountValid = false;
    std::optional<entt::entity> mount;
    bool overHoldList = false;
};

HoverState ComputeHover(const entt::registry& registry, const core::ContentLibrary& content,
                        entt::entity rigRoot, const Layout& layout,
                        const std::vector<entt::entity>& mounts, const FlightOverlayState& state,
                        Vector2 cursor) {
    HoverState hover;
    if (!state.draggedModule.empty()) {
        hover.mountIndex = ModulesRowAt(layout.mountList, static_cast<int>(mounts.size()),
                                        state.mountScroll, cursor);
        if (hover.mountIndex.has_value()) {
            const entt::entity mount = mounts[static_cast<std::size_t>(*hover.mountIndex)];
            hover.mount = mount;
            hover.mountValid = CanMount(registry, content, rigRoot, mount, state.draggedModule);
        }
    } else if (state.draggedFromMount != entt::null) {
        hover.overHoldList = CheckCollisionPointRec(cursor, layout.holdList);
    }
    return hover;
}

// Rig name, aggregate structural integrity, and mass/power -- live, or previewed against the
// current drag's hover target (architecture.md 12.30.7: "both live against the pending swap").
void DrawHeader(Rectangle header, const sr::ui::Fonts& fonts, const std::string& rigName,
                float integrity, const RigTotals& current,
                const std::optional<RigTotals>& pending) {
    DrawTextEx(fonts.heading, ("LOADOUT -- " + rigName).c_str(), {header.x, header.y}, 20.0f, 1.0f,
               sr::ui::kValueBright);

    float x = header.x;
    const float y = header.y + 28.0f;
    auto DrawStat = [&](const std::string& label, const std::string& value, Color valueColor) {
        DrawTextEx(fonts.body, label.c_str(), {x, y}, 13.0f, 1.0f, sr::ui::kLabelDim);
        x += MeasureTextEx(fonts.body, label.c_str(), 13.0f, 1.0f).x;
        DrawTextEx(fonts.body, value.c_str(), {x, y}, 13.0f, 1.0f, valueColor);
        x += MeasureTextEx(fonts.body, value.c_str(), 13.0f, 1.0f).x;
    };
    auto DrawSeparator = [&]() {
        DrawTextEx(fonts.body, " -- ", {x, y}, 13.0f, 1.0f, sr::ui::kLabelDim);
        x += MeasureTextEx(fonts.body, " -- ", 13.0f, 1.0f).x;
    };

    DrawStat("HULL ", std::to_string(static_cast<int>(integrity * 100.0f)) + "%",
             IntegrityStatusColor(integrity));
    DrawSeparator();

    char massBuf[48];
    if (pending.has_value()) {
        std::snprintf(massBuf, sizeof(massBuf), "%d -> %d KG", static_cast<int>(current.mass),
                      static_cast<int>(pending->mass));
    } else {
        std::snprintf(massBuf, sizeof(massBuf), "%d KG", static_cast<int>(current.mass));
    }
    DrawStat("MASS ", massBuf, sr::ui::kValueBright);
    DrawSeparator();

    const RigTotals& power = pending.value_or(current);
    const bool overdrawn = power.powerDraw > power.powerGeneration;
    char powerBuf[64];
    if (pending.has_value()) {
        std::snprintf(powerBuf, sizeof(powerBuf), "%d -> %d / %d",
                      static_cast<int>(current.powerDraw), static_cast<int>(pending->powerDraw),
                      static_cast<int>(pending->powerGeneration));
    } else {
        std::snprintf(powerBuf, sizeof(powerBuf), "%d / %d", static_cast<int>(power.powerDraw),
                      static_cast<int>(power.powerGeneration));
    }
    DrawStat("PWR ", powerBuf, overdrawn ? sr::ui::kStatusCritical : sr::ui::kValueBright);
}

void DrawColumnLabel(Rectangle bounds, const sr::ui::Fonts& fonts, const std::string& title) {
    DrawTextEx(fonts.body, title.c_str(), {bounds.x, bounds.y}, 14.0f, 1.0f, sr::ui::kValueBright);
    const float dividerY = bounds.y + bounds.height;
    DrawLineEx({bounds.x, dividerY}, {bounds.x + bounds.width, dividerY}, 1.0f, sr::ui::kDivider);
}

// A bordered icon box (border colour carries condition/hover state, the glyph inside carries
// identity) plus a two-line label/value card -- Bay's/Repair's own row treatment, generalised to
// this overlay's narrower columns, rather than the generic sr::ui::ListView's flat monogram-
// prefixed text line.
void DrawModuleRow(Rectangle bounds, const sr::ui::Fonts& fonts, const sr::ui::Row& row,
                   Color chrome, float borderThickness) {
    const Rectangle iconBox{bounds.x, bounds.y + (bounds.height - kIconBoxSize) / 2.0f,
                            kIconBoxSize, kIconBoxSize};
    DrawRectangleLinesEx(iconBox, borderThickness, chrome);
    if (row.glyph[0] != '\0') {
        const Vector2 glyphSize = MeasureTextEx(fonts.heading, row.glyph, 13.0f, 1.0f);
        DrawTextEx(fonts.heading, row.glyph,
                   {iconBox.x + (iconBox.width - glyphSize.x) / 2.0f,
                    iconBox.y + (iconBox.height - glyphSize.y) / 2.0f},
                   13.0f, 1.0f, chrome);
    }

    const float textX = iconBox.x + iconBox.width + 10.0f;
    const Color labelColor = row.style.disabled ? sr::ui::kLabelDim : sr::ui::kValueBright;
    DrawTextEx(fonts.heading, row.label.c_str(), {textX, bounds.y + 4.0f}, 13.0f, 1.0f, labelColor);
    if (!row.value.empty()) {
        DrawTextEx(fonts.body, row.value.c_str(), {textX, bounds.y + bounds.height - 17.0f}, 11.0f,
                   1.0f, row.style.disabled ? sr::ui::kStatusCritical : sr::ui::kLabelDim);
    }
}

void DrawRowList(Rectangle bounds, const sr::ui::Fonts& fonts, const std::vector<sr::ui::Row>& rows,
                 const std::string& emptyMessage, float scrollOffset, int highlightIndex,
                 bool highlightValid) {
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                     static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    if (rows.empty()) {
        DrawTextEx(fonts.body, emptyMessage.c_str(), {bounds.x, bounds.y}, 13.0f, 1.0f,
                   sr::ui::kLabelDim);
        EndScissorMode();
        return;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float y = bounds.y + static_cast<float>(i) * kRowHeight - scrollOffset;
        if (y + kRowHeight < bounds.y || y > bounds.y + bounds.height) {
            continue;  // Scrolled out of view -- scissor alone would clip the draw, not skip it.
        }
        if (i > 0) {
            DrawLineEx({bounds.x, y}, {bounds.x + bounds.width, y}, 1.0f, sr::ui::kDivider);
        }
        const sr::ui::Row& row = rows[i];
        const bool isHighlighted = static_cast<int>(i) == highlightIndex;
        const Color chrome = isHighlighted
                                 ? (highlightValid ? sr::ui::kStatusGood : sr::ui::kStatusCritical)
                             : row.style.disabled ? sr::ui::kLabelDim
                                                  : IntegrityStatusColor(row.style.integrity);
        DrawModuleRow({bounds.x, y, bounds.width, kRowHeight}, fonts, row, chrome,
                      isHighlighted ? 2.0f : (row.style.disabled ? 1.0f : 1.5f));
    }
    EndScissorMode();
}

// A thin chrome-coloured thumb along the list's right edge once its rows outgrow `bounds` --
// the only signal (besides the wheel itself) that more rows exist below/above the fold. A no-op
// when everything already fits.
void DrawScrollbar(Rectangle bounds, int rowCount, float scrollOffset) {
    const float contentHeight = static_cast<float>(rowCount) * kRowHeight;
    const float maxScroll = contentHeight - bounds.height;
    if (maxScroll <= 0.0f) {
        return;
    }
    constexpr float kThumbWidth = 3.0f;
    const Rectangle track{bounds.x + bounds.width - kThumbWidth, bounds.y, kThumbWidth,
                          bounds.height};
    const float thumbHeight = std::max(16.0f, bounds.height * (bounds.height / contentHeight));
    const float thumbY = track.y + (bounds.height - thumbHeight) * (scrollOffset / maxScroll);
    DrawRectangleRec(track, sr::ui::kPanelGlass);
    DrawRectangleRec({track.x, thumbY, track.width, thumbHeight}, sr::ui::kPanelChrome);
}

// A Row-shaped label following the cursor while a drag is live, styled exactly like the row it
// was picked up from -- the one genuinely new draw call architecture.md 12.30.7 adds, since the
// module being carried has to stay visible mid-drag.
void DrawGhost(const sr::ui::Fonts& fonts, const sr::ui::Row& row, Vector2 cursor) {
    const Vector2 textSize = MeasureTextEx(fonts.heading, row.label.c_str(), 13.0f, 1.0f);
    const float width = kIconBoxSize + 10.0f + textSize.x + 12.0f;
    const Rectangle bounds{cursor.x + kGhostOffset, cursor.y + kGhostOffset, width, kRowHeight};
    DrawRectangleRec(bounds, sr::ui::kPanelGlass);
    DrawRectangleLinesEx(bounds, 1.5f, sr::ui::kPanelChrome);
    DrawModuleRow(bounds, fonts, row, sr::ui::kPanelChrome, 1.5f);
}

std::vector<sr::ui::Row> BuildHoldRows(const core::ContentLibrary& content,
                                       const std::vector<ModuleId>& held,
                                       const ModuleId& draggedModule) {
    std::vector<sr::ui::Row> rows;
    for (const ModuleId& id : held) {
        sr::ui::Row row;
        if (const ModuleDef* module = content.FindModule(id)) {
            ModuleGlyph(module->kind, row.glyph);
            row.label = module->displayName;
            row.value = std::to_string(static_cast<int>(module->mass)) + " KG";
        } else {
            row.label = id.str();
        }
        row.style.disabled = id == draggedModule;  // Already being carried.
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<sr::ui::Row> BuildMountRows(const entt::registry& registry,
                                        const core::ContentLibrary& content,
                                        const std::vector<entt::entity>& mounts,
                                        entt::entity draggedFromMount) {
    std::vector<sr::ui::Row> rows;
    for (const entt::entity mount : mounts) {
        sr::ui::Row row;
        if (const MountRef* ref = registry.try_get<MountRef>(mount)) {
            row.label = ref->id.str();
        }
        if (const ShellRole* role = registry.try_get<ShellRole>(mount)) {
            ShellGlyph(role->kind, row.glyph);
        }
        if (const Health* health = registry.try_get<Health>(mount)) {
            row.style.integrity = health->max > 0.0f ? health->current / health->max : 0.0f;
        }

        if (registry.all_of<Destroyed>(mount)) {
            row.value = "DESTROYED -- REBUILD AT ENGINEERING";
            row.style.disabled = true;
        } else if (IsEmpty(registry, mount)) {
            row.value = "EMPTY";
        } else {
            const MountedModules& mounted = registry.get<MountedModules>(mount);
            const ModuleId& moduleId = mounted.ids.back();
            const ModuleDef* module = content.FindModule(moduleId);
            row.value = module != nullptr ? module->displayName : moduleId.str();
        }
        if (mount == draggedFromMount) {
            row.style.disabled = true;  // Already being carried.
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

// The carried module or hardpoint's own glyph/label, for the ghost following the cursor.
sr::ui::Row BuildGhostRow(const entt::registry& registry, const core::ContentLibrary& content,
                          const FlightOverlayState& state) {
    sr::ui::Row ghost;
    if (!state.draggedModule.empty()) {
        if (const ModuleDef* module = content.FindModule(state.draggedModule)) {
            ModuleGlyph(module->kind, ghost.glyph);
            ghost.label = module->displayName;
        } else {
            ghost.label = state.draggedModule.str();
        }
        return ghost;
    }
    if (const ShellRole* role = registry.try_get<ShellRole>(state.draggedFromMount)) {
        ShellGlyph(role->kind, ghost.glyph);
    }
    if (const MountedModules* mounted = registry.try_get<MountedModules>(state.draggedFromMount);
        mounted != nullptr && !mounted->ids.empty()) {
        const ModuleDef* module = content.FindModule(mounted->ids.back());
        ghost.label = module != nullptr ? module->displayName : mounted->ids.back().str();
    }
    return ghost;
}

}  // namespace

void Draw(const entt::registry& registry, entt::entity rigRoot, const core::ContentLibrary& content,
          const sr::ui::Fonts& fonts) {
    if (!IsOpen(registry) || rigRoot == entt::null) {
        return;
    }

    const Layout layout = ComputeLayout();
    sr::ui::DrawPanelFrame(PanelBounds());

    const entt::entity singleton = FindSingleton(registry);
    const FlightOverlayState state = singleton != entt::null
                                         ? registry.get<FlightOverlayState>(singleton)
                                         : FlightOverlayState{};

    const std::vector<ModuleId> held = HeldModules(registry, rigRoot, content);
    const std::vector<entt::entity> mounts = OrderedMounts(registry, rigRoot);
    const Vector2 cursor = GetMousePosition();
    const bool dragging = !state.draggedModule.empty() || state.draggedFromMount != entt::null;
    const HoverState hover =
        dragging ? ComputeHover(registry, content, rigRoot, layout, mounts, state, cursor)
                 : HoverState{};

    const float integrity = rig_attachment::AggregateStructuralIntegrity(registry, rigRoot);
    const RigTotals current = CurrentTotals(registry, rigRoot);
    const std::optional<RigTotals> pending =
        PendingTotals(registry, content, rigRoot, state, hover.mount, hover.overHoldList);
    std::string rigName = "VESSEL";
    if (const DisplayName* name = registry.try_get<DisplayName>(rigRoot)) {
        rigName = name->value;
    }
    DrawHeader(layout.header, fonts, rigName, integrity, current, pending);

    DrawColumnLabel(layout.holdLabel, fonts, "HOLD");
    DrawColumnLabel(layout.mountLabel, fonts, "MOUNTS");

    DrawRowList(layout.holdList, fonts, BuildHoldRows(content, held, state.draggedModule),
                "HOLD EMPTY", state.holdScroll, -1, false);
    DrawScrollbar(layout.holdList, static_cast<int>(held.size()), state.holdScroll);
    if (dragging && state.draggedFromMount != entt::null) {
        DrawRectangleLinesEx(layout.holdList, 2.0f,
                             hover.overHoldList ? sr::ui::kStatusGood : sr::ui::kPanelChrome);
    }

    DrawRowList(
        layout.mountList, fonts, BuildMountRows(registry, content, mounts, state.draggedFromMount),
        "NO MOUNTS AVAILABLE", state.mountScroll, hover.mountIndex.value_or(-1), hover.mountValid);
    DrawScrollbar(layout.mountList, static_cast<int>(mounts.size()), state.mountScroll);

    if (dragging) {
        DrawGhost(fonts, BuildGhostRow(registry, content, state), cursor);
    }
}

}  // namespace sr::space::ui::modules_menu
