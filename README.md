# Peak Control

An editor for the Novation Peak (firmware 2.1): a VST3 plugin plus a standalone app for Windows.
For Novation Peak; not affiliated with or endorsed by Novation / Focusrite.

Status: **first full build, not yet tested on the synth** (see [docs/HARDWARE-TEST.md](docs/HARDWARE-TEST.md)).
Plan: [docs/PLAN.md](docs/PLAN.md) · research: [docs/RESEARCH.md](docs/RESEARCH.md) ·
code layout: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) · UI preview: [docs/panel-mockup.html](docs/panel-mockup.html).

## What it does

- **Main / FX / Matrix tabs**: every patch parameter of the Peak (262) as knobs, buttons and lists, laid out like the
  mockup. Settings that only live in the Peak's menus sit behind each panel's ▶.
- **Live**: a change goes to the synth at once as CC or NRPN; knobs turned on the Peak move the editor.
  The few parameters with no MIDI number (FX mod matrix) are sent by rewriting the patch into the edit buffer.
- **DAW**: every parameter can be automated; the project remembers the patch and the settings, and does not
  overwrite the synth on load (use Send for that).
- **Mod matrix**: 16 slots + 4 FX slots; drag a source (rail, envelope or LFO tab) onto a knob to fill a free slot;
  orange rings show modulated knobs.
- **Fetch / Send**: read the Peak's current patch, or write the editor's into its edit buffer (not its memory).
- **Presets tab**: read the four banks (read only), search, filter by category, load (program change),
  import / export .syx.
- **Save**: into a slot you pick, after reading the slot's old patch into Documents/Peak Control/Backups, then
  reading it back to check. Or to a .syx file.
- **Setup tab**: ports, MIDI channel, output straight to the port or through the DAW, a connection checklist.
- **Playheads (off by default)**: an "approx" dot on LFO 1/2 in Sync range, following the DAW; a gate light on the
  envelopes for notes passing through the plugin.

## Download

Every push to `main` builds a zip on GitHub Actions: **Releases → latest**. It holds `Peak Control.vst3`,
`Peak Control.exe` (standalone) and `VERSION.txt`. Copy the `.vst3` to `C:\Program Files\Common Files\VST3`.

## Peak settings

| Setting | Value | Why |
|---|---|---|
| CC/NRPN | **Rec+Trans** | the editor sends and receives both |
| MIDI channel | the same as on the Setup tab | |
| Novation Components | closed | Windows lets one program at a time use the Peak's port |

## What is still provisional

To be confirmed on the synth (see the hardware test):

- Patch byte offsets: only name, category / genre, Voice, the mod and FX matrices and the NRPN-MSB-25 block are
  mapped. The rest is sent live but not read from a patch yet (Fetch leaves those knobs as they were).
- The 0–255 CC pair coding, the bank numbering of program changes, the reply format of slot requests.
- Value list order for mod sources / destinations, FX matrix, wavetables, category / genre, delay sync.

## Building

No local compiler needed:

```sh
sh tools/test-docker.sh      # core tests (gcc in Docker)
sh tools/build-linux.sh      # compile the whole plugin on Linux + the page snapshot tool
```

Windows builds come from CI (`cmake -B build && cmake --build build --config Release`).

## Licence

AGPL-3.0, see [LICENSE](LICENSE). Fonts: Barlow, IBM Plex Mono (SIL Open Font License, `assets/fonts`).
