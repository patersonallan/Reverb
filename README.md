# Undertow — cavern reverb (VST3)

A guitar reverb plugin inspired by "otherworldly" reverb pedals such as the
EarthQuaker Devices Afterneath: a swarm of short, interacting delays that can
go from metallic, rhythmic echoes to a huge smeared wash, with a regenerating
feedback path that can self-oscillate.

It is an original algorithm, not a circuit-accurate clone.

## Controls

| Knob | What it does |
|---|---|
| **LENGTH** | Decay time, 0.3 s – 15 s |
| **DIFFUSE** | Low = a swarm of discrete echoes, high = a smooth wash |
| **DAMPEN** | Tone of the tail: bright (left) to dark (right) |
| **DRAG** | Spacing of the delay swarm: tight and metallic (left) to cavernous (right). Turn it while notes ring out and the tail warps and bends in pitch. |
| **REFLECT** | Regeneration. Past about 3 o'clock it can feed back on itself. The loop is soft-clipped, so it stays controlled. |
| **MIX** | Dry/wet balance (fully clockwise = 100% wet) |
| **Footswitch** | On/off with trails: the tail rings out when you switch off |

Double-click a knob to reset it to its default. Drag up and down or round to turn it.
Every control can be automated in Reaper.

---

## Option A — let GitHub build it for you (no tools to install)

1. Create a new repository on GitHub and upload this whole folder, including the hidden `.github` folder.
2. Open the **Actions** tab. The "Build Undertow VST3" workflow runs automatically. It takes about 5–10 minutes.
3. When it finishes, open the run and download **Undertow-VST3-Windows** or **Undertow-VST3-macOS** under *Artifacts*.
4. Unzip it and copy `Undertow.vst3` to your plugin folder (see "Install" below).

## Option B — build it yourself

You need **CMake 3.22+** and **Git**, plus a C++ compiler:

* **Windows:** Visual Studio 2022 Community with the "Desktop development with C++" workload.
* **macOS:** Xcode, or just the command-line tools (`xcode-select --install`).

In a terminal inside this folder, run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Undertow_VST3
```

The first configure downloads JUCE 8, which takes a minute. The plugin ends up at:

```
build/Undertow_artefacts/Release/VST3/Undertow.vst3
```

There is also an `Undertow_Standalone` target. It builds a standalone app, which is handy for testing without a DAW.

## Install and load in Reaper

1. Copy `Undertow.vst3` to:
   * Windows: `C:\Program Files\Common Files\VST3\`
   * macOS: `~/Library/Audio/Plug-Ins/VST3/`

   Alternatively, add the build folder to Reaper's VST paths.
2. In Reaper, go to **Options → Preferences → Plug-ins → VST** and click **Re-scan**.
3. On your guitar track, click **FX → Add**, then search for **Undertow**.

**macOS note:** if you downloaded the plugin from GitHub, macOS may block it. Run this once:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Undertow.vst3
```

## Starting points

| Sound | Length | Diffuse | Dampen | Drag | Reflect | Mix |
|---|---|---|---|---|---|---|
| Lush cavern | 4 s | 70% | 35% | 55% | 15% | 45% |
| Insect swarm (discrete echoes) | 2.5 s | 0% | 30% | 35% | 0% | 55% |
| Metallic shimmer-box | 3 s | 40% | 20% | 5% | 30% | 50% |
| Endless drone (play then hold) | 12 s | 80% | 50% | 80% | 85% | 60% |

## Project layout

```
CMakeLists.txt            build script (fetches JUCE automatically)
Source/SwarmReverb.h      the reverb engine (plain C++, no JUCE)
Source/PluginProcessor.*  parameters, audio processing, state saving
Source/PluginEditor.*     the pedal UI
tests/test_reverb.cpp     offline stability tests + demo render
.github/workflows/        cloud build for Windows and macOS
```

To run the engine tests on their own:
`g++ -std=c++17 -O2 tests/test_reverb.cpp -o test && ./test`
