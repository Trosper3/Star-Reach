#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include <nlohmann/json.hpp>

#include "core/registries/BlueprintJson.h"
#include "core/registries/JsonReader.h"

using Catch::Approx;
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

TEST_CASE("A weapon module's new behavior-modifier fields parse and forward", "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "test_weapon"},
        {"displayName", "Test Weapon"},
        {"kind", "weapon"},
        {"weapon",
         {{"continuous", true},
          {"homingTurnRatePerSecond", 2.5},
          {"chargeToFire", true},
          {"chargeSecondsToFire", 1.2},
          {"burstCount", 3},
          {"burstIntervalSeconds", 0.08},
          {"maxAmmo", 6.0},
          {"consistency", 0.9},
          {"accuracyRadians", 0.3}}},
    };
    LoadReport report;
    const ModuleDef def = Parse(node, report);

    CHECK(report.ok());
    CHECK(def.weapon.continuous);
    CHECK(def.weapon.homingTurnRatePerSecond == Approx(2.5f));
    CHECK(def.weapon.chargeToFire);
    CHECK(def.weapon.chargeSecondsToFire == Approx(1.2f));
    CHECK(def.weapon.burstCount == 3);
    CHECK(def.weapon.burstIntervalSeconds == Approx(0.08f));
    CHECK(def.weapon.maxAmmo == Approx(6.0f));
    CHECK(def.weapon.consistency == Approx(0.9f));
    CHECK(def.weapon.accuracyRadians == Approx(0.3f));
}

TEST_CASE(
    "A weapon module authoring none of the new behavior-modifier fields keeps every one at its "
    "pre-existing default",
    "[blueprint-json]") {
    const nlohmann::json node = {
        {"id", "pulse_cannon_i"},
        {"displayName", "Pulse Cannon I"},
        {"kind", "weapon"},
    };
    LoadReport report;
    const ModuleDef def = Parse(node, report);

    CHECK(report.ok());
    CHECK_FALSE(def.weapon.continuous);
    CHECK_FALSE(def.weapon.chargeToFire);
    CHECK(def.weapon.burstCount == 1);
    CHECK(def.weapon.maxAmmo == Approx(-1.0f));
    CHECK(def.weapon.consistency == Approx(1.0f));
    CHECK_FALSE(def.weapon.colorOverride.has_value());
}

TEST_CASE("A weapon's colorOverride hex string parses both the RGB and RGBA forms",
          "[blueprint-json]") {
    const nlohmann::json rgbNode = {
        {"id", "test_weapon_rgb"},
        {"displayName", "Test Weapon RGB"},
        {"kind", "weapon"},
        {"weapon", {{"colorOverride", "ff8800"}}},
    };
    LoadReport rgbReport;
    const ModuleDef rgbDef = Parse(rgbNode, rgbReport);

    CHECK(rgbReport.ok());
    REQUIRE(rgbDef.weapon.colorOverride.has_value());
    CHECK(rgbDef.weapon.colorOverride->r == 0xff);
    CHECK(rgbDef.weapon.colorOverride->g == 0x88);
    CHECK(rgbDef.weapon.colorOverride->b == 0x00);
    CHECK(rgbDef.weapon.colorOverride->a == 0xff);  // Alpha defaults to opaque when omitted.

    const nlohmann::json rgbaNode = {
        {"id", "test_weapon_rgba"},
        {"displayName", "Test Weapon RGBA"},
        {"kind", "weapon"},
        {"weapon", {{"colorOverride", "112233aa"}}},
    };
    LoadReport rgbaReport;
    const ModuleDef rgbaDef = Parse(rgbaNode, rgbaReport);

    REQUIRE(rgbaDef.weapon.colorOverride.has_value());
    CHECK(rgbaDef.weapon.colorOverride->r == 0x11);
    CHECK(rgbaDef.weapon.colorOverride->g == 0x22);
    CHECK(rgbaDef.weapon.colorOverride->b == 0x33);
    CHECK(rgbaDef.weapon.colorOverride->a == 0xaa);
}

TEST_CASE("A weapon's malformed colorOverride hex string leaves it nullopt without failing load",
          "[blueprint-json]") {
    // Cosmetic field, fails open (ParseWeaponStats' own comment) -- a typo must not take down
    // content load over a color.
    const nlohmann::json node = {
        {"id", "test_weapon_bad_color"},
        {"displayName", "Test Weapon Bad Color"},
        {"kind", "weapon"},
        {"weapon", {{"colorOverride", "not-a-color"}}},
    };
    LoadReport report;
    const ModuleDef def = Parse(node, report);

    CHECK(report.ok());
    CHECK_FALSE(def.weapon.colorOverride.has_value());
}
