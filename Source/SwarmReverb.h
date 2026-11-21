// SwarmReverb.h
// ---------------------------------------------------------------------------
// "Undertow" reverb engine. An original algorithm inspired by the
// cavernous "swarm of delays" style of reverb pedals such as the
// EarthQuaker Devices Afterneath. No JUCE dependency, so it can be unit
// tested on its own.
//
// Signal flow (per channel pair):
//
//   in ──► pre-diffusion (4 allpasses, amount = DIFFUSE)
//      ──► 8-line feedback delay network (Householder mixing)
//            each line: modulated delay ─► damping LPF (DAMPEN)
//                       ─► allpass smear (DIFFUSE) ─► feedback gain (LENGTH)
//            line lengths scale with DRAG (smoothed slowly, so turning DRAG
//            while notes ring makes the swarm warp and pitch-bend)
//      ──► wet out ──► REFLECT: long regenerating delay fed back into the
//                      input through a soft clipper (can self-oscillate)
//
//   out = dry * dryGain + wet * wetGain   (MIX, equal-power)
//   Bypass keeps the reverb tail ringing out ("trails").
// ---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace undertow
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

//==============================================================================
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // Value written `delay` pushes ago (delay >= 1).
    float readInt (int delay) const { return buffer[(size_t) ((writePos - delay) & mask)]; }

    // Fractional read with cubic Hermite interpolation (delay >= 2).
    float read (float delay) const
    {
        const int di = (int) delay;
        const float f = delay - (float) di;
        const float xm1 = readInt (di - 1);
        const float x0  = readInt (di);
        const float x1  = readInt (di + 1);
        const float x2  = readInt (di + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writePos = 0;
};

//==============================================================================
class Allpass
{
public:
    void prepare (int delaySamples)
    {
        length = std::max (1, delaySamples);
        line.prepare (length + 1);
    }
    void clear() { line.clear(); }

    float process (float x, float g)
    {
        const float delayed = line.readInt (length);
        const float v = x + g * delayed;
        line.push (v);
        return delayed - g * v;
    }

private:
    DelayLine line;
    int length = 1;
};

//==============================================================================
struct OnePoleLP
{
    float z = 0.0f;
    float process (float x, float a) { z += a * (x - z); return z; }
};

// Smooth saturator: ~linear below `headroom`, levels off around 1.5x headroom.
inline float softSat (float x, float headroom)
{
    const float v = std::clamp (x / headroom, -3.0f, 3.0f);
    return headroom * v * (27.0f + v * v) / (27.0f + 9.0f * v * v);
}

struct DCBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
    float process (float x)
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

//==============================================================================
struct SwarmParams
{
    float mix     = 0.4f;  // 0..1
    float length  = 3.0f;  // decay time (RT60) in seconds, 0.3..15
    float diffuse = 0.6f;  // 0..1  (0 = discrete swarm of echoes, 1 = smooth wash)
    float dampen  = 0.4f;  // 0..1  (0 = bright, 1 = dark)
    float drag    = 0.5f;  // 0..1  (delay spacing: short/metallic .. long/cavernous)
    float reflect = 0.2f;  // 0..1  (regeneration; high values self-oscillate)
    bool engaged  = true;
};

//==============================================================================
class SwarmReverb
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const int maxLine = (int) std::ceil (fs * 1.0f);
        for (auto& l : lines) l.prepare (maxLine);
        for (int c = 0; c < 2; ++c) reflectLine[c].prepare ((int) std::ceil (fs * 1.2f));

        // Pre-diffuser lengths (ms) — slightly different per channel for width.
        const float preMs[2][4] = { { 4.77f, 3.59f, 12.73f, 9.31f },
                                    { 5.13f, 3.97f, 11.83f, 8.71f } };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 4; ++i)
                preDiff[c][i].prepare (ms (preMs[c][i]));

        const float branchMs[N] = { 7.1f, 9.7f, 11.3f, 13.9f, 8.3f, 10.9f, 12.1f, 15.7f };
        for (int i = 0; i < N; ++i)
            branchAp[i].prepare (ms (branchMs[i]));

        reset();
        snapToTargets = true;
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& r : reflectLine) r.clear();
        for (auto& c : preDiff) for (auto& a : c) a.clear();
        for (auto& a : branchAp) a.clear();
        for (auto& d : damp) d = {};
        for (auto& d : reflectDamp) d = {};
        for (auto& d : reflectDc) d = {};
        prevWet[0] = prevWet[1] = 0.0f;
        for (int i = 0; i < N; ++i) lfoPhase[i] = (float) i / (float) N;
    }

    void setParams (const SwarmParams& p) { target = p; }

    // In-place stereo processing.
    void process (float* left, float* right, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            if (controlCounter-- <= 0)
            {
                updateControl();
                controlCounter = kControlInterval - 1;
            }

            // Slowly glide the delay spacing: this is what makes DRAG "warp".
            baseDelay += dragGlide * (targetBaseDelay - baseDelay);
            engagedSmooth += 0.002f * ((target.engaged ? 1.0f : 0.0f) - engagedSmooth);

            const float dryL = left[n];
            const float dryR = right[n];

            // --- REFLECT path: long regenerating delay, soft clipped ----------
            const float reflectDelay = std::clamp (baseDelay * 3.1f, 4.0f, fs * 1.15f);
            float regen[2];
            for (int c = 0; c < 2; ++c)
            {
                float r = reflectLine[c].read (reflectDelay);
                r = reflectDamp[c].process (r, dampCoef);
                r = reflectDc[c].process (r);
                regen[c] = std::tanh (r * reflectGain);
                reflectLine[c].push (prevWet[c]);
            }

            // --- input + pre-diffusion -----------------------------------------
            float inj[2] = { dryL * engagedSmooth + regen[0], dryR * engagedSmooth + regen[1] };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 4; ++i)
                    inj[c] = preDiff[c][i].process (inj[c], preDiffG);

            // --- FDN ------------------------------------------------------------
            float s[N];
            for (int i = 0; i < N; ++i)
            {
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                const float mod = modDepth * std::sin (kTwoPi * lfoPhase[i]);
                const float d = std::max (3.0f, baseDelay * kRatios[i] + mod);
                float x = lines[i].read (d);
                x = damp[i].process (x, dampCoef);
                x = branchAp[i].process (x, branchApG);
                s[i] = x;
            }

            float wetL = 0.0f, wetR = 0.0f, sum = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                wetL += s[i] * kOutL[i];
                wetR += s[i] * kOutR[i];
                sum += s[i];
            }
            wetL = softSat (wetL * kOutScale, 1.0f);
            wetR = softSat (wetR * kOutScale, 1.0f);

            const float hh = sum * (2.0f / (float) N); // Householder reflection
            for (int i = 0; i < N; ++i)
            {
                const float fb = (s[i] - hh) * gains[i];
                const float in = (i & 1) ? inj[1] : inj[0];
                // Saturating the loop keeps self-oscillation musical and bounded.
                lines[i].push (softSat (fb + in * kInGain, 0.9f));
            }

            prevWet[0] = wetL;
            prevWet[1] = wetR;

            // --- mix (dry passes at unity when bypassed; tail keeps ringing) ----
            const float dryGain = 1.0f + engagedSmooth * (dryMixGain - 1.0f);
            left[n]  = dryL * dryGain + wetL * wetMixGain;
            right[n] = dryR * dryGain + wetR * wetMixGain;
        }
    }

