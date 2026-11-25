#!/usr/bin/env bash
# Records a four-AI match from its start (Padel.Match.Attract), the broadcast view and a side view
# of one player, at a fixed 60 Hz (Source/Padel/Private/Tests/PadelMatchRecorder.cpp), and makes
# broadcast.mp4 and side.mp4. SAMPLE=1 plays the players with the Game Animation Sample's
# locomotion (Padel.Animation.SampleLocomotion 1, docs/decisions/0014-sample-locomotion.md).
#
# Usage: Tools/RecordMatchClip.sh <output folder> [seconds, default 30] [player id, default 0]
# Env:   UE_ROOT, SAMPLE=1, LABEL (burnt into the videos), SKIP_RENDER=1 to only re-encode
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT="$PWD/Padel.uproject"
OUT="$(mkdir -p "${1:?Usage: Tools/RecordMatchClip.sh <output folder> [seconds] [player]}" && cd "$1" && pwd)"
SECONDS_TO_RECORD="${2:-30}"
PLAYER="${3:-0}"
case "$(uname -s)" in
  Darwin) UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"; EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"; FONT="/System/Library/Fonts/Supplemental/Arial Bold.ttf" ;;
  Linux) UE_ROOT="${UE_ROOT:?Set UE_ROOT}"; EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"; FONT="/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" ;;
  *) echo "Unsupported OS" >&2; exit 2 ;;
esac
CMDS="Padel.Animation.SampleLocomotion ${SAMPLE:-0}, Padel.Match.Attract, Padel.Record.Clip $OUT $SECONDS_TO_RECORD $PLAYER"
if [ "${SKIP_RENDER:-0}" != "1" ]; then
"$EDITOR" "$PROJECT" -game -benchmark -fps=60 -deterministic -windowed -ResX=1280 -ResY=720 \
  -ExecCmds="$CMDS" -dpcvars=r.SkinCache.Mode=0 -RenderOffscreen -unattended -nosound -NoP4 -stdout -FullStdOutLogOutput > "$OUT/game.log" 2>&1 || true
fi
# drawtext reads ':' as an option separator.
LABEL="$(printf '%s' "${LABEL:-}" | sed 's/:/\\:/g')"
ENC=(-c:v libx264 -pix_fmt yuv420p -crf 18 -preset medium -movflags +faststart)
for V in broadcast side; do
  [ -f "$OUT/$V/frame_00000.png" ] || { echo "No $V frames (see $OUT/game.log)" >&2; continue; }
  ffmpeg -hide_banner -loglevel error -y -framerate 60 -i "$OUT/$V/frame_%05d.png" \
    -vf "drawtext=fontfile='$FONT':text='$LABEL':x=16:y=14:fontsize=h/24:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=8,drawtext=fontfile='$FONT':text='%{frame_num}':x=w-tw-16:y=14:fontsize=h/26:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=6" \
    -r 60 "${ENC[@]}" "$OUT/$V.mp4"
  echo "$OUT/$V.mp4"
done
