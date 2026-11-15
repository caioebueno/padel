#!/usr/bin/env bash
# Parses every GDScript outside addons/ with warnings-as-errors from project.godot
# (e.g. untyped declarations). Usage: tools/check_scripts.sh
set -uo pipefail
cd "$(dirname "$0")/.."
GODOT="${GODOT:-godot}"

if [ ! -d .godot ]; then
  "$GODOT" --headless --path . --import >/dev/null 2>&1 || true
fi

failed=0
while IFS= read -r file; do
  out=$("$GODOT" --headless --path . --check-only --script "res://$file" 2>&1)
  if echo "$out" | grep -qE "SCRIPT ERROR|Parse Error|Failed to load script"; then
    echo "::error file=$file::GDScript errors"
    echo "$out" | grep -E "ERROR|at:" || true
    failed=1
  fi
done < <(find scripts scenes tests tools -name '*.gd' -not -path '*/.godot/*' | sort)

if [ "$failed" -ne 0 ]; then
  exit 1
fi
echo "All scripts parse."
