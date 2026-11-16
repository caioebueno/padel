"""Retargets Epic's UE5 mannequin idle, walk and jog clips onto the placeholder skeleton,
for the C++ player pose (docs/decisions/0006-metahuman-players.md). Runs inside the Unreal
Editor after import_assets.py:

  Output Log (Python): exec(open("<repo>/Tools/AssetPipeline/unreal/animation_setup.py").read())
  or headless: UnrealEditor-Cmd Padel.uproject -run=pythonscript -script=<this file>

The source clips come from the engine's Third Person template (TemplateResources), which
the script copies into /Game/Characters/Mannequins first (git-ignored: Epic content, rebuilt
from the engine install). It creates, from text like the rest of the pipeline:

  /Game/Padel/Animations/Retarget/IK_PadelMannequin, IK_PadelPlaceholder
  /Game/Padel/Animations/Retarget/RTG_Mannequin_To_Placeholder
  /Game/Padel/Animations/Locomotion/A_Idle, A_Walk_{Fwd,Bwd,Left,Right}, A_Jog_{...}

the clip paths Data/Animation.json names (IdleAsset, WalkAssets, JogAssets).
Log lines start with [padel-anim]; the last one is "[padel-anim] done" or "... FAILED".
"""

from __future__ import annotations

import json
import shutil
from pathlib import Path

import unreal

PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
ENGINE = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.engine_dir())).resolve()
TEMPLATE = ENGINE.parent / "Templates" / "TemplateResources" / "High" / "Characters" / "Content" / "Mannequins"
MANNEQUIN_DIR = PROJECT / "Content" / "Characters" / "Mannequins"
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
RETARGET_DIR = "/Game/Padel/Animations/Retarget"
MANNEQUIN_RIG = f"{RETARGET_DIR}/IK_PadelMannequin"
PLACEHOLDER_RIG = f"{RETARGET_DIR}/IK_PadelPlaceholder"
RETARGETER = f"{RETARGET_DIR}/RTG_Mannequin_To_Placeholder"
STAGING = "/Game/Padel/Animations/Locomotion/_Staging"
ANIMS = "/Game/Characters/Mannequins/Anims/Unarmed"
# Source clip -> the name Animation.json expects.
CLIPS = {
    f"{ANIMS}/MM_Idle": "A_Idle",
    f"{ANIMS}/Walk/MF_Unarmed_Walk_Fwd": "A_Walk_Fwd",
    f"{ANIMS}/Walk/MF_Unarmed_Walk_Bwd": "A_Walk_Bwd",
    f"{ANIMS}/Walk/MF_Unarmed_Walk_Left": "A_Walk_Left",
    f"{ANIMS}/Walk/MF_Unarmed_Walk_Right": "A_Walk_Right",
    f"{ANIMS}/Jog/MF_Unarmed_Jog_Fwd": "A_Jog_Fwd",
    f"{ANIMS}/Jog/MF_Unarmed_Jog_Bwd": "A_Jog_Bwd",
    f"{ANIMS}/Jog/MF_Unarmed_Jog_Left": "A_Jog_Left",
    f"{ANIMS}/Jog/MF_Unarmed_Jog_Right": "A_Jog_Right",
}
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SOURCE = unreal.RetargetSourceOrTarget.SOURCE
TARGET = unreal.RetargetSourceOrTarget.TARGET
failures: list[str] = []


def log(msg: str) -> None:
    unreal.log(f"[padel-anim] {msg}")
    print(f"[padel-anim] {msg}", flush=True)


def fail(msg: str) -> None:
    failures.append(msg)
    unreal.log_error(f"[padel-anim] FAILED {msg}")
    print(f"[padel-anim] FAILED {msg}", flush=True)


def object_path(path: str) -> str:
    return path.split(".")[0]


def fresh_asset(path: str, asset_class, factory):
    if ASSETS.does_asset_exist(path):
        ASSETS.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return TOOLS.create_asset(name, folder, asset_class, factory)


def copy_template() -> bool:
    if not TEMPLATE.is_dir():
        fail(f"no Third Person template content at {TEMPLATE}")
        return False
    if not MANNEQUIN_DIR.is_dir():
        shutil.copytree(TEMPLATE, MANNEQUIN_DIR)
        log(f"copied the UE5 mannequin from {TEMPLATE}")
        unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/Characters"], True)
    return True


