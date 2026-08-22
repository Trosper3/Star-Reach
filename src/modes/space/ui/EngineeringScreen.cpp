#include "modes/space/ui/EngineeringScreen.h"

#include <raylib.h>

#include <algorithm>
#include <optional>
#include <string>

#include "core/registries/ContentLibrary.h"
#include "shared/components/Docking.h"
#include "shared/components/Engineer.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Refactor.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::engineering_screen {
namespace {

constexpr float kPanelWidth = 640.0f;
constexpr float kPanelTop = 132.0f;
constexpr float kHeaderHeight = 44.0f;
constexpr float kSiblingStripHeight = 28.0f;
constexpr float kListHeight = 220.0f;
constexpr float kColumnGap = 16.0f;

// One-letter monogram per ShellKind (features.md 3.9's glyph-carries-identity rule) -- the same
// placeholder every other docked screen's own row list uses; duplicated locally rather than
// shared, since none of them may depend on each other and this is the whole of it.
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

entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

// The living Engineering-kind facility hardpoint PlayerLocation currently names, or entt::null --
// the same "this screen is active exactly while standing on its own gate hardpoint" pattern
// modes/space/ui/BayView.h's CurrentBay establishes.
entt::entity CurrentFacility(const entt::registry& registry, entt::entity shell) {
    if (shell == entt::null || registry.all_of<Destroyed>(shell)) {
        return entt::null;
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(shell);
    if (facility == nullptr || facility->kind != FacilityKind::Engineering) {
        return entt::null;
    }
    return shell;
}

// Every living Engineering-kind hardpoint on `station`, in Rig::children order -- every sibling
// bench, unlike DockedFacility (shared/rig/DockedFacility.h) which resolves only the one
// PlayerLocation currently names. Mirrors BayView.h's own SiblingBays.
std::vector<entt::entity> SiblingBenches(const entt::registry& registry, entt::entity station) {
    std::vector<entt::entity> benches;
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return benches;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(child);
        if (facility != nullptr && facility->kind == FacilityKind::Engineering) {
            benches.push_back(child);
        }
    }
    return benches;
}

bool HasDependentChild(const entt::registry& registry, entt::entity rigRoot,
                       entt::entity hardpoint) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity child : rig->children) {
        if (child == hardpoint) {
            continue;
        }
        const StructuralAttachment* attachment = registry.try_get<StructuralAttachment>(child);
        if (attachment != nullptr && attachment->attachedTo == hardpoint) {
            return true;
        }
    }
    return false;
}

// The hardpoint in `rigRoot`'s Rig carrying MountRef::id == `mount`, or entt::null. Whether it is
// living or Destroyed is the caller's concern -- this only answers "is there an entity at all."
entt::entity FindMount(const entt::registry& registry, entt::entity rigRoot, const MountId& mount) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return entt::null;
    }
    for (const entt::entity child : rig->children) {
        const MountRef* ref = registry.try_get<MountRef>(child);
        if (ref != nullptr && ref->id == mount) {
            return child;
        }
    }
    return entt::null;
}

struct Layout {
    Rectangle content{};
    Rectangle header{};
    Rectangle siblingStrip{};  // Zero height when there is only one bench.
    Rectangle leftList{};
    Rectangle rightList{};
};

Layout ComputeLayout(Rectangle bounds, bool showSiblingStrip) {
    const Rectangle content = sr::ui::PanelContentRect(bounds);
    Layout layout;
    layout.content = content;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    float y = content.y + kHeaderHeight;
    if (showSiblingStrip) {
        layout.siblingStrip = {content.x, y, content.width, kSiblingStripHeight};
        y += kSiblingStripHeight;
    }
    const float columnWidth = (content.width - kColumnGap) * 0.5f;
    layout.leftList = {content.x, y, columnWidth, kListHeight};
    layout.rightList = {content.x + columnWidth + kColumnGap, y, columnWidth, kListHeight};
    return layout;
}

Rectangle PanelBounds(bool showSiblingStrip) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float height = kHeaderHeight + (showSiblingStrip ? kSiblingStripHeight : 0.0f) +
                         kListHeight + 2.0f * sr::ui::kPanelPadding;
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth, height};
}

}  // namespace

std::vector<ModuleRow> ModuleRows(const entt::registry& registry, entt::entity requester) {
    std::vector<ModuleRow> rows;
    for (const ItemStack& stack : cargo_view::Merged(registry, requester)) {
        if (stack.kind != ItemKind::Module) {
            continue;
        }
        ModuleRow entry;
        entry.module = ModuleId(stack.id);
        entry.row.label = stack.id;
        entry.row.value = "DECONSTRUCT";
        rows.push_back(std::move(entry));
    }
    return rows;
}

