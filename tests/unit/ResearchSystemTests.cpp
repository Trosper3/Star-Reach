#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/ResearchRecord.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/ResearchSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/NetworkOwner.h"
#include "shared/components/Research.h"
#include "shared/components/Rig.h"

using sr::Destroyed;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::KnowledgeNetworkId;
using sr::ModuleId;
using sr::MountId;
using sr::MountRef;
using sr::NetworkOwner;
using sr::ParentRig;
using sr::ResearchJob;
using sr::Rig;
using sr::StartResearchRequest;
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

// The stable mount id every test's lab hardpoint carries -- ResearchJob::facility must name a
// real MountRef for rig_factory::FindHardpoint to resolve the job back to its bench (architecture
// .md 12.30.8's amendment: a job cannot name its own bench without one).
const MountId kLab("lab");

// Attaches a living FacilityKind::Research hardpoint to `station`, the gate ResearchSystem::Tick
// now requires before it will advance any of that station's jobs.
entt::entity GiveResearchFacility(entt::registry& registry, entt::entity station,
                                  int grade = 1, int capacity = 0) {
    const entt::entity hardpoint = registry.create();
    registry.emplace<ParentRig>(hardpoint, station);
    registry.emplace<FacilityRef>(hardpoint, FacilityKind::Research, grade, capacity);
    registry.emplace<MountRef>(hardpoint, kLab);
    registry.emplace<Rig>(station, std::vector<entt::entity>{hardpoint});
    return hardpoint;
}

// A docked requester (StartResearchRequest's target) carrying Docked + NetworkOwner, the two
// components ProcessStartRequests needs to resolve "which station" and "the actor's network."
entt::entity MakeRequester(entt::registry& registry, entt::entity station,
                           const KnowledgeNetworkId& network) {
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    registry.emplace<NetworkOwner>(requester, network);
    return requester;
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
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 10.0f, KnowledgeNetworkId("net"), kLab});
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
    facility.researchJobs.push_back(ResearchJob{ModuleId("pulse_cannon_i"), 9.0f, 10.0f, network, kLab});
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
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 10.0f, KnowledgeNetworkId("net"), kLab});
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
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 10.0f, KnowledgeNetworkId("net"), kLab});
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
        ResearchJob{ModuleId("pulse_cannon_i"), 9.0f, 10.0f, KnowledgeNetworkId("net"), kLab});
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
    facility.researchJobs.push_back(ResearchJob{ModuleId("pulse_cannon_i"), 9.0f, 10.0f, network, kLab});
    facility.researchJobs.push_back(ResearchJob{ModuleId("shield_mk1"), 0.0f, 10.0f, network, kLab});
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
        ResearchJob{ModuleId("pulse_cannon_i"), 4.0f, 10.0f, KnowledgeNetworkId("net"), kLab});
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

TEST_CASE("DurationSeconds derives from facility grade against features.md 2.4's settled table",
          "[research-system]") {
    CHECK(research_system::DurationSeconds(1) == 60.0f);   // Common: 100%.
    CHECK(research_system::DurationSeconds(7) == 18.0f);   // Mythic: 30% of 60s.
    CHECK(research_system::DurationSeconds(0) == 60.0f);   // Clamped up to grade 1.
    CHECK(research_system::DurationSeconds(99) == 18.0f);  // Clamped down to grade 7.
}

TEST_CASE("StartResearchRequest at a living lab grants exactly one unlock into the actor's "
          "network",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);

    const entt::entity station = registry.create();
    GiveResearchFacility(registry, station);
    const entt::entity requester = MakeRequester(registry, station, network);
    registry.emplace<StartResearchRequest>(requester,
                                           StartResearchRequest{ModuleId("pulse_cannon_i"), kLab});

    // One tick to consume the request into a job, then enough dt to complete it (grade 1 = 60s).
    research_system::Tick(MakeContext(world, intents, content, &knowledge, 1.0f));
    REQUIRE_FALSE(registry.all_of<StartResearchRequest>(requester));
    REQUIRE(registry.get<StationFacility>(station).researchJobs.size() == 1);

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 60.0f));
    CHECK(registry.get<StationFacility>(station).researchJobs.empty());
    CHECK(knowledge.Get(network)->unlockedBlueprints.count("pulse_cannon_i") == 1);
}

