#include "../Source/MotelVoxDSP.h"
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace motelvox;
static const double fs = 48000.0;
static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++failures; } else std::printf("  ok:   %s\n", msg); } while (0)

static float runSinePeakOut (float amp, Settings s, float seconds = 4.0f)
{
    Compressor c; c.prepare (fs, 1); c.setSettings (s);
    const int N = (int) (fs * seconds);
    std::vector<float> x ((size_t) N);
    for (int i = 0; i < N; ++i) x[(size_t) i] = amp * std::sin (2.0 * M_PI * 1000.0 * i / fs);
    float* ch[1] = { x.data() };
    c.process (ch, 1, N);
    float pk = 0; for (int i = N - (int) (fs * 0.25); i < N; ++i) pk = std::max (pk, std::fabs (x[(size_t) i]));
    return pk;
}

int main()
{
    std::printf("Static curve (comp -12 dB, gate off, release 150 ms)\n");
    Settings s; s.compressionDb = -12.0f;
    const float k = dbToGain (-12.0f);
    for (float a : { 0.01f, 0.05f, 0.2f, 0.5f, 1.0f })
    {
        const float out = runSinePeakOut (a, s);
        const float expect = a * 0.99f / (k + (1.0f - k) * a);
        std::printf("  in %6.2f dBFS -> out %6.2f dBFS (ideal curve %6.2f)\n",
                    20 * std::log10 (a), 20 * std::log10 (out), 20 * std::log10 (expect));
        CHECK (std::fabs (20 * std::log10 (out / expect)) < 1.5f, "within 1.5 dB of the ideal curve");
    }

    std::printf("Compression knob at 0 dB is transparent apart from 0.99 scale\n");
    { Settings z; z.compressionDb = 0.0f; const float out = runSinePeakOut (0.5f, z);
      CHECK (std::fabs (out - 0.495f) < 0.01f, "0.5 in -> ~0.495 out"); }

    std::printf("Gate\n");
    {
        Settings off; off.compressionDb = 0.0f; Settings on = off; on.gateDb = -40.0f;
        const float quiet = 0.002f; // -54 dBFS, below the -40 dB threshold
        const float a = runSinePeakOut (quiet, off), b = runSinePeakOut (quiet, on);
        std::printf("  quiet signal: gate off %.5f, gate on %.5f\n", a, b);
        CHECK (b < a * 0.5f, "gate attenuates signal below threshold");
        const float loud = runSinePeakOut (0.3f, on), loudOff = runSinePeakOut (0.3f, off);
        CHECK (std::fabs (loud - loudOff) < 0.01f, "gate leaves signal above threshold alone");
    }

    std::printf("Latency and lookahead\n");
    {
        Compressor c; c.prepare (fs, 1); Settings z; z.compressionDb = 0.0f; c.setSettings (z);
        std::vector<float> x (2048, 0.0f); x[100] = 0.5f; float* ch[1] = { x.data() };
        c.process (ch, 1, 2048);
        int pos = 0; for (int i = 0; i < 2048; ++i) if (std::fabs (x[(size_t) i]) > std::fabs (x[(size_t) pos])) pos = i;
        std::printf("  reported latency %d samples, impulse moved %d samples\n", c.latencySamples(), pos - 100);
        CHECK (pos - 100 == c.latencySamples(), "impulse delayed exactly by reported latency");
        Compressor c96; c96.prepare (96000.0, 2);
        std::printf("  latency @48k = %d, @96k = %d\n", c.latencySamples(), c96.latencySamples());
    }

    std::printf("Lookahead catches a sudden peak (no overshoot above 0.99 at -12 dB)\n");
    {
        Settings z; z.compressionDb = -12.0f;
        Compressor c; c.prepare (fs, 1); c.setSettings (z);
        const int N = (int) fs;
        std::vector<float> x ((size_t) N);
        for (int i = 0; i < N; ++i) x[(size_t) i] = (i < 24000 ? 0.05f : 0.9f) * std::sin (2.0 * M_PI * 1000.0 * i / fs);
        float* ch[1] = { x.data() }; c.process (ch, 1, N);
        float pk = 0; for (float v : x) pk = std::max (pk, std::fabs (v));
        std::printf("  peak out = %.3f\n", pk);
        CHECK (pk < 1.2f, "no runaway after a sudden loud burst");
    }

    std::printf("Stability: silence, noise, extremes\n");
    {
        Compressor c; c.prepare (fs, 2); Settings z; z.compressionDb = -30; z.gateDb = -30; z.gainDb = 18; c.setSettings (z);
        std::mt19937 rng (1); std::normal_distribution<float> nd (0, 0.3f);
        std::vector<float> l (96000), r (96000);
        for (size_t i = 0; i < l.size(); ++i) { const bool silent = i > 48000; l[i] = silent ? 0.0f : nd (rng); r[i] = silent ? 0.0f : nd (rng); }
        float* ch[2] = { l.data(), r.data() }; c.process (ch, 2, (int) l.size());
        bool ok = true; for (size_t i = 0; i < l.size(); ++i) if (!std::isfinite (l[i]) || !std::isfinite (r[i])) ok = false;
        CHECK (ok, "all output finite");
    }

    std::printf("Stereo link: both channels get the same gain\n");
    {
        Compressor c; c.prepare (fs, 2); Settings z; z.compressionDb = -15; c.setSettings (z);
        const int N = 48000; std::vector<float> l ((size_t) N), r ((size_t) N);
        for (int i = 0; i < N; ++i) { const float v = std::sin (2.0 * M_PI * 500.0 * i / fs); l[(size_t) i] = 0.8f * v; r[(size_t) i] = 0.1f * v; }
        float* ch[2] = { l.data(), r.data() }; c.process (ch, 2, N);
        float ratio = 0; int cnt = 0;
        for (int i = N - 2000; i < N; ++i) if (std::fabs (l[(size_t) i]) > 0.05f) { ratio += r[(size_t) i] / l[(size_t) i]; ++cnt; }
        ratio /= (float) cnt; std::printf("  right/left = %.3f (expect 0.125)\n", ratio);
        CHECK (std::fabs (ratio - 0.125f) < 0.005f, "channel balance preserved");
    }

    std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
