#ifndef UI_AUDIO_H
#define UI_AUDIO_H

// =============================================================================
// [b-effects] Audio director (F-01).
// Owns the synthesised sound buffers, a 16-voice effect pool (P1 panned left,
// P2 panned right) and the day/night music stems that crossfade with daylight.
// Synthesis runs on a worker thread at startup, so the menu appears at once;
// sounds requested before the buffers are ready are skipped silently.
// Set the environment variable EC_AUDIO=0 to disable audio completely.
// =============================================================================

#include <SFML/Audio.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>
#include "UI_audio_synth.h"

class UI_audio {
public:
    static UI_audio& get();

    enum class Scene {
        Menu,    // Main menu: soft day stem
        Match,   // In a match: crossfade day <-> night with nightAmount
        Paused,  // In a match, paused: music ducked
        Silent   // Fade music out (e.g. victory fanfare playing)
    };

    void init();       // Starts background synthesis (idempotent)
    void shutdown();   // Stops all sounds and frees SFML audio objects (idempotent; call before exit)
    bool isReady() const { return sfxBuilt; }

    // Effects. player 1 = left (pan -0.6), 2 = right (+0.6), 0 = centre
    void play(AudioSynth::Sfx id, int player = 0, float volume = 1.0f, float pitch = 1.0f);
    // Pan from an x position on the 1600-wide virtual canvas
    void playAtX(AudioSynth::Sfx id, float x, float volume = 1.0f, float pitch = 1.0f);

    // Once per frame. nightAmount: 0 = full day, 1 = full night.
    void update(float dt, Scene scene, float nightAmount);

    // Settings (0..100). Master = Settings "Сила на звука"; SFX on/off = "Звукови ефекти".
    void setMasterVolume(int percent);
    void setSfxEnabled(bool on);
    void setSfxVolume(int percent);
    void setMusicVolume(int percent);
    void setMusicEnabled(bool on);
    int getMasterVolume() const { return masterPct; }
    bool isSfxEnabled() const { return sfxOn; }
    int getSfxVolume() const { return sfxPct; }
    int getMusicVolume() const { return musicPct; }
    bool isMusicEnabled() const { return musicOn; }

    UI_audio(const UI_audio&) = delete;
    UI_audio& operator=(const UI_audio&) = delete;

private:
    UI_audio();
    ~UI_audio();

    static constexpr int VOICES = 16;

    bool disabled = false;
    bool started = false;
    std::thread worker;
    std::atomic<bool> cancel{ false };
    std::atomic<bool> sfxSamplesReady{ false };
    std::atomic<bool> musicSamplesReady{ false };

    std::vector<std::vector<std::int16_t>> sfxSamples;
    std::vector<std::int16_t> daySamples;
    std::vector<std::int16_t> nightSamples;

    bool sfxBuilt = false;
    bool musicBuilt = false;
    std::vector<std::unique_ptr<sf::SoundBuffer>> sfxBuffers;
    std::vector<std::unique_ptr<sf::Sound>> voices;
    std::array<float, VOICES> voiceStartTime{};
    std::unique_ptr<sf::SoundBuffer> dayBuffer;
    std::unique_ptr<sf::SoundBuffer> nightBuffer;
    std::unique_ptr<sf::Sound> dayMusic;
    std::unique_ptr<sf::Sound> nightMusic;

    std::array<float, AudioSynth::SFX_COUNT> lastPlayTime{};
    float clock = 0.0f;
    float dayLevel = 0.0f;    // smoothed music gains 0..1
    float nightLevel = 0.0f;
    std::uint32_t rngState = 0x1234567u;

    int masterPct = 80;
    int sfxPct = 100;
    int musicPct = 70;
    bool sfxOn = true;
    bool musicOn = true;

    void buildReadyBuffers();
    float masterGain() const;
    float randomPitchJitter(float amount);
};

#endif // UI_AUDIO_H
