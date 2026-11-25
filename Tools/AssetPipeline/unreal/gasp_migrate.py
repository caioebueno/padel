"""Copies the Game Animation Sample's own locomotion set into the Padel project, unchanged.

The movement restart (docs/decisions/0014-sample-locomotion.md) drives the players with Epic's
Game Animation Sample as Epic ships it: its UEFN mannequin skeleton and clips (no retarget),
its Dense Stand pose search databases, schemas and normalization set, and Epic's retargeter
from that mannequin to a MetaHuman. This script runs inside the sample project (Python Editor
Script Plugin) and migrates those packages, with their dependencies, into the Padel project's
Content folder. Packages already there are skipped, never overwritten.

Everything it copies lands under /Game/Characters/UEFN_Mannequin and /Game/MetaHumans, both
git-ignored (Epic content for Unreal projects; MetaHuman content under the MetaHuman licence),
so it stays on the machine that ran it, like the clips animation_setup.py imports.

Usage (from the shell; PADEL_CONTENT is the Padel project's Content folder):
  PADEL_CONTENT=/path/to/padel/Content UnrealEditor <GameAnimationSample.uproject> \
      -run=pythonscript -script=Tools/AssetPipeline/unreal/gasp_migrate.py -nullrhi -unattended
"""
import os

import unreal

MM = "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Databases"

# The Dense Stand sets the sample's chooser (CHT_PoseSearchDatabases_Dense) picks from on the
# ground, standing: idles, turns in place, and each gait's starts, loops, pivots, stops and
# spin transitions. Lands, jumps, traversal and crouch aren't padel.
DATABASES = [
    "PSD_Dense_Stand_Idles",
    "PSD_Dense_Stand_TurnInPlace",
    "PSD_Dense_Stand_Walk_Starts",
    "PSD_Dense_Stand_Walk_Loops",
    "PSD_Dense_Stand_Walk_Pivots",
    "PSD_Dense_Stand_Walk_Stops",
    "PSD_Dense_Stand_Walk_SpinTransition",
    "PSD_Dense_Stand_Run_Starts",
    "PSD_Dense_Stand_Run_Loops",
    "PSD_Dense_Stand_Run_Pivots",
    "PSD_Dense_Stand_Run_Stops",
    "PSD_Dense_Stand_Run_SpinTransition",
    "PSD_Dense_Stand_Sprint_Starts",
    "PSD_Dense_Stand_Sprint_Loops",
    "PSD_Dense_Stand_Sprint_Pivots",
    "PSD_Dense_Stand_Sprint_Stops",
]

OTHERS = [
    "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin",
    "/Game/MetaHumans/Common/Common/Rigs/RTG_UEFN_to_Metahuman_nrw",
]


def find_database(name):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for asset in registry.get_assets_by_path("/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData", recursive=True):
        if str(asset.asset_name) == name:
            return str(asset.package_name)
    raise RuntimeError(f"{name} not found in the sample")


def main():
    destination = os.environ.get("PADEL_CONTENT")
    if not destination or not os.path.isdir(destination):
        raise RuntimeError("Set PADEL_CONTENT to the Padel project's Content folder")
    packages = [find_database(name) for name in DATABASES] + OTHERS
    for package in packages:
        unreal.log(f"gasp_migrate: {package}")
    options = unreal.MigrationOptions()
    options.set_editor_property("prompt", False)
    options.set_editor_property("ignore_dependencies", False)
    options.set_editor_property("asset_conflict", unreal.AssetMigrationConflict.SKIP)
    unreal.AssetToolsHelpers.get_asset_tools().migrate_packages(packages, destination, options)
    unreal.log(f"gasp_migrate: done, {len(packages)} packages and their dependencies into {destination}")


main()
