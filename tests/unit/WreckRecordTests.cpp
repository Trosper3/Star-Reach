#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/WreckRecord.h"

using sr::ModuleId;
using sr::Vec2;
using sr::core::galaxy::WreckLedger;
using sr::core::galaxy::WreckRecord;

TEST_CASE("WreckLedger returns a stored record by its id", "[wreck]") {
    WreckLedger ledger;
    WreckRecord record;
    record.systemId = "sol";
    record.position = Vec2{10.0f, 5.0f};
    record.modules.push_back(ModuleId("pulse_cannon_i"));
    record.lifetimeSeconds = 60.0f;

    const WreckLedger::Id id = ledger.Add(record);

    const WreckRecord* found = ledger.Find(id);
    REQUIRE(found != nullptr);
    CHECK(found->systemId == "sol");
    CHECK(found->position == Vec2{10.0f, 5.0f});
    REQUIRE(found->modules.size() == 1);
    CHECK(found->modules.front() == ModuleId("pulse_cannon_i"));
}

TEST_CASE("WreckLedger returns nullptr for an unknown id", "[wreck]") {
    WreckLedger ledger;
    CHECK(ledger.Find(999) == nullptr);
}

TEST_CASE("WreckLedger.Remove clears a stored record", "[wreck]") {
    WreckLedger ledger;
    const WreckLedger::Id id = ledger.Add(WreckRecord{});

    ledger.Remove(id);

    CHECK(ledger.Find(id) == nullptr);
    CHECK(ledger.Count() == 0);
}

TEST_CASE("WreckLedger.Remove on an unknown id is a no-op", "[wreck]") {
    WreckLedger ledger;
    const WreckLedger::Id id = ledger.Add(WreckRecord{});

    ledger.Remove(id + 1);

    CHECK(ledger.Count() == 1);
}

TEST_CASE("WreckLedger keeps records independent by id", "[wreck]") {
    WreckLedger ledger;
    WreckRecord sol;
    sol.systemId = "sol";
    WreckRecord kepler;
    kepler.systemId = "kepler";

    const WreckLedger::Id solId = ledger.Add(sol);
    const WreckLedger::Id keplerId = ledger.Add(kepler);

    REQUIRE(solId != keplerId);
    CHECK(ledger.Find(solId)->systemId == "sol");
    CHECK(ledger.Find(keplerId)->systemId == "kepler");
}

TEST_CASE("WreckLedger.Tick ages every stored record and expires the ones that reach zero",
          "[wreck]") {
    WreckLedger ledger;
    WreckRecord record;
    record.lifetimeSeconds = 5.0f;
    const WreckLedger::Id id = ledger.Add(record);

    ledger.Tick(3.0f);
    REQUIRE(ledger.Find(id) != nullptr);
    CHECK(ledger.Find(id)->lifetimeSeconds < 5.0f);

    ledger.Tick(3.0f);
    CHECK(ledger.Find(id) == nullptr);
}
