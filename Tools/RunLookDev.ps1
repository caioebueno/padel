# Opens the shared look-dev level on Windows (docs/decisions/0009): every court part and the
# venue, lit by a Data/Lighting.json preset, seen from the Data/LookDev.json cameras.
# Usage: Tools\RunLookDev.ps1 [-Preset OutdoorNight] [-Camera CornerHigh] [-SkipBuild]
# Env:   UE_ROOT  engine install, default "C:\Program Files\Epic Games\UE_5.8"
param(
    [string]$Preset = "",
    [string]$Camera = "Broadcast",
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $Root "Padel.uproject"
$UeRoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }

if (-not $SkipBuild) {
    & "$UeRoot\Engine\Build\BatchFiles\Build.bat" PadelEditor Win64 Development "-Project=$Project" -WaitMutex
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$Cmds = "Padel.LookDev.Camera $Camera"
if ($Preset) { $Cmds = "Padel.LookDev.Preset $Preset; $Cmds" }
& "$UeRoot\Engine\Binaries\Win64\UnrealEditor.exe" $Project "/Engine/Maps/Entry?game=LookDev" -game -windowed -ResX=1920 -ResY=1080 "-ExecCmds=$Cmds" -log
