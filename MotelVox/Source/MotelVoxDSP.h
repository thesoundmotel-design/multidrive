// MotelVoxDSP.h  -  The Sound Motel
// Plain C++17, no JUCE dependency, so it can be unit-tested anywhere.
//
// Structure follows what was read from the RVox processing code (RVoxMproc).
// Items marked [GUESS] were NOT recovered from the binaries.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace motelvox
{
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

struct Settings
{
    float compressionDb = -10.0f;  // gain applied at full scale, dB (<= 0). 0 = no compression
    float gateDb        = -79.0f;  // gate threshold, dB. <= -79 means gate off
    float releaseMs     = 150.0f;  // detector release time (63 % definition)
    float gainDb        = 0.0f;    // output gain
};

// Three PARALLEL first-order allpass filters. The detector level is the largest
// absolute value among the input and the three phase-rotated copies, so
// asymmetric peaks (voice) are caught more accurately.
class Detector
{
public:
    void reset() noexcept { for (int i = 0; i < 3; ++i) { xPrev[i] = 0.0f; yPrev[i] = 0.0f; } }

    float process (float x) noexcept
    {
        static constexpr float c[3] = { 0.6303f, 0.3583f, 0.1607f };   // recovered from RVox.dll
        float m = std::fabs (x);
        for (int i = 0; i < 3; ++i)
        {
            const float y = c[i] * (x - yPrev[i]) + xPrev[i];          // y[n] = c(x[n]-y[n-1]) + x[n-1]
            xPrev[i] = x;
            yPrev[i] = y;
            m = std::max (m, std::fabs (y));
        }
        return m;
    }

private:
    float xPrev[3] {}, yPrev[3] {};
};

// Sliding-window minimum (monotonic queue).
class SlidingMin
{
public:
    void prepare (int windowLength)
    {
        window = std::max (1, windowLength);
        cap = window + 1;
        vals.assign ((size_t) cap, 1.0f);
        idx.assign ((size_t) cap, 0);
        reset();
    }
    void reset() noexcept { head = 0; count = 0; counter = 0; }

    float push (float v) noexcept
    {
        while (count > 0 && vals[(size_t) ((head + count - 1) % cap)] >= v) --count;
        const int pos = (head + count) % cap;
        vals[(size_t) pos] = v;
        idx[(size_t) pos] = counter;
        ++count;
        while (idx[(size_t) head] <= counter - window) { head = (head + 1) % cap; --count; }
        ++counter;
        return vals[(size_t) head];
    }

private:
    std::vector<float> vals;
    std::vector<int64_t> idx;
    int window = 1, cap = 2, head = 0, count = 0;
    int64_t counter = 0;
};

class BoxFilter
{
public:
    void prepare (int n) { len = std::max (1, n); buf.assign ((size_t) len, 1.0f); sum = (double) len; pos = 0; }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 1.0f); sum = (double) len; pos = 0; }
    float process (float x) noexcept
    {
        sum += (double) x - (double) buf[(size_t) pos];
        buf[(size_t) pos] = x;
        pos = (pos + 1) % len;
        return (float) (sum / (double) len);
    }
private:
    std::vector<float> buf;
    double sum = 1.0;
    int len = 1, pos = 0;
};

