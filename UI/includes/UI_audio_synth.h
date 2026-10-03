#ifndef UI_AUDIO_SYNTH_H
#define UI_AUDIO_SYNTH_H

// =============================================================================
// [b-effects] Procedural audio synthesis (F-01).
// Every sound effect and both music stems are generated from code at startup:
// no audio files ship with the game. Pure C++ (no SFML), so headless tests can
// render and analyse the buffers. UI_audio turns the samples into sf::SoundBuffers.
// =============================================================================

#include <cstdint>
#include <vector>

namespace AudioSynth {

constexpr unsigned SFX_RATE = 44100;    // Short effects: full bandwidth
constexpr unsigned MUSIC_RATE = 22050;  // Long music loops: half rate keeps startup fast
constexpr float MUSIC_BPM = 96.0f;
constexpr int MUSIC_BARS = 16;          // 16 bars of 4/4 = 40 s loop at 96 BPM

enum class Sfx : int {
    MineWood = 0,
    MineIron,
    MineCopper,
    MineCoal,
    MineSilicon,
    MineSilver,
    MineGold,
    BuildOk,
    BuildDenied,
    LandBuy,
    Upgrade,
    Demolish,
    Thunder,
    Destroyed,
    SettleWon,
    SettleLost,
    Victory,
    UiClick,
    UiConfirm,
    Sunrise,
    Nightfall,
    Count
};

enum class Music : int {
    Day = 0,   // Bright plucked arpeggios, bass and light drums
    Night,     // Same chords and tempo: pads, soft bells and echo (crossfades seamlessly with Day)
    Count
};

constexpr int SFX_COUNT = static_cast<int>(Sfx::Count);

const char* sfxName(Sfx id);

// Mono 16-bit samples. Deterministic: the same id always yields the same samples.
std::vector<std::int16_t> renderSfx(Sfx id, unsigned sampleRate = SFX_RATE);

// Mono 16-bit seamless loop of exactly musicLoopSamples(sampleRate) samples.
std::vector<std::int16_t> renderMusic(Music stem, unsigned sampleRate = MUSIC_RATE);

float musicLoopSeconds();
std::size_t musicLoopSamples(unsigned sampleRate);

// Analysis helpers (tests and tools): 0..1 relative to full scale
float peakLevel(const std::vector<std::int16_t>& samples);
float rmsLevel(const std::vector<std::int16_t>& samples);

} // namespace AudioSynth

#endif // UI_AUDIO_SYNTH_H
