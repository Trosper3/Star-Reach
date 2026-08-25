#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/knowledge/KnowledgeNetwork.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/ui/CodexScreen.h"

using sr::FactionId;
using sr::core::ContentLibrary;
using sr::core::knowledge::KnowledgeNetwork;
using sr::space::ui::codex_screen::CodexState;
using sr::space::ui::codex_screen::DistinctFactions;
using sr::space::ui::codex_screen::DistinctTiers;
using sr::space::ui::codex_screen::Entries;
using sr::space::ui::codex_screen::Entry;
using sr::space::ui::codex_screen::EntryKind;
using sr::space::ui::codex_screen::Filtered;

namespace {

ContentLibrary LoadShippedContent() {
    ContentLibrary library;
    library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    return library;
}

Entry MakeEntry(std::string id, EntryKind kind, std::string displayName, FactionId faction,
                int tier) {
    Entry entry;
    entry.id = std::move(id);
    entry.kind = kind;
    entry.displayName = std::move(displayName);
    entry.faction = std::move(faction);
    entry.tier = tier;
    return entry;
}

}  // namespace

// architecture.md 12.30.6's own Tests bullet: "an item present in NetworkOwner but not
// ContentLibrary is impossible by construction" -- exercised directly rather than asserted in
// shipped code (Entries()'s own header comment). Every id here is a real, shipped id (one per
// ContentLibrary def kind), so a correct Entries() must resolve and tag all three.
TEST_CASE("Entries resolves every unlocked id against ContentLibrary, tagged by kind",
          "[codex-screen]") {
    const ContentLibrary content = LoadShippedContent();

    KnowledgeNetwork network;
    network.unlockedBlueprints.insert("pulse_cannon_i");         // A real ModuleDef id.
    network.unlockedBlueprints.insert("shell_fighter_chassis");  // A real ShellDef id.
    network.unlockedBlueprints.insert("iron");                   // A real ElementDef id.

    const std::vector<Entry> entries = Entries(network, content);
    REQUIRE(entries.size() == 3);

    auto find = [&](const std::string& id) -> const Entry& {
        for (const Entry& entry : entries) {
            if (entry.id == id) {
                return entry;
            }
        }
        FAIL("entry not found: " + id);
        return entries[0];
    };

    CHECK(find("pulse_cannon_i").kind == EntryKind::Module);
    CHECK_FALSE(find("pulse_cannon_i").displayName.empty());
    CHECK(find("shell_fighter_chassis").kind == EntryKind::Shell);
    CHECK(find("iron").kind == EntryKind::Material);
    CHECK(find("iron").displayName == "Iron");
}

TEST_CASE("Entries skips an id absent from ContentLibrary rather than fabricating a row",
          "[codex-screen]") {
    const ContentLibrary content = LoadShippedContent();

    KnowledgeNetwork network;
    network.unlockedBlueprints.insert("pulse_cannon_i");
    network.unlockedBlueprints.insert("no_such_item");  // Cannot happen via ResearchSystem::Grant.

    const std::vector<Entry> entries = Entries(network, content);
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].id == "pulse_cannon_i");
}

TEST_CASE("DistinctFactions/DistinctTiers list each value once, in first-seen/sorted order",
          "[codex-screen]") {
    const std::vector<Entry> entries = {
        MakeEntry("a", EntryKind::Module, "A", FactionId("aegis"), 3),
        MakeEntry("b", EntryKind::Shell, "B", FactionId("aegis"), 1),
        MakeEntry("c", EntryKind::Material, "C", FactionId(), 2),
    };

    const std::vector<FactionId> factions = DistinctFactions(entries);
    REQUIRE(factions.size() == 2);
    CHECK(factions[0] == FactionId("aegis"));
    CHECK(factions[1] == FactionId());

    const std::vector<int> tiers = DistinctTiers(entries);
    REQUIRE(tiers.size() == 3);
    CHECK(tiers[0] == 1);
    CHECK(tiers[1] == 2);
    CHECK(tiers[2] == 3);
}

TEST_CASE("Filtered narrows by search text without touching the underlying entries",
          "[codex-screen]") {
    const std::vector<Entry> entries = {
        MakeEntry("a", EntryKind::Module, "Pulse Cannon", FactionId(), 1),
        MakeEntry("b", EntryKind::Module, "Autocannon", FactionId(), 1),
    };

    CodexState state;
    state.searchQuery = "cannon";
    CHECK(Filtered(entries, state).size() == 2);  // Both match, case-insensitively.

    state.searchQuery = "pulse";
    const std::vector<Entry> filtered = Filtered(entries, state);
    REQUIRE(filtered.size() == 1);
    CHECK(filtered[0].id == "a");
    CHECK(entries.size() == 2);  // Untouched.
}

TEST_CASE("Filtered narrows by faction and tier chip selection", "[codex-screen]") {
    const std::vector<Entry> entries = {
        MakeEntry("a", EntryKind::Module, "A", FactionId("aegis"), 1),
        MakeEntry("b", EntryKind::Module, "B", FactionId("kore"), 1),
        MakeEntry("c", EntryKind::Module, "C", FactionId("aegis"), 2),
    };

    CodexState state;
    state.factionFilter = FactionId("aegis");
    const std::vector<Entry> byFaction = Filtered(entries, state);
    REQUIRE(byFaction.size() == 2);

    state.factionFilter = FactionId();  // Back to "ALL".
    state.tierFilter = 2;
    const std::vector<Entry> byTier = Filtered(entries, state);
    REQUIRE(byTier.size() == 1);
    CHECK(byTier[0].id == "c");
}
