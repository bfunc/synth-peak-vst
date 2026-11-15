# Peak Control — research notes

Editor (VST3 + standalone) for the Novation Peak, built like Trigon6 Control (`D:\Projects\hardware-synth-editors\sequential-trigon6-vst`).
Started 2026-11-13. Status: **core (parameter table, MIDI, SysEx) written and tested 2026-11-15; no plugin yet.**

## Decisions so far

| Topic | Decision |
|---|---|
| Repo | `D:\Projects\hardware-synth-editors\novation-peak-vst`, github `bfunc/synth-peak-vst`, AGPL (local git since 2026-11-15; GitHub not created yet) |
| Name | Peak Control |
| Scope v1 | Main panel + Mod page (16-slot mod matrix, FX matrix) |
| Style | Serum / Vital-style software layout in Peak colours, not a copy of the hardware front panel (no Novation logo, no "PEAK" wordmark) |
| First step | Web mockup `docs/panel-mockup.html`, JUCE port only after approval |
| Synth | User's Peak, firmware 2.1.0.8, USB, connected to Home |
| Safety | No writes to the synth's memory, no sound-changing messages, without the user's yes |

Later (not v1): randomise / morph, XY performance, favourites, user wavetables, global settings, V-Macro page, Summit.

## Sources

- Peak User Guide 1.2 (the newest English guide): https://files.kraftmusic.com/media/ownersmanual/Novation_Peak_User_Guide.pdf
  (same file as https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Peak%201.2%20Manual%20-%20English.pdf).
  MIDI parameters list on pp. 40–43, mod sources/destinations pp. 39–40, wavetables p. 37.
- Firmware 2.0 addendum: https://fael-downloads-prod.focusrite.com/customer/prod/downloads/Summit%20%26%20Peak%202.0%20Firmware%20Update%20Addendum%20V1%20English%20-%20EN.pdf
- Firmware 2.1 addendum (covers 2.0 + 2.1, has a MIDI list on p. 18): https://fael-downloads-prod.focusrite.com/customer/prod/downloads/summit_peak_2.1_firmware_update_addendum_v1_english_en.pdf
- Downloads page (no SysEx / MIDI spec there): https://downloads.novationmusic.com/novation/synthesisers/peak
- KnobKraft Orm, `adaptations/Novation_Summit.py` (AGPL, header and commands): https://github.com/christofmuc/KnobKraft-orm
- Pistol Peak Editor (web, **proprietary**: facts only, never code): https://peakeditor.pistolinstruments.com/ (format notes in `/llms-full.txt`)
- Gearspace Peak thread p. 96 (edit buffer request, "no checksum"; blocks automated fetch): https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1149964-novation-peak-96.html
- Competitor: MIDISynth-Peak 2.0 (screenshot from the user).

## Confirmed on the user's Peak (2026-11-13, read-only)

- Windows ports: one input and one output, both named `Peak`.
- Identity request `F0 7E 7F 06 01 F7` → `F0 7E 7F 06 02 00 20 29 7E 00 00 00 02 01 00 08 F7`
  (Novation, family `7E 00`, member `00 00`, firmware **2.1.0.8**).
- Current patch request `F0 00 20 29 01 10 00 7E 40 00 00 00 00 00 F7` → one **527-byte** message,
  header `F0 00 20 29 01 10 00 7E 00 00 00 00 00 00 00 00`, then 510 data bytes, then `F7`.
  Saved as `dumps/2026-11-13-editbuffer-perc-up-geo.syx` (patch "Perc Up GEO").
- The Summit header (`01 11 01 33`) gets no answer.

Confirmed 2026-11-15 (probe, read only; the user turned knobs on the Peak):

- The user's Peak sends on **MIDI channel 13** (the editor now follows the channel it hears).
- Slot request `41 00 00 00 <bank> <slot>` → reply cmd `01`, bank byte = request's bank (1 = A), slot byte; bank 0 → the
  edit buffer (cmd `00`). A 000 = "Prophet 5 GEO", A 005 = "Shifter GEO".
