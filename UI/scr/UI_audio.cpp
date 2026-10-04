// =============================================================================
// [b-effects] Audio director (F-01): procedural buffers, voice pool, music crossfade.
// =============================================================================
#include "../includes/UI_audio.h"
#include "../includes/UI_ease.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

using AudioSynth::Sfx;

namespace {

// Relative loudness of each effect after synthesis normalisation
float sfxGain(Sfx id) {
    switch (id) {
        case Sfx::MineWood: case Sfx::MineIron: case Sfx::MineCopper: case Sfx::MineCoal:
        case Sfx::MineSilicon: case Sfx::MineSilver: case Sfx::MineGold: return 0.5f;
        case Sfx::BuildOk: return 0.8f;
        case Sfx::BuildDenied: return 0.45f;
        case Sfx::LandBuy: return 0.75f;
        case Sfx::Upgrade: return 0.75f;
        case Sfx::Demolish: return 0.7f;
        case Sfx::Thunder: return 0.9f;
        case Sfx::Destroyed: return 0.9f;
        case Sfx::SettleWon: return 0.8f;
        case Sfx::SettleLost: return 0.8f;
        case Sfx::Victory: return 1.0f;
        case Sfx::UiClick: return 0.35f;
        case Sfx::UiConfirm: return 0.45f;
        case Sfx::Sunrise: return 0.45f;
        case Sfx::Nightfall: return 0.45f;
        default: return 0.6f;
    }
}

// Minimum seconds between two plays of the same effect (stops machine-gun stacking at 6x speed)
float sfxMinInterval(Sfx id) {
    switch (id) {
        case Sfx::UiClick: return 0.03f;
        case Sfx::BuildDenied: return 0.25f;
        case Sfx::Thunder: return 0.4f;
        case Sfx::Sunrise: case Sfx::Nightfall: return 2.0f;
        default: return 0.06f;
    }
}

const float PLAYER_PAN = 0.6f;
const float MENU_MUSIC_LEVEL = 0.75f;
const float PAUSED_MUSIC_LEVEL = 0.35f;
const float MUSIC_BASE_GAIN = 0.55f; // music sits under the effects
const float MUSIC_FADE_RATE = 1.6f;  // 1/s, about 2 s crossfade

int clampPct(int p) { return std::max(0, std::min(100, p)); }

} // namespace

UI_audio& UI_audio::get() {
    static UI_audio instance;
    return instance;
}

UI_audio::UI_audio() {
    if (const char* env = std::getenv("EC_AUDIO")) {
        if (std::strcmp(env, "0") == 0 || std::strcmp(env, "off") == 0) disabled = true;
    }
    lastPlayTime.fill(-100.0f);
    voiceStartTime.fill(-100.0f);
}

UI_audio::~UI_audio() {
    shutdown();
}

void UI_audio::init() {
    if (started || disabled) return;
    started = true;
    try {
        worker = std::thread([this]() {
            std::vector<std::vector<std::int16_t>> fx(AudioSynth::SFX_COUNT);
            for (int i = 0; i < AudioSynth::SFX_COUNT && !cancel.load(); ++i) {
                fx[static_cast<std::size_t>(i)] = AudioSynth::renderSfx(static_cast<Sfx>(i));
            }
            if (cancel.load()) return;
            sfxSamples.swap(fx);
            sfxSamplesReady.store(true);
            std::vector<std::int16_t> d = AudioSynth::renderMusic(AudioSynth::Music::Day);
            if (cancel.load()) return;
            std::vector<std::int16_t> n = AudioSynth::renderMusic(AudioSynth::Music::Night);
            if (cancel.load()) return;
            daySamples.swap(d);
            nightSamples.swap(n);
            musicSamplesReady.store(true);
        });
        std::cout << "[UI_audio] Synthesising sound effects and music in the background...\n";
    } catch (const std::exception& e) {
        std::cerr << "[UI_audio] Could not start the audio thread (" << e.what() << "); audio disabled.\n";
        disabled = true;
    }
}

