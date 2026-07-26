#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

using Catch::Approx;

TEST_CASE("Normalized returns zero rather than NaN for a zero vector", "[math]") {
    // The reason this is a test and not a comment: a NaN transform does not crash. It silently
    // poisons every downstream system, and the symptom shows up three systems away from here.
    const sr::Vec2 result = sr::Normalized({0.0f, 0.0f});
    CHECK(result.x == 0.0f);
    CHECK(result.y == 0.0f);
}

TEST_CASE("Normalized produces a unit vector", "[math]") {
    const sr::Vec2 result = sr::Normalized({3.0f, 4.0f});
    CHECK(sr::Length(result) == Approx(1.0f));
    CHECK(result.x == Approx(0.6f));
    CHECK(result.y == Approx(0.8f));
}

TEST_CASE("Angle round-trips through FromAngle and ToAngle", "[math]") {
    const float angle = sr::DegToRad(37.0f);
    CHECK(sr::ToAngle(sr::FromAngle(angle)) == Approx(angle));
}

TEST_CASE("WrapAngle keeps values in (-pi, pi]", "[math]") {
    CHECK(sr::WrapAngle(sr::kTwoPi + 0.5f) == Approx(0.5f));
    CHECK(sr::WrapAngle(-sr::kTwoPi - 0.5f) == Approx(-0.5f));
    CHECK(std::abs(sr::WrapAngle(3.0f * sr::kPi)) == Approx(sr::kPi));
}

TEST_CASE("AngleDelta takes the short way around", "[math]") {
    // A turret at 175 degrees ordered to -175 should traverse 10 degrees, not 350. This is the
    // specific bug the helper exists to prevent.
    const float from = sr::DegToRad(175.0f);
    const float to = sr::DegToRad(-175.0f);
    CHECK(sr::RadToDeg(sr::AngleDelta(from, to)) == Approx(10.0f).margin(0.01f));
}

TEST_CASE("RotateToward clamps to the step and stops exactly on target", "[math]") {
    const float result = sr::RotateToward(0.0f, 1.0f, 0.25f);
    CHECK(result == Approx(0.25f));

    const float arrived = sr::RotateToward(0.9f, 1.0f, 0.5f);
    CHECK(arrived == Approx(1.0f));
}

TEST_CASE("Rotated preserves length", "[math]") {
    const sr::Vec2 v{10.0f, 0.0f};
    const sr::Vec2 spun = sr::Rotated(v, sr::DegToRad(90.0f));
    CHECK(sr::Length(spun) == Approx(10.0f));
    CHECK(spun.y == Approx(10.0f));
}
