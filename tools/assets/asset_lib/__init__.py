"""Asset pipeline library for Padel Street (CLAUDE.md, tools/assets/README.md).

Plain Python 3.11+, standard library plus `requests` for the AI providers.
Blender work happens in tools/assets/blender/, which this package launches.
"""

from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent.parent
REPO_ROOT = TOOLS_DIR.parent.parent
CONFIG_DIR = TOOLS_DIR / "config"
PROMPTS_DIR = TOOLS_DIR / "prompts"
BLENDER_DIR = TOOLS_DIR / "blender"
# Raw downloads from AI services and hand-off files (Mixamo, AccuRIG) land here.
# Git ignores it: the manifest records where every cleaned asset came from.
SOURCE_DIR = REPO_ROOT / "assets" / "_source"
INCOMING_DIR = REPO_ROOT / "assets" / "_incoming"
MANIFEST_PATH = REPO_ROOT / "assets" / "manifest.csv"
