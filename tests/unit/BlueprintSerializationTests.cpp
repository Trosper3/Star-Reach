#include <catch2/catch_test_macros.hpp>

#include "core/serialization/BlueprintSerialization.h"

using sr::core::serialization::ByteReader;
using sr::core::serialization::ByteWriter;

namespace {

// tests/ is one of the three places allowed to construct content types in C++ (Law 10), which is
// exactly what a serialization round-trip needs: a blueprint with no data file behind it.
sr::ShipBlueprint MakeTwoMountShip() {
    sr::ShipBlueprint bp;
    bp.id = sr::BlueprintId("aegis_vanguard");
    bp.schemaVersion = 1;
    bp.displayName = "Aegis Vanguard";
    bp.faction = sr::FactionId("aegis");
    bp.mobile = true;
    bp.structuralMassLimit = 250.0f;

    sr::MountBlueprint root;
    root.id = sr::MountId("root");
    root.shell = sr::ShellId("chassis");
    root.localOffset = {0.0f, 0.0f};
    root.modules.push_back(sr::ModuleId("cell"));
    bp.rig.mounts.push_back(root);

    sr::MountBlueprint weapon;
    weapon.id = sr::MountId("weapon_fwd");
    weapon.shell = sr::ShellId("gun");
    weapon.attachedTo = sr::MountId("root");
    weapon.localOffset = {12.0f, -3.0f};
    weapon.localRotation = 0.25f;
    weapon.traverseRadians = 1.2f;
    weapon.modules.push_back(sr::ModuleId("cannon"));
    weapon.modules.push_back(sr::ModuleId("plate"));
    bp.rig.mounts.push_back(weapon);

    return bp;
}

}  // namespace

TEST_CASE("ShipBlueprint round-trips through Encode/Decode field for field", "[serialization]") {
    const sr::ShipBlueprint original = MakeTwoMountShip();

    ByteWriter writer;
    sr::core::serialization::Encode(writer, original);

    ByteReader reader(writer.Data());
    sr::ShipBlueprint decoded;
    REQUIRE(sr::core::serialization::Decode(reader, decoded));
    CHECK(reader.Ok());
    CHECK(reader.Remaining() == 0);

    CHECK(decoded.id.str() == original.id.str());
    CHECK(decoded.schemaVersion == original.schemaVersion);
    CHECK(decoded.displayName == original.displayName);
    CHECK(decoded.faction.str() == original.faction.str());
    CHECK(decoded.mobile == original.mobile);
    CHECK(decoded.structuralMassLimit == original.structuralMassLimit);

    REQUIRE(decoded.rig.mounts.size() == original.rig.mounts.size());
    for (std::size_t i = 0; i < original.rig.mounts.size(); ++i) {
        const sr::MountBlueprint& a = original.rig.mounts[i];
        const sr::MountBlueprint& b = decoded.rig.mounts[i];
        CHECK(a.id.str() == b.id.str());
        CHECK(a.shell.str() == b.shell.str());
        CHECK(a.attachedTo.str() == b.attachedTo.str());
        CHECK(a.localOffset == b.localOffset);
        CHECK(a.localRotation == b.localRotation);
        CHECK(a.traverseRadians == b.traverseRadians);
        REQUIRE(a.modules.size() == b.modules.size());
        for (std::size_t m = 0; m < a.modules.size(); ++m) {
            CHECK(a.modules[m].str() == b.modules[m].str());
        }
    }
}

TEST_CASE("A stationary rig (mobile=false) round-trips the same way a mobile one does",
          "[serialization]") {
    // The same Encode/Decode pair carries a station's rig as a fighter's -- there is no second
    // format for mobile=false content (Law 4's unified rig, extended to the wire).
    sr::ShipBlueprint station = MakeTwoMountShip();
    station.id = sr::BlueprintId("aegis_outpost");
    station.mobile = false;

    ByteWriter writer;
    sr::core::serialization::Encode(writer, station);

    ByteReader reader(writer.Data());
    sr::ShipBlueprint decoded;
    REQUIRE(sr::core::serialization::Decode(reader, decoded));
    CHECK_FALSE(decoded.mobile);
    CHECK(decoded.id.str() == "aegis_outpost");
}

TEST_CASE("Decode fails cleanly on a buffer truncated mid-rig", "[serialization]") {
    const sr::ShipBlueprint original = MakeTwoMountShip();
    ByteWriter writer;
    sr::core::serialization::Encode(writer, original);

    std::vector<uint8_t> truncated = writer.Data();
    truncated.resize(truncated.size() / 2);

    ByteReader reader(truncated);
    sr::ShipBlueprint decoded;
    CHECK_FALSE(sr::core::serialization::Decode(reader, decoded));
    CHECK_FALSE(reader.Ok());
}
