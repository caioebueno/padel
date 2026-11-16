"""Exports the Game Animation Sample clips animation_setup.py uses, as FBX, into this repo's
SourceArt/_incoming/GASP/ (git-ignored). Run it in Epic's Game Animation Sample project
(free on Fab), not in Padel:

  UnrealEditor-Cmd "<GameAnimationSample>/GameAnimationSample.uproject" -run=pythonscript
      -script=<repo>/Tools/AssetPipeline/unreal/gasp_export.py

Log lines start with [padel-gasp]; the last one is "[padel-gasp] done" or "... FAILED".
"""

from __future__ import annotations

from pathlib import Path

import unreal

OUT = Path(__file__).resolve().parents[3] / "SourceArt" / "_incoming" / "GASP"
ANIMS = "/Game/Characters/UEFN_Mannequin/Animations"
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
]


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    failed = False
    for clip in CLIPS:
        seq = unreal.load_asset(f"{ANIMS}/{clip}")
        if not isinstance(seq, unreal.AnimSequence):
            unreal.log_error(f"[padel-gasp] FAILED no {ANIMS}/{clip}")
            failed = True
            continue
        options = unreal.FbxExportOption()
        options.set_editor_property("export_morph_targets", False)
        options.set_editor_property("export_preview_mesh", False)
        task = unreal.AssetExportTask()
        task.object = seq
        task.filename = str(OUT / f"{clip.rsplit('/', 1)[-1]}.fbx")
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.AnimSequenceExporterFBX()
        task.options = options
        if unreal.Exporter.run_asset_export_task(task):
            unreal.log(f"[padel-gasp] {task.filename}")
        else:
            unreal.log_error(f"[padel-gasp] FAILED export {clip}")
            failed = True
    unreal.log("[padel-gasp] FAILED" if failed else "[padel-gasp] done")


main()
