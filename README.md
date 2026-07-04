# AmbiTap-Pd

[![CI](https://github.com/tap/AmbiTap-Pd/actions/workflows/ci.yml/badge.svg)](https://github.com/tap/AmbiTap-Pd/actions/workflows/ci.yml)

Pure Data externals for higher-order ambisonics (AmbiX: ACN ordering, SN3D
normalization), built as thin wrappers over the **AmbiTap** library. The whole
set ships as one library, `ambitap`, exposing `ambitap.*~` objects.

Uses Pd's **multichannel signals** (Pd ≥ 0.54): one patch cord carries the whole
`(order+1)²`-channel HOA bus, mirroring the Max target's MC objects.

## Status

All sixteen objects build into the single `ambitap` library, each with a help
patch. Order is a creation argument unless noted; parameters arrive as float or
symbol messages on the left inlet.

| Object | Creation args | Messages |
|---|---|---|
| `ambitap.encode~` | `<order>` | `azimuth` `elevation` `gain` |
| `ambitap.rotate~` | `<order>` | `yaw` `pitch` `roll` |
| `ambitap.decode~` | `<order> <layout>` | `decoder_type` {`mode_match`,`allrad`,`epad`}, `max_re` |
| `ambitap.binaural~` | `<order>` (1–5) | `volume` `yaw` `pitch` `roll` `hrtf_dataset` {`ls`,`magls`} |
| `ambitap.mirror~` | `<order>` | `flip_lr` `flip_fb` `flip_ud` |
| `ambitap.format~` | `<order>` (0–3) | `direction` {`ambix_to_fuma`,`fuma_to_ambix`} |
| `ambitap.vmic~` | `<order>` | `azimuth` `elevation` `max_re` |
| `ambitap.directional~` | `<order>` | `azimuth` `elevation` `gain` |
| `ambitap.doppler~` | `<order>` | `distance` `speed_of_sound` `max_distance` |
| `ambitap.compress~` | `<order>` | `threshold` `ratio` `attack` `release` `makeup_gain` |
| `ambitap.energyvec~` | — | `smoothing_time` |
| `ambitap.bed2hoa~` | `<order> <layout>` | `gain` |
| `ambitap.distance~` | `<order>` | `distance` `reference_distance` `attenuation` `air_absorption` `speed_of_sound` `max_distance` `doppler` `nfc` |
| `ambitap.panbin~` | — | `azimuth` `elevation` `gain` |
| `ambitap.xtc~` | — | `span` `distance` `regularization` `bypass` |
| `ambitap.room~` | `<order>` (0–3) | `dim_x/y/z` `source_x/y/z` `listener_x/y/z` `rt60` `direct` `er` `tail` `gain` `rt60band <hz> <s>` `reflections <x0 x1 y0 y1 z0 z1>` |

`decode~` and `bed2hoa~` layouts: `stereo` `quad` `surround_5_1` `surround_7_1`
`surround_7_1_4` `cube` `hexagon` `octagon` (`5.1`/`7.1`/`7.1.4` also accepted).

The last five objects mirror the AmbiTap-Max object set: `bed2hoa~` (surround
bed → HOA), `distance~` (Doppler + 1/r gain + air absorption + NFC-HOA),
`panbin~` (direct per-source binaural, no ambisonic bus), `xtc~` (transaural
crosstalk cancellation for two loudspeakers), and `room~` (image-source early
reflections + SH-domain FDN reverb). `panbin~`, `xtc~`, and `room~` are
convolution-based: they (re)allocate for the host block size / sample rate when
the DSP graph is compiled and stay silent for non-power-of-two blocks. `xtc~`
takes two signal inlets (left/right program) and outputs two **loudspeaker**
feeds, not headphones.

## Layout

```
AmbiTap-Pd/
├── CMakeLists.txt        builds externals/ambitap.<pd_darwin|pd_linux|dll>
├── pd/m_pd.h             vendored Pd 0.55 header (mac/linux need no Pd binary)
├── submodules/AmbiTap    the core library (headers + Ooura FFT), a submodule
├── src/
│   ├── ambitap.<name>~.cpp   one file per class (+ its <name>_setup())
│   └── ambitap_setup.cpp     ambitap_setup() — registers every class
├── help/
│   ├── ambitap.<name>~-help.pd   right-click → Help on any object
│   └── gen_help.py               regenerates the help patches
└── externals/           built library
```

A single library file is used (rather than one external per object) so the
dotted class names work cleanly: Pd derives the setup symbol from the *filename*
(`ambitap` → `ambitap_setup`), and that function registers `ambitap.encode~`
etc. by symbol.

## Build

CMake, consuming the AmbiTap core as a submodule (its `AmbiTap::ambitap` target
supplies the headers, Eigen, the Ooura FFT, and C++20):

```bash
git submodule update --init --recursive        # fetch the AmbiTap core
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel                 # -> externals/ambitap.pd_<platform>
```

Outputs `externals/ambitap.pd_darwin` (macOS), `.pd_linux` (Linux), or `.dll`
(Windows). For a macOS universal binary add
`-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`. To build against a core checkout
outside the submodule, pass `-DAMBITAP_CORE_DIR=/path/to/AmbiTap`.

On macOS and Linux, Pd's own symbols resolve at load time, so the vendored
`pd/m_pd.h` is all that's needed — no Pd installation. **Windows** additionally
needs Pd's import library: pass `-DPD_LIB=/path/to/pd.lib` (from a Pd install's
`bin/`). Windows is not yet exercised in CI.

## Use in Pd (≥ 0.54)

Put this folder on Pd's search path (Preferences → Paths, or `-path`), then:

```
[declare -lib ambitap]
```

loads the library; now `[ambitap.encode~ 3]` and friends are available. The HOA
objects carry the whole `(order+1)²`-channel bus on one multichannel patch cord —
connect them to each other and to MC-aware objects.

## Help

Each object has a `help/ambitap.<name>~-help.pd` patch — right-click an object →
**Help**, or open them directly. Each opens with a runnable example chain (most
end in `binaural~ → dac~` so they're audible on headphones) and message boxes for
every parameter. They `[declare -lib ambitap]` themselves, so they work as soon
as the library is on Pd's path. Regenerate them with `python3 help/gen_help.py`.

## Roadmap

- SOFA HRTF selection for `binaural~` (a `sofa` message to load a user file),
  matching AmbiTap-Max. Deferred: it needs libmysofa built into the Makefile
  and a Pd search-path resolver.
