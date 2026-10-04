// [b-effects] Headless test for the procedural audio synthesiser (F-01).
// Builds alone (the synth has no SFML dependency): it includes the implementation directly so the
// Makefile `make test` rule (engine objects only) links it too.
// Optional: EC_WAV_DIR=<dir> writes every buffer as a .wav for listening/inspection.
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../UI/scr/UI_audio_synth.cpp"

using namespace AudioSynth;

static int failures = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

static void writeWav(const std::string& path, const std::vector<std::int16_t>& s, unsigned rate) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::uint32_t dataBytes = static_cast<std::uint32_t>(s.size() * 2);
    std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(dataBytes);
    if (!s.empty()) std::fwrite(s.data(), 2, s.size(), f);
    std::fclose(f);
}

int main() {
    const char* wavDir = std::getenv("EC_WAV_DIR");
    auto t0 = std::chrono::steady_clock::now();

    std::printf("[test_audio_synth] %d sound effects\n", SFX_COUNT);
    for (int i = 0; i < SFX_COUNT; ++i) {
        Sfx id = static_cast<Sfx>(i);
        std::vector<std::int16_t> s = renderSfx(id);
        float secs = static_cast<float>(s.size()) / static_cast<float>(SFX_RATE);
        float peak = peakLevel(s);
        float rms = rmsLevel(s);
        std::printf("  %-13s %5.2f s  peak %.2f  rms %.3f\n", sfxName(id), secs, peak, rms);
        CHECK(!s.empty(), "sfx renders samples");
        CHECK(secs > 0.02f && secs < 3.0f, "sfx length between 20 ms and 3 s");
        CHECK(peak > 0.3f && peak <= 0.95f, "sfx peak normalised with headroom");
        CHECK(rms > 0.005f, "sfx is audible");
        // Click-free tail: the last 2 ms are near silence
        std::size_t tail = SFX_RATE / 500;
        int tailMax = 0;
        for (std::size_t k = s.size() - std::min(tail, s.size()); k < s.size(); ++k) tailMax = std::max(tailMax, std::abs(static_cast<int>(s[k])));
        CHECK(tailMax < 32767 / 20, "sfx ends without a click");
        CHECK(renderSfx(id) == s, "sfx is deterministic");
        if (wavDir) writeWav(std::string(wavDir) + "/" + sfxName(id) + ".wav", s, SFX_RATE);
    }
    CHECK(renderSfx(Sfx::Count).empty(), "invalid id renders nothing");

    auto t1 = std::chrono::steady_clock::now();
    std::vector<std::int16_t> day = renderMusic(Music::Day);
    std::vector<std::int16_t> night = renderMusic(Music::Night);
    auto t2 = std::chrono::steady_clock::now();

    std::size_t N = musicLoopSamples(MUSIC_RATE);
    std::printf("  music loop %.1f s = %u samples; day rms %.3f peak %.2f, night rms %.3f peak %.2f\n",
                musicLoopSeconds(), static_cast<unsigned>(N), rmsLevel(day), peakLevel(day), rmsLevel(night), peakLevel(night));
    CHECK(std::fabs(musicLoopSeconds() - 40.0f) < 0.01f, "16 bars at 96 BPM = 40 s");
    CHECK(day.size() == N && night.size() == N, "both stems have exactly the loop length (crossfade stays in sync)");
    CHECK(rmsLevel(day) > 0.05f && rmsLevel(day) < 0.5f, "day stem level");
    CHECK(rmsLevel(night) > 0.04f && rmsLevel(night) < 0.5f, "night stem level");
    CHECK(day != night, "stems differ");
    // Seamless loop: the jump from the last to the first sample is no bigger than ordinary neighbours
    auto seam = [](const std::vector<std::int16_t>& m) {
        int jump = std::abs(static_cast<int>(m.back()) - static_cast<int>(m.front()));
        long long avg = 0;
        for (std::size_t k = 1; k < m.size(); ++k) avg += std::abs(static_cast<int>(m[k]) - static_cast<int>(m[k - 1]));
        avg /= static_cast<long long>(m.size() - 1);
        return std::make_pair(jump, static_cast<int>(avg));
    };
    auto sd = seam(day), sn = seam(night);
    std::printf("  loop seam jump day %d (avg step %d), night %d (avg step %d)\n", sd.first, sd.second, sn.first, sn.second);
    CHECK(sd.first < std::max(2000, sd.second * 12), "day loop seam is continuous");
    CHECK(sn.first < std::max(2000, sn.second * 12), "night loop seam is continuous");
    if (wavDir) {
        writeWav(std::string(wavDir) + "/music_day.wav", day, MUSIC_RATE);
        writeWav(std::string(wavDir) + "/music_night.wav", night, MUSIC_RATE);
    }

    double sfxMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double musMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
    std::printf("  synthesis time: sfx (x2 for determinism) %.0f ms, music %.0f ms\n", sfxMs, musMs);

    if (failures) {
        std::printf("[test_audio_synth] FAILED (%d)\n", failures);
        return 1;
    }
    std::printf("[test_audio_synth] PASS\n");
    return 0;
}
