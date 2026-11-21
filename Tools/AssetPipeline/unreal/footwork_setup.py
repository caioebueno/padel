"""Builds the padel footwork motion matching database from the video footwork
(docs/decisions/0012-video-footwork.md). Step 3 of 3, after mocap/run_wham.sh and
mocap/wham_to_fbx.py. Runs inside the Unreal Editor, after animation_setup.py (it uses that
script's placeholder IK Rig, schema and normalization set):

  Output Log (Python): exec(open("<repo>/Tools/AssetPipeline/unreal/footwork_setup.py").read())
  or headless: UnrealEditor-Cmd Padel.uproject -run=pythonscript -script=<this file>

From SourceArt/_incoming/Footwork/*.fbx (git-ignored: the clips carry the SMPL body) it makes:

  /Game/Characters/Footwork/<first clip>       the SMPL body and skeleton (git-ignored folder)
  /Game/Characters/Footwork/<clip>             every clip on that skeleton
  /Game/Padel/Animations/Retarget/IK_PadelFootwork, RTG_Footwork_To_Placeholder
  /Game/Padel/Animations/MotionMatching/Footwork/Clips/<clip>   retargeted onto the placeholder,
                                               root motion on (the search reads it), root locked
  /Game/Padel/Animations/MotionMatching/Footwork/PSD_Padel_Footwork
                                               the database, on the Walk Loops schema and settings

and rebuilds the normalization set over every database, so footwork and Game Animation Sample
costs compare. Data/Animation.json lists the database in MotionMatchingExtraDatabases (searched
when built, skipped when not) and in MotionMatchingWalkDatabases (the walk band, where padel
footwork lives: under MotionMatchingRunFromMps).

The FBX skeleton uses the Unreal mannequin's bone names, so the auto-generated IK Rig chains
match the placeholder's one to one; both rest poses are T-poses, so nothing is aligned.
Log lines start with [padel-footwork]; the last one is "[padel-footwork] done" or "... FAILED".
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
INCOMING = PROJECT / "SourceArt" / "_incoming" / "Footwork"
GASP_MANIFEST = PROJECT / "SourceArt" / "_incoming" / "GASP" / "motion_matching.json"
CONFIG = PROJECT / "Tools" / "AssetPipeline" / "config" / "footwork.json"
SOURCE_DIR = "/Game/Characters/Footwork"
RETARGET_DIR = "/Game/Padel/Animations/Retarget"
SOURCE_RIG = f"{RETARGET_DIR}/IK_PadelFootwork"
PLACEHOLDER_RIG = f"{RETARGET_DIR}/IK_PadelPlaceholder"
RETARGETER = f"{RETARGET_DIR}/RTG_Footwork_To_Placeholder"
MM_DIR = "/Game/Padel/Animations/MotionMatching"
FOOTWORK_DIR = f"{MM_DIR}/Footwork"
CLIPS_DIR = f"{FOOTWORK_DIR}/Clips"
STAGING = f"{FOOTWORK_DIR}/_Staging"
DATABASE = f"{FOOTWORK_DIR}/PSD_Padel_Footwork"
NORMALIZATION = f"{MM_DIR}/PSN_Stand"
# Footwork is mostly under 2.5 m/s, so it takes the walk loops' schema and database settings.
TEMPLATE_DATABASE = f"{MM_DIR}/PSD_Stand_Walk_Loops"
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SOURCE = unreal.RetargetSourceOrTarget.SOURCE
TARGET = unreal.RetargetSourceOrTarget.TARGET
failures: list[str] = []


def log(msg: str) -> None:
    unreal.log(f"[padel-footwork] {msg}")
    print(f"[padel-footwork] {msg}", flush=True)


def fail(msg: str) -> None:
    failures.append(msg)
    unreal.log_error(f"[padel-footwork] FAILED {msg}")
    print(f"[padel-footwork] FAILED {msg}", flush=True)


def import_fbx(fbx: Path, skeleton: unreal.Skeleton | None) -> None:
    """The first clip brings the body and skeleton; the rest are animations on that skeleton."""
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    # The clips are 29.97 fps (NTSC video). UE 5.8's Interchange FBX import reads that as 29 fps
    # and rejects any clip whose length isn't a whole number of frames, so resample to 30 and snap.
    anim = ui.get_editor_property("anim_sequence_import_data")
    anim.set_editor_property("use_default_sample_rate", False)
    anim.set_editor_property("custom_sample_rate", 30)
    anim.set_editor_property("snap_to_closest_frame_boundary", True)
    if skeleton is None:
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    else:
        ui.set_editor_property("import_mesh", False)
        ui.set_editor_property("skeleton", skeleton)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(fbx))
    task.set_editor_property("destination_path", SOURCE_DIR)
    task.set_editor_property("destination_name", fbx.stem)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    TOOLS.import_asset_tasks([task])


def find_assets(folder: str, cls) -> list:
    found = []
    for path in ASSETS.list_assets(folder, recursive=False):
        asset = unreal.load_asset(path.split(".")[0])
        if isinstance(asset, cls):
            found.append(asset)
    return found


def import_clips(fbxs: list[Path]) -> tuple[unreal.SkeletalMesh | None, list[unreal.AnimSequence]]:
    meshes = find_assets(SOURCE_DIR, unreal.SkeletalMesh) if ASSETS.does_directory_exist(SOURCE_DIR) else []
    if not meshes:
        import_fbx(fbxs[0], None)
        meshes = find_assets(SOURCE_DIR, unreal.SkeletalMesh)
        if not meshes:
            fail(f"{fbxs[0].name} did not import as a skeletal mesh")
            return None, []
    mesh = meshes[0]
    skeleton = mesh.get_editor_property("skeleton")
    have = {a.get_name() for a in find_assets(SOURCE_DIR, unreal.AnimSequence)}
    todo = [f for f in fbxs if f.stem not in have and f"{f.stem}_Anim" not in have]
    for fbx in todo:
        import_fbx(fbx, skeleton)
    clips = [a for a in find_assets(SOURCE_DIR, unreal.AnimSequence) if a.get_skeleton() == skeleton]
    log(f"{len(clips)} clip(s) on {mesh.get_name()} ({len(todo)} imported now)")
    if not clips:
        fail(f"no animation imported from {INCOMING} (see the import errors above)")
    return mesh, clips


def auto_rig(path: str, mesh: unreal.SkeletalMesh) -> unreal.IKRigDefinition | None:
    """Same as animation_setup.py's: the auto-generated retarget chains and full-body IK."""
    rig = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if not isinstance(rig, unreal.IKRigDefinition):
        folder, name = path.rsplit("/", 1)
        rig = TOOLS.create_asset(name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
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


def build_retargeter(source_mesh, target_mesh) -> unreal.IKRetargeter | None:
    source = auto_rig(SOURCE_RIG, source_mesh)
    target = unreal.load_asset(PLACEHOLDER_RIG)
    if not source or not isinstance(target, unreal.IKRigDefinition):
        if source:
            fail(f"no {PLACEHOLDER_RIG}: run animation_setup.py first")
        return None
    rtg = unreal.load_asset(RETARGETER) if ASSETS.does_asset_exist(RETARGETER) else None
    if not isinstance(rtg, unreal.IKRetargeter):
        folder, name = RETARGETER.rsplit("/", 1)
        rtg = TOOLS.create_asset(name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.remove_all_ops()
    ctrl.set_ik_rig(SOURCE, source)
    ctrl.set_ik_rig(TARGET, target)
    ctrl.set_preview_mesh(SOURCE, source_mesh)
    ctrl.set_preview_mesh(TARGET, target_mesh)
    ctrl.add_default_ops()
    ctrl.assign_ik_rig_to_all_ops(SOURCE, source)
    ctrl.assign_ik_rig_to_all_ops(TARGET, target)
    ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    ASSETS.save_loaded_asset(rtg)
    log(f"built {RETARGETER}")
    return rtg


def retarget(clips, source_mesh, target_mesh, rtg) -> list[unreal.AnimSequence]:
    names = [c.get_name() for c in clips]
    todo = [c for c in clips if not ASSETS.does_asset_exist(f"{CLIPS_DIR}/{c.get_name()}")]
    if todo:
        if ASSETS.does_directory_exist(STAGING):
            ASSETS.delete_directory(STAGING)
        inputs = unreal.IKRetargetBatchOperationInputs()
        inputs.set_editor_property("assets_to_retarget", [ASSETS.find_asset_data(c.get_path_name()) for c in todo])
        inputs.set_editor_property("source_mesh", source_mesh)
        inputs.set_editor_property("target_mesh", target_mesh)
        inputs.set_editor_property("ik_retarget_asset", rtg)
        inputs.set_editor_property("target_path", STAGING)
        inputs.set_editor_property("include_referenced_assets", False)
        inputs.set_editor_property("overwrite_existing_files", True)
        made = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        wanted = {c.get_name() for c in todo}
        for data in made:
            asset = data.get_asset()
            if isinstance(asset, unreal.AnimSequence) and str(data.asset_name) in wanted:
                if not ASSETS.rename_asset(asset.get_path_name(), f"{CLIPS_DIR}/{data.asset_name}"):
                    fail(f"could not move {data.asset_name} to {CLIPS_DIR}")
        if ASSETS.does_directory_exist(STAGING):
            ASSETS.delete_directory(STAGING)
        log(f"retargeted {len(todo)} clip(s) into {CLIPS_DIR}")
    out = []
    for name in names:
        seq = unreal.load_asset(f"{CLIPS_DIR}/{name}")
        if not isinstance(seq, unreal.AnimSequence):
            fail(f"{CLIPS_DIR}/{name} was not made")
            continue
        # As for the Game Animation Sample clips: the search reads the root's path, the pose plays in place.
        seq.set_editor_property("enable_root_motion", True)
        seq.set_editor_property("force_root_lock", True)
        ASSETS.save_asset(f"{CLIPS_DIR}/{name}", only_if_is_dirty=False)
        out.append(seq)
    return out


def build_database(settings: dict, clips: list[unreal.AnimSequence]) -> None:
    builder = unreal.PadelMotionMatchingBuilder
    template = unreal.load_asset(TEMPLATE_DATABASE)
    normalization = unreal.load_asset(NORMALIZATION)
    if not isinstance(template, unreal.PoseSearchDatabase) or not isinstance(normalization, unreal.PoseSearchNormalizationSet):
        fail(f"no {TEMPLATE_DATABASE} or {NORMALIZATION}: run animation_setup.py with motion matching first")
        return
    if not GASP_MANIFEST.is_file():
        fail(f"no {GASP_MANIFEST}: run gasp_export.py (the walk loops' settings come from it)")
        return
    manifest = json.loads(GASP_MANIFEST.read_text(encoding="utf-8"))
    source = next((d for d in manifest["databases"] if d["name"].replace("_Dense_", "_") == TEMPLATE_DATABASE.rsplit("/", 1)[-1]), None)
    if source is None:
        fail(f"motion_matching.json has no settings for {TEMPLATE_DATABASE}")
        return
    db_settings = dict(source["settings"])
    overrides = json.loads(CONFIG.read_text(encoding="utf-8"))["database_overrides"]
    db_settings.update({k: v for k, v in overrides.items() if not k.startswith("_")})
    db = builder.find_or_create_asset(unreal.PoseSearchDatabase, DATABASE)
    error = builder.build_database(db, template.get_editor_property("schema"), normalization, clips, json.dumps(db_settings)) if db else f"could not make {DATABASE}"
    if error:
        fail(f"{DATABASE}: {error}")
        return
    ASSETS.save_asset(DATABASE, only_if_is_dirty=False)

    # One normalization over everything searched together, then every index again.
    paths = [*settings["MotionMatchingDatabases"], DATABASE]
    databases = [unreal.load_asset(p) for p in paths]
    if not all(isinstance(d, unreal.PoseSearchDatabase) for d in databases):
        fail("a database in MotionMatchingDatabases isn't built: run animation_setup.py first")
        return
    error = builder.build_normalization_set(normalization, databases)
    if error:
        fail(f"{NORMALIZATION}: {error}")
        return
    ASSETS.save_asset(NORMALIZATION, only_if_is_dirty=False)
    for path, database in zip(paths, databases):
        error = builder.build_index(database)
        if error:
            fail(error)
        ASSETS.save_asset(path, only_if_is_dirty=False)
    seconds = sum(c.get_play_length() for c in clips)
    log(f"{DATABASE.rsplit('/', 1)[-1]}: {len(clips)} clip(s), {seconds / 60:.1f} min; reindexed {len(paths)} database(s)")


def main() -> None:
    settings = json.loads((PROJECT / "Data" / "Animation.json").read_text(encoding="utf-8"))[0]
    if DATABASE not in settings.get("MotionMatchingExtraDatabases", []):
        fail(f"Data/Animation.json MotionMatchingExtraDatabases doesn't list {DATABASE}")
        return
    fbxs = sorted(INCOMING.glob("*.fbx"))
    if not fbxs:
        log(f"no FBX in {INCOMING}: run Tools/AssetPipeline/mocap/wham_to_fbx.py first; nothing to build")
        return
    placeholder = unreal.load_asset(settings["TeamMeshes"][0])
    if not isinstance(placeholder, unreal.SkeletalMesh):
        fail(f"no placeholder at {settings['TeamMeshes'][0]}: run import_assets.py first")
        return
    mesh, clips = import_clips(fbxs)
    if not mesh or not clips:
        return
    rtg = build_retargeter(mesh, placeholder)
    if not rtg:
        return
    retargeted = retarget(clips, mesh, placeholder, rtg)
    if retargeted and not failures:
        build_database(settings, retargeted)


main()
log("FAILED" if failures else "done")
