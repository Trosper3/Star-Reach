#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"

using namespace sr::core;

TEST_CASE("Intents are drained by type", "[intent]") {
    IntentQueue queue;
    queue.Push(SetThrottleIntent{ActorId{1}, 1.0f, 0.0f, 0.0f});
    queue.Push(FireWeaponsIntent{ActorId{1}});
    queue.Push(SetThrottleIntent{ActorId{2}, -1.0f, 0.0f, 0.5f});

    int throttles = 0;
    float summedForward = 0.0f;
    queue.ForEach<SetThrottleIntent>([&](const SetThrottleIntent& intent) {
        ++throttles;
        summedForward += intent.forward;
    });

    int fires = 0;
    queue.ForEach<FireWeaponsIntent>([&](const FireWeaponsIntent&) { ++fires; });

    CHECK(throttles == 2);
    CHECK(fires == 1);
    CHECK(summedForward == 0.0f);
}

TEST_CASE("Clear empties the queue between ticks", "[intent]") {
    IntentQueue queue;
    queue.Push(FireWeaponsIntent{ActorId{1}});
    REQUIRE(queue.Size() == 1);

    queue.Clear();
    CHECK(queue.Empty());
}

TEST_CASE("Intents carry stable ids, never entity handles", "[intent]") {
    // Law 2, hard rule 1. This test is a tripwire: if someone adds an entt::entity field to an
    // intent, SetTargetIntent stops being constructible from a network id alone and this fails
    // to compile. That is the intended failure mode -- an intent produced on one machine is
    // consumed on another, where that handle means something else entirely.
    SetTargetIntent intent;
    intent.actor = ActorId{7};
    intent.targetNetworkId = 42;
    intent.targetMount = sr::MountId("wing_port");

    CHECK(intent.targetNetworkId == 42);
    CHECK(intent.targetMount.str() == "wing_port");
}
