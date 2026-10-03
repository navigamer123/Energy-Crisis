// =============================================================================
// [b-effects] Procedural audio synthesis (F-01): oscillators, FM bells,
// Karplus-Strong plucks, filtered noise and a tiny sequencer for two music stems.
// =============================================================================
// The game builds with -O0; synthesis is pure number crunching run once at startup,
// so this file is always optimised (about 10x faster startup audio).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O2")
#endif
#include "../includes/UI_audio_synth.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace AudioSynth {

namespace {

const float PI = 3.14159265358979f;
const float TAU = 2.0f * PI;

// Deterministic xorshift noise in [-1, 1]
struct Rng {
    std::uint32_t s;
    explicit Rng(std::uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
    float next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s & 0xFFFFFFu) / 8388607.5f - 1.0f;
    }
};

inline float midiHz(float midi) { return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f); }

enum class Wave { Sine, Tri, Square, Saw };

inline float osc(Wave w, float phase) { // phase in cycles
    float p = phase - std::floor(phase);
    switch (w) {
        case Wave::Sine: return std::sin(TAU * p);
        case Wave::Tri: return 4.0f * std::fabs(p - 0.5f) - 1.0f;
        case Wave::Square: return p < 0.5f ? 1.0f : -1.0f;
        case Wave::Saw: return 2.0f * p - 1.0f;
    }
    return 0.0f;
}

inline float lpCoef(float cutoffHz, unsigned rate) {
    float c = 1.0f - std::exp(-TAU * cutoffHz / static_cast<float>(rate));
    return std::min(1.0f, std::max(0.0001f, c));
}

// Float mix buffer. In wrap mode writes past the end continue at the start (seamless music loops).
struct Buf {
    std::vector<float> d;
    unsigned rate;
    bool wrap;
    Buf(std::size_t samples, unsigned r, bool w) : d(samples, 0.0f), rate(r), wrap(w) {}
    long size() const { return static_cast<long>(d.size()); }
    void add(long i, float v) {
        long n = size();
        if (n == 0) return;
        if (wrap) {
            i %= n;
            if (i < 0) i += n;
            d[static_cast<std::size_t>(i)] += v;
        } else if (i >= 0 && i < n) {
            d[static_cast<std::size_t>(i)] += v;
        }
    }
    long idx(float seconds) const { return static_cast<long>(std::lround(seconds * static_cast<float>(rate))); }
};

// Attack (linear) -> exponential decay, short click-free release at the end of dur.
// Incremental: one multiply per sample instead of an exp().
struct Env {
    long attackN, relStart, relN, total;
    float decayMul, level = 1.0f;
    long i = 0;
    Env(float dur, float attack, float decay, unsigned rate) {
        float r = static_cast<float>(rate);
        total = static_cast<long>(std::lround(dur * r));
        attackN = std::max(1L, static_cast<long>(std::lround(attack * r)));
        relN = std::max(1L, static_cast<long>(std::lround(0.012f * r)));
        relStart = total - relN;
        decayMul = std::exp(-1.0f / (std::max(decay, 0.0001f) * r));
    }
    float next() {
        float e;
        if (i < attackN) {
            e = static_cast<float>(i) / static_cast<float>(attackN);
        } else {
            e = level;
            level *= decayMul;
        }
        if (i > relStart) e *= std::max(0.0f, static_cast<float>(total - i) / static_cast<float>(relN));
        ++i;
        return e;
    }
};

// Fast 2^x for the small exponents used by vibrato (|x| < 0.1)
inline float exp2Small(float x) { return 1.0f + x * (0.693147f + x * 0.240227f); }

struct ToneOpt {
    Wave wave = Wave::Sine;
    float amp = 0.5f;
    float attack = 0.003f;
    float decay = 0.2f;
    float vibratoHz = 0.0f;
    float vibratoCents = 0.0f;
    float lowpassHz = 0.0f; // 0 = off
    float detuneCents = 0.0f; // second oscillator, 0 = off
};

// Oscillator note with exponential pitch glide f0 -> f1 over dur
void tone(Buf& b, float t0, float dur, float f0, float f1, const ToneOpt& o) {
    long start = b.idx(t0);
    long n = b.idx(dur);
    float phase = 0.0f, phase2 = 0.25f;
    float lp = 0.0f;
    float a = (o.lowpassHz > 0.0f) ? lpCoef(o.lowpassHz, b.rate) : 1.0f;
    float ratio = (f0 > 0.0f) ? f1 / f0 : 1.0f;
    float glide = (n > 0) ? std::pow(ratio, 1.0f / static_cast<float>(n)) : 1.0f; // per-sample pitch multiplier
    float det = std::pow(2.0f, o.detuneCents / 1200.0f);
    float invRate = 1.0f / static_cast<float>(b.rate);
    float f = f0;
    Env env(dur, o.attack, o.decay, b.rate);
    for (long i = 0; i < n; ++i) {
        float fNow = f;
        if (o.vibratoHz > 0.0f) {
            float t = static_cast<float>(i) * invRate;
            fNow *= exp2Small(o.vibratoCents / 1200.0f * std::sin(TAU * o.vibratoHz * t));
        }
        f *= glide;
        phase += fNow * invRate;
        if (phase > 1024.0f) phase -= 1024.0f;
        float s = osc(o.wave, phase);
        if (o.detuneCents != 0.0f) {
            phase2 += fNow * det * invRate;
            if (phase2 > 1024.0f) phase2 -= 1024.0f;
            s = 0.5f * (s + osc(o.wave, phase2));
        }
        lp += a * (s - lp);
        b.add(start + i, lp * o.amp * env.next());
    }
}

