# Builds the editor target and runs the Unreal Automation tests headless on Windows.
# Usage: Tools\RunUnrealTests.ps1 [-Filter "Padel."] [-SkipBuild]
# Env:   UE_ROOT  engine install, default "C:\Program Files\Epic Games\UE_5.8"
param(
    [string]$Filter = "Padel.",
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $Root "Padel.uproject"
$ReportDir = Join-Path $Root "Saved\AutomationReports"
$UeRoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }

if (-not $SkipBuild) {
    & "$UeRoot\Engine\Build\BatchFiles\Build.bat" PadelEditor Win64 Development "-Project=$Project" -WaitMutex
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (Test-Path $ReportDir) { Remove-Item -Recurse -Force $ReportDir }
& "$UeRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $Project `
    "-ExecCmds=Automation RunTests $Filter; Quit" `
    "-TestExit=Automation Test Queue Empty" `
    "-ReportExportPath=$ReportDir" `
    -unattended -nullrhi -nosplash -nosound -NoP4 -stdout

$Index = Join-Path $ReportDir "index.json"
if (-not (Test-Path $Index)) { Write-Error "No automation report: the editor did not run the tests."; exit 1 }
$Report = Get-Content $Index -Raw | ConvertFrom-Json
$Failed = @($Report.tests | Where-Object { $_.state -ne "Success" })
Write-Host ("Unreal Automation: {0}/{1} passed" -f ($Report.tests.Count - $Failed.Count), $Report.tests.Count)
foreach ($Test in $Failed) { Write-Host "  FAILED $($Test.fullTestPath)" }
if ($Failed.Count -gt 0 -or $Report.tests.Count -eq 0) { exit 1 }