private:
    static constexpr int kControlInterval = 32;
    static constexpr float kRatios[N] = { 1.000f, 1.153f, 1.297f, 1.471f, 1.613f, 1.789f, 1.931f, 2.137f };
    static constexpr float kOutL[N]   = {  1, -1,  1, -1,  1,  1, -1, -1 };
    static constexpr float kOutR[N]   = {  1,  1, -1, -1,  1, -1,  1, -1 };
    static constexpr float kOutScale  = 0.30f;
    static constexpr float kInGain    = 0.5f;

    int ms (float m) const { return std::max (1, (int) std::lround (m * 0.001f * fs)); }

    void updateControl()
    {
        const SwarmParams& p = target;
        const float k = snapToTargets ? 1.0f : 0.15f;

        auto glide = [k] (float& v, float t) { v += k * (t - v); };

        glide (sDiffuse, std::clamp (p.diffuse, 0.0f, 1.0f));
        glide (sDampen,  std::clamp (p.dampen,  0.0f, 1.0f));
        glide (sReflect, std::clamp (p.reflect, 0.0f, 1.0f));
        glide (sMix,     std::clamp (p.mix,     0.0f, 1.0f));
        glide (sLength,  std::clamp (p.length,  0.1f, 30.0f));

        // DRAG: base line spacing 6 ms .. 190 ms (exponential).
        const float dragMs = 6.0f * std::pow (190.0f / 6.0f, std::clamp (p.drag, 0.0f, 1.0f));
        targetBaseDelay = dragMs * 0.001f * fs;
        if (snapToTargets) baseDelay = targetBaseDelay;

        // DAMPEN: 18 kHz .. 700 Hz lowpass inside the loop.
        const float fc = std::min (18000.0f * std::pow (700.0f / 18000.0f, sDampen), 0.45f * fs);
        dampCoef = 1.0f - std::exp (-kTwoPi * fc / fs);

        preDiffG  = 0.72f * sDiffuse;
        branchApG = 0.62f * sDiffuse;

        // LENGTH: per-line gain for the requested RT60 (uses nominal delay).
        for (int i = 0; i < N; ++i)
        {
            const float d = baseDelay * kRatios[i];
            gains[i] = std::pow (10.0f, -3.0f * d / (sLength * fs));
        }

        // REFLECT: 0 .. 1.15 (above ~0.9 the loop can sustain itself).
        reflectGain = 1.15f * std::pow (sReflect, 1.3f);

        // MIX: equal-power crossfade.
        dryMixGain = std::cos (sMix * 0.5f * kPi);
        wetMixGain = std::sin (sMix * 0.5f * kPi) * 1.2f;

        // Gentle chorusing of the lines, ~0.35 ms deep.
        modDepth = 0.00035f * fs;
        for (int i = 0; i < N; ++i)
            lfoInc[i] = (0.11f + 0.087f * (float) i) / fs;

        dragGlide = 1.0f - std::exp (-1.0f / (0.35f * fs)); // ~350 ms glide
        snapToTargets = false;
    }

    float fs = 48000.0f;
    SwarmParams target;
    bool snapToTargets = true;
    int controlCounter = 0;

    std::array<DelayLine, N> lines;
    std::array<Allpass, N> branchAp;
    std::array<OnePoleLP, N> damp;
    Allpass preDiff[2][4];
    DelayLine reflectLine[2];
    OnePoleLP reflectDamp[2];
    DCBlocker reflectDc[2];

    float gains[N] {};
    float lfoPhase[N] {};
    float lfoInc[N] {};
    float prevWet[2] {};

    float sDiffuse = 0, sDampen = 0, sReflect = 0, sMix = 0, sLength = 3;
    float baseDelay = 1000.0f, targetBaseDelay = 1000.0f, dragGlide = 0.0001f;
    float dampCoef = 0.5f, preDiffG = 0.0f, branchApG = 0.0f, reflectGain = 0.0f;
    float dryMixGain = 1.0f, wetMixGain = 0.0f, modDepth = 0.0f;
    float engagedSmooth = 1.0f;
};

} // namespace undertow
