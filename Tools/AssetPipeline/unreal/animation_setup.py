"""Retargets athletic stand-in clips onto the placeholder skeleton, for the C++ player pose
(docs/decisions/0006-metahuman-players.md, phase 0 of the movement plan). Runs inside the
Unreal Editor after import_assets.py:

  Output Log (Python): exec(open("<repo>/Tools/AssetPipeline/unreal/animation_setup.py").read())
  or headless: UnrealEditor-Cmd Padel.uproject -run=pythonscript -script=<this file>

The clips are Epic's Game Animation Sample (free on Fab under its Standard License; Epic content for Unreal Engine projects only): an upright idle, crouched
loops for the low adjustment and side-shuffle steps, run loops for the faster movement.
gasp_export.py, run in the sample project, writes them as FBX to SourceArt/_incoming/GASP/
(git-ignored). This script imports them onto the UE5 mannequin skeleton (the bone names
match), into /Game/Characters/GASP (git-ignored), then retargets them. The mannequin itself
comes from the engine's Third Person template, copied into /Game/Characters/Mannequins. It
creates, from text like the rest of the pipeline:

  /Game/Padel/Animations/Retarget/IK_PadelMannequin, IK_PadelPlaceholder
  /Game/Padel/Animations/Retarget/RTG_Mannequin_To_Placeholder
  /Game/Padel/Animations/Locomotion/A_Idle, A_Walk_{Fwd,Bwd,Left,Right}, A_Jog_{...}

the clip paths Data/Animation.json names (IdleAsset, WalkAssets, JogAssets). The clips
carry root motion; the pose plays them in place (root locked) and the game moves the pawn,
so each clip's root speed is logged for WalkClipSpeedMps and RunClipSpeedMps.
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
INCOMING = PROJECT / "SourceArt" / "_incoming" / "GASP"
GASP_DIR = "/Game/Characters/GASP"
# Game Animation Sample clip (FBX in INCOMING, imported into GASP_DIR) -> the name
# Animation.json expects. Biomechanics doc 2.1 to 2.4: an upright idle until the ready-stance
# blend (phase 2; the crouched idle reads as a squat), low
# adjustment steps forward and back, a low side shuffle, then an athletic run and strafe.
# The "Relaxed" sets are used because every direction in them moves at the same root speed
# (crouch 1.65 m/s, run 3.75 m/s), and the pose plays all four directions at one rate:
# WalkClipSpeedMps and RunClipSpeedMps in Data/Animation.json.
CLIPS = {
    f"{GASP_DIR}/M_Neutral_Stand_Idle_Loop": "A_Idle",
    f"{GASP_DIR}/M_Relaxed_Crouch_Loop_F": "A_Walk_Fwd",
    f"{GASP_DIR}/M_Relaxed_Crouch_Loop_B": "A_Walk_Bwd",
    f"{GASP_DIR}/M_Relaxed_Crouch_Loop_LL": "A_Walk_Left",
    f"{GASP_DIR}/M_Relaxed_Crouch_Loop_RR": "A_Walk_Right",
    f"{GASP_DIR}/M_Relaxed_Run_Loop_F": "A_Jog_Fwd",
    f"{GASP_DIR}/M_Relaxed_Run_Loop_B": "A_Jog_Bwd",
    f"{GASP_DIR}/M_Relaxed_Run_Loop_LL": "A_Jog_Left",
    f"{GASP_DIR}/M_Relaxed_Run_Loop_RR_offset": "A_Jog_Right",
    # N1 ready-stance depth: the crouched idle and the upright walk the pose mixes with the
    # crouch by ReadyCrouchAlpha* (the Relaxed walk moves at 1.65 m/s too, like the crouch).
    f"{GASP_DIR}/M_Neutral_Crouch_Idle_Loop": "A_Ready",
    f"{GASP_DIR}/M_Relaxed_Walk_Loop_F": "A_WalkUpright_Fwd",
    f"{GASP_DIR}/M_Relaxed_Walk_Loop_B": "A_WalkUpright_Bwd",
    f"{GASP_DIR}/M_Relaxed_Walk_Loop_LL": "A_WalkUpright_Left",
    f"{GASP_DIR}/M_Relaxed_Walk_Loop_RR_offset": "A_WalkUpright_Right",
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
        # The deleted package stays in memory until collected, and blocks the new asset.
        unreal.SystemLibrary.collect_garbage()
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


def import_gasp_clips(skeleton: unreal.Skeleton) -> bool:
    """Imports the exported Game Animation Sample FBX onto the mannequin skeleton."""
    tasks = []
    for path in CLIPS:
        name = path.rsplit("/", 1)[-1]
        fbx = INCOMING / f"{name}.fbx"
        if not fbx.is_file():
            fail(f"missing {fbx}: run gasp_export.py in the Game Animation Sample project first")
            return False
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", False)
        ui.set_editor_property("import_animations", True)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("skeleton", skeleton)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(fbx))
        task.set_editor_property("destination_path", GASP_DIR)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    missing = [p for p in CLIPS if not ASSETS.does_asset_exist(p)]
    for p in missing:
        fail(f"{p} did not import")
    if not missing:
        log(f"imported {len(CLIPS)} Game Animation Sample clips into {GASP_DIR}")
    return not missing


def root_speed_mps(seq: unreal.AnimSequence) -> float:
    """Horizontal root speed over the whole loop, in metres a second."""
    length = seq.get_play_length()
    lib = unreal.AnimationLibrary
    start = lib.get_bone_pose_for_time(seq, "root", 0.0, False).translation
    end = lib.get_bone_pose_for_time(seq, "root", length, False).translation
    return ((end.x - start.x) ** 2 + (end.y - start.y) ** 2) ** 0.5 / 100.0 / max(length, 1e-3)


def auto_rig(path: str, mesh: unreal.SkeletalMesh) -> unreal.IKRigDefinition | None:
    # Reuse the rig if it exists: UE 5.8 can't replace one the retargeter still holds.
    rig = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if not isinstance(rig, unreal.IKRigDefinition):
        rig = fresh_asset(path, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    for chain in list(ctrl.get_retarget_chains()) if hasattr(ctrl, "get_retarget_chains") else []:
        ctrl.remove_retarget_chain(chain.get_editor_property("chain_name"))
    while hasattr(ctrl, "get_num_solvers") and ctrl.get_num_solvers() > 0:
        ctrl.remove_solver(0)
    if not ctrl.set_skeletal_mesh(mesh) or not ctrl.apply_auto_generated_retarget_definition():
        fail(f"{path}: could not generate retarget chains for {mesh.get_path_name()}")
        return None
    ctrl.apply_auto_fbik()
    ASSETS.save_loaded_asset(rig)
    log(f"built {path}")
    return rig


def main() -> None:
    settings = json.loads((PROJECT / "Data" / "Animation.json").read_text(encoding="utf-8"))[0]
    wanted = [settings["IdleAsset"], *settings["WalkAssets"], *settings["JogAssets"], settings["ReadyAsset"], *settings["WalkUprightAssets"]]
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

    if not import_gasp_clips(manny.get_editor_property("skeleton")):
        return

    source = auto_rig(MANNEQUIN_RIG, manny)
    target = auto_rig(PLACEHOLDER_RIG, placeholder)
    if not source or not target:
        return
    rtg = unreal.load_asset(RETARGETER) if ASSETS.does_asset_exist(RETARGETER) else None
    if not isinstance(rtg, unreal.IKRetargeter):
        rtg = fresh_asset(RETARGETER, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.remove_all_ops()
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
        seq = unreal.load_asset(object_path(path))
        if not isinstance(seq, unreal.AnimSequence):
            fail(f"{path} was not made")
            continue
        speed = root_speed_mps(seq)
        # In place: the game moves the pawn, the clip only moves the limbs.
        seq.set_editor_property("force_root_lock", True)
        log(f"{path.rsplit('/', 1)[-1]}: {seq.get_play_length():.2f} s, root speed {speed:.2f} m/s")
        ASSETS.save_asset(object_path(path), only_if_is_dirty=False)
    if ASSETS.does_directory_exist(STAGING):
        ASSETS.delete_directory(STAGING)
    log("FAILED" if failures else "done")


main()