std::vector<entt::entity> DeletableHardpoints(const entt::registry& registry,
                                              entt::entity rigRoot) {
    std::vector<entt::entity> deletable;
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return deletable;
    }
    for (const entt::entity hardpoint : rig->children) {
        if (!HasDependentChild(registry, rigRoot, hardpoint)) {
            deletable.push_back(hardpoint);
        }
    }
    return deletable;
}

std::vector<MountId> RebuildableMounts(const entt::registry& registry, entt::entity rigRoot,
                                       const RigBlueprint& blueprint) {
    std::vector<MountId> missing;
    for (const MountBlueprint& mount : blueprint.mounts) {
        if (FindMount(registry, rigRoot, mount.id) == entt::null) {
            missing.push_back(mount.id);
        }
    }
    return missing;
}

std::vector<MountRow> MountRows(const entt::registry& registry, entt::entity rigRoot,
                                const RigBlueprint& blueprint) {
    std::vector<MountRow> rows;
    const std::vector<entt::entity> deletable = DeletableHardpoints(registry, rigRoot);
    const std::vector<MountId> rebuildable = RebuildableMounts(registry, rigRoot, blueprint);
    rows.reserve(blueprint.mounts.size());

    for (const MountBlueprint& mount : blueprint.mounts) {
        MountRow entry;
        entry.mount = mount.id;
        entry.row.label = mount.id.str();

        const bool missing =
            std::find(rebuildable.begin(), rebuildable.end(), mount.id) != rebuildable.end();
        if (missing) {
            // "Outline only, MISSING" (architecture.md 12.30.5) -- greyed only when its authored
            // attachedTo parent is itself missing or Destroyed, the orphan refusal RefactorSystem
            // applies at request time, mirrored here so the click never has a reason to fail.
            bool parentReady = mount.attachedTo.empty();
            if (!parentReady) {
                const entt::entity parent = FindMount(registry, rigRoot, mount.attachedTo);
                parentReady = parent != entt::null && !registry.all_of<Destroyed>(parent);
            }
            entry.row.style.integrity = 0.0f;
            entry.row.style.disabled = !parentReady;
            entry.row.value = "REBUILD";
        } else {
            entry.hardpoint = FindMount(registry, rigRoot, mount.id);
            entry.destroyed = registry.all_of<Destroyed>(entry.hardpoint);
            entry.deletable =
                std::find(deletable.begin(), deletable.end(), entry.hardpoint) != deletable.end();
            if (const ShellRole* role = registry.try_get<ShellRole>(entry.hardpoint)) {
                ShellGlyph(role->kind, entry.row.glyph);
            }
            if (const Health* health = registry.try_get<Health>(entry.hardpoint)) {
                entry.row.style.integrity =
                    health->max > 0.0f ? health->current / health->max : 0.0f;
            }
            entry.row.style.disabled = entry.destroyed || !entry.deletable;
            entry.row.value = entry.destroyed ? "DESTROYED" : "DELETE";
        }
        rows.push_back(std::move(entry));
    }
    return rows;
}

entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction) {
    for (auto [vessel, docked, faction] : registry.view<Docked, FactionRef>().each()) {
        if (docked.station == station && faction.id == playerFaction) {
            return vessel;
        }
    }
    return entt::null;
}

namespace {

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
    const BlueprintRef* blueprintRef = registry.try_get<BlueprintRef>(ctx.requester);
    if (blueprintRef == nullptr) {
        return ctx;
    }
    const ShipBlueprint* ship = content.FindShip(blueprintRef->id);
    if (ship != nullptr) {
        ctx.blueprint = &ship->rig;
    }
    return ctx;
}

}  // namespace

