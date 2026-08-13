#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/ResearchRecord.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/ResearchSystem.h"
#include "shared/components/Facility.h"
#include "shared/components/Research.h"
#include "shared/components/Rig.h"

using sr::Destroyed;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::KnowledgeNetworkId;
using sr::ModuleId;
using sr::ParentRig;
using sr::ResearchJob;
using sr::Rig;
using sr::StationFacility;
using sr::core::galaxy::ResearchRecord;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkOwnerKind;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace research_system = sr::space::research_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, KnowledgeStore* knowledge,
                          float dt = 1.0f) {
    SystemContext ctx{world, intents, content, dt, 0};
    ctx.knowledge = knowledge;
    return ctx;
}

// Attaches a living FacilityKind::Research hardpoint to `station`, the gate ResearchSystem::Tick
// now requires before it will advance any of that station's jobs.
entt::entity GiveResearchFacility(entt::registry& registry, entt::entity station) {
    const entt::entity hardpoint = registry.create();
    registry.emplace<ParentRig>(hardpoint, station);
    registry.emplace<FacilityRef>(hardpoint, FacilityKind::Research);
    registry.emplace<Rig>(station, std::vector<entt::entity>{hardpoint});
    return hardpoint;
}

}  // namespace

TEST_CASE("ResearchSystem advances a docked lab's job progress by dt", "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;

    const entt::entity station = registry.create();
    GiveResearchFacility(registry, station);
    StationFacility facility;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 10.0f, KnowledgeNetworkId("net")});
    registry.emplace<StationFacility>(station, facility);

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 2.0f));

    REQUIRE(registry.get<StationFacility>(station).researchJobs.size() == 1);
    CHECK(registry.get<StationFacility>(station).researchJobs.front().progress == 2.0f);
}

TEST_CASE("ResearchSystem grants exactly once on completion and clears the job",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);

    const entt::entity station = registry.create();
    GiveResearchFacility(registry, station);
    StationFacility facility;
    facility.researchJobs.push_back(ResearchJob{ModuleId("pulse_cannon_i"), 9.0f, 10.0f, network});
    registry.emplace<StationFacility>(station, facility);

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 2.0f));

    CHECK(registry.get<StationFacility>(station).researchJobs.empty());
    REQUIRE(knowledge.Get(network) != nullptr);
    CHECK(knowledge.Get(network)->unlockedBlueprints.count("pulse_cannon_i") == 1);

    // A second tick with no jobs left must not grant again.
    research_system::Tick(MakeContext(world, intents, content, &knowledge, 2.0f));
    CHECK(knowledge.Get(network)->unlockedBlueprints.size() == 1);
}

TEST_CASE("ResearchSystem does not advance a job at a station with no living Research facility",
          "[research-system]") {
    // architecture.md 13.3/12.30.6: Tick used to view StationFacility with no facility check at
    // all, so blowing the lab off a station never stopped the jobs running inside it.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;

    const entt::entity station = registry.create();  // no Rig, no Research facility at all
    StationFacility facility;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 10.0f, KnowledgeNetworkId("net")});
    registry.emplace<StationFacility>(station, facility);

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 2.0f));

    REQUIRE(registry.get<StationFacility>(station).researchJobs.size() == 1);
    CHECK(registry.get<StationFacility>(station).researchJobs.front().progress == 0.0f);
}

TEST_CASE("ResearchSystem does not advance a job behind a Destroyed Research hardpoint",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;

    const entt::entity station = registry.create();
    const entt::entity hardpoint = GiveResearchFacility(registry, station);
    registry.emplace<Destroyed>(hardpoint);
    StationFacility facility;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 10.0f, KnowledgeNetworkId("net")});
    registry.emplace<StationFacility>(station, facility);

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 2.0f));

    CHECK(registry.get<StationFacility>(station).researchJobs.front().progress == 0.0f);
}

TEST_CASE(
    "ResearchSystem freezes a completed job at durationSeconds when there is no knowledge "
    "pointer, leaving the input intact and the job resumable",
    "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = registry.create();
    GiveResearchFacility(registry, station);
    StationFacility facility;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 9.0f, 10.0f, KnowledgeNetworkId("net")});
    registry.emplace<StationFacility>(station, facility);

    const SystemContext ctx{world, intents, content, 2.0f, 0};  // knowledge defaults null.
    research_system::Tick(ctx);                                 // Must not crash.

    REQUIRE(registry.get<StationFacility>(station).researchJobs.size() == 1);
    const ResearchJob& frozen = registry.get<StationFacility>(station).researchJobs.front();
    CHECK(frozen.progress == 10.0f);
    CHECK(frozen.item == ModuleId("pulse_cannon_i"));

    // Resuming with a real knowledge pointer grants immediately -- the input was never lost.
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);
    registry.get<StationFacility>(station).researchJobs.front().targetNetwork = network;
    research_system::Tick(MakeContext(world, intents, content, &knowledge, 0.0f));

    CHECK(registry.get<StationFacility>(station).researchJobs.empty());
    REQUIRE(knowledge.Get(network) != nullptr);
    CHECK(knowledge.Get(network)->unlockedBlueprints.count("pulse_cannon_i") == 1);
}

TEST_CASE("ResearchSystem advances two concurrent jobs at the same station independently",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);

    const entt::entity station = registry.create();
    GiveResearchFacility(registry, station);
    StationFacility facility;
    facility.researchJobs.push_back(ResearchJob{ModuleId("pulse_cannon_i"), 9.0f, 10.0f, network});
    facility.researchJobs.push_back(ResearchJob{ModuleId("shield_mk1"), 0.0f, 10.0f, network});
    registry.emplace<StationFacility>(station, facility);

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 2.0f));

    const StationFacility& after = registry.get<StationFacility>(station);
    REQUIRE(after.researchJobs.size() == 1);
    CHECK(after.researchJobs.front().item == ModuleId("shield_mk1"));
    CHECK(after.researchJobs.front().progress == 2.0f);
    CHECK(knowledge.Get(network)->unlockedBlueprints.count("pulse_cannon_i") == 1);
}

TEST_CASE("CollapseResearchJobs/PromoteResearchJobs round-trip and resume at caught-up progress",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    const entt::entity station = registry.create();
    StationFacility facility;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 4.0f, 10.0f, KnowledgeNetworkId("net")});
    registry.emplace<StationFacility>(station, facility);

    const std::vector<ResearchRecord> records =
        research_system::CollapseResearchJobs(registry, station, "sol");

    REQUIRE(records.size() == 1);
    CHECK(records.front().systemId == "sol");
    CHECK(records.front().progress == 4.0f);
    CHECK(registry.get<StationFacility>(station).researchJobs.empty());

    // The system was demoted for 3 seconds; promotion must bank that elapsed time at 1x.
    research_system::PromoteResearchJobs(registry, station, records, 3.0f);

    const StationFacility& promoted = registry.get<StationFacility>(station);
    REQUIRE(promoted.researchJobs.size() == 1);
    CHECK(promoted.researchJobs.front().progress == 7.0f);  // min(duration, 4.0 + 3.0)
    CHECK(promoted.researchJobs.front().item == ModuleId("pulse_cannon_i"));
}
