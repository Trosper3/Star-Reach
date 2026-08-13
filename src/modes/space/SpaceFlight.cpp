#include "modes/space/SpaceFlight.h"

#include <functional>
#include <string>
#include <vector>

#include "modes/space/factories/RigFactory.h"
#include "modes/space/factories/WorldGen.h"
#include "modes/space/render/IconRenderer.h"
#include "modes/space/render/WorldRenderer.h"
#include "modes/space/systems/LootSystem.h"
#include "modes/space/systems/SystemSchedule.h"
#include "modes/space/ui/AvionicsMenu.h"
#include "modes/space/ui/BridgeView.h"
#include "modes/space/ui/CockpitHud.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Warp.h"

namespace sr::space {

SpaceFlight::SpaceFlight(const core::ContentLibrary& content,
                         core::economy::FactionEconomy& economy,
                         core::galaxy::WreckLedger& wreckLedger)
    : world_("sol", "Sol"), content_(content), economy_(economy), wreckLedger_(wreckLedger) {}

void SpaceFlight::OnEnter() {
    // Factories populate the world here once they land (first vertical slice, step 5-7):
    // WorldGen seeds the system, ShipFactory instantiates the player rig from its blueprint,
    // NpcFactory adds the opposition. Nothing is constructed inline in this file -- Law 5.
}

void SpaceFlight::Update(float realDeltaSeconds) {
    // Polled once per real frame, same as the window itself -- IsKeyPressed's "pressed this
    // frame" state does not survive being checked mid-tick, so this runs before the fixed-step
    // loop rather than inside it. The DockRequest/UndockRequest it may write is still visible to
    // every tick this frame runs (Law 9's established idiom; see AvionicsMenu.h).
    ui::avionics_menu::Update(world_.Registry());

    clock_.Advance(realDeltaSeconds);

    while (clock_.ConsumeStep()) {
        const SystemContext ctx{
            world_, intents_, content_, core::kFixedDeltaSeconds, clock_.ElapsedTicks(), &economy_};
        RunTick(ctx);
    }

    // Copied out before WarpToSystem runs, never read from `request` after: WarpToSystem replaces
    // world_'s registry, which is exactly what `request` lives in.
    bool warpRequested = false;
    std::string targetSystemId;
    Vec2 spawnPosition;
    float spawnRotation = 0.0f;
    for (auto [entity, request] : world_.Registry().view<SystemWarpRequest>().each()) {
        targetSystemId = request.targetSystemId;
        spawnPosition = request.spawnPosition;
        spawnRotation = request.spawnRotation;
        warpRequested = true;
        break;  // Exactly one PlayerControlled entity can hold this (shared/components/Identity.h).
    }
    if (warpRequested) {
        WarpToSystem(targetSystemId, spawnPosition, spawnRotation);
    }

    // Drained after the whole schedule, never mid-list: a system's view of this tick's input
    // must not depend on its position in the order.
    intents_.Clear();
}

void SpaceFlight::WarpToSystem(const std::string& targetSystemId, Vec2 spawnPosition,
                               float spawnRotation) {
    entt::registry& departing = world_.Registry();

    entt::entity player = entt::null;
    for (auto [entity] : departing.view<PlayerControlled>().each()) {
        player = entity;
        break;
    }
    if (player == entt::null) {
        return;  // Nothing to hand off -- OnEnter hasn't placed a player yet (pre-existing gap).
    }

    const BlueprintId blueprint = departing.get<BlueprintRef>(player).id;
    const FactionId faction = departing.get<FactionRef>(player).id;
    const Wallet wallet =
        departing.all_of<Wallet>(player) ? departing.get<Wallet>(player) : Wallet{};

    // Demote every DeathWreck left behind (architecture.md section 12.5) -- collected first,
    // since CollapseDeathWreck destroys the entity it's given and destroying mid-iteration over
    // the same view it came from is unsafe.
    std::vector<entt::entity> wrecks;
    for (const entt::entity entity : departing.view<DeathWreck>()) {
        wrecks.push_back(entity);
    }
    for (const entt::entity wreck : wrecks) {
        wreckLedger_.Add(loot_system::CollapseDeathWreck(departing, wreck, world_.SystemId()));
    }

    // Tear down the departing SystemWorld and stand up the destination. Move-assignment destroys
    // everything still in `world_` before replacing it -- Law 2's "clean handoff": nothing
    // survives this line except what was captured above. `departing` is dangling after this.
    world_ = SystemWorld(targetSystemId);

    // Seeded from the target system's id alone, not a real galaxy coordinate -- there is no
    // system-adjacency/topology store yet to derive a proper core::galaxy::Seeding cascade from
    // (architecture.md section 12.5's noted follow-up). Deterministic per id in the meantime.
    const unsigned int seed = static_cast<unsigned int>(std::hash<std::string>{}(targetSystemId));
    world_gen::PopulateSystem(world_, content_, seed);

    const rig_factory::SpawnResult spawned = rig_factory::Spawn(
        world_, content_,
        rig_factory::SpawnParams{blueprint, faction, spawnPosition, spawnRotation});
    if (!spawned.ok()) {
        return;  // Unreachable in practice -- the blueprint just came from a live rig.
    }

    entt::registry& arriving = world_.Registry();
    arriving.emplace<PlayerControlled>(spawned.root);
    // Wallet carries over; cargo does not. CargoHold now lives per cargo-bay hardpoint
    // (architecture.md 12.23), and RigFactory::Spawn rebuilds every hardpoint empty from the
    // blueprint -- there is nothing on `player` to copy forward the way a single root-level
    // CargoHold used to be. This is the documented, accepted gap architecture.md 12.23 itself
    // names: per-bay carry-over belongs in P12.31's RigState (a per-mount delta against a
    // BlueprintId, which already has to carry MountedModules/ShellInstance too), not a
    // regression introduced here -- hardpoint damage/refits already don't carry over for the
    // identical reason (WarpToSystem's own doc comment, SpaceFlight.h).
    arriving.emplace<Wallet>(spawned.root, wallet);

    // Promote every wreck this system is owed back into an entity now that it's resident again.
    for (const core::galaxy::WreckLedger::Id id : wreckLedger_.IdsForSystem(targetSystemId)) {
        if (const core::galaxy::WreckRecord* record = wreckLedger_.Find(id)) {
            loot_system::PromoteDeathWreck(arriving, *record);
            wreckLedger_.Remove(id);
        }
    }
}

void SpaceFlight::Draw() const {
    // Camera math belongs in this file (Law 7); the draw calls themselves belong in
    // modes/space/render/.
    const render::CameraView camera{cameraTarget_, cameraZoom_};
    const float alpha = InterpolationAlpha();
    render::DrawWorld(world_, camera, alpha);
    // Outside DrawWorld's BeginMode2D/EndMode2D on purpose -- IconRenderer projects world space
    // to screen space itself, so its reticle stays a fixed pixel size under zoom instead of
    // scaling with the world like WorldRenderer's sprites do.
    render::DrawTargetReticle(world_.Registry(), camera, alpha);
    render::DrawWorld(world_, render::CameraView{cameraTarget_, cameraZoom_}, InterpolationAlpha());

    // modes/space/ui/ -- screen-space, outside DrawWorld's BeginMode2D/EndMode2D.
    ui::cockpit_hud::Draw(world_.Registry());
    ui::avionics_menu::Draw(world_.Registry());
    ui::bridge_view::Draw(world_.Registry());
}

void SpaceFlight::OnExit() {}

}  // namespace sr::space
