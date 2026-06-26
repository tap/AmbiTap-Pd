# AmbiTap-Pd

Pure Data externals for higher-order ambisonics (AmbiX: ACN ordering, SN3D
normalization), built as thin wrappers over the **AmbiTap** library. The whole
set ships as one library, `ambitap`, exposing `ambitap.*~` objects.

Uses Pd's **multichannel signals** (Pd ≥ 0.54): one patch cord carries the whole
`(order+1)²`-channel HOA bus, mirroring the Max target's MC objects.

## Status

All eleven objects build into the single `ambitap` library, each with a help
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

`decode~` layouts: `stereo` `quad` `surround_5_1` `surround_7_1` `surround_7_1_4`
`cube` `hexagon` `octagon` (`5.1`/`7.1`/`7.1.4` also accepted).

## Layout

```
AmbiTap-Pd/
├── Makefile              builds externals/ambitap.pd_darwin (universal)
├── pd/m_pd.h             vendored Pd 0.55 header (build needs no Pd binary)
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

```bash
make                       # -> externals/ambitap.pd_darwin (x86_64 + arm64)
# AmbiTap elsewhere? make AMBITAP_ROOT=/path/to/AmbiTap
```

Pd symbols are resolved at load time, so only the vendored `pd/m_pd.h` is needed
to build — no Pd installation required.

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

- Make AmbiTap a git submodule once this is a published repo; add CI.
