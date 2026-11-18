"""Exports the Game Animation Sample clips animation_setup.py uses, as FBX, into this repo's
SourceArt/_incoming/GASP/ (git-ignored). Run it in Epic's Game Animation Sample project
(free on Fab), not in Padel:

  UnrealEditor-Cmd "<GameAnimationSample>/GameAnimationSample.uproject" -run=pythonscript
      -script=<repo>/Tools/AssetPipeline/unreal/gasp_export.py

Two sets of clips:

- CLIPS, the 14 loops the C++ pose blended before motion matching (and still does as its
  fallback when the motion matching databases aren't built).
- M1 motion matching (docs/animation/MOVEMENT_FLUENCY.md): every clip in the sample's Dense
  Stand locomotion databases (DATABASES). A database's animation list isn't readable from
  Python, so the clips come from its package dependencies (a database references each clip
  it holds, and its schema). The database and schema settings Python can read go into
  SourceArt/_incoming/GASP/motion_matching.json, which animation_setup.py rebuilds the
  databases from on the placeholder skeleton.

Log lines start with [padel-gasp]; the last one is "[padel-gasp] done" or "... FAILED".
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal

OUT = Path(__file__).resolve().parents[3] / "SourceArt" / "_incoming" / "GASP"
MANIFEST = OUT / "motion_matching.json"
ANIMS = "/Game/Characters/UEFN_Mannequin/Animations"
MM_DIR = f"{ANIMS}/MotionMatchingData/Databases/Dense"
# Keep in step with CLIPS in animation_setup.py.
CLIPS = [
    "Idle/M_Neutral_Stand_Idle_Loop",
    "Crouch/M_Relaxed_Crouch_Loop_F",
    "Crouch/M_Relaxed_Crouch_Loop_B",
    "Crouch/M_Relaxed_Crouch_Loop_LL",
    "Crouch/M_Relaxed_Crouch_Loop_RR",
    "Run/M_Relaxed_Run_Loop_F",
    "Run/M_Relaxed_Run_Loop_B",
    "Run/M_Relaxed_Run_Loop_LL",
    "Run/M_Relaxed_Run_Loop_RR_offset",
    "Idle/M_Neutral_Crouch_Idle_Loop",
    "Walk/M_Relaxed_Walk_Loop_F",
    "Walk/M_Relaxed_Walk_Loop_B",
    "Walk/M_Relaxed_Walk_Loop_LL",
    "Walk/M_Relaxed_Walk_Loop_RR_offset",
]
# M1: the sample's Dense Stand locomotion set (Data/Animation.json MotionMatchingDatabases
# names the copies animation_setup.py builds from them).
DATABASES = [
    f"PSD_Dense_Stand_{gait}_{kind}"
    for gait in ("Walk", "Run", "Sprint")
    for kind in ("Loops", "Starts", "Stops", "Pivots")
] + ["PSD_Dense_Stand_Idles", "PSD_Dense_Stand_TurnInPlace"]

# Database and schema properties copied into the manifest when Python can read them.
DATABASE_PROPS = [
    "continuing_pose_cost_bias",
    "base_cost_bias",
    "looping_cost_bias",
    "exclude_from_database_parameters",
    "additional_extrapolation_time",
    "tags",
]
SCHEMA_PROPS = ["sample_rate", "data_preprocessor", "number_of_permutations", "permutations_sample_rate", "permutations_time_offset"]


def log(msg: str) -> None:
    unreal.log(f"[padel-gasp] {msg}")
    print(f"[padel-gasp] {msg}", flush=True)


def to_json(value):
    """A reflected value as plain JSON."""
    if isinstance(value, (bool, int, float, str)) or value is None:
        return value
    if isinstance(value, unreal.Name):
        return str(value)
    if isinstance(value, (list, tuple, unreal.Array)):
        return [to_json(v) for v in value]
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, unreal.FloatInterval):
        return {"min": value.min, "max": value.max}
    if isinstance(value, unreal.EnumBase):
        return str(value.name)
    return str(value)


def read_props(obj, names: list[str]) -> dict:
    out = {}
    for name in names:
        try:
            out[name] = to_json(obj.get_editor_property(name))
        except Exception:  # not exposed to Python in this engine version
            pass
    return out


def describe_channel(channel) -> dict:
    """A feature channel's class and the settings the schema rebuild needs."""
    info = {"class": channel.get_class().get_name()}
    for name in ("weight", "sample_time_offset", "input_query_pose", "heading_axis", "offset", "bone",
                 "sample_role", "use_feature_channel_position", "normalization_group"):
        try:
            value = channel.get_editor_property(name)
        except Exception:
            continue
        if isinstance(value, unreal.BoneReference):
            value = str(value.get_editor_property("bone_name"))
        info[name] = to_json(value)
    try:
        info["samples"] = [
            {"offset": s.get_editor_property("offset"), "flags": int(s.get_editor_property("flags")), "weight": s.get_editor_property("weight")}
            for s in channel.get_editor_property("samples")
        ]
    except Exception:
        pass
    try:
        info["sampled_bones"] = [
            {
                "bone": str(b.get_editor_property("reference").get_editor_property("bone_name")),
                "flags": int(b.get_editor_property("flags")),
                "weight": b.get_editor_property("weight"),
            }
            for b in channel.get_editor_property("sampled_bones")
        ]
    except Exception:
        pass
    try:
        info["sub_channels"] = [describe_channel(c) for c in channel.get_editor_property("sub_channels") if c]
    except Exception:
        pass
    return info


