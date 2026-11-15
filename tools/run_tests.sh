#!/usr/bin/env bash
# Runs the GUT suite headless. Usage: tools/run_tests.sh [extra GUT args]
# Set GODOT to your Godot 4.7.2 binary if it is not on PATH as "godot".
set -euo pipefail
cd "$(dirname "$0")/.."
GODOT="${GODOT:-godot}"

# A fresh checkout has no .godot/ cache: import first so class_name scripts
# and resources resolve.
if [ ! -d .godot ]; then
  "$GODOT" --headless --path . --import >/dev/null 2>&1 || true
fi
mkdir -p test_results
"$GODOT" --headless --path . -s addons/gut/gut_cmdln.gd -gconfig=res://.gutconfig.json "$@"
