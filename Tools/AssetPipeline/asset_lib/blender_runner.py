"""Runs a script from Tools/AssetPipeline/blender/ with a JSON job.

Two ways to get Blender, in this order:
  BLENDER=/path/to/blender        the Blender app (4.2 LTS or newer), run with -b
  BLENDER_PYTHON=/path/to/python  a Python that can `import bpy` (pip install bpy)
If neither is set, the current Python is used when it can import bpy.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from . import BLENDER_DIR


class BlenderError(RuntimeError):
    pass


def _python_has_bpy(python: str) -> bool:
    try:
        return subprocess.run([python, "-c", "import bpy"], capture_output=True, timeout=120).returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


def command(script: Path, job_path: Path) -> list[str]:
    blender = os.environ.get("BLENDER") or shutil.which("blender")
    if blender:
        return [blender, "-b", "--factory-startup", "--python-exit-code", "1", "--python", str(script), "--", str(job_path)]
    python = os.environ.get("BLENDER_PYTHON") or sys.executable
    if _python_has_bpy(python):
        return [python, str(script), "--", str(job_path)]
    raise BlenderError(
        "Blender not found. Set BLENDER to the Blender app (on a Mac: "
        "/Applications/Blender.app/Contents/MacOS/Blender), or BLENDER_PYTHON to a Python with `pip install bpy`."
    )


def run(script_name: str, job: dict[str, Any]) -> dict[str, Any]:
    """Runs blender/<script_name>.py and returns the report it writes."""
    script = BLENDER_DIR / f"{script_name}.py"
    with tempfile.TemporaryDirectory(prefix="padel_assets_") as tmp:
        job = dict(job)
        job["report"] = str(Path(tmp) / "report.json")
        job_path = Path(tmp) / "job.json"
        job_path.write_text(json.dumps(job), encoding="utf-8")
        proc = subprocess.run(command(script, job_path), capture_output=True, text=True)
        if proc.returncode != 0 or not Path(job["report"]).is_file():
            tail = "\n".join((proc.stdout + proc.stderr).splitlines()[-40:])
            raise BlenderError(f"blender/{script_name}.py failed for {job.get('id')}:\n{tail}")
        return json.loads(Path(job["report"]).read_text(encoding="utf-8"))
