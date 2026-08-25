#include <catch2/catch_test_macros.hpp>

#include <string>

#include <nlohmann/json.hpp>

#include "core/registries/BlueprintJson.h"
#include "core/registries/JsonReader.h"

using sr::FacilityKind;
using sr::FactionId;
using sr::ModuleDef;
using sr::core::JsonReader;
using sr::core::LoadReport;
using sr::core::ParseModuleDef;

namespace {

ModuleDef Parse(const nlohmann::json& node, LoadReport& report) {
    const JsonReader reader(node, "modules.json", "modules[0]", report);
    return ParseModuleDef(reader);
}

}  // namespace

TEST_CASE("A facility module authoring no kind fails to load, naming the file and key",
          "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "bad_facility"},
        {"displayName", "Bad Facility"},
        {"kind", "facility"},
        {"facility", nlohmann::json::object()},
    };
    LoadReport report;
    Parse(node, report);

    REQUIRE_FALSE(report.ok());
    bool foundKindError = false;
    for (const auto& error : report.errors) {
        if (error.file == "modules.json" && error.message.find("kind") != std::string::npos) {
            foundKindError = true;
        }
    }
    CHECK(foundKindError);
}

TEST_CASE("A facility module authoring an unknown kind fails to load", "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "bad_facility"},
        {"displayName", "Bad Facility"},
        {"kind", "facility"},
        {"facility", {{"kind", "not_a_real_kind"}}},
    };
    LoadReport report;
    Parse(node, report);

    CHECK_FALSE(report.ok());
}

TEST_CASE("A module with no facility block at all parses without error", "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "pulse_cannon_i"},
        {"displayName", "Pulse Cannon I"},
        {"kind", "weapon"},
    };
    LoadReport report;
    Parse(node, report);

    CHECK(report.ok());
}

TEST_CASE("A facility module authoring no grade defaults nothing silently -- it stays 1",
          "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "trade_exchange_i"},
        {"displayName", "Trade Exchange I"},
        {"kind", "facility"},
        {"facility", {{"kind", "trade"}}},
    };
    LoadReport report;
    const ModuleDef def = Parse(node, report);

    CHECK(report.ok());
    CHECK(def.facility.kind == FacilityKind::Trade);
    CHECK(def.facility.grade == 1);
}

TEST_CASE("A facility module's authored grade is parsed and forwarded", "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "engineering_bay_i"},
        {"displayName", "Engineering Bay I"},
        {"kind", "facility"},
        {"facility", {{"kind", "engineering"}, {"grade", 4}}},
    };
    LoadReport report;
    const ModuleDef def = Parse(node, report);

    CHECK(report.ok());
    CHECK(def.facility.grade == 4);
}

TEST_CASE("A module authoring no faction/tier defaults to unfactioned tier 1", "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "pulse_cannon_i"},
        {"displayName", "Pulse Cannon I"},
        {"kind", "weapon"},
    };
    LoadReport report;
    const ModuleDef def = Parse(node, report);

    CHECK(report.ok());
    CHECK(def.faction == FactionId());
    CHECK(def.tier == 1);
}

TEST_CASE(
    "A module's authored faction/tier are parsed and forwarded, distinct from "
    "FacilityStats::grade",
    "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "pulse_cannon_ii"},
        {"displayName", "Pulse Cannon II"},
        {"kind", "weapon"},
        {"faction", "aegis"},
        {"tier", 3},
    };
    LoadReport report;
    const ModuleDef def = Parse(node, report);

    CHECK(report.ok());
    CHECK(def.faction == FactionId("aegis"));
    CHECK(def.tier == 3);
}