void Update(entt::registry& registry, const FactionId& playerFaction,
            const core::ContentLibrary& content) {
    const ResolvedContext ctx = Resolve(registry, playerFaction, content);
    if (ctx.requester == entt::null || ctx.blueprint == nullptr) {
        return;
    }

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const std::vector<entt::entity> siblings = SiblingBenches(registry, ctx.station);
    const Layout layout = ComputeLayout(PanelBounds(siblings.size() > 1), siblings.size() > 1);

    if (siblings.size() > 1) {
        const std::optional<int> hit = sr::ui::TabStripHitTest(
            layout.siblingStrip, static_cast<int>(siblings.size()), input.cursor);
        if (hit.has_value()) {
            const entt::entity target = siblings[static_cast<std::size_t>(*hit)];
            if (target != ctx.facility) {
                registry.remove<PlayerLocation>(ctx.facility);
                registry.emplace<PlayerLocation>(target, PlayerLocation{target});
            }
            return;
        }
    }

    const std::vector<ModuleRow> modules = ModuleRows(registry, ctx.requester);
    const std::optional<int> moduleHit = sr::ui::ListViewRowAt(
        layout.leftList, static_cast<int>(modules.size()), 0.0f, input.cursor);
    if (moduleHit.has_value() && *moduleHit < static_cast<int>(modules.size())) {
        registry.emplace_or_replace<DeconstructModuleRequest>(
            ctx.requester,
            DeconstructModuleRequest{modules[static_cast<std::size_t>(*moduleHit)].module});
        return;
    }

    const std::vector<MountRow> mounts = MountRows(registry, ctx.requester, *ctx.blueprint);
    const std::optional<int> mountHit = sr::ui::ListViewRowAt(
        layout.rightList, static_cast<int>(mounts.size()), 0.0f, input.cursor);
    if (!mountHit.has_value() || *mountHit >= static_cast<int>(mounts.size())) {
        return;
    }
    const MountRow& row = mounts[static_cast<std::size_t>(*mountHit)];
    if (row.row.style.disabled) {
        return;
    }
    if (row.hardpoint == entt::null) {
        registry.emplace_or_replace<RebuildMountRequest>(ctx.requester,
                                                         RebuildMountRequest{row.mount});
    } else {
        registry.emplace_or_replace<DeleteHardpointRequest>(ctx.requester,
                                                            DeleteHardpointRequest{row.hardpoint});
    }
}

void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::ContentLibrary& content) {
    const ResolvedContext ctx = Resolve(registry, playerFaction, content);
    if (ctx.requester == entt::null || ctx.blueprint == nullptr) {
        return;
    }

    const std::vector<entt::entity> siblings = SiblingBenches(registry, ctx.station);
    const bool showSiblingStrip = siblings.size() > 1;
    const Layout layout = ComputeLayout(PanelBounds(showSiblingStrip), showSiblingStrip);
    sr::ui::DrawPanelFrame(PanelBounds(showSiblingStrip));

    std::string facilityName = "ENGINEERING";
    if (const DisplayName* name = registry.try_get<DisplayName>(ctx.station)) {
        facilityName = name->value;
    }
    const FacilityRef& facilityRef = registry.get<FacilityRef>(ctx.facility);
    const std::string header = facilityName + "  GRADE " + std::to_string(facilityRef.grade);
    DrawText(header.c_str(), static_cast<int>(layout.header.x), static_cast<int>(layout.header.y),
             18, sr::ui::kValueBright);

    const Health* facilityHealth = registry.try_get<Health>(ctx.facility);
    const float facilityIntegrity = facilityHealth != nullptr && facilityHealth->max > 0.0f
                                        ? facilityHealth->current / facilityHealth->max
                                        : 1.0f;
    const Color gaugeColor = facilityIntegrity > 0.5f   ? sr::ui::kStatusGood
                             : facilityIntegrity > 0.2f ? sr::ui::kStatusCaution
                                                        : sr::ui::kStatusCritical;
    const Rectangle gaugeBounds{layout.header.x + layout.header.width * 0.5f, layout.header.y,
                                layout.header.width * 0.5f, 20.0f};
    sr::ui::DrawGauge(gaugeBounds, "FACILITY INTEGRITY", facilityIntegrity, gaugeColor);

    if (showSiblingStrip) {
        std::vector<std::string> labels;
        labels.reserve(siblings.size());
        int selected = -1;
        for (std::size_t i = 0; i < siblings.size(); ++i) {
            labels.push_back("BENCH " + std::to_string(i + 1));
            if (siblings[i] == ctx.facility) {
                selected = static_cast<int>(i);
            }
        }
        sr::ui::DrawTabStrip(layout.siblingStrip, labels, selected);
    }

    std::vector<sr::ui::Row> moduleWidgetRows;
    for (const ModuleRow& entry : ModuleRows(registry, ctx.requester)) {
        moduleWidgetRows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.leftList, moduleWidgetRows, 0.0f, "CARGO HOLD EMPTY");

    std::vector<sr::ui::Row> mountWidgetRows;
    for (const MountRow& entry : MountRows(registry, ctx.requester, *ctx.blueprint)) {
        mountWidgetRows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.rightList, mountWidgetRows, 0.0f, "NO MOUNTS");
}

}  // namespace sr::space::ui::engineering_screen
