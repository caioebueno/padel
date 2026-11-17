"""Builds the slice players' MetaHumans from Tools/AssetPipeline/config/metahumans.json
(docs/decisions/0006-metahuman-players.md), with no clicks in the MetaHuman Creator:

  preset copy -> height -> hair and facial hair -> team-kit shirt and shorts
  -> Auto-Rig (joints only) -> texture download (2K) -> optimised Medium assembly

into the folder Data/PlayerBodies.json names for the player. Needs the full editor (the
assembly bakes materials on the GPU), the MetaHuman Creator Core Data, a saved MetaHuman
sign-in (made once by an Auto-Rig in the Creator) and about 10 GB of free memory:

  UnrealEditor Padel.uproject -ExecutePythonScript="<repo>/Tools/AssetPipeline/unreal/metahuman_build.py"

or from the Output Log (Python) with exec(open(...).read()). Pass players to build only
some: set the environment variable PADEL_MH_PLAYERS=selva_arunde,ruben_halvaro.
Log lines start with [padel-mh-build]; the last one is "done" or "FAILED ...". Run
metahuman_setup.py afterwards for the retargeter.
"""

from __future__ import annotations

import json
import os
from pathlib import Path

import unreal

PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
PRESETS = "/MetaHumanCharacter/Optional/Presets"
GROOMS = "/MetaHumanCharacter/Optional/Grooms/Bindings"
GARMENT = "/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment"
SOURCE_DIR = "/Game/Padel/Characters/MetaHumans/Source"
ASSETS = unreal.EditorAssetLibrary
failures: list[str] = []


def log(msg: str) -> None:
    unreal.log(f"[padel-mh-build] {msg}")


def fail(msg: str) -> None:
    failures.append(msg)
    unreal.log_error(f"[padel-mh-build] FAILED {msg}")


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def srgb(hex_colour: str) -> unreal.LinearColor:
    def ch(v: int) -> float:
        c = v / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    h = hex_colour.lstrip("#")
    return unreal.LinearColor(ch(int(h[0:2], 16)), ch(int(h[2:4], 16)), ch(int(h[4:6], 16)), 1.0)


def target_of(body_row) -> tuple[str, str]:
    """('/Game/.../MetaHumans', 'SelvaArunde') from '/Game/.../MetaHumans/SelvaArunde/BP_SelvaArunde.BP_SelvaArunde_C'."""
    folder = body_row["MetaHumanClass"].split(".")[0].rsplit("/", 1)[0]
    root, name = folder.rsplit("/", 1)
    return root, name


def select(character, slot: str, wardrobe_path: str) -> unreal.MetaHumanPaletteItemKey | None:
    item = unreal.load_asset(wardrobe_path)
    if item is None:
        fail(f"{character.get_name()}: no wardrobe item {wardrobe_path}")
        return None
    key = character.internal_collection.try_add_item_from_wardrobe_item(slot, item)
    if key is None:
        fail(f"{character.get_name()}: {slot} refused {wardrobe_path}")
        return None
    character.internal_collection.default_instance.set_single_slot_selection(slot, key)
    return key


def clear_slot(character, slot: str) -> None:
    character.internal_collection.default_instance.set_single_slot_selection(slot, unreal.MetaHumanPaletteItemKey())


def set_param(params, name: str, colour: unreal.LinearColor) -> bool:
    param = next((p for p in params if str(p.name) == name), None)
    if param is None:
        return False
    param.set_color(value=colour)
    return True