void UI_audio::shutdown() {
    cancel.store(true);
    if (worker.joinable()) worker.join();
    if (dayMusic) dayMusic->stop();
    if (nightMusic) nightMusic->stop();
    for (auto& v : voices) {
        if (v) v->stop();
    }
    voices.clear();
    dayMusic.reset();
    nightMusic.reset();
    sfxBuffers.clear();
    dayBuffer.reset();
    nightBuffer.reset();
    sfxBuilt = false;
    musicBuilt = false;
}

void UI_audio::buildReadyBuffers() {
    if (!sfxBuilt && sfxSamplesReady.load()) {
        const std::vector<sf::SoundChannel> mono = { sf::SoundChannel::Mono };
        try {
            for (const auto& s : sfxSamples) {
                sfxBuffers.push_back(std::make_unique<sf::SoundBuffer>(s.data(), s.size(), 1u, AudioSynth::SFX_RATE, mono));
            }
            for (int i = 0; i < VOICES; ++i) {
                auto v = std::make_unique<sf::Sound>(*sfxBuffers.front());
                v->setSpatializationEnabled(false);
                voices.push_back(std::move(v));
            }
            sfxBuilt = true;
            std::cout << "[UI_audio] " << sfxBuffers.size() << " procedural sound effects ready (" << VOICES << " voices).\n";
        } catch (const std::exception& e) {
            std::cerr << "[UI_audio] Sound effects unavailable (" << e.what() << ").\n";
            disabled = true;
        }
        sfxSamples.clear();
        sfxSamples.shrink_to_fit();
    }
    if (!musicBuilt && musicSamplesReady.load() && !disabled) {
        if (worker.joinable()) worker.join();
        const std::vector<sf::SoundChannel> mono = { sf::SoundChannel::Mono };
        try {
            dayBuffer = std::make_unique<sf::SoundBuffer>(daySamples.data(), daySamples.size(), 1u, AudioSynth::MUSIC_RATE, mono);
            nightBuffer = std::make_unique<sf::SoundBuffer>(nightSamples.data(), nightSamples.size(), 1u, AudioSynth::MUSIC_RATE, mono);
            dayMusic = std::make_unique<sf::Sound>(*dayBuffer);
            nightMusic = std::make_unique<sf::Sound>(*nightBuffer);
            for (sf::Sound* m : { dayMusic.get(), nightMusic.get() }) {
                m->setSpatializationEnabled(false);
                m->setLooping(true);
                m->setVolume(0.0f);
            }
            // Start together: both stems share tempo, chords and length, so they stay aligned
            dayMusic->play();
            nightMusic->play();
            musicBuilt = true;
            std::cout << "[UI_audio] Day/night music stems ready (" << AudioSynth::musicLoopSeconds() << " s loops).\n";
        } catch (const std::exception& e) {
            std::cerr << "[UI_audio] Music unavailable (" << e.what() << ").\n";
        }
        daySamples.clear();
        daySamples.shrink_to_fit();
        nightSamples.clear();
        nightSamples.shrink_to_fit();
    }
}

float UI_audio::masterGain() const {
    float m = static_cast<float>(masterPct) / 100.0f;
    return m * m; // perceptual curve: 50% sounds about half as loud
}

float UI_audio::randomPitchJitter(float amount) {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    float r = static_cast<float>(rngState & 0xFFFFu) / 65535.0f; // own RNG: never disturbs the match seed
    return 1.0f + (r * 2.0f - 1.0f) * amount;
}

void UI_audio::play(Sfx id, int player, float volume, float pitch) {
    float pan = (player == 1) ? -PLAYER_PAN : ((player == 2) ? PLAYER_PAN : 0.0f);
    float x = 800.0f + pan * 800.0f;
    playAtX(id, x, volume, pitch);
}

