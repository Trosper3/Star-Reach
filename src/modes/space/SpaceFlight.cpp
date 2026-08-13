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
#include "modes/space/ui/FlightControls.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Warp.h"

namespace sr::space {

SpaceFlight::SpaceFlight(const core::ContentLibrary& content,
                         core::economy::FactionEconomy& economy,
                         core::galaxy::WreckLedger& wreckLedger)
    : world_("sol", "Sol"), content_(content), economy_(economy), wreckLedger_(wreckLedger) {}

void SpaceFlight::OnEnter() {
    // Reset before populating, not just on the way out: without this, entering a second time in
    // the same process (architecture.md 12.29 -- quit to main menu, then start a new game)
    // populates the new system on top of whatever `world_`/`clock_` still held from the last one,
    // producing two suns and two players sharing a registry. §12.29 imposes this requirement on
    // step 1 even though the quit-to-menu path itself lands later.
    world_ = SystemWorld("sol", "Sol");
    clock_ = core::FixedTimestep{};

    SpawnPlayerInto("sol", BlueprintId(kStartingBlueprint), FactionId{}, Vec2{0.0f, 0.0f}, 0.0f,
                    Wallet{});
}

void SpaceFlight::Update(float realDeltaSeconds) {
    // Polled once per real frame, same as the window itself -- IsKeyPressed's "pressed this
    // frame" state does not survive being checked mid-tick, so this runs before the fixed-step
    // loop rather than inside it. The DockRequest/UndockRequest it may write is still visible to
    // every tick this frame runs (Law 9's established idiom; see AvionicsMenu.h).
    ui::avionics_menu::Update(world_.Registry());
    ui::flight_controls::Poll(intents_, kLocalPlayerActorId);

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

void SpaceFlight::SpawnPlayerInto(const std::string& targetSystemId, const BlueprintId& blueprint,
                                  const FactionId& faction, Vec2 spawnPosition, float spawnRotation,
                                  Wallet wallet) {
    // Seeded from the target system's id alone, not a real galaxy coordinate -- there is no
    // system-adjacency/topology store yet to derive a proper core::galaxy::Seeding cascade from
    // (architecture.md section 12.5's noted follow-up). Deterministic per id in the meantime.
    const unsigned int seed = static_cast<unsigned int>(std::hash<std::string>{}(targetSystemId));
    world_gen::PopulateSystem(world_, content_, seed);

    const rig_factory::SpawnResult spawned = rig_factory::Spawn(
        world_, content_,
        rig_factory::SpawnParams{blueprint, faction, spawnPosition, spawnRotation});
    if (!spawned.ok()) {
        return;  // Unreachable in practice -- the blueprint just came from a live rig or content.
    }

    entt::registry& arriving = world_.Registry();
    // Wallet carries over on a warp; cargo does not. CargoHold now lives per cargo-bay hardpoint
    // (architecture.md 12.23), and RigFactory::Spawn rebuilds every hardpoint (cargo bay
    // included) empty from the blueprint -- there is nothing on the departing rig to copy forward
    // the way a single root-level CargoHold used to be. This is the documented, accepted gap
    // architecture.md 12.23 itself names: per-bay carry-over belongs in P12.31's RigState (a
    // per-mount delta against a BlueprintId, which already has to carry
    // MountedModules/ShellInstance too), not a regression introduced here -- hardpoint
    // damage/refits already don't carry over for the identical reason (this class's own doc
    // comment on WarpToSystem).
    arriving.emplace<Wallet>(spawned.root, wallet);
    // Not PlayerControlled: architecture.md 12.30.1 makes PlayerLocation the sole source of
    // truth and PlayerControlled a derived tag (P4-01) -- writing both here would let them
    // disagree about where the player is. Self-referential for a fighter: there is no separate
    // cockpit hardpoint entity yet, so the rig root is its own "shell".
    arriving.emplace<PlayerLocation>(spawned.root, PlayerLocation{spawned.root});
    // ActorRef (architecture.md 12.24 step 2): resolves core::Intent's ActorId back to this
    // entity for PlayerInputSystem. Written here, at spawn, never by a system.
    arriving.emplace<ActorRef>(spawned.root, ActorRef{kLocalPlayerActorId});
}

void SpaceFlight::WarpToSystem(const std::string& targetSystemId, Vec2 spawnPosition,
                               float spawnRotation) {
    entt::registry& departing = world_.Registry();

    entt::entity player = entt::null;
    for (const entt::entity entity : departing.view<PlayerLocation>()) {
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

    SpawnPlayerInto(targetSystemId, blueprint, faction, spawnPosition, spawnRotation, wallet);

    // Promote every wreck this system is owed back into an entity now that it's resident again.
    entt::registry& arriving = world_.Registry();
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

void SpaceFlight::OnExit() {
    // Releases the world eagerly rather than waiting for the next OnEnter -- architecture.md
    // 12.29 (quit to main menu). OnEnter resets `world_`/`clock_` again on its own before
    // populating, so this isn't load-bearing for re-entrancy by itself, but it frees the
    // departed system's registry the moment the player leaves rather than holding it alive at
    // the main menu for no reason.
    world_ = SystemWorld("sol", "Sol");
    intents_.Clear();
}

}  // namespace sr::space
