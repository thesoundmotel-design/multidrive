# Motel Vox  -  The Sound Motel

One-knob vocal compressor + gate, modelled on what was learned from the Waves RVox processing code.
Formats: VST3 (Windows, Mac), AU (Mac), Standalone. Logo is embedded in the interface.

## Build the macOS installer
Push changes to `main` or run **Build Motel Vox macOS installer** from the repository's **Actions** tab.
Download the `MotelVox-macOS-installer` artifact from the completed run. It contains AU and VST3 plugins
installed to the standard system plugin folders. The package is unsigned; on first open, use right-click > Open.

To build locally on macOS, build the AU and VST3 targets, then run `bash installer/build-macos-installer.sh`.

## Build locally
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target MotelVox_VST3
(Needs CMake 3.22+ and a C++17 compiler; JUCE is downloaded automatically.)

## Test the DSP (no JUCE needed)
    g++ -std=c++17 -O2 tests/dsp_test.cpp -o dsp_test && ./dsp_test

## Controls
| Knob | Range | Meaning |
|---|---|---|
| Compression | -30 .. 0 dB | gain applied at full scale; more negative = more compression |
| Gate | Off, -79 .. -20 dB | threshold of a gentle downward expander |
| Release | 10 .. 1000 ms | detector release time |
| Gain | -18 .. +18 dB | output level |

Automatic makeup gain (0.99 / k) is applied, so quiet material is raised as compression increases.
Latency is reported to the host (78 samples at 48 kHz).

## What is measured vs. guessed
Recovered from the binaries: three parallel allpass detector filters (0.6303 / 0.3583 / 0.1607), max-of-abs
detector, instant-attack exponential release, curve gain = k / (k + (1-k) x level), gate = min(1, level/threshold)
with off at -79 dB, minimum-gain lookahead of about 1 ms (power-of-two window), 250 ms / 500 ms smoothing,
0.99 / k makeup, anti-denormal guard.

Guessed (replace once measured against a real RVox): knob ranges and defaults, the program-dependent release
curve (original uses 256-entry tables), the final gain smoothing filter, stereo linking, the GUI layout.

Not included: AAX (Pro Tools). That needs the Avid AAX SDK and PACE/iLok signing.
