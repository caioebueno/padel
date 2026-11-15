#!/usr/bin/env bash
# Builds the editor target and runs the Unreal Automation tests headless.
# macOS (Apple Silicon) and Linux. On Windows use Tools/RunUnrealTests.ps1.
#
# Usage: Tools/RunUnrealTests.sh [test filter, default "Padel."]
# Env:   UE_ROOT  engine install, default "/Users/Shared/Epic Games/UE_5.8" on macOS
#        SKIP_BUILD=1 to reuse the last build
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT="$PWD/Padel.uproject"
FILTER="${1:-Padel.}"
REPORT_DIR="$PWD/Saved/AutomationReports"

case "$(uname -s)" in
  Darwin)
    UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
    PLATFORM=Mac
    BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
    EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
    ;;
  Linux)
    UE_ROOT="${UE_ROOT:?Set UE_ROOT to your Unreal Engine 5.8 install}"
    PLATFORM=Linux
    BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
    EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd"
    ;;
  *)
    echo "Unsupported OS; on Windows use Tools/RunUnrealTests.ps1" >&2
    exit 2
    ;;
esac

if [ ! -x "$BUILD_SH" ]; then
  echo "Unreal Engine not found at UE_ROOT=$UE_ROOT" >&2
  exit 2
fi

if [ "${SKIP_BUILD:-0}" != "1" ]; then
  "$BUILD_SH" PadelEditor "$PLATFORM" Development -Project="$PROJECT" -WaitMutex
fi

rm -rf "$REPORT_DIR"
"$EDITOR" "$PROJECT" \
  -ExecCmds="Automation RunTests $FILTER; Quit" \
  -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="$REPORT_DIR" \
  -unattended -nullrhi -nosplash -nosound -NoP4 -stdout -FullStdOutLogOutput || true

# The editor's exit code is not reliable for test results: read the report.
python3 - "$REPORT_DIR/index.json" <<'PY'
import json, sys
path = sys.argv[1]
try:
    with open(path, encoding="utf-8-sig") as f:
        report = json.load(f)
except FileNotFoundError:
    print(f"No automation report at {path}: the editor did not run the tests.")
    sys.exit(1)
tests = report.get("tests", [])
failed = [t for t in tests if t.get("state") != "Success"]
print(f"Unreal Automation: {len(tests) - len(failed)}/{len(tests)} passed")
for t in failed:
    print(f"  FAILED {t.get('fullTestPath')}")
    for e in t.get("entries", []):
        ev = e.get("event", {})
        if ev.get("type") == "Error":
            print(f"    {ev.get('message')}")
sys.exit(1 if failed or not tests else 0)
PY