- 0–255 CC pairs: MSB CC first = value >> 1, then LSB CC = 0 or 64 (low bit << 6). Cutoff CC 29/61, Osc 1 Coarse CC 14/46.
- Resonance CC 79, Osc 1 Range CC 3 (63–66), Filter Shape NRPN 0:46 (0–2) sent as CC 99 / 98 / 6 with no CC 38.
- Panel controls rechecked one by one (groups 1-4): envelopes (Amp 86-89, Mod 1 90-93, Mod 2 94/95/117/103, Mod Env
  select NRPN 0:59), oscillators (Fine 15/47, Coarse 14/46 · 17/49 · 20/52, Shape source NRPN 0:16, Wave NRPN 0:14,
  Env2 / LFO2 pitch 9 / 16+48), mixer 23-27 + 55-59, filter (Overdrive 80, Key track 75, LFO1 28/60, Osc3 76, Env
  select NRPN 0:47, Slope NRPN 0:45), LFO rates 30/62 · 31/63, LFO1 Range NRPN 0:68, LFO2 Range CC 83, LFO2 Wave NRPN
  0:78, FX (Dist 104, Chorus 105, Delay 108 / 110, Reverb 112 / 113), Glide 5 / 35, Arp on / latch NRPN 0:121 / 0:122,
  Arp gate 116: all as in the manual. (The earlier "CC 88 from Attack" was the Sustain fader.)
- Corrections to the manual: the panel **Shape** knob sends the amount of the selected Shape Source (CC 12 / 119 / 33
  for Osc 1); the filter **Env Depth** knob sends CC 77 or 78 by the Env Select; **Delay Sync Time** is 16 values
  (NRPN 0:94 = 0..15, "4 beats".."32nd T"); **Animate 1 / 2** send CC 114 / 115 = 127 on press, 0 on release.
- **Program change from the panel: CC 32 = bank (1..4 = A-D), then Program Change**; no CC 0.
- Unknown: NRPN 62:0 (0 / 1) comes with the wave button and with patch changes; the editor ignores it.
- V-Sync is menu-only on the Peak.

## SysEx patch format

```
F0 00 20 29 01 10 00 7E <cmd> 00 00 00 <bank> <slot> 00 00 <510 data bytes> F7     = 527 bytes
```

| cmd | Meaning | Status |
|---|---|---|
| `40` | request current patch (edit buffer); short form `F0 … 7E 40 00 00 00 00 00 F7` | confirmed |
| `00` | patch to edit buffer (also the reply to `40`) | reply confirmed, write not tried |
| `41` | request one patch: `… 41 00 00 00 <bank 1-4> <slot 0-127> F7` | community |
| `01` | write patch to bank/slot | community, writes memory |
| `4A` | request a whole bank (128 messages) | community |
| `44` | global settings dump | community |
| `46` `47` `06` `07` `49` | user wavetables (request, names, write, rename, commit) | community, experimental |

