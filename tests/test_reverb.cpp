// Offline tests + demo render for the Undertow engine.
#include "../Source/SwarmReverb.h"
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>

using namespace undertow;
static const double FS = 48000.0;

struct Stats { float peak = 0; bool finite = true; double tailRms = 0; };

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, sr = (uint32_t) FS, br = sr * 4;
    const uint16_t ch = 2, bits = 16, align = 4, fmt = 1; const uint32_t fmtLen = 16, riffLen = 36 + dataBytes;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riffLen, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&fmt, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&sr, 4, 1, f); std::fwrite (&br, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
        for (float v : { L[i], R[i] }) { int16_t s = (int16_t) std::lround (std::clamp (v, -1.0f, 1.0f) * 32767.0f); std::fwrite (&s, 2, 1, f); }
    std::fclose (f);
}

static Stats run (SwarmParams p, std::vector<float> in, double seconds, bool sweepDrag = false,
                  std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr)
{
    SwarmReverb rv; rv.prepare (FS); rv.setParams (p);
    const int total = (int) (seconds * FS); in.resize ((size_t) total, 0.0f);
    std::vector<float> L (in), R (in);
    const int block = 256;
    for (int s = 0; s < total; s += block)
    {
        if (sweepDrag) { p.drag = 0.5f + 0.5f * std::sin (s / FS * 1.3); rv.setParams (p); }
        rv.process (L.data() + s, R.data() + s, std::min (block, total - s));
    }
    Stats st; double acc = 0; int cnt = 0;
    for (int i = 0; i < total; ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) st.finite = false;
        st.peak = std::max ({ st.peak, std::fabs (L[i]), std::fabs (R[i]) });
        if (i > total - (int) FS) { acc += L[i] * L[i]; ++cnt; }
    }
    st.tailRms = std::sqrt (acc / cnt);
    if (outL) *outL = L;
    if (outR) *outR = R;
    return st;
}

// Karplus-Strong "guitar" so the demo isn't just clicks.
static void pluck (std::vector<float>& buf, double t0, float freq, float amp, unsigned seed)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> u (-1, 1);
    const int period = (int) (FS / freq); std::vector<float> ks ((size_t) period);
    for (auto& v : ks) v = u (rng);
    const int start = (int) (t0 * FS), len = (int) (FS * 2.5);
    if ((int) buf.size() < start + len) buf.resize ((size_t) (start + len), 0.0f);
    int idx = 0;
    for (int i = 0; i < len; ++i)
    {
        const int nx = (idx + 1) % period;
        const float v = ks[(size_t) idx];
        ks[(size_t) idx] = 0.497f * (v + ks[(size_t) nx]);
        buf[(size_t) (start + i)] += amp * v * (1.0f - (float) i / len);
        idx = nx;
    }
}

int main()
{
    std::vector<float> impulse (1, 1.0f);
    std::mt19937 rng (1); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
    std::vector<float> noise ((size_t) (FS * 0.5)); for (auto& v : noise) v = u (rng);

    struct Case { const char* name; SwarmParams p; std::vector<float>* in; double secs; bool sweep; };
    SwarmParams def;
    SwarmParams wet = def; wet.mix = 1.0f;
    SwarmParams maxAll { 1.0f, 15.0f, 1.0f, 0.0f, 1.0f, 1.0f, true };
    SwarmParams minAll { 1.0f, 0.3f, 0.0f, 1.0f, 0.0f, 0.0f, true };
    SwarmParams selfOsc { 1.0f, 15.0f, 0.3f, 0.2f, 0.1f, 1.0f, true };
    SwarmParams shortDiscrete { 1.0f, 2.0f, 0.0f, 0.3f, 0.7f, 0.0f, true };

    Case cases[] = {
        { "default impulse",        def,     &impulse, 6,  false },
        { "full wet impulse",       wet,     &impulse, 6,  false },
        { "wet noise burst",        wet,     &noise,   6,  false },
        { "max everything noise",   maxAll,  &noise,  20,  false },
        { "min everything noise",   minAll,  &noise,   6,  false },
        { "self-osc noise",         selfOsc, &noise,  20,  false },
        { "discrete swarm impulse", shortDiscrete, &impulse, 6, false },
        { "drag sweep noise",       maxAll,  &noise,  20,  true  },
    };
    bool ok = true;
    for (auto& c : cases)
    {
        auto st = run (c.p, *c.in, c.secs, c.sweep);
        std::printf ("%-24s finite=%d peak=%.3f lastSecRms=%.5f\n", c.name, st.finite, st.peak, st.tailRms);
        ok &= st.finite && st.peak < 4.0f;
    }

    // Decay check: with mix=1, LENGTH=1s vs 8s, RMS of 2nd second should differ a lot.
    for (float len : { 1.0f, 8.0f })
    {
        SwarmParams p = wet; p.length = len; p.reflect = 0;
        std::vector<float> L, R; run (p, noise, 4, false, &L, &R);
        auto rms = [&] (double a, double b) { double s = 0; int n = 0; for (int i = (int) (a * FS); i < (int) (b * FS); ++i) { s += L[i] * L[i]; ++n; } return std::sqrt (s / n); };
        std::printf ("length=%.0fs: rms 0.5-1s=%.4f  2-3s=%.4f  drop=%.1f dB\n", len, rms (0.5, 1), rms (2, 3), 20 * std::log10 (rms (0.5, 1) / rms (2, 3)));
    }

    // Demo: dry phrase, then same phrase through a few settings.
    std::vector<float> phrase;
    const float notes[] = { 196.0f, 246.9f, 293.7f, 392.0f, 329.6f, 293.7f };
    for (int i = 0; i < 6; ++i) pluck (phrase, 0.35 * i, notes[i], 0.35f, 10 + i);
    pluck (phrase, 2.4, 98.0f, 0.3f, 99); pluck (phrase, 2.4, 146.8f, 0.25f, 98); pluck (phrase, 2.4, 196.0f, 0.25f, 97);

    std::vector<float> demoL, demoR;
    auto append = [&] (const std::vector<float>& l, const std::vector<float>& r) { demoL.insert (demoL.end(), l.begin(), l.end()); demoR.insert (demoR.end(), r.begin(), r.end()); };
    { std::vector<float> d = phrase; d.resize ((size_t) (FS * 5)); append (d, d); }
    SwarmParams demos[] = {
        { 0.45f, 4.0f, 0.7f, 0.35f, 0.55f, 0.15f, true },  // lush cavern
        { 0.55f, 2.5f, 0.0f, 0.3f, 0.35f, 0.0f, true },    // discrete swarm
        { 0.6f, 12.0f, 0.8f, 0.5f, 0.8f, 0.85f, true },    // huge + reflect
    };
    for (auto& p : demos) { std::vector<float> l, r; run (p, phrase, 9, false, &l, &r); append (l, r); }
    { SwarmParams p { 0.6f, 6.0f, 0.6f, 0.4f, 0.6f, 0.5f, true }; std::vector<float> l, r; run (p, phrase, 9, true, &l, &r); append (l, r); }
    float pk = 0; for (size_t i = 0; i < demoL.size(); ++i) pk = std::max ({ pk, std::fabs (demoL[i]), std::fabs (demoR[i]) });
    for (size_t i = 0; i < demoL.size(); ++i) { demoL[i] *= 0.9f / pk; demoR[i] *= 0.9f / pk; }
    writeWav ("undertow_demo.wav", demoL, demoR);

    std::printf (ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
