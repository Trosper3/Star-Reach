#pragma once

#include <cmath>

namespace sr {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = kPi * 2.0f;

constexpr float DegToRad(float degrees) {
    return degrees * (kPi / 180.0f);
}
constexpr float RadToDeg(float radians) {
    return radians * (180.0f / kPi);
}

// Wraps to (-pi, pi]. Every angular difference in steering, turret traverse, and firing-arc
// checks must go through this -- comparing raw accumulated headings is the classic source of
// a turret that spins the long way around.
inline float WrapAngle(float radians) {
    radians = std::fmod(radians + kPi, kTwoPi);
    if (radians < 0.0f) {
        radians += kTwoPi;
    }
    return radians - kPi;
}

// Shortest signed rotation from `from` to `to`.
inline float AngleDelta(float from, float to) {
    return WrapAngle(to - from);
}

// Rotates `from` toward `to` by at most `maxStep` radians.
inline float RotateToward(float from, float to, float maxStep) {
    const float delta = AngleDelta(from, to);
    if (std::abs(delta) <= maxStep) {
        return WrapAngle(to);
    }
    return WrapAngle(from + (delta > 0.0f ? maxStep : -maxStep));
}

}  // namespace sr
