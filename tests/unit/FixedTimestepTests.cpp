#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/time/FixedTimestep.h"

using Catch::Approx;
using sr::core::FixedTimestep;
using sr::core::kFixedDeltaSeconds;
using sr::core::kMaxCatchUpSteps;

namespace {

int DrainSteps(FixedTimestep& clock) {
    int steps = 0;
    while (clock.ConsumeStep()) {
        ++steps;
    }
    return steps;
}

}  // namespace

TEST_CASE("A frame shorter than one tick produces no steps", "[time]") {
    FixedTimestep clock;
    clock.Advance(kFixedDeltaSeconds * 0.5f);
    CHECK(DrainSteps(clock) == 0);
}

TEST_CASE("Leftover time carries into the next frame", "[time]") {
    FixedTimestep clock;
    clock.Advance(kFixedDeltaSeconds * 0.6f);
    CHECK(DrainSteps(clock) == 0);
    clock.Advance(kFixedDeltaSeconds * 0.6f);
    CHECK(DrainSteps(clock) == 1);
}

TEST_CASE("A slow frame catches up with multiple steps", "[time]") {
    FixedTimestep clock;
    clock.Advance(kFixedDeltaSeconds * 3.0f);
    CHECK(DrainSteps(clock) == 3);
}

TEST_CASE("Catch-up is capped so a stall cannot spiral", "[time]") {
    // Without the ceiling, a long stall produces a burst of hundreds of ticks, which stalls
    // again, and the simulation never recovers. Dropping time is the correct trade here.
    FixedTimestep clock;
    clock.Advance(10.0f);
    CHECK(DrainSteps(clock) == kMaxCatchUpSteps);
}

TEST_CASE("Alpha reports the fraction toward the next tick", "[time]") {
    FixedTimestep clock;
    clock.Advance(kFixedDeltaSeconds * 1.25f);
    DrainSteps(clock);
    CHECK(clock.Alpha() == Approx(0.25f).margin(0.001f));
}

TEST_CASE("Elapsed ticks count consumed steps only", "[time]") {
    FixedTimestep clock;
    clock.Advance(kFixedDeltaSeconds * 2.9f);
    DrainSteps(clock);
    CHECK(clock.ElapsedTicks() == 2);
}