def build_one(spec, body_row, kit, subsystem) -> None:
    player = spec["player"]
    root, name = target_of(body_row)
    source_path = f"{SOURCE_DIR}/MH_{name}"
    if ASSETS.does_asset_exist(source_path):
        ASSETS.delete_asset(source_path)
    character = ASSETS.duplicate_asset(f"{PRESETS}/{spec['preset']}", source_path)
    if not isinstance(character, unreal.MetaHumanCharacter):
        fail(f"{player}: could not copy preset {spec['preset']}")
        return
    log(f"{player}: {source_path} from preset {spec['preset']}")

    if spec.get("hair"):
        select(character, "Hair", f"{GROOMS}/{spec['hair']}")
    if spec.get("beard"):
        select(character, "Beard", f"{GROOMS}/{spec['beard']}")
    else:
        clear_slot(character, "Beard")
        clear_slot(character, "Mustache")
    outfit = select(character, "Outfits", GARMENT)

    if not subsystem.try_add_object_to_edit(character):
        fail(f"{player}: the character is open somewhere else")
        return
    try:
        constraints = subsystem.get_body_constraints(character)
        by_name = {str(c.name).lower().replace(" ", "_"): c for c in constraints}
        if "height" in by_name:
            by_name["height"].is_active = True
            by_name["height"].target_measurement = float(spec["height_cm"])
            subsystem.set_body_constraints(character, list(by_name.values()))
            subsystem.commit_body_state(character)
        else:
            fail(f"{player}: no height constraint")

        # Team kit on the Creator's default garment: shirt and shorts in the kit colours,
        # trims in the kit's trim colour. Grey on grey read as one long gown (0006).
        if outfit is not None:
            subsystem.assemble_for_preview(character=character)
            path = unreal.MetaHumanPaletteItemPath(item_key=outfit)
            params = character.internal_collection.default_instance.get_instance_parameters(item_path=path)
            wanted = {
                "PrimaryColorShirt": kit["Shirt"]["color"],
                "SecondaryColorShirt": kit["Trim"]["color"],
                "PrimaryColorShort": kit["Shorts"]["color"],
                "SecondaryColorShort": kit["Trim"]["color"],
            }
            missing = [n for n, c in wanted.items() if not set_param(params, n, srgb(c))]
            if missing:
                # Not fatal: the game tints the shirt and shorts per player at spawn anyway
                # (ShirtColour/ShortsColour in PlayerBodies.json).
                log(f"{player}: garment colours left as built (no {', '.join(missing)}); the game applies the kit")

        rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rig.blocking = True
        rig.report_progress = False
        rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        subsystem.request_auto_rigging(character, rig)
        log(f"{player}: auto-rigged")

        textures = unreal.MetaHumanCharacterTextureRequestParams()
        textures.blocking = True
        textures.report_progress = False
        subsystem.request_texture_sources(character, textures)
        log(f"{player}: textures downloaded")
        ASSETS.save_loaded_asset(character)

        params = unreal.MetaHumanCharacterEditorBuildParameters()
        params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        params.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        params.absolute_build_path = root
        params.name_override = name
        params.common_folder_path = "/Game/MetaHumans/Common"
        subsystem.build_meta_human(character=character, params=params)
    finally:
        if subsystem.is_object_added_for_editing(character):
            subsystem.remove_object_to_edit(character)

    built = f"{root}/{name}/BP_{name}"
    if ASSETS.does_asset_exist(built):
        ASSETS.save_directory(f"{root}/{name}", only_if_is_dirty=False, recursive=True)
        ASSETS.save_directory("/Game/MetaHumans/Common", only_if_is_dirty=True, recursive=True)
        log(f"{player}: assembled {built}")
    else:
        fail(f"{player}: assembly left no {built}")


def main() -> None:
    config = load_json(PROJECT / "Tools" / "AssetPipeline" / "config" / "metahumans.json")
    bodies = {r["Name"]: r for r in load_json(PROJECT / "Data" / "PlayerBodies.json")}
    palette = load_json(PROJECT / "Tools" / "AssetPipeline" / "config" / "palette.json")
    only = [p for p in os.environ.get("PADEL_MH_PLAYERS", "").split(",") if p]
    subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    for spec in config["players"]:
        player = spec["player"]
        if (only and player not in only) or (not only and not spec.get("build")):
            continue
        row = bodies.get(player)
        kit = palette.get(row.get("Kit", "")) if row else None
        if not row or not kit:
            fail(f"{player}: needs a Data/PlayerBodies.json row with a Kit from palette.json")
            continue
        try:
            build_one(spec, row, kit, subsystem)
        except Exception as error:  # keep going with the next player
            fail(f"{player}: {error}")
    log(f"FAILED {len(failures)} step(s)" if failures else "done")


main()
if os.environ.get("PADEL_MH_QUIT") == "1":
    unreal.SystemLibrary.quit_editor()
