#!/usr/bin/env bash
# Opens the shared look-dev level (docs/decisions/0006): every court part and the venue,
# lit by a Data/Lighting.json preset, seen from the Data/LookDev.json cameras.
#
# Usage: Tools/RunLookDev.sh [preset] [camera]     e.g. Tools/RunLookDev.sh OutdoorNight CornerHigh
# Env:   UE_ROOT  engine install, default "/Users/Shared/Epic Games/UE_5.8" on macOS
#        SKIP_BUILD=1 to reuse the last build
# In the game, open the console (`) for Padel.LookDev.Preset/NextPreset/Camera/NextCamera/Report,
# and stat unit / stat gpu for the performance budget.
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT="$PWD/Padel.uproject"
PRESET="${1:-}"
CAMERA="${2:-Broadcast}"

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
    EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
    ;;
  *)
    echo "Unsupported OS; on Windows run UnrealEditor.exe Padel.uproject /Engine/Maps/Entry?game=LookDev -game" >&2
    exit 2
    ;;
esac

if [ "${SKIP_BUILD:-0}" != "1" ]; then
  "$BUILD_SH" PadelEditor "$PLATFORM" Development -Project="$PROJECT" -WaitMutex
fi

CMDS="Padel.LookDev.Camera $CAMERA"
if [ -n "$PRESET" ]; then
  CMDS="Padel.LookDev.Preset $PRESET; $CMDS"
fi
exec "$EDITOR" "$PROJECT" "/Engine/Maps/Entry?game=LookDev" -game -windowed -ResX=1920 -ResY=1080 \
  -ExecCmds="$CMDS" -log