class Compressor
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        nCh = std::max (1, std::min (2, numChannels));

        // Lookahead: smallest power of two >= samples in 1 ms (64 @ 44.1/48 kHz, 128 @ 96 kHz)
        const int oneMs = (int) std::floor (fs / 1000.0);
        L = 1;
        while (L < oneMs) L <<= 1;

        M = std::max (1, L / 4);                 // [GUESS] smoothing length (original: fixed-point triangular filter)
        D = (L - 1) + (M - 1);                   // total audio delay = latency

        delay.assign ((size_t) nCh, std::vector<float> ((size_t) D + 1, 0.0f));
        detectors.assign ((size_t) nCh, Detector());
        slidingMin.prepare (L);
        box1.prepare (M);
        box2.prepare (M);

        fallCoef    = onePole (0.250);           // recovered: 250 ms
        riseCoef    = onePole (0.500);           // recovered: 500 ms
        paramSmooth = onePole (0.020);
        reset();
        setSettings (Settings());
        k = kTarget; outGain = outTarget;
    }

    int latencySamples() const noexcept { return D; }

    void reset() noexcept
    {
        for (auto& d : detectors) d.reset();
        for (auto& ch : delay) std::fill (ch.begin(), ch.end(), 0.0f);
        slidingMin.reset(); box1.reset(); box2.reset();
        env = 0.0f; s1 = 1.0f; s2 = 1.0f; coef2 = 0.0f; wpos = 0; lastGain = 1.0f;
    }

    void setSettings (const Settings& s) noexcept
    {
        kTarget   = std::min (1.0f, std::max (0.003f, dbToGain (s.compressionDb)));
        gateOn    = s.gateDb > -79.0f;
        gateInv   = gateOn ? dbToGain (-s.gateDb) : 1.0f;     // gate gain = min(1, level / threshold)
        relCoef   = onePole (std::max (0.001, (double) s.releaseMs * 0.001));
        outTarget = dbToGain (s.gainDb);
    }

    // Gain reduction of the last processed sample in dB (<= 0), gate included.
    float lastGainReductionDb() const noexcept { return 20.0f * std::log10 (std::max (1.0e-6f, lastGain)); }

    void process (float* const* data, int numChannels, int numSamples) noexcept
    {
        const int chans = std::min (numChannels, nCh);
        constexpr float tiny = 1.0e-30f;   // anti-denormal offset (present in the generic Windows build)

        for (int n = 0; n < numSamples; ++n)
        {
            // 1. detector (channels linked: loudest wins)  [GUESS: stereo linking]
            float p = 0.0f;
            for (int ch = 0; ch < chans; ++ch)
                p = std::max (p, detectors[(size_t) ch].process (data[ch][n]));

            k       = kTarget   + (k - kTarget)         * paramSmooth;
            outGain = outTarget + (outGain - outTarget) * paramSmooth;

            // 2. envelope: instant attack, exponential release
            env = (p > env) ? p : p + (env - p) * relCoef;
            env += tiny; env -= tiny;

            // 3. gate: linear downward expander below threshold
            const float gate = gateOn ? std::min (1.0f, env * gateInv) : 1.0f;

            // 4. one-knob compression curve
            const float gc = k / (k + (1.0f - k) * env);

            // 5. lookahead: minimum of the required gain over the window
            const float gmin = slidingMin.push (gc);

            // 6a. stage 1: follow falls instantly, recover with a 500 ms one-pole
            s1 = gmin + (s1 - gmin) * (s1 > gmin ? fallCoef : riseCoef);
            const float o1 = std::min (gmin, s1);

            // 6b. stage 2: program-dependent recovery  [GUESS: original uses lookup tables]
            float o2 = o1;
            if (s2 <= o1) o2 = o1 + coef2 * (s2 - o1);
            s2 = o2;
            coef2 = onePole (0.010 + 0.090 * (double) (1.0f - o2));

            // 7. smoothing of the final gain (two box filters = triangular)
            const float g = box2.process (box1.process (o2 * gate));
            lastGain = g;

            // 8. apply to delayed audio with automatic makeup (0.99 / k)
            const float makeup = 0.99f / k * outGain;
            for (int ch = 0; ch < chans; ++ch)
            {
                auto& line = delay[(size_t) ch];
                line[(size_t) wpos] = data[ch][n];
                const int rpos = (wpos + 1) % (D + 1);
                data[ch][n] = line[(size_t) rpos] * g * makeup;
            }
            wpos = (wpos + 1) % (D + 1);
        }
    }

private:
    float onePole (double seconds) const noexcept { return (float) std::exp (-1.0 / (fs * seconds)); }

    double fs = 44100.0;
    int nCh = 2, L = 64, M = 16, D = 78, wpos = 0;

    std::vector<Detector> detectors;
    std::vector<std::vector<float>> delay;
    SlidingMin slidingMin;
    BoxFilter box1, box2;

    float kTarget = 0.3f, k = 0.3f, outTarget = 1.0f, outGain = 1.0f;
    float gateInv = 1.0f, relCoef = 0.99f, fallCoef = 0.9f, riseCoef = 0.9f, paramSmooth = 0.9f;
    bool gateOn = false;
    float env = 0.0f, s1 = 1.0f, s2 = 1.0f, coef2 = 0.0f, lastGain = 1.0f;
};
} // namespace motelvox
