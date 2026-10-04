// [b-effects] Headless test for the easing helpers used by the juice pack (UI/includes/UI_ease.h).
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "../UI/includes/UI_ease.h"


static int failures = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    std::printf("[test_ui_ease]\n");

    // Endpoints and clamping
    CHECK(near(Ease::easeOutCubic(0.0f), 0.0f) && near(Ease::easeOutCubic(1.0f), 1.0f), "easeOutCubic endpoints");
    CHECK(near(Ease::easeInCubic(0.0f), 0.0f) && near(Ease::easeInCubic(1.0f), 1.0f), "easeInCubic endpoints");
    CHECK(near(Ease::easeInOutSine(0.5f), 0.5f), "easeInOutSine midpoint");
    CHECK(near(Ease::easeOutBack(0.0f), 0.0f) && near(Ease::easeOutBack(1.0f), 1.0f), "easeOutBack endpoints");
    CHECK(near(Ease::easeOutCubic(-3.0f), 0.0f) && near(Ease::easeOutCubic(7.0f), 1.0f), "inputs are clamped");
    CHECK(near(Ease::easeOutElastic(1.0f), 1.0f) && near(Ease::easeOutElastic(0.0f), 0.0f), "elastic endpoints");

    // Pop-in overshoots (that is the point) but by a bounded amount
    float peak = 0.0f;
    for (int i = 0; i <= 100; ++i) peak = std::fmax(peak, Ease::easeOutBack(static_cast<float>(i) / 100.0f, 2.4f));
    std::printf("  easeOutBack(2.4) peak %.3f\n", peak);
    CHECK(peak > 1.05f && peak < 1.35f, "pop-in overshoot between 5% and 35%");

    // Monotonic ease-out
    float prev = -1.0f;
    bool mono = true;
    for (int i = 0; i <= 100; ++i) {
        float v = Ease::easeOutCubic(static_cast<float>(i) / 100.0f);
        if (v < prev) mono = false;
        prev = v;
    }
    CHECK(mono, "easeOutCubic is monotonic");

    // smoothDamp converges to the target without overshoot at 60 and 10 fps
    for (float dt : { 1.0f / 60.0f, 0.1f }) {
        float x = 0.5f, v = 0.0f;
        bool over = false;
        for (int i = 0; i < static_cast<int>(3.0f / dt); ++i) {
            x = Ease::smoothDamp(x, 0.8f, v, 0.55f, dt);
            if (x > 0.8f + 1e-5f) over = true;
        }
        CHECK(!over, "smoothDamp never overshoots");
        CHECK(near(x, 0.8f, 0.003f), "smoothDamp reaches the target in 3 s");
    }
    // ... and downwards
    {
        float x = 0.9f, v = 0.0f;
        for (int i = 0; i < 180; ++i) x = Ease::smoothDamp(x, 0.2f, v, 0.55f, 1.0f / 60.0f);
        CHECK(near(x, 0.2f, 0.003f), "smoothDamp works downwards");
    }

    // approach() is frame-rate independent
    float a60 = 0.0f, a10 = 0.0f;
    for (int i = 0; i < 60; ++i) a60 = Ease::approach(a60, 1.0f, 2.0f, 1.0f / 60.0f);
    for (int i = 0; i < 10; ++i) a10 = Ease::approach(a10, 1.0f, 2.0f, 0.1f);
    CHECK(near(a60, a10, 1e-3f), "approach gives the same result at 60 and 10 fps");
    CHECK(near(a60, 1.0f - std::exp(-2.0f), 1e-3f), "approach follows exp decay");

    if (failures) {
        std::printf("[test_ui_ease] FAILED (%d)\n", failures);
        return 1;
    }
    std::printf("[test_ui_ease] PASS\n");
    return 0;
}