def auto_rig(path: str, mesh: unreal.SkeletalMesh) -> unreal.IKRigDefinition | None:
    rig = fresh_asset(path, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    if not ctrl.set_skeletal_mesh(mesh) or not ctrl.apply_auto_generated_retarget_definition():
        fail(f"{path}: could not generate retarget chains for {mesh.get_path_name()}")
        return None
    ctrl.apply_auto_fbik()
    ASSETS.save_loaded_asset(rig)
    log(f"built {path}")
    return rig


def main() -> None:
    settings = json.loads((PROJECT / "Data" / "Animation.json").read_text(encoding="utf-8"))[0]
    wanted = [settings["IdleAsset"], *settings["WalkAssets"], *settings["JogAssets"]]
    if sorted(p.rsplit("/", 1)[-1] for p in wanted) != sorted(CLIPS.values()):
        fail("Data/Animation.json IdleAsset/WalkAssets/JogAssets don't match this script's clip names")
        return
    out_dir = wanted[0].rsplit("/", 1)[0]

    placeholder = unreal.load_asset(settings["TeamMeshes"][0])
    if not isinstance(placeholder, unreal.SkeletalMesh):
        fail(f"no placeholder at {settings['TeamMeshes'][0]}: run import_assets.py first")
        return
    if not copy_template():
        return
    manny = unreal.load_asset(MANNY)
    if not isinstance(manny, unreal.SkeletalMesh):
        fail(f"no mannequin mesh at {MANNY}")
        return

    source = auto_rig(MANNEQUIN_RIG, manny)
    target = auto_rig(PLACEHOLDER_RIG, placeholder)
    if not source or not target:
        return
    rtg = fresh_asset(RETARGETER, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.set_ik_rig(SOURCE, source)
    ctrl.set_ik_rig(TARGET, target)
    ctrl.set_preview_mesh(SOURCE, manny)
    ctrl.set_preview_mesh(TARGET, placeholder)
    ctrl.add_default_ops()
    ctrl.assign_ik_rig_to_all_ops(SOURCE, source)
    ctrl.assign_ik_rig_to_all_ops(TARGET, target)
    ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    # The placeholder stands in a T-pose and the mannequin in an A-pose: match them first.
    ctrl.auto_align_all_bones(TARGET)
    ASSETS.save_loaded_asset(rtg)
    log(f"built {RETARGETER}")

    clips = []
    for path in CLIPS:
        if not ASSETS.does_asset_exist(path):
            fail(f"missing source clip {path}")
            return
        clips.append(unreal.EditorAssetLibrary.find_asset_data(path))
    if ASSETS.does_directory_exist(STAGING):
        ASSETS.delete_directory(STAGING)
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", clips)
    inputs.set_editor_property("source_mesh", manny)
    inputs.set_editor_property("target_mesh", placeholder)
    inputs.set_editor_property("ik_retarget_asset", rtg)
    inputs.set_editor_property("target_path", STAGING)
    inputs.set_editor_property("include_referenced_assets", False)
    inputs.set_editor_property("overwrite_existing_files", True)
    made = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    log(f"retargeted {len(made)} asset(s)")

    for data in made:
        asset = data.get_asset()
        if not isinstance(asset, unreal.AnimSequence):
            continue
        src_name = str(data.asset_name)
        name = next((n for p, n in CLIPS.items() if p.rsplit("/", 1)[-1] == src_name), None)
        if name is None:
            continue
        dest = f"{out_dir}/{name}"
        if ASSETS.does_asset_exist(dest):
            ASSETS.delete_asset(dest)
        if not ASSETS.rename_asset(asset.get_path_name(), dest):
            fail(f"could not move {src_name} to {dest}")
    for path in wanted:
        if not ASSETS.does_asset_exist(object_path(path)):
            fail(f"{path} was not made")
        else:
            ASSETS.save_asset(object_path(path), only_if_is_dirty=False)
    if ASSETS.does_directory_exist(STAGING):
        ASSETS.delete_directory(STAGING)
    log("FAILED" if failures else "done")


main()
