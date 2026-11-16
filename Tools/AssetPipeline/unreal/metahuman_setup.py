"""Builds the retargeter that drives the MetaHuman players from the placeholder skeleton,
and checks that every MetaHuman named in Data/PlayerBodies.json is on disk
(docs/decisions/0006-metahuman-players.md). Runs inside the Unreal Editor, after
import_assets.py and after the MetaHumans are assembled:

  Output Log (Python): exec(open("Tools/AssetPipeline/unreal/metahuman_setup.py").read())

It creates, from text like the rest of the pipeline (never edit them by hand, run it again):

  /Game/Padel/Animations/Retarget/IK_PadelPlaceholder                 IK Rig of SK_PlayerPlaceholderA
  /Game/Padel/Animations/Retarget/RTG_PadelPlaceholder_To_MetaHuman   placeholder -> MetaHuman body

Log lines start with [padel-metahuman]; the last one is "[padel-metahuman] done" or
"[padel-metahuman] FAILED ...".
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
RETARGET_DIR = "/Game/Padel/Animations/Retarget"
SOURCE_RIG = f"{RETARGET_DIR}/IK_PadelPlaceholder"
RETARGETER = f"{RETARGET_DIR}/RTG_PadelPlaceholder_To_MetaHuman"
METAHUMAN_RIG = "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig"
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SOURCE = unreal.RetargetSourceOrTarget.SOURCE
TARGET = unreal.RetargetSourceOrTarget.TARGET
failures: list[str] = []


def log(msg: str) -> None:
    unreal.log(f"[padel-metahuman] {msg}")
    print(f"[padel-metahuman] {msg}", flush=True)


def fail(msg: str) -> None:
    failures.append(msg)
    unreal.log_error(f"[padel-metahuman] FAILED {msg}")
    print(f"[padel-metahuman] FAILED {msg}", flush=True)


def load_json(name: str):
    return json.loads((PROJECT / "Data" / name).read_text(encoding="utf-8"))


def object_path(path: str) -> str:
    """'/Game/A/B.B_C' or '/Game/A/B.B' -> '/Game/A/B'."""
    return path.split(".")[0]


def fresh_asset(path: str, asset_class, factory):
    """Deletes and recreates an asset, so a run always gives the same result."""
    if ASSETS.does_asset_exist(path):
        ASSETS.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return TOOLS.create_asset(name, folder, asset_class, factory)


def placeholder_mesh(settings) -> unreal.SkeletalMesh | None:
    meshes = settings.get("TeamMeshes") or []
    mesh = unreal.load_asset(meshes[0]) if meshes else None
    if not isinstance(mesh, unreal.SkeletalMesh):
        fail(f"no placeholder skeletal mesh at {meshes[:1]}: run import_assets.py first")
        return None
    return mesh


MISSING: list[str] = []


def metahuman_bodies(bodies) -> dict[str, unreal.SkeletalMesh | None]:
    """Player row -> the body skeletal mesh of its assembled MetaHuman, or None.
    Players whose MetaHuman Blueprint is not on disk go in MISSING."""
    found: dict[str, unreal.SkeletalMesh | None] = {}
    for row in bodies:
        key = row["Name"]
        cls_path = row["MetaHumanClass"]
        bp = unreal.load_asset(object_path(cls_path))
        if bp is None:
            folder = object_path(cls_path).rsplit("/", 1)[0]
            candidates = [a for a in ASSETS.list_assets(folder, recursive=False) if a.rsplit("/", 1)[-1].startswith("BP_")] \
                if ASSETS.does_directory_exist(folder) else []
            hint = f" (found {', '.join(candidates)}: fix MetaHumanClass in Data/PlayerBodies.json)" if candidates else ""
            log(f"{key}: no MetaHuman at {cls_path}{hint}; it shows the placeholder")
            MISSING.append(key)
            found[key] = None
            continue
        # The body mesh sits next to the Blueprint; pick the first skeletal mesh named like a body.
        folder = object_path(cls_path).rsplit("/", 1)[0]
        body = None
        for asset in ASSETS.list_assets(folder, recursive=True):
            name = asset.rsplit("/", 1)[-1].split(".")[0].lower()
            if "body" in name:
                loaded = unreal.load_asset(asset)
                if isinstance(loaded, unreal.SkeletalMesh):
                    body = loaded
                    break
        log(f"{key}: MetaHuman found at {cls_path}" + ("" if body else " (no body mesh found next to it)"))
        found[key] = body
    return found


def build_source_rig(mesh: unreal.SkeletalMesh) -> unreal.IKRigDefinition | None:
    # animation_setup.py builds the same rig and its retargeter uses it: reuse, don't replace.
    existing = unreal.load_asset(SOURCE_RIG)
    if isinstance(existing, unreal.IKRigDefinition):
        return existing
    rig = fresh_asset(SOURCE_RIG, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    if not ctrl.set_skeletal_mesh(mesh):
        fail(f"IK rig: {mesh.get_path_name()} was refused")
        return None
    # The placeholder uses the UE5 mannequin's bone names, which the auto definition knows.
    if not ctrl.apply_auto_generated_retarget_definition():
        fail("IK rig: no retarget chains could be generated for the placeholder skeleton")
        return None
    ctrl.apply_auto_fbik()
    ASSETS.save_loaded_asset(rig)
    log(f"built {SOURCE_RIG}")
    return rig


def target_rig(body: unreal.SkeletalMesh | None) -> unreal.IKRigDefinition | None:
    rig = unreal.load_asset(METAHUMAN_RIG)
    if isinstance(rig, unreal.IKRigDefinition):
        return rig
    if body is None:
        fail(f"no MetaHuman IK rig at {METAHUMAN_RIG} and no MetaHuman body to build one from")
        return None
    # Fallback for an engine whose MetaHuman plugin moved its rig: generate one the same way.
    path = f"{RETARGET_DIR}/IK_PadelMetaHuman"
    rig = fresh_asset(path, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(body)
    ctrl.apply_auto_generated_retarget_definition()
    ctrl.apply_auto_fbik()
    ASSETS.save_loaded_asset(rig)
    log(f"built {path} (the plugin's {METAHUMAN_RIG} was not found)")
    return rig


def build_retargeter(source: unreal.IKRigDefinition, target: unreal.IKRigDefinition,
                     source_mesh: unreal.SkeletalMesh, target_mesh: unreal.SkeletalMesh | None) -> None:
    rtg = fresh_asset(RETARGETER, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.set_ik_rig(SOURCE, source)
    ctrl.set_ik_rig(TARGET, target)
    ctrl.set_preview_mesh(SOURCE, source_mesh)
    if target_mesh is not None:
        ctrl.set_preview_mesh(TARGET, target_mesh)
    ctrl.add_default_ops()
    ctrl.assign_ik_rig_to_all_ops(SOURCE, source)
    ctrl.assign_ik_rig_to_all_ops(TARGET, target)
    ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    # The placeholder is in a T-pose and MetaHumans in an A-pose: line the source's chains up
    # with the target's so the arms don't come out raised.
    if target_mesh is not None:
        ctrl.auto_align_all_bones(SOURCE)
    else:
        log("no MetaHuman assembled yet: chains mapped, pose not aligned. Run again after assembling one.")
    ASSETS.save_loaded_asset(rtg)
    log(f"built {RETARGETER}")


# Outfit material slot -> palette part. The Creator's default garment is a shirt and shorts
# that both come out grey, which reads as one long gown; MetaHuman garments colour through
# these two parameters.
KIT_PARTS = {"_Shirt": "Shirt", "_Short": "Shorts", "_Pants": "Shorts", "_Socks": "Socks", "_Shoe": "Shoes"}
KIT_PARAMS = ("diffuse_color_1", "diffuse_color_2")


def srgb_to_linear(hex_colour: str) -> unreal.LinearColor:
    def ch(v: int) -> float:
        c = v / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    h = hex_colour.lstrip("#")
    return unreal.LinearColor(ch(int(h[0:2], 16)), ch(int(h[2:4], 16)), ch(int(h[4:6], 16)), 1.0)


def paint_kits(bodies) -> None:
    """Colours each assembled MetaHuman's garments with its team kit from palette.json."""
    palette = json.loads((PROJECT / "Tools" / "AssetPipeline" / "config" / "palette.json").read_text(encoding="utf-8"))
    lib = unreal.MaterialEditingLibrary
    for row in bodies:
        if row["Name"] in MISSING:
            continue
        kit = palette.get(row.get("Kit", ""))
        if not kit:
            fail(f"{row['Name']}: Kit {row.get('Kit')!r} is not a palette group")
            continue
        folder = object_path(row["MetaHumanClass"]).rsplit("/", 1)[0]
        painted = []
        for path in ASSETS.list_assets(folder, recursive=True):
            mi = unreal.load_asset(path)
            if not isinstance(mi, unreal.MaterialInstanceConstant):
                continue
            name = mi.get_name()
            part = next((p for suffix, p in KIT_PARTS.items() if name.endswith(suffix)), None)
            if part is None or part not in kit:
                continue
            colour = srgb_to_linear(kit[part]["color"])
            names = [str(n) for n in lib.get_vector_parameter_names(mi)]
            for param in KIT_PARAMS:
                if param in names:
                    lib.set_material_instance_vector_parameter_value(mi, param, colour)
            lib.update_material_instance(mi)
            ASSETS.save_loaded_asset(mi)
            painted.append(f"{name}={kit[part]['color']}")
        log(f"{row['Name']}: kit {row['Kit']}: " + (", ".join(painted) if painted else "no garment materials found"))


def main() -> None:
    settings = load_json("Animation.json")[0]
    bodies = load_json("PlayerBodies.json")
    if object_path(settings.get("MetaHumanRetargeter", "")) != RETARGETER:
        fail(f"Data/Animation.json MetaHumanRetargeter should name {RETARGETER}")

    mesh = placeholder_mesh(settings)
    found = metahuman_bodies(bodies)
    target_mesh = next((b for b in found.values() if b is not None), None)
    if mesh is not None:
        source = build_source_rig(mesh)
        target = target_rig(target_mesh)
        if source and target:
            build_retargeter(source, target, mesh, target_mesh)

    paint_kits(bodies)

    if MISSING:
        log(f"players still on the placeholder: {', '.join(MISSING)}")
    if failures:
        log(f"FAILED {len(failures)} step(s)")
    else:
        log("done")


main()