// Two-operator FM bell: inharmonic ratio gives metal/glass, index decays with the note
void fmBell(Buf& b, float t0, float dur, float fc, float ratio, float index, float amp, float decay, float attack = 0.001f) {
    long start = b.idx(t0);
    long n = b.idx(dur);
    float pc = 0.0f, pm = 0.0f;
    float dc = fc / static_cast<float>(b.rate), dm = fc * ratio / static_cast<float>(b.rate);
    Env env(dur, attack, decay, b.rate);
    for (long i = 0; i < n; ++i) {
        float e = env.next();
        pm += dm;
        if (pm > 1.0f) pm -= 1.0f;
        float mod = index * e * std::sin(TAU * pm);
        pc += dc;
        if (pc > 1.0f) pc -= 1.0f;
        b.add(start + i, amp * e * std::sin(TAU * pc + mod));
    }
}

struct NoiseOpt {
    float amp = 0.5f;
    float attack = 0.001f;
    float decay = 0.05f;
    float lpStart = 8000.0f; // low-pass cutoff sweeps lpStart -> lpEnd
    float lpEnd = 8000.0f;
    float hpHz = 0.0f;       // 0 = off
    bool brown = false;      // integrated (rumbling) noise
};

void noise(Buf& b, float t0, float dur, const NoiseOpt& o, std::uint32_t seed) {
    Rng rng(seed);
    long start = b.idx(t0);
    long n = b.idx(dur);
    float lp = 0.0f, hpLp = 0.0f, br = 0.0f;
    float hpA = (o.hpHz > 0.0f) ? lpCoef(o.hpHz, b.rate) : 0.0f;
    bool sweep = std::fabs(o.lpEnd - o.lpStart) > 1.0f;
    float lpA = lpCoef(o.lpStart, b.rate);
    Env env(dur, o.attack, o.decay, b.rate);
    for (long i = 0; i < n; ++i) {
        if (sweep && (i & 31) == 0) { // cutoff sweep updated every 32 samples
            float k = (n > 0) ? static_cast<float>(i) / static_cast<float>(n) : 0.0f;
            lpA = lpCoef(o.lpStart * std::pow(o.lpEnd / o.lpStart, k), b.rate);
        }
        float x = rng.next();
        if (o.brown) {
            br = 0.985f * br + 0.15f * x;
            x = br;
        }
        lp += lpA * (x - lp);
        float s = lp;
        if (o.hpHz > 0.0f) {
            hpLp += hpA * (s - hpLp);
            s -= hpLp;
        }
        b.add(start + i, s * o.amp * env.next());
    }
}

// Karplus-Strong plucked string
void pluck(Buf& b, float t0, float freq, float dur, float amp, float brightness, std::uint32_t seed) {
    Rng rng(seed);
    std::size_t period = static_cast<std::size_t>(std::max(2.0f, static_cast<float>(b.rate) / freq));
    std::vector<float> line(period);
    float lp = 0.0f;
    float a = 0.25f + 0.7f * brightness;
    for (std::size_t i = 0; i < period; ++i) {
        lp += a * (rng.next() - lp);
        line[i] = lp;
    }
    long start = b.idx(t0);
    long n = b.idx(dur);
    std::size_t pos = 0;
    float damp = 0.996f;
    Env env(dur, 0.0005f, 10.0f, b.rate);
    for (long i = 0; i < n; ++i) {
        std::size_t next = (pos + 1 == period) ? 0 : pos + 1;
        float out = line[pos];
        line[pos] = damp * 0.5f * (line[pos] + line[next]);
        pos = next;
        b.add(start + i, out * amp * env.next());
    }
}

void kick(Buf& b, float t0, float amp) {
    ToneOpt o;
    o.amp = amp;
    o.attack = 0.001f;
    o.decay = 0.11f;
    tone(b, t0, 0.32f, 140.0f, 42.0f, o);
    NoiseOpt n;
    n.amp = amp * 0.25f;
    n.decay = 0.006f;
    n.lpStart = n.lpEnd = 3000.0f;
    noise(b, t0, 0.03f, n, 77u + static_cast<std::uint32_t>(t0 * 1000.0f));
}

void snare(Buf& b, float t0, float amp) {
    NoiseOpt n;
    n.amp = amp;
    n.decay = 0.06f;
    n.lpStart = 7000.0f;
    n.lpEnd = 3000.0f;
    n.hpHz = 900.0f;
    noise(b, t0, 0.22f, n, 991u + static_cast<std::uint32_t>(t0 * 1000.0f));
    ToneOpt o;
    o.wave = Wave::Tri;
    o.amp = amp * 0.4f;
    o.decay = 0.04f;
    tone(b, t0, 0.12f, 220.0f, 160.0f, o);
}

