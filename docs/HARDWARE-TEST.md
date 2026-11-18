# Peak Control — hardware test

What only the real Peak can confirm, in a safe order. Each step says what it sends. Nothing writes the synth's
memory until step 7, and that one only into a slot you pick, after a backup.

Setup: download the zip from GitHub Releases → "latest", unzip, run `Peak Control.exe` (standalone first).
On the Peak: Settings → CC/NRPN = **Rec+Trans**, note the MIDI channel. Close Novation Components / MIDISynth.

| # | Step | Sends | Expect / note down |
|---|---|---|---|
| 1 | Setup tab: checklist | identity request (read only) | "Input: Peak", "Output: Peak", "Peak found, firmware 2.1.0.8" |
| 2 | Start the standalone app (it reads the Peak's sound by itself) | edit-buffer request (read only) | patch name in the display; Voice knobs, mod matrix (Matrix tab) match the synth |
| 3 | Turn knobs on the Peak (cutoff, resonance, an envelope, osc coarse) | nothing | the same knobs move in the editor. Coarse / cutoff use 0–255 CC pairs: do they land on the same value? |
| 4 | Turn knobs in the editor: cutoff, resonance, amp attack | CC / CC pairs | the sound changes; **listen for zipper / jumps on cutoff** (0–255 pair coding) |
| 5 | Change NRPN parameters: filter shape, LFO 1 wave, arp on, a mod-matrix slot | NRPN | the sound / OLED follows |
| 6 | FX Matrix tab: change an FX slot amount | **whole patch to the edit buffer** (first edit-buffer write) | sound follows; listen for clicks, cut notes, arp / LFO restart. Synth memory is not touched |
| 7 | Save → pick a spare slot (e.g. D 127) → Save | reads the slot (backup), writes it, reads it back | "Saved to D 127 and checked."; backup .syx in Documents/Peak Control/Backups |
| 8 | Presets tab → Read bank A | 128 slot requests (read only) | names fill in; about 1 min |
| 9 | Double-click a patch in the list / ‹ › in the top bar | Bank Select + Program Change | the Peak switches to that slot (CC 32 = 1..4 for A-D, confirmed) |
| 10 | Pick a patch on the Peak's panel | nothing | the editor fetches it (if the Peak transmits program changes) |
| 11 | In a DAW (VST3): automate cutoff, play notes through the plugin | CC + notes | sound follows automation; with Setup → playheads on: gate light, LFO dot in Sync range |
| 12 | Open the menus (▶) on Osc / Filter / Voice: Fixed Note, Saw Density Detune, Pan, Spread Mode | NRPN | the synth follows |

Please note anything odd: parameters that jump, values that differ between editor and OLED, list names in the
wrong order (mod sources / destinations, wavetables, categories), patches that don't load.
