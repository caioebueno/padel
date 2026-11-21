#!/bin/bash
# Runs WHAM (MIT, github.com/yohanshin/WHAM) on every clip in ./clips and
# packs the results into wham_results.tar.gz (docs/decisions/0012-video-footwork.md).
# Step 1 of 3: then wham_to_fbx.py, then Tools/AssetPipeline/unreal/footwork_setup.py.
# Made for a vast.ai instance started from the Docker image
#   yusun9/wham-vitpose-dpvo-cuda11.3-python3.9
# on an RTX 3090 / A5000 / A6000 (CUDA 11.3 does not support RTX 40xx cards well).
#
# Usage, from the instance's terminal:
#   bash run_wham.sh            # asks once for your SMPL and SMPLify logins
#   bash run_wham.sh one        # only footwork-08_3m10, the test clip
set -e
# No --visualize: the image has no pytorch3d. Check a result by drawing its vertices over the video.
cd "$(dirname "$0")"
ROOT=$PWD

if [ ! -d WHAM ]; then
  git clone --recursive https://github.com/yohanshin/WHAM.git
fi
cd WHAM
# ViTPose is installed from the repo's own copy (the image may point elsewhere).
python -c "import mmpose" 2>/dev/null || pip install -q -e third-party/ViTPose
if [ ! -f checkpoints/wham_vit_w_3dpw.pth.tar ]; then
  pip install -q gdown
  bash fetch_demo_data.sh      # prompts for the SMPLify and SMPL logins
fi

if [ "$1" = "one" ]; then CLIPS="$ROOT/clips/footwork-08_3m10.mp4"; else CLIPS="$ROOT"/clips/*.mp4; fi

for v in $CLIPS; do
  name=$(basename "$v" .mp4)
  if [ -f "output/footwork/$name/wham_output.pkl" ]; then echo "skip $name"; continue; fi
  echo "=== $name"
  # The main camera does not move, so SLAM is skipped: WHAM then treats the
  # camera as fixed and the court as the world.
  python demo.py --video "$v" --output_pth output/footwork \
      --estimate_local_only --save_pkl
done

# The SMPL body file goes along: the FBX converter needs its skeleton.
# It stays under your SMPL licence; keep it out of the repo.
cp dataset/body_models/smpl/SMPL_NEUTRAL.pkl output/footwork/
# Drop the mesh vertices (about 95% of the size); the converter rebuilds
# them from pose + shape with the SMPL file.
python - <<'PY'
import glob, joblib
for p in glob.glob('output/footwork/*/wham_output.pkl'):
    r = joblib.load(p)
    slim = {k: {kk: vv for kk, vv in v.items() if kk != 'verts'} for k, v in r.items()}
    joblib.dump(slim, p.replace('wham_output.pkl', 'wham_slim.pkl'), compress=3)
PY
cd output && tar czf "$ROOT/wham_results.tar.gz" footwork/SMPL_NEUTRAL.pkl footwork/*/wham_slim.pkl
echo "Done: $ROOT/wham_results.tar.gz"
