#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/ResearchRecord.h"

using sr::KnowledgeNetworkId;
using sr::ModuleId;
using sr::core::galaxy::ResearchLedger;
using sr::core::galaxy::ResearchRecord;

TEST_CASE("ResearchLedger returns a stored record by its id", "[research]") {
    ResearchLedger ledger;
    ResearchRecord record;
    record.systemId = "sol";
    record.item = ModuleId("pulse_cannon_i");
    record.progress = 4.0f;
    record.durationSeconds = 10.0f;
    record.targetNetwork = KnowledgeNetworkId("player-network");

    const ResearchLedger::Id id = ledger.Add(record);

    const ResearchRecord* found = ledger.Find(id);
    REQUIRE(found != nullptr);
    CHECK(found->systemId == "sol");
    CHECK(found->item == ModuleId("pulse_cannon_i"));
    CHECK(found->progress == 4.0f);
    CHECK(found->durationSeconds == 10.0f);
    CHECK(found->targetNetwork == KnowledgeNetworkId("player-network"));
}

TEST_CASE("ResearchLedger returns nullptr for an unknown id", "[research]") {
    ResearchLedger ledger;
    CHECK(ledger.Find(999) == nullptr);
}

TEST_CASE("ResearchLedger.Remove clears a stored record", "[research]") {
    ResearchLedger ledger;
    const ResearchLedger::Id id = ledger.Add(ResearchRecord{});

    ledger.Remove(id);

    CHECK(ledger.Find(id) == nullptr);
    CHECK(ledger.Count() == 0);
}

TEST_CASE("ResearchLedger.Remove on an unknown id is a no-op", "[research]") {
    ResearchLedger ledger;
    const ResearchLedger::Id id = ledger.Add(ResearchRecord{});

    ledger.Remove(id + 1);

    CHECK(ledger.Count() == 1);
}

TEST_CASE("ResearchLedger.IdsForSystem finds only records for that system", "[research]") {
    ResearchLedger ledger;
    ResearchRecord sol;
    sol.systemId = "sol";
    ResearchRecord kepler;
    kepler.systemId = "kepler";

    const ResearchLedger::Id solId = ledger.Add(sol);
    const ResearchLedger::Id keplerId = ledger.Add(kepler);

    const std::vector<ResearchLedger::Id> solIds = ledger.IdsForSystem("sol");
    REQUIRE(solIds.size() == 1);
    CHECK(solIds.front() == solId);

    const std::vector<ResearchLedger::Id> keplerIds = ledger.IdsForSystem("kepler");
    REQUIRE(keplerIds.size() == 1);
    CHECK(keplerIds.front() == keplerId);
}
