"""Asset pipeline library for Padel Street (CLAUDE.md, Tools/AssetPipeline/README.md).

Plain Python 3.11+, standard library plus `requests` for the AI providers.
Blender work happens in Tools/AssetPipeline/blender/ and Unreal work in
Tools/AssetPipeline/unreal/; this package launches both.
"""

from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent.parent
REPO_ROOT = TOOLS_DIR.parent.parent
CONFIG_DIR = TOOLS_DIR / "config"
PROMPTS_DIR = TOOLS_DIR / "prompts"
BLENDER_DIR = TOOLS_DIR / "blender"
UNREAL_DIR = TOOLS_DIR / "unreal"
# Game-ready source files (FBX), in Git LFS. Unreal imports these into Content/Padel/.
SOURCE_ART_DIR = REPO_ROOT / "SourceArt"
# Raw downloads from AI services and hand-off files (MetaHuman, Mixamo, AccuRIG).
# Git ignores both: the manifest records where every cleaned asset came from.
RAW_DIR = SOURCE_ART_DIR / "_raw"
INCOMING_DIR = SOURCE_ART_DIR / "_incoming"
MANIFEST_PATH = SOURCE_ART_DIR / "manifest.csv"
