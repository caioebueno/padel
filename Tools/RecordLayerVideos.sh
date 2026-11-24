#!/usr/bin/env bash
# Phase 0 of the movement plan: renders the motion probe's layer video path once per animation
# layer (Padel.Animation.LayerVideo, Source/Padel/Private/Tests/PadelLayerVideoTests.cpp) and
# makes the videos: one mp4 per variant, a labelled grid of all of them, and a 0.5x copy of the grid.
#
#   A clip root motion, B clips on the path with no warps, C + locomotion warps,
#   D + body layers (full stack), E D on the MetaHuman.
#
# Needs a renderer (not -nullrhi) and ffmpeg (with drawtext). macOS / Linux.
#
# Usage: Tools/RecordLayerVideos.sh <output folder> [variants, default ABCDE]
# Env:   UE_ROOT  engine install, default "/Users/Shared/Epic Games/UE_5.8" on macOS
#        SKIP_BUILD=1 to reuse the last build, SKIP_RENDER=1 to only re-encode existing frames
#        WIDTH / HEIGHT (default 1920 x 1080)
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT="$PWD/Padel.uproject"
OUT="$(mkdir -p "${1:?Usage: Tools/RecordLayerVideos.sh <output folder> [variants]}" && cd "$1" && pwd)"
VARIANTS="${2:-ABCDE}"
WIDTH="${WIDTH:-1920}"
HEIGHT="${HEIGHT:-1080}"

case "$(uname -s)" in
  Darwin)
    UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
    PLATFORM=Mac
    EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
    FONT="/System/Library/Fonts/Supplemental/Arial Bold.ttf"
    ;;
  Linux)
    UE_ROOT="${UE_ROOT:?Set UE_ROOT to your Unreal Engine 5.8 install}"
    PLATFORM=Linux
    EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
    FONT="/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
    ;;
  *)
    echo "Unsupported OS" >&2
    exit 2
    ;;
esac
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/$PLATFORM/Build.sh"

if [ "${SKIP_RENDER:-0}" != "1" ]; then
  if [ "${SKIP_BUILD:-0}" != "1" ]; then
    "$BUILD_SH" PadelEditor "$PLATFORM" Development -Project="$PROJECT" -WaitMutex
  fi
  for V in $(echo "$VARIANTS" | fold -w1); do rm -rf "$OUT/$V"; done
  "$EDITOR" "$PROJECT" \
    -ExecCmds="Automation RunTests Padel.Animation.LayerVideo; Quit" \
    -TestExit="Automation Test Queue Empty" \
    -ReportExportPath="$OUT/report" \
    -PadelLayerVideo="$OUT" -PadelLayerVideoVariants="$VARIANTS" \
    -PadelLayerVideoWidth="$WIDTH" -PadelLayerVideoHeight="$HEIGHT" \
    -RenderOffscreen -unattended -nosplash -nosound -NoP4 -stdout -FullStdOutLogOutput > "$OUT/editor.log" 2>&1 || true
fi

label() {
  case "$1" in
    A) echo "A  clip root motion (pure clips)" ;;
    B) echo "B  clips on the path, no warps" ;;
    C) echo "C  B + warps (orientation, stride, foot lock)" ;;
    D) echo "D  C + body layers (full stack)" ;;
    E) echo "E  D on the MetaHuman" ;;
  esac
}

ENC=(-c:v libx264 -pix_fmt yuv420p -crf 16 -preset slow -movflags +faststart)
TILES=()
for V in $(echo "$VARIANTS" | fold -w1); do
  if [ ! -f "$OUT/$V/frame_00000.png" ]; then
    echo "No frames for $V (see $OUT/editor.log)" >&2
    continue
  fi
  TEXT="$(label "$V")"
  # Frame number and time burnt in so variants can be compared frame by frame.
  ffmpeg -hide_banner -loglevel error -y -framerate 60 -i "$OUT/$V/frame_%05d.png" \
    -vf "drawtext=fontfile='$FONT':text='$TEXT':x=24:y=20:fontsize=h/22:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=10,drawtext=fontfile='$FONT':text='%{frame_num}  %{pts\:hms}':x=w-tw-24:y=20:fontsize=h/30:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=8" \
    -r 60 "${ENC[@]}" "$OUT/$V.mp4"
  TILES+=("$V")
  echo "$OUT/$V.mp4"
done

# The grid: three across, two down, each tile half size; an empty tile stays black.
if [ "${#TILES[@]}" -ge 2 ]; then
  TW=$((WIDTH / 2)); TH=$((HEIGHT / 2))
  INPUTS=(); FILTER=""; LAYOUT=""
  for I in "${!TILES[@]}"; do
    INPUTS+=(-i "$OUT/${TILES[$I]}.mp4")
    FILTER+="[$I:v]scale=$TW:$TH,setsar=1[t$I];"
    COL=$((I % 3)); ROW=$((I / 3))
    LAYOUT+="$((COL * TW))_$((ROW * TH))|"
  done
  N=${#TILES[@]}
  STACK=""
  for I in $(seq 0 $((N - 1))); do STACK+="[t$I]"; done
  FILTER+="${STACK}xstack=inputs=$N:layout=${LAYOUT%|}:fill=black,pad=$((TW * 3)):$((TH * ((N + 2) / 3))):0:0:black[g]"
  ffmpeg -hide_banner -loglevel error -y "${INPUTS[@]}" -filter_complex "$FILTER" -map "[g]" -r 60 "${ENC[@]}" "$OUT/grid.mp4"
  ffmpeg -hide_banner -loglevel error -y -i "$OUT/grid.mp4" -vf "setpts=2.0*PTS" -r 60 "${ENC[@]}" "$OUT/grid_0.5x.mp4"
  echo "$OUT/grid.mp4"
  echo "$OUT/grid_0.5x.mp4"
fi
echo "Numbers: $OUT/metrics.md"