void UI_audio::playAtX(Sfx id, float x, float volume, float pitch) {
    if (disabled || !sfxBuilt || !sfxOn || masterPct <= 0 || sfxPct <= 0) return;
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= AudioSynth::SFX_COUNT || idx >= static_cast<int>(sfxBuffers.size())) return;
    if (clock - lastPlayTime[static_cast<std::size_t>(idx)] < sfxMinInterval(id)) return;
    lastPlayTime[static_cast<std::size_t>(idx)] = clock;

    // Free voice, else steal the oldest one
    int best = 0;
    float oldest = 1e9f;
    for (int i = 0; i < VOICES; ++i) {
        if (voices[static_cast<std::size_t>(i)]->getStatus() != sf::SoundSource::Status::Playing) {
            best = i;
            oldest = -1.0f;
            break;
        }
        if (voiceStartTime[static_cast<std::size_t>(i)] < oldest) {
            oldest = voiceStartTime[static_cast<std::size_t>(i)];
            best = i;
        }
    }
    sf::Sound& v = *voices[static_cast<std::size_t>(best)];
    v.stop();
    v.setBuffer(*sfxBuffers[static_cast<std::size_t>(idx)]);
    float pan = std::max(-0.75f, std::min(0.75f, (x - 800.0f) / 800.0f));
    v.setPan(pan);
    bool mining = (idx >= static_cast<int>(Sfx::MineWood) && idx <= static_cast<int>(Sfx::MineGold));
    v.setPitch(std::max(0.25f, pitch * (mining ? randomPitchJitter(0.05f) : 1.0f)));
    float vol = 100.0f * masterGain() * (static_cast<float>(sfxPct) / 100.0f) * sfxGain(id) * std::max(0.0f, std::min(1.5f, volume));
    v.setVolume(std::max(0.0f, std::min(100.0f, vol)));
    v.play();
    voiceStartTime[static_cast<std::size_t>(best)] = clock;
}

void UI_audio::update(float dt, Scene scene, float nightAmount) {
    if (disabled) return;
    clock += std::max(0.0f, dt);
    buildReadyBuffers();
    if (!musicBuilt) return;

    float night = Ease::clamp01(nightAmount);
    float targetDay = 0.0f, targetNight = 0.0f;
    switch (scene) {
        case Scene::Menu: targetDay = MENU_MUSIC_LEVEL; break;
        case Scene::Match: targetDay = 1.0f - night; targetNight = night; break;
        case Scene::Paused: targetDay = (1.0f - night) * PAUSED_MUSIC_LEVEL; targetNight = night * PAUSED_MUSIC_LEVEL; break;
        case Scene::Silent: break;
    }
    // Equal-power crossfade
    targetDay = std::sqrt(std::max(0.0f, targetDay));
    targetNight = std::sqrt(std::max(0.0f, targetNight));
    dayLevel = Ease::approach(dayLevel, targetDay, MUSIC_FADE_RATE, dt);
    nightLevel = Ease::approach(nightLevel, targetNight, MUSIC_FADE_RATE, dt);

    float music = (musicOn ? static_cast<float>(musicPct) / 100.0f : 0.0f) * masterGain() * MUSIC_BASE_GAIN * 100.0f;
    dayMusic->setVolume(std::max(0.0f, std::min(100.0f, music * dayLevel)));
    nightMusic->setVolume(std::max(0.0f, std::min(100.0f, music * nightLevel)));
}

void UI_audio::setMasterVolume(int percent) { masterPct = clampPct(percent); }
void UI_audio::setSfxEnabled(bool on) { sfxOn = on; }
void UI_audio::setSfxVolume(int percent) { sfxPct = clampPct(percent); }
void UI_audio::setMusicVolume(int percent) { musicPct = clampPct(percent); }
void UI_audio::setMusicEnabled(bool on) { musicOn = on; }
