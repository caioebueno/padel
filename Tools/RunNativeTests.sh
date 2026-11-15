#!/usr/bin/env bash
# Builds PadelCore standalone and runs its native tests (no Unreal Engine needed).
# Usage: Tools/RunNativeTests.sh [doctest args, e.g. -tc="Court.Layout*"]
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build/native}"
cmake -S Tests/Native -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" --parallel
"$BUILD_DIR/padel_core_tests" "$@"
