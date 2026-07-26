#pragma once

#include <cmath>

// The simulation's vector type.
//
// This is deliberately NOT raylib's Vector2. sr_shared and sr_core do not link raylib
// (architecture.md Law 8), and every component below this line has to be constructible in a
// headless unit test and in tools/economy_sim. Conversion to Vector2 happens at the render
// boundary, in sr_shared_ui or a mode's render/ directory -- never in a component.
namespace sr {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }

    constexpr Vec2& operator+=(const Vec2& o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    constexpr Vec2& operator-=(const Vec2& o) {
        x -= o.x;
        y -= o.y;
        return *this;
    }
    constexpr Vec2& operator*=(float s) {
        x *= s;
        y *= s;
        return *this;
    }

    constexpr bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
};

constexpr float Dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

// 2D analogue of the cross product: the z component of a x b. Sign gives turn direction.
constexpr float Cross(const Vec2& a, const Vec2& b) {
    return a.x * b.y - a.y * b.x;
}

constexpr float LengthSquared(const Vec2& v) {
    return Dot(v, v);
}

inline float Length(const Vec2& v) {
    return std::sqrt(LengthSquared(v));
}

constexpr float DistanceSquared(const Vec2& a, const Vec2& b) {
    return LengthSquared(b - a);
}

inline float Distance(const Vec2& a, const Vec2& b) {
    return Length(b - a);
}

// Returns the zero vector for a zero-length input rather than producing NaN. Callers doing
// steering or aim math rely on this; a NaN transform silently poisons every downstream system.
inline Vec2 Normalized(const Vec2& v) {
    const float len = Length(v);
    return len > 0.0f ? v / len : Vec2{0.0f, 0.0f};
}

// Radians, counter-clockwise, zero pointing along +x.
inline Vec2 FromAngle(float radians) {
    return {std::cos(radians), std::sin(radians)};
}

inline float ToAngle(const Vec2& v) {
    return std::atan2(v.y, v.x);
}

inline Vec2 Rotated(const Vec2& v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

constexpr Vec2 Lerp(const Vec2& a, const Vec2& b, float t) {
    return a + (b - a) * t;
}

}  // namespace sr
