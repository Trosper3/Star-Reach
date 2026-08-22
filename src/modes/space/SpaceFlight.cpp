#include "modes/space/SpaceFlight.h"

#include <functional>
#include <string>
#include <vector>

#include "modes/space/factories/RigFactory.h"
#include "modes/space/factories/WorldGen.h"
#include "modes/space/render/IconRenderer.h"
#include "modes/space/render/WorldRenderer.h"
#include "modes/space/systems/LootSystem.h"
#include "modes/space/systems/PlayerRecordSystem.h"
#include "modes/space/systems/SpawnSystem.h"
#include "modes/space/systems/SystemSchedule.h"
#include "modes/space/ui/AvionicsMenu.h"
#include "modes/space/ui/BayView.h"
#include "modes/space/ui/BridgeView.h"
#include "modes/space/ui/CockpitHud.h"
#include "modes/space/ui/FlightControls.h"
#include "modes/space/ui/ResearchScreen.h"
#include "modes/space/ui/ModulesMenu.h"
#include "modes/space/ui/StorageMenu.h"
#include "modes/space/ui/SystemMenu.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/NetworkOwner.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/components/Warp.h"

namespace sr::space {
namespace {

// The player's shell entity (the entity carrying PlayerLocation -- their own rig root while
// flying, a facility hardpoint while docked), found by PlayerLocation, not PlayerControlled --
// architecture.md 12.30.1 makes PlayerLocation the sole source of truth. PlayerControlled is now
// derived from this every tick by modes/space/systems/PlayerLocationSystem.h (P4-01), but camera
// follow below wants wherever the player is actually standing (WorldTransform resolves for a
// hardpoint the same way it does for a rig root), which is this helper's job, not
// PlayerControlled's. entt::null if OnEnter hasn't placed a player.
entt::entity FindPlayer(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

// The player's own vessel root, regardless of where they are currently standing -- unlike
// FindPlayer/PlayerLocation's own shell, this always resolves to the ship itself: self-
// referential while flying (no ParentRig), ParentRig::root while boarded on a different owned
// hull (architecture.md 12.30.2's Board), and -- while standing on a facility hardpoint, which
// belongs to the STATION, not the player (architecture.md 12.30.1) -- whichever of the player's
// own vessels is Docked there. entt::null if OnEnter hasn't placed a player, or (rare) none is
// found. Resolved once here and threaded into both flight overlays (architecture.md 12.30.7),
// the same "caller resolves shared context once" rule RepairScreen's playerFaction threading
// already follows -- modes/*/ui/ may not include systems/ (section 2.3), so this cannot live in
// either overlay file itself.
entt::entity PlayerVesselRoot(const entt::registry& registry, const FactionId& playerFaction) {
    const entt::entity shell = FindPlayer(registry);
    if (shell == entt::null) {
        return entt::null;
    }
    if (registry.all_of<FacilityRef>(shell)) {
        const ParentRig* parent = registry.try_get<ParentRig>(shell);
        const entt::entity station = parent != nullptr ? parent->root : entt::null;
        if (station == entt::null) {
            return entt::null;
        }
        for (auto [vessel, docked, faction] : registry.view<Docked, FactionRef>().each()) {
            if (docked.station == station && faction.id == playerFaction) {
                return vessel;
            }
        }
        return entt::null;
    }
    if (const ParentRig* parent = registry.try_get<ParentRig>(shell)) {
        return parent->root;
    }
    return shell;
}

}  // namespace

SpaceFlight::SpaceFlight(core::ContentLibrary& content, core::economy::FactionEconomy& economy,
                         core::galaxy::WreckLedger& wreckLedger,
                         core::knowledge::KnowledgeStore& knowledge,
                         core::diplomacy::DiplomacyMatrix& diplomacy,
                         core::diplomacy::Reputation& reputation)
    : world_("sol", "Sol"),
      content_(content),
      economy_(economy),
      wreckLedger_(wreckLedger),
      knowledge_(knowledge),
      diplomacy_(diplomacy),
      reputation_(reputation) {}

void SpaceFlight::OnEnter() {
    // Reset before populating, not just on the way out: without this, entering a second time in
    // the same process (architecture.md 12.29 -- quit to main menu, then start a new game)
    // populates the new system on top of whatever `world_`/`clock_` still held from the last one,
    // producing two suns and two players sharing a registry. §12.29 imposes this requirement on
    // step 1 even though the quit-to-menu path itself lands later.
    world_ = SystemWorld("sol", "Sol");
    clock_ = core::FixedTimestep{};
    // architecture.md 12.29: a latch, mirroring MainMenu::OnEnter() resetting its own
    // startRequested_/quitRequested_ -- without this, quitting to the menu and starting a new
    // game would read the prior game's confirmed quit and bounce straight back out.
    returnToMenuRequested_ = false;

    PopulateWorld("sol");

    // architecture.md 12.36: resolved here, before the rig exists, rather than passed as
    // FactionId{} the way SpawnPlayerAt's own `faction.empty() -> blueprint->faction` fallback
    // would -- ResolveSpawnPlacement needs a real FactionId to match a friendly DockingBay
    // against, and there is no rig yet to read one off of. Unreachable in practice: the starting
    // blueprint always resolves against loaded content.
    const ShipBlueprint* startingBlueprint = content_.FindShip(BlueprintId(kStartingBlueprint));
    const FactionId faction =
        startingBlueprint != nullptr ? startingBlueprint->faction : FactionId{};

    // Falls back to the reference position itself (unreachable in practice: WorldGen always
    // spawns a friendly station) rather than propagating nullopt -- OnEnter has no per-tick retry
    // to lean on the way SpawnSystem::ResolveRespawns does.
    const Vec2 spawnPosition =
        spawn_system::ResolveSpawnPlacement(world_.Registry(), faction, Vec2{0.0f, 0.0f})
            .value_or(Vec2{0.0f, 0.0f});
    // A fresh network for a fresh save -- there is no prior NetworkOwner to carry forward the way
    // WarpToSystem's own call below does (architecture.md 12.30.6: "the player's network is
    // bootstrapped once per save").
    const KnowledgeNetworkId network = knowledge_.Create(core::knowledge::NetworkOwnerKind::Player);
    SpawnPlayerAt(BlueprintId(kStartingBlueprint), faction, spawnPosition, 0.0f, Wallet{}, network);
}

void SpaceFlight::Update(float realDeltaSeconds) {
    entt::registry& registry = world_.Registry();

    // Esc's ladder and any Resume/Quit/Confirm/Cancel click (architecture.md 12.29) -- polled
    // every frame regardless of pause state, for the same "IsKeyPressed reads this frame's
    // press" reason avionics_menu::Update below is polled unconditionally too.
    ui::system_menu::Update(registry);
    if (ui::system_menu::QuitConfirmed(registry)) {
        returnToMenuRequested_ = true;
    }
    if (ui::system_menu::IsOpen(registry)) {
        // Paused: the system menu confers no tactical value (features.md 3.4's exception is what
        // makes freezing here legal), so nothing below may run -- no flight input, no tick
        // advance, no real time banked in clock_'s accumulator, or resuming would fast-forward
        // the world through however long the player sat in the menu.
        return;
    }

    // Polled once per real frame, same as the window itself -- IsKeyPressed's "pressed this
    // frame" state does not survive being checked mid-tick, so this runs before the fixed-step
    // loop rather than inside it. The DockRequest/UndockRequest it may write is still visible to
    // every tick this frame runs (Law 9's established idiom; see AvionicsMenu.h).
    // Threaded in rather than looked up by ui/ itself -- modes/*/ui/ may not include systems/
    // (section 2.3), and both files below need "is this hull mine" (architecture.md 12.30.2).
    const FactionId playerFaction = player_record_system::FactionOf(registry);
    ui::avionics_menu::Update(registry, playerFaction);
    ui::bridge_view::Update(registry);
    // Threaded in rather than looked up by ui/ itself -- modes/*/ui/ may not include systems/
    // (section 2.3), and this screen needs both "is this station mine" and the knowledge store's
    // already-known check (architecture.md 12.30.6).
    ui::research_screen::Update(registry, player_record_system::FactionOf(registry), knowledge_);
    // architecture.md 12.30.7: available everywhere, gated on nothing -- both run whether the
    // player is flying or docked over any screen (features.md 3.10), never facility-gated the
    // way bridge_view's own tabs are. PlayerVesselRoot, not PlayerLocation's own shell, since
    // standing on a facility hardpoint while docked must still resolve to the player's own ship.
    {
        const entt::entity vesselRoot =
            PlayerVesselRoot(registry, player_record_system::FactionOf(registry));
        ui::storage_menu::Update(registry, vesselRoot);
        ui::modules_menu::Update(registry, vesselRoot);
    }
    ui::bay_view::Update(registry, playerFaction);
    ui::flight_controls::Poll(intents_, kLocalPlayerActorId,
                              render::CameraView{cameraTarget_, cameraZoom_});

    clock_.Advance(realDeltaSeconds);

    while (clock_.ConsumeStep()) {
        // architecture.md 12.24 step 6: the four galaxy-wide pointers, alongside `economy` above.
        // `craftedModules` aliases `content_` itself, non-const -- see this class's constructor
        // comment.
        const SystemContext ctx{world_,
                                intents_,
                                content_,
                                core::kFixedDeltaSeconds,
                                clock_.ElapsedTicks(),
                                &economy_,
                                &knowledge_,
                                &diplomacy_,
                                &reputation_,
                                &content_};
        RunTick(ctx);
    }

    // architecture.md 12.24 step 3: the raw (un-interpolated) tick position. Draw() blends this
    // tick's WorldTransform against the player's PreviousTransform using alpha, same as every
    // sprite WorldRenderer draws -- snapping cameraTarget_ straight to WorldTransform here would
    // judder against interpolated sprites between ticks.
    if (const entt::entity player = FindPlayer(world_.Registry()); player != entt::null) {
        cameraTarget_ = world_.Registry().get<WorldTransform>(player).position;
    }

    ProcessWarpRequests();

    // Drained after the whole schedule, never mid-list: a system's view of this tick's input
    // must not depend on its position in the order.
    intents_.Clear();
}

void SpaceFlight::ProcessWarpRequests() {
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
}

void SpaceFlight::PopulateWorld(const std::string& targetSystemId) {
    // Seeded from the target system's id alone, not a real galaxy coordinate -- there is no
    // system-adjacency/topology store yet to derive a proper core::galaxy::Seeding cascade from
    // (architecture.md section 12.5's noted follow-up). Deterministic per id in the meantime.
    const unsigned int seed = static_cast<unsigned int>(std::hash<std::string>{}(targetSystemId));
    world_gen::PopulateSystem(world_, content_, seed);
}

void SpaceFlight::SpawnPlayerAt(const BlueprintId& blueprint, const FactionId& faction,
                                Vec2 spawnPosition, float spawnRotation, Wallet wallet,
                                KnowledgeNetworkId network) {
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
    // Carried over exactly like Wallet above -- see this method's own header comment on why
    // creating a fresh network here instead would orphan the player's prior unlocks.
    arriving.emplace<NetworkOwner>(spawned.root, NetworkOwner{network});
    // Not PlayerControlled: architecture.md 12.30.1 makes PlayerLocation the sole source of
    // truth and PlayerControlled a derived tag (P4-01) -- writing both here would let them
    // disagree about where the player is. Self-referential for a fighter: there is no separate
    // cockpit hardpoint entity yet, so the rig root is its own "shell".
    arriving.emplace<PlayerLocation>(spawned.root, PlayerLocation{spawned.root});
    // ActorRef (architecture.md 12.24 step 2): resolves core::Intent's ActorId back to this
    // entity for PlayerInputSystem. Written here, at spawn, never by a system.
    arriving.emplace<ActorRef>(spawned.root, ActorRef{kLocalPlayerActorId});
    // The player record (architecture.md 12.30.3): survives independent of PlayerLocation's
    // shell, but not independent of the registry itself -- `arriving` is a fresh SystemWorld on
    // every warp, so this re-creates the record there too, the same reason Wallet is threaded
    // through as a parameter instead of assumed to still exist.
    player_record_system::SetFaction(arriving, faction);
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
    // Carried over, never re-Create()'d -- SpawnPlayerAt's header comment explains why.
    const KnowledgeNetworkId network =
        departing.all_of<NetworkOwner>(player)
            ? departing.get<NetworkOwner>(player).network
            : knowledge_.Create(core::knowledge::NetworkOwnerKind::Player);

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

    PopulateWorld(targetSystemId);
    SpawnPlayerAt(blueprint, faction, spawnPosition, spawnRotation, wallet, network);

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
    const float alpha = InterpolationAlpha();

    // Interpolated here, not in Update() -- cameraTarget_ holds the raw tick position; blending
    // it toward the player's live WorldTransform by alpha every frame is what keeps the camera in
    // lockstep with WorldRenderer's own per-sprite interpolation instead of trailing it.
    Vec2 cameraPosition = cameraTarget_;
    const entt::registry& registry = world_.Registry();
    if (const entt::entity player = FindPlayer(registry); player != entt::null) {
        if (const auto* prev = registry.try_get<PreviousTransform>(player)) {
            cameraPosition =
                Lerp(prev->position, registry.get<WorldTransform>(player).position, alpha);
        }
    }
    const render::CameraView camera{cameraPosition, cameraZoom_};
    render::DrawWorld(world_, camera, alpha);
    // Outside DrawWorld's BeginMode2D/EndMode2D on purpose -- IconRenderer projects world space
    // to screen space itself, so its reticle stays a fixed pixel size under zoom instead of
    // scaling with the world like WorldRenderer's sprites do.
    render::DrawAimReticle(registry, camera);

    // modes/space/ui/ -- screen-space, outside DrawWorld's BeginMode2D/EndMode2D.
    const FactionId playerFaction = player_record_system::FactionOf(registry);
    ui::cockpit_hud::Draw(world_.Registry());
    ui::avionics_menu::Draw(world_.Registry(), playerFaction);
    ui::bridge_view::Draw(world_.Registry());
    ui::research_screen::Draw(registry, player_record_system::FactionOf(registry), knowledge_);
    // architecture.md 12.30.7: drawn over the world in flight and over whichever docked screen
    // is also showing (features.md 3.10's "an overlay is defined by being over something, not by
    // what it is over") -- after bridge_view, never gated on it.
    {
        const entt::entity vesselRoot =
            PlayerVesselRoot(registry, player_record_system::FactionOf(registry));
        ui::storage_menu::Draw(registry, vesselRoot);
        ui::modules_menu::Draw(registry, vesselRoot);
    }
    ui::bay_view::Draw(world_.Registry(), playerFaction);
    // Drawn last so it sits on top of every other screen-space overlay -- the only pause in the
    // game (architecture.md 12.29).
    ui::system_menu::Draw(world_.Registry());
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
