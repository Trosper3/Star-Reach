#pragma once

namespace sr::core {

// Gameplay ticks at a fixed 60 Hz; rendering runs free and interpolates
// (architecture.md section 5).
//
// This is not a style preference. Law 2's LOD model banks elapsed macro ticks for unloaded
// systems and fast-forwards them on arrival as a pure function of elapsed time. That round-trip
// only holds if a tick is always the same size -- with a variable timestep, a system simulated
// live and the same system fast-forwarded produce different galaxies.
inline constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;

// Ceiling on catch-up steps in one frame. Without it, a long stall (a breakpoint, a texture
// load) produces a burst of hundreds of ticks that stalls again, and the sim never recovers.
// Time is dropped instead, which is the correct trade for a real-time game.
inline constexpr int kMaxCatchUpSteps = 5;

class FixedTimestep {
public:
    // Feeds real frame time in. Call once per rendered frame.
    void Advance(float realDeltaSeconds) {
        accumulator_ += realDeltaSeconds;
        const float ceiling = kFixedDeltaSeconds * static_cast<float>(kMaxCatchUpSteps);
        if (accumulator_ > ceiling) {
            accumulator_ = ceiling;
        }
    }

    // Drain in a while loop: `while (clock.ConsumeStep()) { TickSystems(); }`
    bool ConsumeStep() {
        if (accumulator_ < kFixedDeltaSeconds) {
            return false;
        }
        accumulator_ -= kFixedDeltaSeconds;
        elapsedTicks_ += 1;
        return true;
    }

    // Fraction of the way to the next tick, in [0, 1). The renderer blends PreviousTransform
    // toward WorldTransform by this.
    float Alpha() const { return accumulator_ / kFixedDeltaSeconds; }

    unsigned long long ElapsedTicks() const { return elapsedTicks_; }

private:
    float accumulator_ = 0.0f;
    unsigned long long elapsedTicks_ = 0;
};

}  // namespace sr::core