void hat(Buf& b, float t0, float amp, float decay) {
    NoiseOpt n;
    n.amp = amp;
    n.decay = decay;
    n.lpStart = n.lpEnd = 10000.0f;
    n.hpHz = 6000.0f;
    noise(b, t0, decay * 4.0f + 0.02f, n, 5151u + static_cast<std::uint32_t>(t0 * 997.0f));
}

void lowpassAll(Buf& b, float cutoff) {
    float a = lpCoef(cutoff, b.rate);
    float lp = 0.0f;
    for (float& s : b.d) {
        lp += a * (s - lp);
        s = lp;
    }
}

// Feedback echo; in wrap mode the delay line runs over the loop several times so the tail wraps seamlessly
void echo(Buf& b, float delaySec, float feedback, float mix) {
    long n = b.size();
    long D = std::max(1L, b.idx(delaySec));
    if (n <= D) return;
    std::vector<float> wet(static_cast<std::size_t>(n), 0.0f);
    int passes = b.wrap ? 4 : 1;
    for (int p = 0; p < passes; ++p) {
        for (long i = 0; i < n; ++i) {
            long j = i - D;
            float prev = 0.0f;
            if (j >= 0) prev = wet[static_cast<std::size_t>(j)];
            else if (b.wrap) prev = wet[static_cast<std::size_t>(j + n)];
            wet[static_cast<std::size_t>(i)] = b.d[static_cast<std::size_t>(i)] + feedback * prev;
        }
    }
    for (long i = 0; i < n; ++i) {
        std::size_t k = static_cast<std::size_t>(i);
        b.d[k] = b.d[k] + mix * (wet[k] - b.d[k]);
    }
}

void mixInto(Buf& dst, const Buf& src, float gain) {
    std::size_t n = std::min(dst.d.size(), src.d.size());
    for (std::size_t i = 0; i < n; ++i) dst.d[i] += src.d[i] * gain;
}

