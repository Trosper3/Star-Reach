#pragma once

#include <cstdint>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <string>
#include <vector>

#include "core/knowledge/KnowledgeNetwork.h"
#include "shared/blueprints/Ids.h"
#include "shared/ui/Row.h"

namespace sr::core {
class ContentLibrary;
}  // namespace sr::core

// modes/space/ui/CodexScreen -- architecture.md 12.30.6, "The Codex -- browsing what is already
// unlocked." A read-only browse of the player's KnowledgeNetwork, distinct from ResearchScreen's
// own queue (what to research next): three sections by item kind (Modules, Shells, Materials),
// each row resolved against ContentLibrary for its display fields, with faction/tier filter
// chips and a search field over the combined set. No new component, no new request -- a pure
// read, so unlike every other docked screen in modes/space/ui/ this one never emplaces anything.
//
// Reached by a button on the Research screen, not its own key or router tab -- it "has no
// facility gate of its own, the same shape architecture.md 12.30.7's overlays have, but opened
// from Research rather than available everywhere" (12.30.6's own words). Concretely: ResearchScreen
// hit-tests its own CODEX button and calls Open() below; this file never gates on a living
// Research hardpoint itself, so the panel stays open (and browsable) even after the player
// undocks, same as the inventory/loadout overlays staying open across a screen change.
//
// Deliberately not a tech tree (features.md section 9 is still unspecified): a flat, filterable
// list, never a graph.
namespace sr::space::ui::codex_screen {

// Which ContentLibrary def kind an unlocked id resolved to -- the Codex's own three-way split.
// Distinct from sr::ItemKind (Module/Element only, CargoView's vocabulary): a KnowledgeNetwork
// unlock names *anything* researchable, including a Shell, which never occupies a CargoHold slot
// and so has no ItemKind of its own.
enum class EntryKind : std::uint8_t { Module, Shell, Material };

// One resolved, unlocked entry: an id from KnowledgeNetwork::unlockedBlueprints joined against
// its ContentLibrary def. `faction`/`tier` come from that def (ModuleDef.h/ShellDef.h/
// ElementDef.h's own fields, added by this issue) -- empty faction means unfactioned content,
// which is everything base_game/ authors today.
struct Entry {
    std::string id;
    EntryKind kind = EntryKind::Module;
    std::string displayName;
    FactionId faction;
    int tier = 1;
    sr::ui::Row row;
};

// Every id in `network`'s unlockedBlueprints, resolved against `content` -- checking FindModule,
// then FindShell, then FindElement. Pure -- no raylib -- so unit-testable. An id resolving
// against none of the three is skipped rather than asserted (defensive, matching every other
// ContentLibrary lookup in modes/space/ui/): ResearchSystem::Grant is unlockedBlueprints' only
// writer, and it never grants an id that was not already resolved against ContentLibrary at
// commit time, so in practice this can't happen -- "impossible by construction," per this
// issue's own Tests bullet, which CodexScreenTests exercises directly rather than asserting it
// in shipped code.
std::vector<Entry> Entries(const core::knowledge::KnowledgeNetwork& network,
                           const core::ContentLibrary& content);

// The distinct FactionId/tier values actually present across `entries`, each in first-seen
// order -- filter chips are built from what the player has actually unlocked, never a fixed
// enum/range, so a network with only unfactioned tier-1 unlocks draws exactly one chip of each
// rather than a full seven-tier roster nothing in it uses.
std::vector<FactionId> DistinctFactions(const std::vector<Entry>& entries);
std::vector<int> DistinctTiers(const std::vector<Entry>& entries);

// Lives on a lazily-created singleton entity (System.h's "one legitimate cache" exception, the
// SystemMenuState precedent) -- not FlightOverlayState: the Codex opens from a Research-screen
// button, not features.md 3.6's everywhere-legal overlay key, so it does not share that state's
// "legal everywhere" precondition even though the open/closed shape is the same.
struct CodexStateSingleton {};
struct CodexState {
    bool open = false;
    std::string searchQuery;
    // Empty faction / tier 0 means "no filter" -- matches every row. Real tiers start at 1, and
    // FactionId's default constructor is already the empty string, so both sentinels are each
    // type's own zero value rather than an invented one.
    FactionId factionFilter;
    int tierFilter = 0;
};

// `entries` narrowed by `state`'s search substring (case-insensitive, matched against
// displayName) and its faction/tier chip selection -- narrows the combined set without touching
// `entries` or `state` (this issue's own Tests bullet).
std::vector<Entry> Filtered(const std::vector<Entry>& entries, const CodexState& state);

bool IsOpen(const entt::registry& registry);

// Opens/closes the panel -- called by ResearchScreen's own button click (Open) and this file's
// own close button/outside-click (Close), never by a key binding of this file's own.
void Open(entt::registry& registry);
void Close(entt::registry& registry);

// Reads this frame's input while open: a click on a faction/tier chip narrows CodexState's
// filter (clicking the already-selected chip clears it back to "ALL"), a click the close button
// calls Close(), and this frame's typed characters (raylib's GetCharPressed/KEY_BACKSPACE) edit
// CodexState::searchQuery. No-op while closed. `vesselRoot` is the player's own vessel root
// (SpaceFlight.cpp's PlayerVesselRoot, threaded in the same way the flight overlays' own
// `rigRoot` already is) -- resolving NetworkOwner from it rather than from PlayerLocation's own
// shell is what keeps the panel meaningful whether the player is flying, boarded on a different
// hull, or standing on a facility hardpoint that belongs to the station, not them.
void Update(entt::registry& registry, entt::entity vesselRoot,
            const core::knowledge::KnowledgeStore& knowledge, const core::ContentLibrary& content);

// Draws the Codex panel when open: title and close button, the search field, faction/tier filter
// chips, and one ListView per EntryKind (Modules, Shells, Materials). No-op while closed.
void Draw(const entt::registry& registry, entt::entity vesselRoot,
          const core::knowledge::KnowledgeStore& knowledge, const core::ContentLibrary& content);

}  // namespace sr::space::ui::codex_screen