def database_clips(db_path: str) -> list[str]:
    """The animation sequences a database references, as asset paths."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False,
    )
    clips = []
    for package in registry.get_dependencies(db_path, options) or []:
        package = str(package)
        if not package.startswith(ANIMS):
            continue
        if isinstance(unreal.load_asset(package), unreal.AnimSequence):
            clips.append(package)
    return sorted(set(clips))


def export_clip(seq: unreal.AnimSequence, filename: Path) -> bool:
    options = unreal.FbxExportOption()
    options.set_editor_property("export_morph_targets", False)
    options.set_editor_property("export_preview_mesh", False)
    task = unreal.AssetExportTask()
    task.object = seq
    task.filename = str(filename)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.AnimSequenceExporterFBX()
    task.options = options
    return bool(unreal.Exporter.run_asset_export_task(task))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    failed = False
    wanted: dict[str, str] = {f"{ANIMS}/{clip}": clip.rsplit("/", 1)[-1] for clip in CLIPS}

    manifest = {"source": "Epic Games, Game Animation Sample Project (Fab)", "databases": [], "schemas": {}}
    for name in DATABASES:
        path = f"{MM_DIR}/{name}"
        db = unreal.load_asset(path)
        if not isinstance(db, unreal.PoseSearchDatabase):
            unreal.log_error(f"[padel-gasp] FAILED no database {path}")
            failed = True
            continue
        clips = database_clips(path)
        if not clips:
            unreal.log_error(f"[padel-gasp] FAILED {name} references no animation sequences")
            failed = True
            continue
        schema = db.get_editor_property("schema")
        entry = {"name": name, "schema": schema.get_name() if schema else None, "settings": read_props(db, DATABASE_PROPS), "clips": []}
        for clip in clips:
            seq = unreal.load_asset(clip)
            short = clip.rsplit("/", 1)[-1]
            wanted[clip] = short
            entry["clips"].append({"name": short, "source": clip, "length_s": seq.get_play_length()})
        manifest["databases"].append(entry)
        log(f"{name}: {len(clips)} clip(s), schema {entry['schema']}")
        if schema and entry["schema"] not in manifest["schemas"]:
            skeletons = []
            try:
                for roled in schema.get_editor_property("skeletons"):
                    skeletons.append({
                        "skeleton": to_json(roled.get_editor_property("skeleton")),
                        "mirror_data_table": to_json(roled.get_editor_property("mirror_data_table")),
                    })
            except Exception:
                pass
            manifest["schemas"][entry["schema"]] = {
                "settings": read_props(schema, SCHEMA_PROPS),
                "skeletons": skeletons,
                "channels": [describe_channel(c) for c in schema.get_editor_property("channels") if c],
            }

    for path, short in sorted(wanted.items()):
        seq = unreal.load_asset(path)
        if not isinstance(seq, unreal.AnimSequence):
            unreal.log_error(f"[padel-gasp] FAILED no {path}")
            failed = True
            continue
        fbx = OUT / f"{short}.fbx"
        if fbx.is_file():
            continue  # exported on an earlier run; delete it to export again
        if export_clip(seq, fbx):
            log(str(fbx))
        else:
            unreal.log_error(f"[padel-gasp] FAILED export {path}")
            failed = True

    MANIFEST.write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    log(f"wrote {MANIFEST} ({sum(len(d['clips']) for d in manifest['databases'])} database clip(s), {len(wanted)} FBX)")
    unreal.log("[padel-gasp] FAILED" if failed else "[padel-gasp] done")


main()
