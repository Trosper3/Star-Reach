#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/ResearchRecord.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/ResearchSystem.h"
#include "shared/components/Research.h"

using sr::KnowledgeNetworkId;
using sr::ModuleId;
using sr::ResearchJob;
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

}  // namespace

TEST_CASE("ResearchSystem advances a job's progress by dt scaled by facility tier",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;

    const entt::entity station = registry.create();
    StationFacility facility;
    facility.researchTier = 2.0f;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 10.0f, KnowledgeNetworkId("net")});
    registry.emplace<StationFacility>(station, facility);

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 1.0f));

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

TEST_CASE("ResearchSystem does nothing when the context has no knowledge pointer",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = registry.create();
    StationFacility facility;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 9.0f, 10.0f, KnowledgeNetworkId("net")});
    registry.emplace<StationFacility>(station, facility);

    const SystemContext ctx{world, intents, content, 2.0f, 0};  // knowledge defaults null.
    research_system::Tick(ctx);                                 // Must not crash.

    CHECK(registry.get<StationFacility>(station).researchJobs.empty());
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
    facility.researchTier = 2.0f;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 4.0f, 10.0f, KnowledgeNetworkId("net")});
    registry.emplace<StationFacility>(station, facility);

    const std::vector<ResearchRecord> records =
        research_system::CollapseResearchJobs(registry, station, "sol");

    REQUIRE(records.size() == 1);
    CHECK(records.front().systemId == "sol");
    CHECK(records.front().progress == 4.0f);
    CHECK(registry.get<StationFacility>(station).researchJobs.empty());

    // The system was demoted for 3 seconds; promotion must bank that elapsed time at the
    // station's tier rather than resuming frozen at 4.0.
    research_system::PromoteResearchJobs(registry, station, records, 3.0f);

    const StationFacility& promoted = registry.get<StationFacility>(station);
    REQUIRE(promoted.researchJobs.size() == 1);
    CHECK(promoted.researchJobs.front().progress == 10.0f);  // min(cost, 4.0 + 3.0 * 2.0)
    CHECK(promoted.researchJobs.front().item == ModuleId("pulse_cannon_i"));
}
