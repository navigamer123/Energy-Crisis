#pragma once
#include <cstdint>

// -----------------------------------------------------------------------------
// GameRng: small deterministic generator owned by the engine (PCG32, XSH-RR variant).
// The same seed and stream give the same numbers on every compiler and platform (unlike
// std::uniform_int_distribution), and the whole state is two integers, so snapshots can save it.
// -----------------------------------------------------------------------------
class GameRng {
public:
    GameRng() { seed(0u, 0u); }
    GameRng(uint64_t seedValue, uint64_t stream) { seed(seedValue, stream); }

    // Different streams with the same seed give independent sequences (weather, hazards, ...)
    void seed(uint64_t seedValue, uint64_t stream) {
        state_ = 0u;
        inc_ = (stream << 1u) | 1u;
        next();
        state_ += seedValue;
        next();
    }

    uint32_t next() {
        uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
    }

    // Uniform integer in [lo, hi] (inclusive; the bounds may come in either order), without modulo bias
    int range(int lo, int hi) {
        if (lo > hi) {
            int t = lo;
            lo = hi;
            hi = t;
        }
        uint64_t span = static_cast<uint64_t>(static_cast<int64_t>(hi) - static_cast<int64_t>(lo)) + 1u;
        if (span > 0xFFFFFFFFull) {
            return static_cast<int>(static_cast<int64_t>(lo) + static_cast<int64_t>(next())); // full 32-bit span
        }
        uint32_t span32 = static_cast<uint32_t>(span);
        uint32_t threshold = (0u - span32) % span32;
        for (;;) {
            uint32_t r = next();
            if (r >= threshold) {
                return static_cast<int>(static_cast<int64_t>(lo) + static_cast<int64_t>(r % span32));
            }
        }
    }

    // Uniform float in [0, 1)
    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }

    // Raw state for snapshots
    uint64_t getState() const { return state_; }
    uint64_t getIncrement() const { return inc_; }
    void setState(uint64_t state, uint64_t increment) {
        state_ = state;
        inc_ = increment | 1u;
    }

    bool operator==(const GameRng& o) const { return state_ == o.state_ && inc_ == o.inc_; }
    bool operator!=(const GameRng& o) const { return !(*this == o); }

private:
    uint64_t state_ = 0u;
    uint64_t inc_ = 1u;
};

// Legacy shared generator (expeditions and old helpers). GameEngine::init seeds it with the match seed,
// but engine logic uses the engine-owned GameRng streams instead.
int randomInt(int min, int max);

// Re-seeds the shared generator used by randomInt (called once per match by GameEngine::init)
void seedRandom(unsigned int seed);
