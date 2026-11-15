# Patch byte mapper

Finds where each parameter lives in the 510 patch data bytes, on the real synth, in about a minute:
for every live parameter it sends value A, reads the edit buffer, sends value B, reads again, and diffs.
The last step writes the original patch back into the edit buffer (the synth's memory is not touched).

```powershell
# 1. table -> TSV (Docker): see the docker line in the commit that added this folder, or:
#    g++ -std=c++17 -Isrc src/core/ParamTable.cpp tools/mapper/dump_table.cpp && ./a.out > tools/mapper/params.tsv
# 2. baseline dump, plan, run (Peak = in 1 / out 2 here; channel 13)
Add-Type -Path tools\probe\MidiProbe.cs
Set-Content plan0.txt "base`t"; [MidiProbe]::Run(1, 2, "plan0.txt", $dir, 30)
python tools\mapper\make_plan.py tools\mapper\params.tsv 13 $dir\base.syx plan.txt
[MidiProbe]::Run(1, 2, "plan.txt", $dir, 40)
python tools\mapper\analyze.py tools\mapper\params.tsv $dir
```

Results go into `kMeasuredOffsets` in `src/core/ParamTable.cpp`.
