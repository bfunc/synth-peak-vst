# Peak Control — plan to v1

Editor (VST3 + standalone, Windows) for the Novation Peak, firmware 2.1. Research and decisions: [RESEARCH.md](RESEARCH.md).
Status 2026-11-15: steps 0, 4, 5, 6, 7, 8 written (code + CI, core tested, UI checked by snapshots). Waiting for the hardware test
([HARDWARE-TEST.md](HARDWARE-TEST.md)), which covers steps 1-3 and 9 in a safe order. Code layout: [ARCHITECTURE.md](ARCHITECTURE.md).

Defaults taken while building (change any): envelopes / LFOs as tabs; patch ‹ › = program change + fetch; Animate pads and
Master are not controls (no MIDI number known); menu-only parameters in a pop-out per panel (▶); hosts: any VST3 host, Windows.

## What I need from the user

### 1. Decisions (before the mockup)

- [ ] Mod Env 1 / 2: one at a time with a switch, or both shown?
- [ ] Animate 1/2 and Patch +/−: working controls or decoration?
- [ ] Master Volume: live control, not saved with the patch?
- [ ] Hidden (menu-only) parameters: where they live in the Serum / Vital-style layout?
- [ ] Hosts: FL Studio only or other DAWs too? Windows only for v1?
- [ ] Anything in MIDISynth-Peak to avoid or to keep?

### 2. Time at the synth (≈1.5–2 h, can be split)

- [ ] Settings check (5 min): CC/NRPN = Rec+Trans, MIDI channel, Local On; Components / MIDISynth closed so the port is free.
- [ ] Listening session (≈60 min): change each of ≈40 menu parameters by one step; log the NRPN sent and the patch bytes changed.
- [ ] Value lists (≈20 min): step through mod sources / destinations, FX destinations, arp types, routing, wavetables on the OLED; confirm names and order.
- [ ] Test sessions (≈15 min each): after the write tests and after the first app build.

### 3. Permissions (each only when we get there)

- [ ] Read all four banks (read-only, ≈1 min).
- [ ] First edit-buffer write (changes the current sound; listen for clicks, cut notes, arp restarts).
- [ ] One spare slot for the save test (e.g. D127), backed up first.

## Steps

| # | Step | Who | Gate |
|---|---|---|---|
| 0 ✓ | Repo skeleton: `synth-peak-vst` with CI from trigon6-vst (Windows zip on push), README, licence | me | user's go |
| 1 | Settings check + listening session → NRPN and byte offset for every parameter | user + me | user's time |
| 2 | Bank read → value ranges over 512 patches, 0–255 pairs, data for the Presets tab | me | user's OK |
| 3 | Write tests: edit buffer (sound side effects), save to the spare slot, read back | user + me | user's yes |
| 4 ✓ | Core in plain C++ with tests (Docker, no synth): parameter table, CC/NRPN + 14-bit coding, patch encode/decode, send queue | me | after 1–2 |
| 5a ✓ | Mockup: Serum / Vital-style main view in Peak colours (`panel-mockup.html`) | me → user review | decisions |
| 5b ✓ | Mockup: menu-only parameters, Mod page (16-slot + FX matrix), top bar | me → user review | 5a approved |
| 6 ✓ | JUCE port: panel from the mockup, live send / receive, Fetch, follow program changes, DAW state save / restore | me | 5b approved |
| 7 ✓ | Presets tab (4 × 128, names, category / genre, filter) + Save panel (slot with backup, or .syx) | me | 3 done |
| 8 ✓ | Hardening: connection checklist (port busy, wrong channel, CC/NRPN off), gentle close, port lost and back | me | — |
| 9 | Release test: standalone + VST3 in the user's DAW from a checklist; fixes; tag `v0.1.0` | user | user says it works |

Steps 1–3 come first: they decide how the core and the UI behave (e.g. whether hidden parameters can be live).
Steps 4 and 5a can run alongside them.

## Limits on my side

- No local Windows compiler: Windows builds come from CI only.
- I can't hear the synth: every sound check is the user's.
- Nothing is written to the synth's memory and no sound-changing message is sent without the user's yes.

## Not in v1

Randomise / morph, XY performance, favourites, user wavetables, global settings, V-Macro page, Summit, Mac.
