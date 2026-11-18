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
- **The editor and the Peak always play the same sound.** When they meet (the Peak connects or comes back), a DAW
  project that brought its own patch sends it to the Peak's edit buffer (never its memory); the standalone app and a
  new plugin copy take the Peak's sound instead. Every parameter can be automated.
- **Mod matrix**: 16 slots + 4 FX slots; drag a source (rail, envelope or LFO tab) onto a knob to fill a free slot;
  orange rings show modulated knobs.
- **Top bar**: ‹ patch › with a status LED (Saved / Edited · n / PC only / Sending / Not sent / Not saved; hover for
  details), **Fetch** (reload this slot as saved on the Peak), **Compare** (hold to hear the saved version),
  **Load** (a .syx: one patch goes into the editor and the Peak; a bank or backup opens on the Presets tab) and
  **Save** (into a slot on the Peak and the PC copy, after backing the slot up and reading it back; or on the PC only).
- **Presets tab**: one list, two sources (never mixed).
  - **Peak**: the PC copy of the Peak's 512 patches. First run offers to copy them all (read only). Read bank /
    Read all (each finished read also saves a dated backup to `Documents\Peak Control\Backups`), Load, Export.
    Slots saved on the PC only are marked; Write to Peak puts them on the synth. A slot the Peak switches to is read
    again quietly, so saves made on the Peak itself show up.
  - **File**: a .syx opened with Load; Try (plays it through the edit buffer, memory untouched),
    Copy to Peak (one slot), Send bank to Peak (backs the bank up first, writes 128 patches, reads them back and checks).
- **One copy talks to the Peak**: a second plugin copy in the same project shows a page that points to the first one.
- **Setup tab**: ports, MIDI channel, output straight to the port or through the DAW, a connection verdict, and a
  scrolling monitor of everything the Peak sends (Copy puts it on the clipboard).
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