- No checksum (Gearspace, Pistol).
- Banks A–D = 1–4, 128 slots each.
- Data values: same ranges as the CC/NRPN values. 0–255 values take two bytes (split 7-bit; Pistol's example: 173 = `56 40`, i.e. high 7 bits then low bit << 6 — to verify).

### Data block map (offsets in the 510 data bytes; absolute = +16)

**Measured 2026-11-15** (tools/mapper, `kMeasuredOffsets` in ParamTable.cpp): 156 parameters found by setting each to two
values and diffing edit-buffer dumps; every older offset confirmed. The layout follows the manual's MIDI table order:
Voice 32-39, osc common 41-44, Osc 1 46-64, Osc 2 67-85, Osc 3 88-106, mixer, filter 123-137, envelopes, LFOs, FX
(routing 200, delay 202-212, reverb 215-225, chorus 227-234), arp, mod matrix 250-313, FX matrix 314-329, NRPN-25
block 346 + LSB. Checked against all 512 stored patches: values in range except Arp Chance 0 (old patches), user
wavetables (64-73) and one Delay Sync Time 17.
Not live: FM 3>1 / 1>2 / 2>3 and Arp Chance ignore their addendum NRPNs (25:13/17/21/33); the editor writes them by
patch rewrite at 346 + LSB (to confirm by ear). Animate 1/2 are not stored.

Older notes (before measuring):

| Bytes | Content | Status |
|---|---|---|
| 0–15 | patch name, 16 ASCII, space padded | confirmed |
| 16 | category (0–14) | likely (6 here) |
| 17 | genre (0–9) | likely |
| 18 | format version? | community |
| 32–39 | Voice: mode 3, unison 0, detune 25, spread 2, octave 64, glide time 60, pre-glide 64, glide on 0 — the manual table's order | likely |
| 40–245 | oscillators, mixer, filter, envelopes, LFOs, FX, arp | to map |
| 235–236 | tempo, 14-bit, BPM ÷ 2? | community |
| 250–313 | mod matrix, 16 slots × 4: source A, source B, depth (64 = 0), destination; empty slot = `00 00 40 00` | likely |
| 314–329 | FX mod matrix, slots × 4 (same layout) | likely |
| 330–345 | more `00 00 40 00` slots — more FX slots? | unknown |
| 346–391 | NRPN MSB 25 parameters, offset = 346 + LSB | community |
| 395–410 | a second name: `Init Patch` | unknown |
| 411–414 | `64 127 0 60` | unknown |
| 415–509 | zero | spare? |

Dump of "Perc Up GEO" (decimal, 16 per row):

```
  0:  80 101 114  99  32  85 112  32  71  69  79  32  32  32  32  32
 16:   6   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0
 32:   3   0  25   2  64  60  64   0   0  19  14 127   0   0  63  64
 48:   0  64   0  64  64   0   0  28   2  64  64  36  23   0  64   0
 64:  76   0   0  66  64   0  64   0  64  64   0   4  35   2 100  64
 80:  88   2   0  64   0  76   0   0  66 127  64  64   0  64  64   0
 96:   3   4   2  64  64  43   0   0  64   0  76   0   0   0   0   0
112:   0   0   0   0   0 127  64  64 123 127 127  28   0   1   2 107
128:   0  99   0  64   0 127   1  64  92   0   0   0  64   0   0   0
144:   1  79  71   9  64   0   0   0   0   0  50   9   0  96   1   0
160:   0   2  75  35  45  64   1   0   0   0  64   0  16   0   0   0
176:   0   0   1   0   0   0   0   0  91   0  16   0   0   0   0   0
192:   1   0   0   0   0   0   0   0   0   0   0  64   1 127   1   6
208:   0  80   7   0   0   0   0   0   1  90  50   1  64  64   4  74
224:   0  40   0 102   1  18 100  64  85  16   0  60   0   3   0   0
240:   0  64  50   0   0   0   0   0   0   3   0   0 113  33   1   0
256:  70  18  15   0  76   4  10   2 105  38   0   0  64   0   0   0
272:  64   0   0   0  64   0   0   0  64   0   0   0  64   0   0   0
288:  64   0   0   0  64   0   0   0  64   0   0   0  64   0   0   0
304:  64   0   0   0  64   0   0   0  64   0   1   0  95   3   1   0
320:  73   2   7   0  74   5   0   0  64   0   0   0  64   0   0   0
336:  64   0   0   0  64   0   0   0  64   0   0  64  17   0  64  17
352:   0   0   0   0  64   0   0   0   0   0   0   0   0   0   0   0
368:   0   0   0   0   0   0   0   0   0   0   0 100   0   0   0   0
384:   0   0   0   0   0   0   0   0   0   0   0  73 110 105 116  32
400:  80  97 116  99 104  32  32  32  32  32  32  64 127   0  60   0
416–509: all 0
```

Note: the mod matrix slots line up at 250 (`0 0 113 33 | 1 0 70 18 | 15 0 76 4 | 10 2 105 38 | 0 0 64 0 …`);
destination 38 > 36 is fine on 2.x (new destinations).

## MIDI parameters (manual 1.2 + 2.1 addendum)

NRPN: CC 99 = MSB, CC 98 = LSB, CC 6 = value (CC 38 = optional LSB). "CC pair a,b": a = MSB, b = LSB of a 0–255 value.
Ranges as printed; the manual has errors (see "Known errors").

### Voice / Glide

| Parameter | Type | Number | Range | Default |
|---|---|---|---|---|
| Patch Category | NRPN | 0:0 | 0–14 | 0 |
| Patch Genre | NRPN | 0:1 | 0–9 | 0 |
| Voice Mode | NRPN | 0:2 | 0–4 | 3 |
| Voice Unison | NRPN | 0:3 | 0–4 | 0 |
| Unison Detune | NRPN | 0:4 | 0–127 | 25 |
| Spread | NRPN | 0:5 | 0–127 | 0 |
| Keyboard Octave | NRPN | 0:6 | 61–67 (−3..+3) | 64 |
| Glide Time | CC | 5 | 0–127 | 60 |
| Pre-Glide | NRPN | 0:7 | 52–76 (−12..+12) | 64 (Off) |
| Glide On | CC | 35 | 0–1 | 0 |
| Pan Position (2.x) | NRPN | 0:51 | 0–127 | 0 |
| Spread Mode (2.x): Diverge, Alternate, Diverge 2, NoteVal | NRPN | 0:52 | 0–3 | 0 |

### Oscillator common

| Parameter | Type | Number | Range | Default |
|---|---|---|---|---|
| Divergence | NRPN | 0:9 | 0–127 | 0 |
| Drift | NRPN | 0:10 | 0–127 | 0 |
| Noise LPF | NRPN | 0:11 | 0–127 | 127 |
| Noise HPF | NRPN | 0:12 | printed "0-0", really 0–127 | 0 |
| FM Osc3→1 (2.0) | NRPN | 25:13 | 0–127 | |
| FM Osc1→2 (2.0) | NRPN | 25:17 | 0–127 | |
| FM Osc2→3 (2.0) | NRPN | 25:21 | 0–127 | |

### Oscillators 1 / 2 / 3

| Parameter | Osc 1 | Osc 2 | Osc 3 | Range | Default |
|---|---|---|---|---|---|
| Range (16' 8' 4' 2') | CC 3 | CC 37 | CC 65 | 63–66 | 64 |
| Coarse | CC 14,46 | CC 17,49 | CC 20,52 | 0–255 (−128..+127) | 128 |
| Fine | CC 15,47 | CC 18,50 | CC 21,53 | 28–228 (−100..+100) | 128 |
| ModEnv2 → Pitch | CC 9 | CC 38 | CC 43 | 1–127 (−63..+63) | 64 |
| LFO2 → Pitch | CC 16,48 | CC 19,51 | CC 22,54 | 1–255 (−127..+127) | 128 |
| Wave (sine, tri, saw, pulse, more) | NRPN 0:14 | NRPN 0:23 | NRPN 0:32 | 0–4 | 2 |
| Wave More (wavetable) | NRPN 0:15 | NRPN 0:24 | NRPN 0:33 | 4–63 | 4 |
| Shape Source (Manual, ModEnv1, LFO1) | NRPN 0:16 | NRPN 0:25 | NRPN 0:34 | 0–2 | 0 |
| Manual Shape | CC 12 | CC 39 | CC 71 | 0–127 (−64..+63) | 64 |
| ModEnv1 → Shape | CC 119 | CC 40 | CC 72 | 0–127 (−64..+63) | 64 |
| LFO1 → Shape | CC 33 | CC 41 | CC 73 | 0–127 (−64..+63) | 64 |
| Vsync | CC 34 | CC 42 | CC 44 | 0–127 | 0 |
| Saw Density | NRPN 0:17 | NRPN 0:26 | NRPN 0:35 | 0–127 | 0 |
| Saw Density Detune | NRPN 0:18 | NRPN 0:27 | NRPN 0:36 | 0–127 | 64? |
| Fixed Note | NRPN 0:19 | NRPN 0:28 | NRPN 0:37 | 0–88 (0 = Off) | 0 |
| Bend Range | NRPN 0:20 | NRPN 0:29 | NRPN 0:38 | 40–88 (−24..+24) | 76 (+12) |

### Mixer

| Parameter | Type | Number | Range | Default |
|---|---|---|---|---|
| Osc 1 | CC pair | 23,55 | 0–255 | 255 |
| Osc 2 | CC pair | 24,56 | 0–255 | 0 |
| Osc 3 | CC pair | 25,57 | 0–255 | 0 |
| Ring 1×2 | CC pair | 26,58 | 0–255 | 0 |
| Noise | CC pair | 27,59 | 0–255 | 0 |
| Patch Level | NRPN | 0:41 | 0–127 | 64 |
| VCA Gain | NRPN | 0:42 | 0–127 | 127 |
| FX Dry Level | NRPN | 0:43 | 0–127 | 127 |
| FX Wet Level | NRPN | 0:44 | 0–127 | 127 |

### Filter

| Parameter | Type | Number | Range | Default |
|---|---|---|---|---|
| Overdrive | CC | 80 | 0–127 | 0 |
| Post Drive (distortion) | CC | 36 | 0–127 | 0 |
| Slope (12 / 24 dB) | NRPN | 0:45 | 0–1 | 1 |
| Shape (LP, BP, HP) | NRPN | 0:46 | 0–2 | 0 |
| Key Tracking | CC | 75 | 0–127 | 127 |
| Resonance | CC | 79 | 0–127 | 0 |
| Frequency | CC pair | 29,61 | 0–255 | 255 |
| LFO1 → Filter | CC pair | 28,60 | 1–255 (−127..+127) | 128 |
| Osc3 → Filter | CC | 76 | 0–127 | 0 |
| Env Select (Amp Env, Mod Env 1) | NRPN | 0:47 | 0–1 | 1 |
| AmpEnv → Filter | CC | 77 | 1–127 (−63..+63) | 64 |
| ModEnv1 → Filter | CC | 78 | 1–127 (−63..+63) | 64 |
| Divergence | NRPN | 0:48 | 0–127 | 0 |

### Envelopes

| Parameter | Amp | Mod 1 | Mod 2 | Range | Default (Amp / Mod) |
|---|---|---|---|---|---|
| Attack | CC 86 | CC 90 | CC 94 | 0–127 | 2 / 2 |
| Decay | CC 87 | CC 91 | CC 95 | 0–127 | 90 / 75 |
| Sustain | CC 88 | CC 92 | CC 117 | 0–127 | 127 / 35 |
| Release | CC 89 | CC 93 | CC 103 | 0–127 | 40 / 45 |
| Velocity | NRPN 0:55 | NRPN 0:60 | NRPN 0:64 | 0–127 (−64..+63) | 64 |
| Trigger (Legato / Re-Trig) | NRPN 0:56 | NRPN 0:61 | NRPN 0:65 | 0–1 | 0 / 1 |
| Hold Time | ? | ? | ? | 0–127 | 0 |
| Repeats | ? | ? | ? | Off, 1–? | Off |
| Delay (2.x) | ? | ? | ? | 0–127 | 0 |

Mod Envelope Select (which env the faders show): NRPN 0:59, 0–1.
Animate envelopes (2.x): Animate 1 Att / Rel NRPN 25:34 / 25:35, Animate 2 Att / Rel 25:36 / 25:37, 0–127.

### LFOs

| Parameter | LFO 1 | LFO 2 | Range | Default |
|---|---|---|---|---|
| Range (Low, High, Sync) | NRPN 0:68 | CC 83 | 0–2 | 0 |
| Rate | CC 30,62 | CC 31,63 | 0–255 | 128 |
| Sync Rate | CC 81 | CC 84 | 0–34 | 16 / 12 |
| Wave (Tri, Saw, Square, S&H) | NRPN 0:69 | NRPN 0:78 | 0–3 | 0 |
| Phase | NRPN 0:70 | NRPN 0:79 | 0–120 (Free + degrees?) | 0 |
| Slew | NRPN 0:71 | NRPN 0:80 | 0–127 | 0 |
| Fade Time | CC 82 | CC 85 | 0–127 | 0 |
| Fade mode (Fade In, Fade Out, Gate In, Gate Out) | NRPN 0:72 | NRPN 0:81 | 0–3 | 0 |
| One Shot | NRPN 0:75 | NRPN 0:84 | 0–1 | 0 |
| Common | NRPN 0:76 | NRPN 0:85 | 0–1 | 0 |
| Fade Sync, MonoTrig, Repeats | ? | ? | | |

LFO 3 / 4 (global, not per voice): Phase 25:38 / 25:41, Slew 25:39 / 25:42, Fade Time 25:40 / 25:43.
Wave, Rate, Rate Sync of LFO 3/4: no published number.

### Effects

| Parameter | Type | Number | Range | Default |
|---|---|---|---|---|
| Distortion Level | CC | 104 | 0–127 | 0 |
| FX Bypass | NRPN | 0:88 | 0–1 | 0 |
| FX Routing (Parallel, …) | NRPN | 0:89 | 0–6 | 0 |
| Delay Level | CC | 108 | 0–127 | 0 |
| Delay Time | CC | 109 | 0–127 | 64 |
| Delay Width | NRPN | 0:92 | 0–127 | 127 |
| Delay Sync | NRPN | 0:93 | 0–1 | 0 |
| Delay Sync Time | NRPN | 0:94 | 0–18 | 4th T |
| Delay Feedback | CC | 110 | 0–127 | 64 |
| Delay LP Damp | NRPN | 0:95 | 0–127 | 85 |
| Delay HP Damp | NRPN | 0:96 | 0–127 | 0 |
| Delay Slew Rate | NRPN | 0:97 | 0–127 | 32 |
| Delay TimeMode (Normal, Double, Treble, QuadLoFi, HexVLoFi) (2.x) | NRPN | 0:98 | 0–4 | 0 |
| Delay L/R Ratio, Style (Original / CrossFed / Dual), Output (PreDamp / PostDamp) | ? | ? | | 1/1 |
| Reverb Level | CC | 112 | 0–127 | 0 |
| Reverb Type (1–3) | NRPN | 0:101 | 0–2 | 2 |
| Reverb Time | CC | 113 | 0–127 | 90 |
| Reverb Damp LP | NRPN | 0:102 | 0–127 | 50 |
| Reverb Damp HP | NRPN | 0:103 | 0–127 | 1 |
| Reverb Size | NRPN | 0:104 | 0–127 | 64 |
| Reverb Mod | NRPN | 0:105 | 0–127 | 64 |
| Reverb Mod Rate | NRPN | 0:106 | 0–127 | 4 |
| Reverb Low Pass | NRPN | 0:107 | 0–127 | 74 |
| Reverb High Pass | NRPN | 0:108 | 0–127 | 0 |
| Reverb Pre Delay | NRPN | 0:109 | 0–127 | 40 |
| Chorus Level | CC | 105 | 0–127 | 0 |
| Chorus Type (1–3) | NRPN | 0:111 | 0–2 | 2 |
| Chorus Rate | CC | 118 | 0–127 | 20 |
| Chorus Mod Depth | NRPN | 0:112 | 0–127 | 64 |
| Chorus Feedback | CC | 107 | 0–127 (−64..+63) | 64 |
| Chorus LP | NRPN | 0:113 | 0–127 | 90 |
| Chorus HP | NRPN | 0:114 | 0–127 | 2 |
| Chorus Mode (Chorus, Phlanger, Flanger) (2.x) | NRPN | 0:115 | 0–2 | 0 |

### Arp / Clock / Animate

| Parameter | Type | Number | Range | Default |
|---|---|---|---|---|
| Clock Rate (BPM) | — | none | 40–240 | 120 |
| Sync Rate | NRPN | 0:116 | 0–18 | 16th |
| Type (Up, Down, UpDown, UpDown2, Played, Random, Chord; 2.x adds Chord2) | NRPN | 0:117 | 0–6 (0–7?) | 0 |
| Rhythm | NRPN | 0:118 | 0–32 | 0 |
| Octaves | NRPN | 0:119 | 0–5 | 1 |
| Gate | CC | 116 | 0–127 | 64 |
| Swing | NRPN | 0:120 | 20–80 | 50 |
| On | NRPN | 0:121 | 0–1 | 0 |
| Key Latch | NRPN | 0:122 | 0–1 | 0 |
| Key Sync | NRPN | 0:123 | 0–1 | 0 |
| Arp Chance (2.x) | NRPN | 25:33 | 1–100 | 100 |
| Animate 1 Hold | CC | 114 | 0–1 | 0 |
| Animate 2 Hold | CC | 115 | 0–1 | 0 |

### Mod matrix

| Parameter | Type | Number | Range |
|---|---|---|---|
| Selected slot | NRPN | 0:125 | 0–15 |
| Slot n Source A | NRPN | n:0 (n = 1..16) | 0–16 printed (more on 2.x) |
| Slot n Source B | NRPN | n:1 | as above |
| Slot n Depth | NRPN | n:2 | 0–127 (−64..+63) |
| Slot n Destination | NRPN | n:3 | 0–36 printed (more on 2.x) |

FX mod matrix (4 slots): no published NRPN.

### Global (not saved in the patch)

Patch Cue NRPN 64:0 (0–1), Aftertouch scale 64:3 (1–10), Tuning table 25:6 (0–16).

### Parameters with no published number

Env Hold / Repeats / Delay (×3), LFO 1/2 Fade Sync / MonoTrig / Repeats, LFO 3/4 Wave / Rate / Sync,
FX mod matrix, Delay Ratio / Style / Output, Clock Rate.

Unused NRPN numbers in MSB 0 (candidates): 0:8, 0:13, 0:21–22, 0:30–31, 0:39–40, 0:49–50, 0:53–54, 0:57–58,
0:62–63, 0:66–67, 0:73–74, 0:77, 0:82–83, 0:86–87, 0:90–91, 0:99–100, 0:110, 0:124; and most of MSB 25.

### Known errors in the manual's table

- Noise HPF printed as "0-0".
- LFO 1 Range is NRPN 0:68, LFO 2 Range is CC 83: asymmetric, check.
- Saw Density Detune default: 0 in the MIDI table, 64 in the Init Patch list.
- Osc 3 Range default printed 64 (−1).
- Mod source range 0–16 and destination range 0–36 predate 2.0 (Noise source; PanPosn, O123Shpe, env Delay/Hold/Sustain destinations added).

## Value lists

**Mod matrix sources (1.2 order):** Direct, ModWheel, AftTouch, ExprPED1, BrthPED2, Velocity, Keyboard, Lfo1+, Lfo1+/-,
Lfo2+, Lfo2+/-, AmpEnv, ModEnv1, ModEnv2, Animate1, Animate2, CV +/-, Lfo3 +, Lfo3 +/-, Lfo4 +, Lfo4 +/-, BndWhl+, BndWhl-;
2.0 adds Noise. Real order and numbers: read off the synth.

**Mod matrix destinations (1.2):** O123Ptch, Osc1Ptch, Osc2Ptch, Osc3Ptch, Osc1VSnc, Osc2VSnc, Osc3VSnc, Osc1Shpe, Osc2Shpe,
Osc3Shpe, Osc1 Lev, Osc2 Lev, Osc3 Lev, NoiseLev, Ring Lev, VcaLevel, Filt Drv, FiltDist, FiltFreq, Filt Res, Lfo1Rate,
Lfo2Rate, AmpEnv A, AmpEnv D, AmpEnv R, ModEnv1A, ModEnv1D, ModEnv1R, ModEnv2A, ModEnv2D, ModEnv2R, FM O1>O2, FM O2>O3,
FM O3>O1, FM Ns>O1, O3>FiltF, Ns>FiltF; 2.x adds PanPosn, O123Shpe, AEnvDel, AEnvHld, AEnvSus, MEnv1Del, MEnv1Hld,
MEnv1Sus, MEnv2Del, MEnv2Hld, MEnv2Sus.

**FX mod sources:** Direct, ModWheel, AftTouch, ExprPED1, BrthPED2, Velocity, Keyboard, Animate1, Animate2, CV +/-,
Lfo3 +, Lfo3 +/-, Lfo4 +, Lfo4 +/-, BndWhl+, BndWhl-; 2.0 adds Noise.

**FX mod destinations:** Dist Lev, Chor Lev, ChorRate, Chor Dep, Chor FB, Del Lev, Del Time, Del FB, Rev Lev, Rev Time,
Rev LPF, Rev HPF; 2.x adds Rev Size, RevLpass, RevHpass, RevLdamp, RevHdamp, Del Slew, DelLdamp, DelHdamp, DelWidth,
DelRatio, Lfo3Rate, Lfo4Rate, NoiseLPF, NoiseHPF.

**Wavetables (Wave More 4–63, manual order, read across):**
BS sine, String, Glassy, Spirals, Random, BassOrgn, Granular, Steel, Zing, Acid, Grime, Sunrise, Tubey, Buzzy, Drow, Swell,
Octaves, Carousel, Heavy, Thicker, Wobbler, Choral, Hedge, Thinner, Chords, Climbing, Hungry, Tides, Didgery, CoinFlip,
Ladders, Tokyo, Harsh, Deep, Lead, Tops, Organ, Dub, Modeling, V.Chord, E. Piano, Eee, Modem, Variance, VoxOooEe, Eris,
Monster, Vocaloid, VoxYahEe, Flame, Screech, Vowelled, Winds, Further, SeaBase, WeirdVox, SoftClav, GlassSaw, Shmorgan, Yeah
(60 names; whether the table is read across or down must be checked on the synth).

**Arp / Delay sync (19 values, slow → fast):** 8 beats, 6 beats, 5+1/3, 4 beats, 3 beats, 2+2/3, 2nd, 4th D, 1+1/3, 4th,
8th D, 4th T, 8th, 16th D, 8th T, 16th, 16th T, 32nd, 32nd T. Delay sync has 16 (starts at 4 beats).

**LFO sync (35 values):** 64, 48, 42, 36, 32, 30, 28, 24 beats, 21+1/3, 20 beats, 18+2/3, 18, 16 beats, 13+1/3, 12 beats,
10+2/3, 8, 6 beats, 5+1/3, 4, 3 beats, 2+2/3, 2nd, 4th D, 1+1/3, 4th, 8th D, 4th T, 8th, 16th D, 8th T, 16th, 16th T, 32nd,
32nd T.

## Firmware 2.0 / 2.1 additions (summary)

Spread modes + Pan, new mod destinations, Noise as mod source, Patch Cue, Animate envelopes, LFO 3/4 Slew / Phase / Fade,
new FX mod destinations, Chorus / Flanger / Phlanger, Delay styles (Original, CrossFed, Dual) + Output (PreDamp, PostDamp),
Lo-Fi delay TimeMode, Arp Chance + Chord 2, envelope Delay (DAHDSR), Noise HPF. 2.1 removed the VCA clamp in the mod
matrix (clicks possible under heavy VCA modulation — synth behaviour).

## How the editor will talk to the Peak

- Live edits of parameters with a CC / NRPN: send those right away (14-bit CC pairs MSB then LSB).
- Live edits of parameters without one: rewrite the whole patch into the edit buffer (`00`), throttled.
- Read the current patch (`40`) on connect and after each program change; the Peak's own menu edits send nothing,
  so the panel catches up only on Fetch.
- Saving into a slot (`01`) only on an explicit Save, with a backup of the slot first.

## Limits and pitfalls

1. SysEx format is unofficial; a firmware update could move bytes. Check the format byte, warn on unknown.
2. Whole-patch writes may cut notes, restart LFOs and arp, drop FX tails; DAW automation of those parameters won't be smooth.
3. The Peak's own menu edits are not transmitted; constant polling would load the link.
4. The manual's table has errors and is out of date — check every parameter on the unit.
5. Windows MIDI ports are exclusive: DAW / Components / MIDISynth holding the `Peak` port blocks the plugin.
   CC/NRPN must be Rec+Trans and channels must match. Echo loops through the DAW. Close the app gently.
6. Values show as raw numbers (no ms / Hz curves); user wavetable names need command `47`.
7. Writing to a slot has no undo on the Peak; bank import writes 128 slots.
8. Controller only (no sound from the plugin); globals and firmware stay in Components; no Summit.
9. Windows builds in CI only; Pistol's code is off limits; "Peak" is Novation's trademark — say "for Novation Peak, not affiliated".

## UI plan

Serum / Vital-style layout in Peak colours (see `panel-mockup.html`): source rail | centre view that switches between
Main / FX / Matrix | modulators column that stays visible. Mod sources drag onto knobs. Mod page: 16-slot matrix + FX matrix.

Open UI questions:
- Mod Env 1 / 2: one at a time with a switch, or both shown?
- Animate 1/2, Patch +/−: working controls or decoration?
- Master Volume: live control, not saved with the patch?
- Where the ≈120 menu-only parameters live in this layout.

## Next steps

1. Listening session (read-only): Peak set to CC/NRPN = Rec+Trans, a logger on the `Peak` port, the user changes each
   menu-only parameter once; log the NRPN it sends and diff the patch before / after → NRPN + byte offset per parameter.
2. Read all four banks (`41`, 512 requests, read-only) for value statistics and the Presets tab — needs the user's OK.
3. First edit-buffer write test (`00`) — only with the user's yes, on a patch they can lose.
4. Mockup, then core tables, then JUCE port, then Presets tab and Save panel.

## Probe tool

WinMM probe (C#, run through PowerShell `Add-Type`), research only: `tools/probe/MidiProbe.cs`, usage in
`tools/probe/README.md` (`[MidiProbe]::List()`, `[MidiProbe]::Ask(inId, outId, @("F0 …"), waitMs, outDir)`; Peak was in 5 / out 6).

## Idea: playheads on LFO and envelope graphs (Vital / Serum style), 2026-11-15

The plugin makes no sound and the Peak sends no LFO / envelope state, so every playhead is a **simulation** in the
plugin, driven by what it can see: the notes it passes to the synth, the host transport (ppq, tempo, playing) and the
patch values.

| Indicator | How | Accuracy |
|---|---|---|
| Beat ruler / host position on sync'd graphs | host ppq | exact (it is the DAW's own position) |
| LFO 1/2 in Sync range | host ppq + sync rate | close, **if** the Peak follows the DAW's MIDI clock and the LFO restarts the way we model it |
| LFO 1/2 Low / High range | rate value -> Hz curve, phase reset at note-on when Phase is not Free | estimate; free-running phase is unknown and drifts |
| LFO 3/4 (global) | rate / wave have no published NRPN, only the patch | estimate, after their bytes are mapped |
| Envelopes, one dot per held note | note on / off times + A/D/S/R value -> ms curves | close once the curves are measured |

Pitfalls:
1. **Notes must pass through the plugin** (FL: notes in the generator's pattern go out via the plugin's MIDI out).
   Notes sent straight to the Peak port, or played on a controller wired to the Peak, are invisible.
   Standalone has no host transport and sees notes only from a MIDI input routed through it.
2. **Unknown curves:** raw 0-127 / 0-255 -> ms / Hz is not published. Needs a measuring session: the user records
   audio (e.g. LFO -> pitch / VCA, envelope -> VCA) at several values; I analyse the WAVs and fit curves.
3. **Peak behaviour we would have to copy:** voice allocation and stealing (8 voices), Mono / Legato, Re-Trig vs
   Legato trigger, LFO Phase (Free vs degrees), Common, One Shot, Fade modes, Slew, Key Sync, env Delay / Hold /
   Repeats (2.x), Animate. Each one not modelled makes the dot wrong in that case.
4. **Modulation:** mod-matrix slots on LFO rate or env times (and aftertouch / mod wheel / velocity behind them) change
   timing live; the simulation can follow known sources (notes, velocity, CCs it passes on) but not all.
5. **Clock:** sync'd LFOs and the arp follow the DAW only if the DAW sends MIDI clock to the Peak; otherwise they run
   on the Peak's own tempo. Option: the plugin sends clock itself (beware a second clock from the DAW).
6. **Timing:** USB MIDI and host buffering add a few ms of offset / jitter: fine for a display.
7. **Menu edits on the Peak** are not transmitted, so the model uses stale values until Fetch.

Cost: a note / transport FIFO from the audio thread, a voice model in the core (testable without the synth), UI repaint at
30-60 fps. Suggested order: beat ruler + sync'd LFO dot and envelope dots with an "approximate" label first; curve
measuring session later to make them match.

**Decision (2026-11-15):** only the easy, well-approximated part. An LFO playhead appears only while that LFO is in
Sync range, driven by the host position (ppq) and the sync rate, with an "approx" label. Off by default (a setting
turns it on). No envelope dots, no free-running LFO dots, no curve measuring for this.
Envelopes get only a **gate light** (no moving dot): an LED on the envelope panel, lit while notes passing through the
plugin are held (with the count), flashing on each note-on. Exact, so no "approx" label. Same setting, off by default.