std::vector<std::int16_t> toPcm(const Buf& b, float targetPeak, float targetRms = 0.0f) {
    float peak = 0.0f;
    double acc = 0.0;
    for (float s : b.d) {
        peak = std::max(peak, std::fabs(s));
        acc += static_cast<double>(s) * s;
    }
    float g = (peak > 0.000001f) ? targetPeak / peak : 0.0f;
    if (targetRms > 0.0f && !b.d.empty()) { // equal loudness (music stems), never above targetPeak
        float rms = static_cast<float>(std::sqrt(acc / static_cast<double>(b.d.size())));
        if (rms > 0.000001f) g = std::min(g, targetRms / rms);
    }
    std::vector<std::int16_t> out(b.d.size());
    for (std::size_t i = 0; i < b.d.size(); ++i) {
        float v = std::max(-1.0f, std::min(1.0f, b.d[i] * g));
        out[i] = static_cast<std::int16_t>(std::lround(v * 32767.0f));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Sound effects
// ---------------------------------------------------------------------------
std::vector<std::int16_t> makeSfx(Sfx id, unsigned rate) {
    float len = 0.5f;
    switch (id) {
        case Sfx::MineWood: len = 0.24f; break;
        case Sfx::MineIron: case Sfx::MineCopper: len = 0.42f; break;
        case Sfx::MineCoal: len = 0.3f; break;
        case Sfx::MineSilicon: len = 0.62f; break;
        case Sfx::MineSilver: len = 0.46f; break;
        case Sfx::MineGold: len = 0.44f; break;
        case Sfx::BuildOk: len = 0.66f; break;
        case Sfx::BuildDenied: len = 0.34f; break;
        case Sfx::LandBuy: len = 0.8f; break;
        case Sfx::Upgrade: len = 0.9f; break;
        case Sfx::Demolish: len = 0.52f; break;
        case Sfx::Thunder: len = 2.4f; break;
        case Sfx::Destroyed: len = 1.15f; break;
        case Sfx::SettleWon: len = 1.05f; break;
        case Sfx::SettleLost: len = 1.05f; break;
        case Sfx::Victory: len = 2.7f; break;
        case Sfx::UiClick: len = 0.045f; break;
        case Sfx::UiConfirm: len = 0.15f; break;
        case Sfx::Sunrise: len = 1.5f; break;
        case Sfx::Nightfall: len = 1.7f; break;
        default: break;
    }
    Buf b(static_cast<std::size_t>(len * static_cast<float>(rate)) + 1, rate, false);
    float peak = 0.9f;
    ToneOpt o;
    NoiseOpt n;

    switch (id) {
        case Sfx::MineWood: {
            o.amp = 1.0f; o.attack = 0.002f; o.decay = 0.03f;
            tone(b, 0.0f, 0.1f, 190.0f, 80.0f, o);
            n.amp = 0.6f; n.decay = 0.016f; n.lpStart = 2600.0f; n.lpEnd = 900.0f;
            noise(b, 0.0f, 0.07f, n, 11u);
            ToneOpt k; k.wave = Wave::Tri; k.amp = 0.35f; k.attack = 0.001f; k.decay = 0.012f;
            tone(b, 0.004f, 0.05f, 620.0f, 540.0f, k);
            break;
        }
        case Sfx::MineIron: {
            n.amp = 0.5f; n.decay = 0.003f; n.lpStart = n.lpEnd = 6000.0f;
            noise(b, 0.0f, 0.012f, n, 21u);
            fmBell(b, 0.0f, 0.42f, 1046.5f, 1.414f, 2.5f, 0.8f, 0.09f);
            fmBell(b, 0.0f, 0.42f, 1568.0f, 2.76f, 1.2f, 0.3f, 0.06f);
            break;
        }
        case Sfx::MineCopper: {
            n.amp = 0.4f; n.decay = 0.003f; n.lpStart = n.lpEnd = 5000.0f;
            noise(b, 0.0f, 0.012f, n, 31u);
            fmBell(b, 0.0f, 0.42f, 784.0f, 2.0f, 1.8f, 0.8f, 0.11f);
            fmBell(b, 0.01f, 0.4f, 1175.0f, 3.01f, 0.9f, 0.25f, 0.07f);
            break;
        }
        case Sfx::MineCoal: {
            for (int k = 0; k < 5; ++k) {
                NoiseOpt g; g.amp = 0.7f * (1.0f - static_cast<float>(k) * 0.15f); g.decay = 0.012f;
                g.lpStart = 1500.0f; g.lpEnd = 600.0f;
                noise(b, static_cast<float>(k) * 0.034f + (k % 2) * 0.006f, 0.05f, g, 41u + static_cast<std::uint32_t>(k));
            }
            o.amp = 0.8f; o.attack = 0.002f; o.decay = 0.04f;
            tone(b, 0.0f, 0.12f, 110.0f, 55.0f, o);
            break;
        }
        case Sfx::MineSilicon: {
            o.amp = 0.5f; o.attack = 0.002f; o.decay = 0.18f;
            tone(b, 0.0f, 0.62f, 1568.0f, 1568.0f, o);
            o.amp = 0.3f; o.decay = 0.12f;
            tone(b, 0.0f, 0.62f, 2349.0f, 2349.0f, o);
            o.amp = 0.25f; o.decay = 0.08f;
            tone(b, 0.03f, 0.55f, 3136.0f, 3136.0f, o);
            n.amp = 0.2f; n.decay = 0.004f; n.hpHz = 4000.0f; n.lpStart = n.lpEnd = 12000.0f;
            noise(b, 0.0f, 0.02f, n, 51u);
            break;
        }
        case Sfx::MineSilver: {
            o.amp = 0.6f; o.attack = 0.001f; o.decay = 0.13f;
            tone(b, 0.0f, 0.46f, 1318.5f, 1318.5f, o);
            o.amp = 0.3f; o.decay = 0.09f;
            tone(b, 0.0f, 0.46f, 2637.0f, 2637.0f, o);
            fmBell(b, 0.0f, 0.46f, 3951.0f, 1.5f, 0.8f, 0.2f, 0.05f);
            break;
        }
        case Sfx::MineGold: {
            ToneOpt c; c.wave = Wave::Square; c.amp = 0.3f; c.attack = 0.001f; c.decay = 0.5f; c.lowpassHz = 5000.0f;
            tone(b, 0.0f, 0.075f, 987.77f, 987.77f, c);
            c.decay = 0.12f;
            tone(b, 0.07f, 0.37f, 1318.5f, 1318.5f, c);
            o.amp = 0.4f; o.attack = 0.001f; o.decay = 0.14f;
            tone(b, 0.07f, 0.37f, 1318.5f, 1318.5f, o);
            break;
        }
        case Sfx::BuildOk: {
            o.amp = 1.0f; o.attack = 0.002f; o.decay = 0.045f;
            tone(b, 0.0f, 0.13f, 140.0f, 55.0f, o);
            n.amp = 0.35f; n.decay = 0.012f; n.lpStart = 1800.0f; n.lpEnd = 600.0f;
            noise(b, 0.0f, 0.05f, n, 61u);
            fmBell(b, 0.06f, 0.5f, 784.0f, 2.0f, 1.2f, 0.45f, 0.16f);
            fmBell(b, 0.14f, 0.5f, 1046.5f, 2.0f, 1.2f, 0.45f, 0.2f);
            break;
        }
        case Sfx::BuildDenied: {
            ToneOpt s; s.wave = Wave::Square; s.amp = 0.4f; s.attack = 0.004f; s.decay = 0.5f; s.lowpassHz = 1600.0f;
            tone(b, 0.0f, 0.13f, 233.0f, 220.0f, s);
            tone(b, 0.15f, 0.17f, 175.0f, 165.0f, s);
            peak = 0.7f;
            break;
        }
        case Sfx::LandBuy: {
            const float notes[] = { 72.0f, 76.0f, 79.0f, 84.0f };
            for (int k = 0; k < 4; ++k) {
                float t = static_cast<float>(k) * 0.07f;
                ToneOpt tr; tr.wave = Wave::Tri; tr.amp = 0.45f; tr.attack = 0.002f; tr.decay = 0.15f;
                tone(b, t, 0.45f, midiHz(notes[k]), midiHz(notes[k]), tr);
                fmBell(b, t, 0.4f, midiHz(notes[k]) * 2.0f, 1.0f, 0.6f, 0.15f, 0.1f);
            }
            n.amp = 0.14f; n.decay = 0.12f; n.hpHz = 6000.0f; n.lpStart = n.lpEnd = 14000.0f;
            noise(b, 0.21f, 0.45f, n, 71u);
            break;
        }
        case Sfx::Upgrade: {
            ToneOpt s; s.wave = Wave::Saw; s.amp = 0.32f; s.attack = 0.01f; s.decay = 1.0f; s.lowpassHz = 2500.0f; s.detuneCents = 12.0f;
            tone(b, 0.0f, 0.46f, 260.0f, 1040.0f, s);
            fmBell(b, 0.38f, 0.45f, 1568.0f, 3.0f, 0.7f, 0.3f, 0.1f);
            fmBell(b, 0.46f, 0.4f, 2093.0f, 3.0f, 0.7f, 0.25f, 0.09f);
            fmBell(b, 0.54f, 0.35f, 2637.0f, 3.0f, 0.7f, 0.22f, 0.08f);
            break;
        }
        case Sfx::Demolish: {
            n.amp = 1.0f; n.decay = 0.12f; n.lpStart = 2200.0f; n.lpEnd = 250.0f;
            noise(b, 0.0f, 0.48f, n, 81u);
            o.amp = 0.9f; o.attack = 0.002f; o.decay = 0.07f;
            tone(b, 0.0f, 0.2f, 90.0f, 40.0f, o);
            NoiseOpt g; g.amp = 0.4f; g.decay = 0.015f; g.lpStart = g.lpEnd = 3000.0f;
            noise(b, 0.1f, 0.06f, g, 82u);
            noise(b, 0.18f, 0.06f, g, 83u);
            break;
        }
        case Sfx::Thunder: {
            NoiseOpt c; c.amp = 1.0f; c.decay = 0.02f; c.lpStart = c.lpEnd = 9000.0f; c.hpHz = 1500.0f;
            noise(b, 0.0f, 0.09f, c, 91u);
            c.amp = 0.5f;
            noise(b, 0.15f, 0.08f, c, 92u);
            NoiseOpt r; r.amp = 1.6f; r.attack = 0.05f; r.decay = 0.75f; r.lpStart = 260.0f; r.lpEnd = 140.0f; r.brown = true;
            noise(b, 0.02f, 2.36f, r, 93u);
            // Slow random rumble modulation
            for (std::size_t i = 0; i < b.d.size(); ++i) {
                float t = static_cast<float>(i) / static_cast<float>(rate);
                float m = 0.75f + 0.25f * std::sin(TAU * 3.1f * t + 0.7f) * std::sin(TAU * 5.3f * t + 1.9f);
                if (t > 0.1f) b.d[i] *= m;
            }
            break;
        }
        case Sfx::Destroyed: {
            n.amp = 1.0f; n.decay = 0.3f; n.lpStart = 4000.0f; n.lpEnd = 150.0f;
            noise(b, 0.0f, 1.1f, n, 101u);
            o.amp = 1.0f; o.attack = 0.003f; o.decay = 0.2f;
            tone(b, 0.0f, 0.6f, 70.0f, 28.0f, o);
            Rng dr(102u);
            for (int k = 0; k < 6; ++k) {
                NoiseOpt g; g.amp = 0.25f; g.decay = 0.01f; g.hpHz = 2500.0f; g.lpStart = g.lpEnd = 9000.0f;
                noise(b, 0.2f + 0.08f * static_cast<float>(k) + 0.03f * dr.next(), 0.04f, g, 103u + static_cast<std::uint32_t>(k));
            }
            break;
        }
        case Sfx::SettleWon: {
            const float notes[] = { 72.0f, 76.0f, 79.0f };
            ToneOpt l; l.wave = Wave::Square; l.amp = 0.28f; l.attack = 0.003f; l.decay = 0.3f; l.lowpassHz = 3500.0f;
            for (int k = 0; k < 3; ++k) tone(b, 0.1f * static_cast<float>(k), 0.14f, midiHz(notes[k]), midiHz(notes[k]), l);
            l.decay = 0.35f; l.vibratoHz = 5.5f; l.vibratoCents = 14.0f;
            tone(b, 0.3f, 0.72f, midiHz(84.0f), midiHz(84.0f), l);
            fmBell(b, 0.3f, 0.7f, midiHz(84.0f), 2.0f, 0.8f, 0.3f, 0.3f);
            ToneOpt t; t.wave = Wave::Tri; t.amp = 0.3f; t.decay = 0.4f;
            tone(b, 0.3f, 0.72f, midiHz(60.0f), midiHz(60.0f), t);
            break;
        }
        case Sfx::SettleLost: {
            ToneOpt l; l.wave = Wave::Tri; l.amp = 0.5f; l.attack = 0.005f; l.decay = 0.3f; l.detuneCents = 9.0f;
            tone(b, 0.0f, 0.26f, midiHz(67.0f), midiHz(67.0f), l);
            tone(b, 0.25f, 0.26f, midiHz(63.0f), midiHz(63.0f), l);
            l.decay = 0.45f;
            tone(b, 0.5f, 0.54f, midiHz(60.0f), midiHz(58.6f), l);
            o.amp = 0.35f; o.decay = 0.5f;
            tone(b, 0.5f, 0.54f, midiHz(36.0f), midiHz(35.0f), o);
            break;
        }
        case Sfx::Victory: {
            const float run[] = { 67.0f, 72.0f, 76.0f, 79.0f };
            ToneOpt l; l.wave = Wave::Square; l.amp = 0.25f; l.attack = 0.003f; l.decay = 0.25f; l.lowpassHz = 3800.0f;
            for (int k = 0; k < 4; ++k) tone(b, 0.09f * static_cast<float>(k), 0.12f, midiHz(run[k]), midiHz(run[k]), l);
            l.decay = 1.2f; l.vibratoHz = 5.0f; l.vibratoCents = 18.0f; l.attack = 0.01f;
            tone(b, 0.36f, 2.3f, midiHz(84.0f), midiHz(84.0f), l);
            fmBell(b, 0.36f, 2.0f, midiHz(84.0f), 2.0f, 1.0f, 0.25f, 0.7f);
            ToneOpt p; p.wave = Wave::Saw; p.amp = 0.12f; p.attack = 0.4f; p.decay = 2.5f; p.lowpassHz = 1400.0f; p.detuneCents = 10.0f;
            for (float m : { 60.0f, 64.0f, 67.0f, 48.0f }) tone(b, 0.3f, 2.38f, midiHz(m), midiHz(m), p);
            kick(b, 0.0f, 0.6f);
            kick(b, 0.36f, 0.8f);
            NoiseOpt cy; cy.amp = 0.25f; cy.decay = 0.7f; cy.hpHz = 5000.0f; cy.lpStart = cy.lpEnd = 13000.0f;
            noise(b, 0.36f, 2.2f, cy, 111u);
            break;
        }
        case Sfx::UiClick: {
            o.amp = 0.6f; o.attack = 0.0005f; o.decay = 0.008f;
            tone(b, 0.0f, 0.04f, 2200.0f, 1800.0f, o);
            n.amp = 0.2f; n.decay = 0.002f; n.hpHz = 3000.0f; n.lpStart = n.lpEnd = 10000.0f;
            noise(b, 0.0f, 0.008f, n, 121u);
            peak = 0.6f;
            break;
        }
        case Sfx::UiConfirm: {
            o.amp = 0.5f; o.attack = 0.002f; o.decay = 0.06f;
            tone(b, 0.0f, 0.14f, 880.0f, 1320.0f, o);
            o.amp = 0.12f;
            tone(b, 0.0f, 0.14f, 1760.0f, 2640.0f, o);
            peak = 0.7f;
            break;
        }
        case Sfx::Sunrise: {
            const float notes[] = { 84.0f, 88.0f, 91.0f };
            for (int k = 0; k < 3; ++k)
                fmBell(b, 0.12f * static_cast<float>(k), 1.38f - 0.12f * static_cast<float>(k), midiHz(notes[k]), 3.5f, 0.4f, 0.3f, 0.5f, 0.02f);
            peak = 0.6f;
            break;
        }
        case Sfx::Nightfall: {
            fmBell(b, 0.0f, 1.68f, 196.0f, 1.4f, 2.0f, 0.6f, 0.55f, 0.01f);
            o.amp = 0.4f; o.attack = 0.05f; o.decay = 0.6f;
            tone(b, 0.0f, 1.68f, 98.0f, 98.0f, o);
            peak = 0.65f;
            break;
        }
        default:
            break;
    }
    if (id != Sfx::Thunder && id != Sfx::Destroyed) lowpassAll(b, 9000.0f);
    return toPcm(b, peak);
}

// ---------------------------------------------------------------------------
// Music: 16 bars, 96 BPM, chords C G Am F | C G F G | Am F C G | Dm F G G
// ---------------------------------------------------------------------------
struct Chord {
    int root;  // MIDI bass root (octave 2)
    bool minor;
};

const Chord PROGRESSION[MUSIC_BARS] = {
    { 36, false }, { 43, false }, { 45, true }, { 41, false },
    { 36, false }, { 43, false }, { 41, false }, { 43, false },
    { 45, true }, { 41, false }, { 36, false }, { 43, false },
    { 38, true }, { 41, false }, { 43, false }, { 43, false },
};

struct Note {
    int bar;      // 0-based
    float beat;   // 0-based beat inside the bar
    float len;    // beats
    int midi;
};

// Melody (second half) shared by both stems so a crossfade never clashes
const Note MELODY[] = {
    { 8, 0.0f, 1.5f, 76 }, { 8, 1.5f, 0.5f, 74 }, { 8, 2.0f, 1.0f, 72 }, { 8, 3.0f, 1.0f, 69 },
    { 9, 0.0f, 1.5f, 72 }, { 9, 1.5f, 0.5f, 69 }, { 9, 2.0f, 0.5f, 72 }, { 9, 2.5f, 0.5f, 74 }, { 9, 3.0f, 1.0f, 76 },
    { 10, 0.0f, 1.0f, 79 }, { 10, 1.0f, 1.0f, 76 }, { 10, 2.0f, 0.5f, 74 }, { 10, 2.5f, 0.5f, 72 }, { 10, 3.0f, 1.0f, 74 },
    { 11, 0.0f, 2.0f, 74 }, { 11, 2.0f, 1.0f, 71 }, { 11, 3.0f, 1.0f, 67 },
    { 12, 0.0f, 1.0f, 69 }, { 12, 1.0f, 1.0f, 74 }, { 12, 2.0f, 1.0f, 77 }, { 12, 3.0f, 1.0f, 76 },
    { 13, 0.0f, 1.5f, 77 }, { 13, 1.5f, 0.5f, 72 }, { 13, 2.0f, 1.0f, 69 }, { 13, 3.0f, 1.0f, 72 },
    { 14, 0.0f, 1.0f, 74 }, { 14, 1.0f, 1.0f, 79 }, { 14, 2.0f, 0.5f, 77 }, { 14, 2.5f, 0.5f, 76 }, { 14, 3.0f, 1.0f, 74 },
    { 15, 0.0f, 2.0f, 71 }, { 15, 2.0f, 2.0f, 74 },
};

// Counter-line for bars 5-8 (half notes)
const Note COUNTER[] = {
    { 4, 0.0f, 2.0f, 79 }, { 4, 2.0f, 2.0f, 76 },
    { 5, 0.0f, 2.0f, 74 }, { 5, 2.0f, 2.0f, 71 },
    { 6, 0.0f, 2.0f, 72 }, { 6, 2.0f, 2.0f, 69 },
    { 7, 0.0f, 2.0f, 71 }, { 7, 2.0f, 2.0f, 74 },
};

inline int third(const Chord& c) { return c.root + (c.minor ? 3 : 4); }
inline int fifth(const Chord& c) { return c.root + 7; }

std::vector<std::int16_t> makeMusic(Music stem, unsigned rate) {
    const std::size_t N = musicLoopSamples(rate);
    const float beat = 60.0f / MUSIC_BPM;
    const float bar = 4.0f * beat;
    Buf dry(N, rate, true);
    Buf send(N, rate, true);
    auto at = [&](int barIdx, float beatPos) { return static_cast<float>(barIdx) * bar + beatPos * beat; };

    if (stem == Music::Day) {
        for (int i = 0; i < MUSIC_BARS; ++i) {
            const Chord& c = PROGRESSION[i];
            // Bass: root (dotted quarter), root (8th on 2&), root (quarter on 3), fifth (8th on 4)
            ToneOpt bs; bs.wave = Wave::Tri; bs.amp = 0.5f; bs.attack = 0.01f; bs.decay = 0.35f; bs.lowpassHz = 900.0f;
            tone(dry, at(i, 0.0f), beat * 1.4f, midiHz(static_cast<float>(c.root)), midiHz(static_cast<float>(c.root)), bs);
            tone(dry, at(i, 1.5f), beat * 0.45f, midiHz(static_cast<float>(c.root)), midiHz(static_cast<float>(c.root)), bs);
            tone(dry, at(i, 2.0f), beat * 0.9f, midiHz(static_cast<float>(c.root)), midiHz(static_cast<float>(c.root)), bs);
            tone(dry, at(i, 3.0f), beat * 0.9f, midiHz(static_cast<float>(fifth(c))), midiHz(static_cast<float>(fifth(c))), bs);
            // Plucked 8th-note arpeggio, octave 4-5
            const int arp[8] = { c.root + 24, third(c) + 24, fifth(c) + 24, c.root + 36, fifth(c) + 24, third(c) + 24, c.root + 36, fifth(c) + 24 };
            for (int k = 0; k < 8; ++k) {
                float acc = (k % 2 == 0) ? 0.3f : 0.22f;
                pluck(send, at(i, 0.5f * static_cast<float>(k)), midiHz(static_cast<float>(arp[k])), beat * 1.2f, acc, 0.55f,
                      1000u + static_cast<std::uint32_t>(i * 8 + k));
            }
            // Soft pad
            ToneOpt pd; pd.wave = Wave::Saw; pd.amp = 0.05f; pd.attack = 0.35f; pd.decay = 6.0f; pd.lowpassHz = 1100.0f; pd.detuneCents = 9.0f;
            for (int m : { c.root + 24, third(c) + 24, fifth(c) + 24 })
                tone(dry, at(i, 0.0f), bar, midiHz(static_cast<float>(m)), midiHz(static_cast<float>(m)), pd);
            // Drums
            kick(dry, at(i, 0.0f), 0.55f);
            kick(dry, at(i, 2.0f), 0.5f);
            if (i % 4 == 3) kick(dry, at(i, 3.5f), 0.35f);
            snare(dry, at(i, 1.0f), 0.22f);
            snare(dry, at(i, 3.0f), 0.24f);
            for (int k = 0; k < 8; ++k) hat(dry, at(i, 0.5f * static_cast<float>(k)), (k % 2) ? 0.1f : 0.055f, 0.018f);
        }
        for (const Note& nt : COUNTER)
            fmBell(send, at(nt.bar, nt.beat), nt.len * beat * 1.1f, midiHz(static_cast<float>(nt.midi)), 2.0f, 0.9f, 0.16f, 0.5f, 0.005f);
        for (const Note& nt : MELODY) {
            ToneOpt ld; ld.wave = Wave::Tri; ld.amp = 0.2f; ld.attack = 0.015f; ld.decay = 0.8f; ld.vibratoHz = 5.2f; ld.vibratoCents = 10.0f; ld.lowpassHz = 3000.0f;
            tone(send, at(nt.bar, nt.beat), nt.len * beat * 0.95f, midiHz(static_cast<float>(nt.midi)), midiHz(static_cast<float>(nt.midi)), ld);
        }
        echo(send, beat * 0.75f, 0.32f, 0.28f);
        mixInto(dry, send, 1.0f);
    } else {
        for (int i = 0; i < MUSIC_BARS; ++i) {
            const Chord& c = PROGRESSION[i];
            // Sustained sine bass
            ToneOpt bs; bs.amp = 0.45f; bs.attack = 0.12f; bs.decay = 3.0f;
            tone(dry, at(i, 0.0f), bar, midiHz(static_cast<float>(c.root)), midiHz(static_cast<float>(c.root)), bs);
            // Warm pad (triangle, slow attack)
            ToneOpt pd; pd.wave = Wave::Tri; pd.amp = 0.09f; pd.attack = 0.9f; pd.decay = 8.0f; pd.lowpassHz = 900.0f; pd.detuneCents = 7.0f;
            for (int m : { c.root + 12, third(c) + 24, fifth(c) + 12, c.root + 24 })
                tone(dry, at(i, 0.0f), bar + beat * 0.5f, midiHz(static_cast<float>(m)), midiHz(static_cast<float>(m)), pd);
            // Quarter-note bells, octave 5
            const int arp[4] = { c.root + 36, fifth(c) + 36, third(c) + 48, fifth(c) + 36 };
            for (int k = 0; k < 4; ++k)
                fmBell(send, at(i, static_cast<float>(k)), beat * 1.6f, midiHz(static_cast<float>(arp[k])), 3.5f, 0.5f, 0.14f, 0.45f, 0.004f);
            // Heartbeat kick and a whisper of shaker
            kick(dry, at(i, 0.0f), 0.28f);
            for (int k = 0; k < 4; ++k) hat(dry, at(i, static_cast<float>(k) + 0.5f), 0.035f, 0.03f);
        }
        for (const Note& nt : MELODY) {
            ToneOpt fl; fl.amp = 0.17f; fl.attack = 0.06f; fl.decay = 1.5f; fl.vibratoHz = 4.5f; fl.vibratoCents = 12.0f;
            tone(send, at(nt.bar, nt.beat), nt.len * beat * 1.05f, midiHz(static_cast<float>(nt.midi - 12)), midiHz(static_cast<float>(nt.midi - 12)), fl);
        }
        echo(send, beat * 0.75f, 0.45f, 0.4f);
        mixInto(dry, send, 1.0f);
    }
    return toPcm(dry, 0.85f, 0.13f);
}

} // namespace

const char* sfxName(Sfx id) {
    switch (id) {
        case Sfx::MineWood: return "mine_wood";
        case Sfx::MineIron: return "mine_iron";
        case Sfx::MineCopper: return "mine_copper";
        case Sfx::MineCoal: return "mine_coal";
        case Sfx::MineSilicon: return "mine_silicon";
        case Sfx::MineSilver: return "mine_silver";
        case Sfx::MineGold: return "mine_gold";
        case Sfx::BuildOk: return "build_ok";
        case Sfx::BuildDenied: return "build_denied";
        case Sfx::LandBuy: return "land_buy";
        case Sfx::Upgrade: return "upgrade";
        case Sfx::Demolish: return "demolish";
        case Sfx::Thunder: return "thunder";
        case Sfx::Destroyed: return "destroyed";
        case Sfx::SettleWon: return "settle_won";
        case Sfx::SettleLost: return "settle_lost";
        case Sfx::Victory: return "victory";
        case Sfx::UiClick: return "ui_click";
        case Sfx::UiConfirm: return "ui_confirm";
        case Sfx::Sunrise: return "sunrise";
        case Sfx::Nightfall: return "nightfall";
        default: return "?";
    }
}

std::vector<std::int16_t> renderSfx(Sfx id, unsigned sampleRate) {
    if (static_cast<int>(id) < 0 || static_cast<int>(id) >= SFX_COUNT || sampleRate < 8000) return {};
    return makeSfx(id, sampleRate);
}

std::vector<std::int16_t> renderMusic(Music stem, unsigned sampleRate) {
    if (sampleRate < 8000) return {};
    return makeMusic(stem, sampleRate);
}

float musicLoopSeconds() { return static_cast<float>(MUSIC_BARS) * 4.0f * 60.0f / MUSIC_BPM; }

std::size_t musicLoopSamples(unsigned sampleRate) {
    return static_cast<std::size_t>(std::lround(musicLoopSeconds() * static_cast<float>(sampleRate)));
}

float peakLevel(const std::vector<std::int16_t>& samples) {
    int peak = 0;
    for (std::int16_t s : samples) peak = std::max(peak, std::abs(static_cast<int>(s)));
    return static_cast<float>(peak) / 32767.0f;
}

float rmsLevel(const std::vector<std::int16_t>& samples) {
    if (samples.empty()) return 0.0f;
    double acc = 0.0;
    for (std::int16_t s : samples) acc += static_cast<double>(s) * static_cast<double>(s);
    return static_cast<float>(std::sqrt(acc / static_cast<double>(samples.size())) / 32767.0);
}

} // namespace AudioSynth
