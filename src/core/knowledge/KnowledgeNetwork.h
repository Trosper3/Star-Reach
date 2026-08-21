#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "shared/blueprints/Ids.h"

// core/knowledge/ -- the persistent store behind features.md 2.5's knowledge networks
// (architecture.md 12.1).
//
// Why core/ and not a component -- this is forced, not chosen:
//   - Law 8 puts galaxy-wide, mode-agnostic state in core/. A faction's network must be readable
//     on the Tier 3 macro tick, where no registry exists at all (features.md 1.1). A component
//     would be unreadable exactly when the faction simulation needs it.
//   - Law 2's first hard rule: entity handles never cross a system boundary, a save file, or the
//     wire. A network outlives any one registry -- it survives the player warping away, and it
//     survives the death of the entity that was carrying its owner.
//
// The entity-side handle is shared/components/NetworkOwner.h -- POD { KnowledgeNetworkId
// network; } -- which points at data living here, never the reverse (the same shape Law 3 already
// uses for blueprints).
//
// This is a store, not a ticking system: nothing about it runs per frame. Writers (documented
// per section 11.4's "document the writer" rule): ResearchSystem grants an unlock on job
// completion (architecture.md 12.1's ResearchSystem subsection); TemplateMarketSystem copies a
// Template on sale (architecture.md 12.7); DiscoverySystem (already built, core/galaxy/
// Discovery.h) would grant discovered systems if/when it is pointed at this store instead of its
// own per-faction set -- not done by this commit, which only builds the store itself.
namespace sr::core::knowledge {

// Bumped whenever the on-disk shape of a KnowledgeStore save changes. Lives here rather than as a
// KnowledgeStore field (contrast shared/blueprints/ShipBlueprint.h's kBlueprintSchemaVersion,
// which travels as an ordinary struct field): a store is a map of networks, not one authored
// object, so there is no natural field to carry a version on. core/serialization/SaveFile.h
// writes this as a leading field around the encoded store instead -- see
// KnowledgeSerialization.h's header comment.
inline constexpr int kKnowledgeSchemaVersion = 1;

// The three owners features.md 2.5 names. A KnowledgeNetwork's contents mean the same thing
// regardless of which kind holds it; this only records which kind for save/UI purposes.
enum class NetworkOwnerKind : std::uint8_t { Player, Commander, Faction };

// Which of a KnowledgeNetwork's three categories an item belongs to (features.md 2.5's table).
// Grant()/Copy() take this rather than three separate methods, so a save/load round trip and a
// future fourth category do not multiply the KnowledgeStore surface.
enum class NetworkEntryKind : std::uint8_t { UnlockedBlueprint, SavedTemplate, DiscoveredSystem };

// One network's contents. Every entry is a stable id, never a blueprint body (Law 10 -- the
// content library already owns those) and never an entt::entity (Law 2).
struct KnowledgeNetwork {
    NetworkOwnerKind ownerKind = NetworkOwnerKind::Player;

    // Reverse-engineering unlocks -- what this network's holder knows how to manufacture
    // (features.md 2.4). Written by ResearchSystem.
    std::unordered_set<std::string> unlockedBlueprints;

    // Saved Templates (features.md 2.2-2.3) -- ShipBlueprint ids the holder designed or bought.
    // Written by CustomizeMenu's SaveTemplateRequest consumer (architecture.md 12.9) and copied
    // between networks by TemplateMarketSystem on a sale (architecture.md 12.7).
    std::unordered_set<std::string> savedTemplates;

    // System ids the holder has discovered (features.md 8, "shared knowledge").
    std::unordered_set<std::string> discoveredSystems;
};

// Owns every knowledge network in the galaxy, keyed by KnowledgeNetworkId. Mode-agnostic,
// registry-agnostic, render-agnostic -- sr_core links no raylib (Law 8), which is what makes this
// constructible and testable with no window, no registry, and no mode.
class KnowledgeStore {
public:
    // Creates a fresh, empty network of the given owner kind and returns its new stable id.
    KnowledgeNetworkId Create(NetworkOwnerKind ownerKind);

    // Destroys a network. No-op if `id` is unknown -- a network dying with its host
    // (features.md 2.5, "networks die with their host") is a normal event, not a bug to guard
    // against with an assert.
    void Destroy(const KnowledgeNetworkId& id);

    // Returns nullptr if `id` is unknown.
    const KnowledgeNetwork* Get(const KnowledgeNetworkId& id) const;

    // Adds one item to the network's category matching `kind`. No-op if `id` is unknown.
    void Grant(const KnowledgeNetworkId& id, NetworkEntryKind kind, const std::string& itemId);

    // Copies one saved Template from `sourceNetwork` into `destNetwork` -- the mechanism behind
    // architecture.md 12.7's "a sale copies a Template from the seller's network into the
    // buyer's," and why destroying the buyer's stations does not un-teach them the design
    // (features.md 2.5). No-op if either id is unknown or `sourceNetwork` does not hold
    // `templateId`.
    void Copy(const KnowledgeNetworkId& sourceNetwork, const KnowledgeNetworkId& destNetwork,
              const std::string& templateId);

    // Serialization support only (core/serialization/KnowledgeSerialization.cpp) -- raw iteration
    // and id-preserving reconstruction, neither of which a gameplay caller needs. Ordinary callers
    // use Create()/Get()/Grant()/Copy()/Destroy() above.
    const std::unordered_map<KnowledgeNetworkId, KnowledgeNetwork>& All() const;
    void LoadNetwork(const KnowledgeNetworkId& id, KnowledgeNetwork network);
    std::uint64_t NextIdCounter() const;
    void SetNextIdCounter(std::uint64_t next);

private:
    std::unordered_map<KnowledgeNetworkId, KnowledgeNetwork> networks_;
    std::uint64_t nextId_ = 1;
};

// A faction's general network is keyed by its own FactionId string rather than a Create()'d
// counter id -- StationFactory.cpp's placeholder emplace originated this rule (a faction's
// stations share one general network, and factories have no KnowledgeStore reachable to Create()
// against); DiscoverySystem and NavigationMap need the identical derivation to land writes and
// reads on the same network, so it lives here once rather than three times.
KnowledgeNetworkId FactionNetworkId(const FactionId& faction);

}  // namespace sr::core::knowledge
