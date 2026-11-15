# Installs the newest CI build on this PC, like Trigon6 Control:
#   builds\latest\            the unzipped release (Peak Control.exe, Peak Control.vst3)
#   Desktop\Peak Control.lnk  shortcut to the standalone app
#   C:\Program Files\Common Files\VST3\Peak Control.vst3   (needs write access to that folder)
# Run:  powershell -ExecutionPolicy Bypass -File tools\install-latest.ps1
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$dest = Join-Path $repo "builds\latest"
$tmp = Join-Path $env:TEMP "peak-control-download"
Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $tmp | Out-Null
gh release download latest -R bfunc/synth-peak-vst -D $tmp --clobber
$zip = Get-ChildItem $tmp -Filter "*.zip" | Select-Object -First 1
Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive $zip.FullName -DestinationPath $dest
Get-Content (Join-Path $dest "VERSION.txt")

$exe = Join-Path $dest "Peak Control.exe"
$lnk = Join-Path ([Environment]::GetFolderPath("Desktop")) "Peak Control.lnk"
$s = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$s.TargetPath = $exe
$s.WorkingDirectory = $dest
$s.Save()
"shortcut: $lnk"

$vstDir = "C:\Program Files\Common Files\VST3"
$vst = Join-Path $vstDir "Peak Control.vst3"
try {
    Remove-Item $vst -Recurse -Force -ErrorAction SilentlyContinue
    Copy-Item (Join-Path $dest "Peak Control.vst3") $vstDir -Recurse
    "vst3: $vst"
} catch {
    "vst3: could not write to $vstDir ($($_.Exception.Message)). Run this script as administrator."
}
