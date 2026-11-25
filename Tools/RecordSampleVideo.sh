#!/usr/bin/env bash
# The movement restart's gate video (docs/decisions/0014-sample-locomotion.md): one pawn on our
# court on the layer videos' path, animated by the Game Animation Sample's locomotion
# (Padel.Animation.SampleVideo), filmed with the sample reference's follow camera; then side by
# side with the reference videos from the sample project if REFERENCE points at them.
#
# Usage: Tools/RecordSampleVideo.sh <output folder>
# Env:   UE_ROOT, SKIP_BUILD=1, SKIP_RENDER=1,
#        REFERENCE=<folder with metahuman.mp4 and mannequin.mp4 from the sample project>
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT="$PWD/Padel.uproject"
OUT="$(mkdir -p "${1:?Usage: Tools/RecordSampleVideo.sh <output folder>}" && cd "$1" && pwd)"
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
    -ExecCmds="Automation RunTests Padel.Animation.SampleVideo; Quit" \
    -TestExit="Automation Test Queue Empty" \
    -ReportExportPath="$OUT/report" -PadelSampleVideo="$OUT" \
    -dpcvars=r.SkinCache.Mode=0 -RenderOffscreen -unattended -nosplash -nosound -NoP4 -stdout -FullStdOutLogOutput > "$OUT/editor.log" 2>&1 || true
fi
ENC=(-c:v libx264 -pix_fmt yuv420p -crf 17 -preset medium -movflags +faststart)
label() { [ "$1" = metahuman ] && echo "Padel - sample locomotion on our MetaHuman" || echo "Padel - sample locomotion, its mannequin"; }
for B in metahuman mannequin; do
  [ -f "$OUT/$B/frame_00000.png" ] || { echo "No frames for $B (see $OUT/editor.log)" >&2; continue; }
  ffmpeg -hide_banner -loglevel error -y -framerate 60 -i "$OUT/$B/frame_%05d.png" \
    -vf "drawtext=fontfile='$FONT':text='$(label $B)':x=16:y=14:fontsize=h/24:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=8,drawtext=fontfile='$FONT':text='%{frame_num}':x=w-tw-16:y=14:fontsize=h/26:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=6" \
    -r 60 "${ENC[@]}" "$OUT/$B.mp4"
  echo "$OUT/$B.mp4"
done
if [ -n "${REFERENCE:-}" ] && [ -f "$REFERENCE/metahuman.mp4" ] && [ -f "$OUT/metahuman.mp4" ]; then
  # Top: the sample project as Epic ships it. Bottom: ours. Left: MetaHuman, right: mannequin.
  ffmpeg -hide_banner -loglevel error -y -i "$REFERENCE/metahuman.mp4" -i "$REFERENCE/mannequin.mp4" -i "$OUT/metahuman.mp4" -i "$OUT/mannequin.mp4" \
    -filter_complex "[0]scale=960:540[a];[1]scale=960:540[b];[2]scale=960:540[c];[3]scale=960:540[d];[a][b][c][d]xstack=inputs=4:layout=0_0|w0_0|0_h0|w0_h0:fill=black" \
    -r 60 "${ENC[@]}" "$OUT/side_by_side.mp4"
  ffmpeg -hide_banner -loglevel error -y -i "$OUT/side_by_side.mp4" -vf "setpts=2.0*PTS" -r 60 "${ENC[@]}" "$OUT/side_by_side_0.5x.mp4"
  echo "$OUT/side_by_side.mp4"
fi
echo "Numbers: $OUT/sample.md"
