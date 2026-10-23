# Prepares the run/ folder: hard-links the Quake 3 pk3 files and the Doom
# IWAD from GameData/ into run/baseq3/ (no copy, GameData/ is never written to).
$root = Split-Path -Parent $PSScriptRoot
$gameData = Join-Path $root "GameData"
$baseq3 = Join-Path $root "run\baseq3"

New-Item -ItemType Directory -Force $baseq3 | Out-Null
Get-ChildItem (Join-Path $gameData "*") -Include *.pk3, *.wad | ForEach-Object {
    $dst = Join-Path $baseq3 $_.Name
    if (-not (Test-Path $dst)) {
        New-Item -ItemType HardLink -Path $dst -Target $_.FullName | Out-Null
        Write-Host "linked $($_.Name)"
    }
}
