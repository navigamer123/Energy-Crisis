#ifndef UI_EASE_H
#define UI_EASE_H

// =============================================================================
// [b-effects] Easing and smoothing helpers for UI animation ("juice").
// Header-only, no SFML dependency, so headless tests can include it.
// All easing functions take t in [0, 1] (clamped) and return the eased value.
// =============================================================================

#include <cmath>

namespace Ease {

inline float clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float easeOutCubic(float t) {
    t = clamp01(t);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

inline float easeInCubic(float t) {
    t = clamp01(t);
    return t * t * t;
}

inline float easeInOutSine(float t) {
    t = clamp01(t);
    return 0.5f - 0.5f * std::cos(t * 3.14159265f);
}

// Overshoots past 1 and settles back (classic "pop-in"); overshoot 1.70158 = ~10%
inline float easeOutBack(float t, float overshoot = 1.70158f) {
    t = clamp01(t);
    float c3 = overshoot + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + overshoot * u * u;
}

// Decaying spring wobble that starts at 0 and ends at 1
inline float easeOutElastic(float t) {
    t = clamp01(t);
    if (t <= 0.0f || t >= 1.0f) return t;
    const float c4 = (2.0f * 3.14159265f) / 3.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

// Critically damped spring towards target (Unity-style SmoothDamp). velocity is in/out state.
// smoothTime ~ time to reach the target; never overshoots.
inline float smoothDamp(float current, float target, float& velocity, float smoothTime, float dt) {
    if (smoothTime < 0.0001f) smoothTime = 0.0001f;
    float omega = 2.0f / smoothTime;
    float x = omega * dt;
    float expo = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    float change = current - target;
    float temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * expo;
    float result = target + (change + temp) * expo;
    // Prevent overshoot
    if ((target - current > 0.0f) == (result > target)) {
        result = target;
        velocity = 0.0f;
    }
    return result;
}

// Frame-rate independent exponential approach (rate = 1/seconds)
inline float approach(float current, float target, float rate, float dt) {
    return target + (current - target) * std::exp(-rate * dt);
}

} // namespace Ease

#endif // UI_EASE_H