TEST_CASE("StartResearchRequest for an item the network already knows queues nothing",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);
    knowledge.Grant(network, sr::core::knowledge::NetworkEntryKind::UnlockedBlueprint,
                    "pulse_cannon_i");

    const entt::entity station = registry.create();
    GiveResearchFacility(registry, station);
    const entt::entity requester = MakeRequester(registry, station, network);
    registry.emplace<StartResearchRequest>(requester,
                                           StartResearchRequest{ModuleId("pulse_cannon_i"), kLab});

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 1.0f));

    REQUIRE_FALSE(registry.all_of<StartResearchRequest>(requester));
    CHECK_FALSE(registry.all_of<StationFacility>(station));  // Never even created a queue.
}

TEST_CASE("Concurrent research jobs cap at FacilityRef::capacity", "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);

    const entt::entity station = registry.create();
    GiveResearchFacility(registry, station, /*grade=*/1, /*capacity=*/1);
    const entt::entity requester = MakeRequester(registry, station, network);

    registry.emplace<StartResearchRequest>(requester,
                                           StartResearchRequest{ModuleId("pulse_cannon_i"), kLab});
    research_system::Tick(MakeContext(world, intents, content, &knowledge, 0.0f));
    REQUIRE(registry.get<StationFacility>(station).researchJobs.size() == 1);

    registry.emplace<StartResearchRequest>(requester,
                                           StartResearchRequest{ModuleId("shield_mk1"), kLab});
    research_system::Tick(MakeContext(world, intents, content, &knowledge, 0.0f));

    // The second request is refused -- the one slot is already spoken for.
    CHECK(registry.get<StationFacility>(station).researchJobs.size() == 1);
    CHECK(registry.get<StationFacility>(station).researchJobs.front().item ==
         ModuleId("pulse_cannon_i"));
}

TEST_CASE("A second StartResearchRequest for an item already queued at this station is refused",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);

    const entt::entity station = registry.create();
    GiveResearchFacility(registry, station);
    const entt::entity requester = MakeRequester(registry, station, network);

    registry.emplace<StartResearchRequest>(requester,
                                           StartResearchRequest{ModuleId("pulse_cannon_i"), kLab});
    research_system::Tick(MakeContext(world, intents, content, &knowledge, 0.0f));
    REQUIRE(registry.get<StationFacility>(station).researchJobs.size() == 1);

    registry.emplace<StartResearchRequest>(requester,
                                           StartResearchRequest{ModuleId("pulse_cannon_i"), kLab});
    research_system::Tick(MakeContext(world, intents, content, &knowledge, 0.0f));

    CHECK(registry.get<StationFacility>(station).researchJobs.size() == 1);
}

TEST_CASE("A job freezes when its own hardpoint is destroyed even if a sibling lab survives",
          "[research-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;

    const entt::entity station = registry.create();
    const entt::entity destroyedLab = GiveResearchFacility(registry, station);
    registry.emplace<Destroyed>(destroyedLab);
    const MountId otherLab("other-lab");
    const entt::entity livingLab = registry.create();
    registry.emplace<ParentRig>(livingLab, station);
    registry.emplace<FacilityRef>(livingLab, FacilityKind::Research);
    registry.emplace<MountRef>(livingLab, otherLab);
    registry.get<Rig>(station).children.push_back(livingLab);

    StationFacility facility;
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 10.0f, KnowledgeNetworkId("net"), kLab});
    facility.researchJobs.push_back(
        ResearchJob{ModuleId("shield_mk1"), 0.0f, 10.0f, KnowledgeNetworkId("net"), otherLab});
    registry.emplace<StationFacility>(station, facility);

    research_system::Tick(MakeContext(world, intents, content, &knowledge, 2.0f));

    const StationFacility& after = registry.get<StationFacility>(station);
    REQUIRE(after.researchJobs.size() == 2);
    CHECK(after.researchJobs[0].progress == 0.0f);  // Behind the destroyed lab -- frozen.
    CHECK(after.researchJobs[1].progress == 2.0f);  // Behind the living lab -- advanced.
}
