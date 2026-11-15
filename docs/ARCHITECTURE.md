# Peak Control — architecture

How the code is split, which thread does what, and how to add things without touching unrelated code.

## Layers

```
src/core     plain C++ (no JUCE), unit-tested in tests/CoreTests.cpp
src/ui/Layout, src/ui/ModMap   UI data (no JUCE), tested with the core
src/link     PeakLink: the one shared owner of the Peak's MIDI ports
src/plugin   the processor and the services it is made of
src/ui       JUCE components: theme, widgets, graphs, pages, editor
```

A layer only uses the layers above it in this list. `core` knows nothing about JUCE; `link` knows `core`;
`plugin` knows `link` and `core`; `ui` knows all of them.

| File | Job | Talks to |
|---|---|---|
| `core/ParamTable` | every patch parameter: id, name, CC / NRPN, range, default, value names, patch byte | — |
| `core/Midi` | CC, 0–255 CC pairs, NRPN encode / decode | ParamTable |
| `core/Sysex` | requests, the 527-byte patch message, values in the 510 data bytes | ParamTable |
| `core/Patch` | values ⇄ patch bytes, keeping bytes the table does not map | Sysex |
| `core/Sync` (+ `SendQueue`) | which changes go out live, throttled; patch-only ones reported | Midi |
| `core/Timing` | patch-rewrite throttle, bank fetch plan, gate tracker, LFO sync phase | — |
| `core/SlotWrite` | save to a slot: backup → write → read back → compare | Sysex |
| `core/Library` | 4 × 128 patches, filter, .syx files | Patch |
| `core/Link` | port matching, program change, connection checklist | — |
| `ui/Layout` | what each panel shows; everything else goes into the panel's ▶ pop-out | ParamTable |
| `ui/ModMap` | mod destination ⇄ knob, drop-a-source logic | ParamTable |
| `link/PeakLink` | ports (shared by all plugin copies), host / direct output, incoming decode | core |
| `plugin/ParamBridge` | table ⇄ JUCE parameters (the only place that knows JUCE's storage) | JUCE |
| `plugin/PatchSession` | current patch bytes, origin, Fetch / Send / program change, rewrites | PeakLink, `SessionHost` |
| `plugin/LibraryService` | bank reading, cache, files, careful save | PeakLink, PatchSession |
| `plugin/PeakProcessor` | wires the above; threads; DAW state | everything in plugin |
| `plugin/UiFeed` | transport + held notes for the UI, atomics only | — |
| `ui/Widgets` | knob, segments, toggle, choice, stepper, bar — bound by parameter id | ParamBridge |
| `ui/PanelKit` | WidgetSet, PanelFrame (+ ▶ pop-out), MoreGrid, DragSource | Widgets, Layout |
| `ui/Graphs` | wave, filter, envelope, LFO (+ playhead), FX pictures | ParamBridge, UiFeed |
| `ui/MainPage`, `FxMatrixPages`, `LibrarySetupPages` | the five tabs | PanelKit, services |
| `ui/PeakEditor` | top bar, page switching, scaling, mod rings, save pop-out | pages |

## Threads

| Thread | Does | Never |
|---|---|---|
| audio (`processBlock`) | transport → UiFeed; notes → gate + pass-through; host mode: live CCs into the host buffer | waits (only try-locks), allocates |
| pump (`PeakProcessor::Pump`) | direct mode: live CCs and passed notes to the port | touches the UI |
| MIDI input (PeakLink) | decodes CC / NRPN / SysEx into rings | sends |
| message (timers) | applies synth changes, PatchSession / LibraryService ticks, ports, UI refresh | blocks on the audio thread |

`syncLock` guards the SyncEngine; the audio thread only try-locks it.

## Adding things

- **A parameter** (e.g. after the listening session finds its NRPN): add one line in `core/ParamTable.cpp`
  with its address and, if known, `offset`. It becomes a DAW parameter, is sent / decoded / read from patches,
  and appears in its panel's ▶ pop-out automatically (`ui::moreParams`). To show it on a panel, add an `Item` in
  `ui/Layout.cpp` and place it in that panel's `resized()`. Tests check ids, ranges, value names and coverage.
- **A patch byte offset**: set `offset` on the parameter (one byte, or two for 0–255). `readValues` / `writeValues`
  pick it up; the tests check that no two parameters share a byte.
- **A value list**: edit the `Names` list in `ParamTable.cpp`; the test checks it has one name per value.
- **A panel**: a `PanelSpec` in `Layout.cpp` (items + which parameters it owns) and a `PanelFrame` subclass that
  lays them out. Widgets are made by `WidgetSet::add`; never construct a widget for a table parameter by hand.
- **A page**: a component (implement `Refreshable` if it animates) added to the `pages` list in `PeakEditor`.
- **A new kind of message to the synth**: build the bytes in `core` (tested), send through `PeakLink`, decide
  when in a plugin service. Anything that writes the synth's memory goes through `SlotWrite`.

## Checks

- `sh tools/test-docker.sh` — core tests (gcc, `-Werror`), seconds.
- `sh tools/build-linux.sh` — compiles the whole plugin on Linux and builds `peak_snapshot`.
- `peak_snapshot <folder>` (in the same container) — draws every page to PNG with the test dump loaded.
- CI (`.github/workflows/build.yml`) — Windows build, tests, zip, "latest" pre-release.
