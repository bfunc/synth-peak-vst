# MIDI probe

Small WinMM tool for talking to the Peak from PowerShell, no compiler needed (`Add-Type` compiles it on the fly).
Used for research only; not part of the editor.

```powershell
Add-Type -Path D:\Projects\hardware-synth-editors\novation-peak-vst\tools\probe\MidiProbe.cs
[MidiProbe]::List()                       # port numbers (Peak was IN 5 / OUT 6 on Home, 2026-11-13)
[MidiProbe]::Ask(5, 6, @("F0 7E 7F 06 01 F7", "F0 00 20 29 01 10 00 7E 40 00 00 00 00 00 F7"), 1500, "C:\temp\out")
```

`Ask` sends each SysEx request (hex string), waits `waitMs` after each one, saves every SysEx reply as
`replyN.syx` in the output folder and returns a log. Only incoming SysEx is collected; short messages (CC/NRPN) are not.

Close Components, MIDISynth or a DAW that holds the `Peak` port first.
