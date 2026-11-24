#!/usr/bin/env bash
# Phase 1 base check: renders five Game Animation Sample clips played straight on the UE5
# mannequin, the placeholder (after the retarget) and the MetaHuman, plus the same moves through
# the motion matcher (Padel.Animation.BaseClips, Source/Padel/Private/Tests/PadelBaseClipsTests.cpp),
# then makes an mp4 per clip and body, a labelled grid (rows: clips; columns: bodies) and a
# 0.5x copy of the grid. Numbers in <folder>/base.md.
#
# Usage: Tools/RecordBaseClips.sh <output folder>
# Env:   UE_ROOT, SKIP_BUILD=1, SKIP_RENDER=1, WIDTH / HEIGHT (default 1280 x 720)
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT="$PWD/Padel.uproject"
OUT="$(mkdir -p "${1:?Usage: Tools/RecordBaseClips.sh <output folder>}" && cd "$1" && pwd)"
WIDTH="${WIDTH:-1280}"
HEIGHT="${HEIGHT:-720}"

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
  *) echo "Unsupported OS" >&2; exit 2 ;;
esac

if [ "${SKIP_RENDER:-0}" != "1" ]; then
  if [ "${SKIP_BUILD:-0}" != "1" ]; then
    "$UE_ROOT/Engine/Build/BatchFiles/$PLATFORM/Build.sh" PadelEditor "$PLATFORM" Development -Project="$PROJECT" -WaitMutex
  fi
  "$EDITOR" "$PROJECT" \
    -ExecCmds="Automation RunTests Padel.Animation.BaseClips; Quit" \
    -TestExit="Automation Test Queue Empty" \
    -ReportExportPath="$OUT/report" \
    -PadelBaseClips="$OUT" -PadelBaseClipsWidth="$WIDTH" -PadelBaseClipsHeight="$HEIGHT" \
    -dpcvars=r.SkinCache.Mode=0 -RenderOffscreen -unattended -nosplash -nosound -NoP4 -stdout -FullStdOutLogOutput > "$OUT/editor.log" 2>&1 || true
fi

CLIPS=(M_Neutral_Walk_Loop_F M_Neutral_Run_Loop_F M_Neutral_Walk_Start_F_Lfoot M_Neutral_Walk_Stop_F_Lfoot M_Neutral_Walk_Turn_R_090_Lfoot)
LABELS=("walk loop" "run loop" "walk start" "walk stop" "walk 90 turn")
BODIES=(1mannequin 2placeholder 3metahuman 4matched)
BODY_LABELS=("UE5 mannequin (no retarget)" "placeholder (retargeted)" "MetaHuman" "motion matcher (A setup)")
ENC=(-c:v libx264 -pix_fmt yuv420p -crf 17 -preset medium -movflags +faststart)
TW=$((WIDTH / 2)); TH=$((HEIGHT / 2))
INPUTS=(); FILTER=""; LAYOUT=""; N=0
for C in "${!CLIPS[@]}"; do
  for B in "${!BODIES[@]}"; do
    DIR="$OUT/${CLIPS[$C]}_${BODIES[$B]}"
    MP4="$OUT/${CLIPS[$C]}_${BODIES[$B]}.mp4"
    TEXT="${LABELS[$C]}  -  ${BODY_LABELS[$B]}"
    if [ -f "$DIR/frame_00000.png" ]; then
      ffmpeg -hide_banner -loglevel error -y -framerate 60 -i "$DIR/frame_%05d.png" \
        -vf "drawtext=fontfile='$FONT':text='$TEXT':x=16:y=14:fontsize=h/20:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=8,drawtext=fontfile='$FONT':text='%{frame_num}':x=w-tw-16:y=14:fontsize=h/24:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=6" \
        -r 60 "${ENC[@]}" "$MP4"
      INPUTS+=(-i "$MP4")
    else
      # A black tile where a body couldn't play (no MetaHuman on this machine, say).
      INPUTS+=(-f lavfi -t 1 -i "color=c=black:s=${WIDTH}x${HEIGHT}:r=60")
    fi
    FILTER+="[$N:v]scale=$TW:$TH,setsar=1[t$N];"
    LAYOUT+="$((B * TW))_$((C * TH))|"
    N=$((N + 1))
  done
done
STACK=""
for I in $(seq 0 $((N - 1))); do STACK+="[t$I]"; done
FILTER+="${STACK}xstack=inputs=$N:layout=${LAYOUT%|}:fill=black[g]"
ffmpeg -hide_banner -loglevel error -y "${INPUTS[@]}" -filter_complex "$FILTER" -map "[g]" -r 60 "${ENC[@]}" "$OUT/grid.mp4"
ffmpeg -hide_banner -loglevel error -y -i "$OUT/grid.mp4" -vf "setpts=2.0*PTS" -r 60 "${ENC[@]}" "$OUT/grid_0.5x.mp4"
echo "$OUT/grid.mp4"
echo "$OUT/grid_0.5x.mp4"
echo "Numbers: $OUT/base.md"
