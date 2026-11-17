# Peak Control

An editor for the Novation Peak (firmware 2.1): a VST3 plugin plus a standalone app for Windows.
For Novation Peak; not affiliated with or endorsed by Novation / Focusrite.

Status: **v0.1.0**, tested on the synth (firmware 2.1.0.8): standalone and VST3, patch writes, lists, DAW automation
([docs/HARDWARE-TEST.md](docs/HARDWARE-TEST.md)).
Plan: [docs/PLAN.md](docs/PLAN.md) · research: [docs/RESEARCH.md](docs/RESEARCH.md) ·
code layout: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

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
- **Presets tab**: one list, two sources (never mixed).
  - **Peak**: Read bank / Read all (read only; each finished read also saves a dated backup to
    `Documents\Peak Control\Backups`), Load (program change), Save to file.
  - **File**: Open a .syx (a patch, a bank or a backup); Try (plays it through the edit buffer, memory untouched),
    Copy to Peak (one slot), Send bank to Peak (backs the bank up first, writes 128 patches, reads them back and checks).
- **Save** (top bar): the editor's patch into a slot you pick, after backing the slot up, then reading it back. Or to a
  .syx file.
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

- FM amounts and Arp Chance: written by patch rewrite (their NRPNs are ignored by firmware 2.1); to confirm by ear.
- Value list order for mod sources / destinations, FX matrix, category / genre (wavetables and delay sync are confirmed).

Confirmed on the synth: every panel CC / NRPN, the 0–255 CC pair coding, bank select (CC 32 = 1..4), slot reads,
edit-buffer writes, and the patch byte of all 260 stored parameters (`tools/mapper`).

## Building

No local compiler needed:

```sh
sh tools/test-docker.sh      # core tests (gcc in Docker)
sh tools/build-linux.sh      # compile the whole plugin on Linux + the page snapshot tool
```

Windows builds come from CI (`cmake -B build && cmake --build build --config Release`).

## Licence

AGPL-3.0, see [LICENSE](LICENSE). Fonts: Barlow, IBM Plex Mono (SIL Open Font License, `assets/fonts`).
