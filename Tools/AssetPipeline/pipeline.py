#!/usr/bin/env python3
"""Padel Street asset pipeline: one command from the catalogue to Unreal assets.

    python3 Tools/AssetPipeline/pipeline.py all              # build everything, then import into Unreal
    python3 Tools/AssetPipeline/pipeline.py build            # Blender only: SourceArt/*.fbx and textures
    python3 Tools/AssetPipeline/pipeline.py build Ball       # just some assets
    python3 Tools/AssetPipeline/pipeline.py generate PlayerGenA   # call the AI service (costs credits)
    python3 Tools/AssetPipeline/pipeline.py unreal           # import SourceArt into Content/Padel
    python3 Tools/AssetPipeline/pipeline.py preview out.png  # render the procedural assets in Blender (--preset, --camera)
    python3 Tools/AssetPipeline/pipeline.py preview-ball out.png  # close-up render of the ball (felt, seam, mark)
    python3 Tools/AssetPipeline/pipeline.py check            # validate config, prompts and names (no Blender)

See Tools/AssetPipeline/README.md for setup (Blender, API keys, Unreal).
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import platform
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from asset_lib import INCOMING_DIR, RAW_DIR, REPO_ROOT, SOURCE_ART_DIR, UNREAL_DIR  # noqa: E402
from asset_lib import ball_textures, blender_runner, catalogue, court_spec, glass, lighting, manifest, providers, textures, turf_textures  # noqa: E402
from asset_lib.catalogue import Asset  # noqa: E402

PROCEDURAL_SCRIPTS = {"court_glass": "build_court_glass", "court_frame": "build_court_frame",
                      "ball": "build_equipment", "ball_trail": "build_equipment", "racket": "build_equipment", "character": "build_character",
                      "net": "build_net", "court_surface": "build_court_surface", "venue": "build_venue"}


def log(msg: str) -> None:
    print(f"[assets] {msg}", flush=True)


def _rel(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def _base_job(asset: Asset) -> dict:
    return {
        "id": asset.asset_name,
        "kind": asset.kind,
        "size": asset.size,
        "budget_tris": asset.budget_tris,
        "lods": [{"ratio": lod.ratio} for lod in asset.lods],
        "out": str(asset.fbx_path),
        "lod_outs": [str(asset.lod_fbx_path(i)) for i in range(1, len(asset.lods) + 1)],
    }


def raw_dir(asset: Asset) -> Path:
    return RAW_DIR / asset.id


def find_raw(asset: Asset) -> Path | None:
    folder = raw_dir(asset)
    for name in ("rigged.glb", "model.glb"):
        if (folder / name).is_file():
            return folder / name
    return None


def generate(asset: Asset) -> Path:
    """Sends the prompt to the asset's AI provider and downloads the GLB (and the rigged GLB)."""
    prompt, negative, options = catalogue.render_prompt(asset)
    provider = providers.make(asset.raw["provider"])
    log(f"{asset.id}: generating with {provider.name} (this can take several minutes)")
    result = provider.generate(prompt, negative, options, asset.budget_tris)
    folder = raw_dir(asset)
    provider.download(result.glb_url, folder / "model.glb")
    final = folder / "model.glb"
    if asset.kind == "skeletal" and asset.raw.get("rig"):
        log(f"{asset.id}: rigging with {provider.name}")
        result = provider.rig(result, asset.size["height_m"], asset.raw["rig"].get("skeleton", "ue5_mannequin"))
        final = provider.download(result.glb_url, folder / "rigged.glb")
    provenance = {
        "provider": provider.name,
        "tasks": result.task_chain,
        "prompt": prompt,
        "negative": negative,
        "prompt_sha256": hashlib.sha256((prompt + "\n" + negative).encode()).hexdigest(),
        "generated_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "licence": provider.licence,
    }
    (folder / "provenance.json").write_text(json.dumps(provenance, indent=2), encoding="utf-8")
    log(f"{asset.id}: saved {_rel(final)}")
    return final


def build_asset(asset: Asset, spec: dict, palette: dict, allow_generate: bool) -> list[dict] | None:
    job = _base_job(asset)
    if asset.source == "procedural":
        builder = asset.raw["builder"]
        job.update({"builder": builder, "spec": spec, "palette": palette,
                    "palette_group": asset.raw.get("palette", ""), "part": asset.raw.get("part", "solid"),
                    "body": asset.raw.get("body", {}), "net": asset.raw.get("net", {}),
                    "surface": turf_textures.load_config(), "textures_dir": str(textures.TEXTURE_DIR)})
        if builder == "venue":
            # Trusses and masts carry the fixtures Data/Lighting.json lights (decision 0006).
            job["rigs"] = lighting.venue_rigs()
        report = blender_runner.run(PROCEDURAL_SCRIPTS[builder], job)
        source = f"procedural: Tools/AssetPipeline/blender/{PROCEDURAL_SCRIPTS[builder]}.py"
        licence = "original (this repo)"
    else:
        if asset.source == "ai":
            raw = find_raw(asset)
            if raw is None:
                if not allow_generate:
                    log(f"{asset.id}: skipped (no download in {_rel(raw_dir(asset))}; run `generate {asset.id}` or pass --generate)")
                    return None
                raw = generate(asset)
            prov = json.loads((raw.parent / "provenance.json").read_text(encoding="utf-8"))
            source = f"{prov['provider']} tasks {', '.join(prov['tasks'])}; prompt sha256 {prov['prompt_sha256'][:12]}; {prov['generated_at']}"
            licence = prov["licence"]
        else:
            raw = INCOMING_DIR / asset.raw["file"]
            if not raw.is_file():
                log(f"{asset.id}: skipped (missing {_rel(raw)})")
                return None
            source = asset.raw.get("credit", f"hand-off file {asset.raw['file']}")
            licence = asset.raw.get("licence", "")
            if not licence:
                raise SystemExit(f"{asset.id}: external assets need a 'licence' in assets.json")
        job["source_file"] = str(raw)
        report = blender_runner.run("cleanup", job)
    tris = report["triangles"]
    if tris > asset.budget_tris:
        raise SystemExit(f"{asset.id}: {tris} triangles is over the budget of {asset.budget_tris}")
    lod_note = f", LODs {report['lod_triangles']}" if report.get("lod_triangles") else ""
    log(f"{asset.id}: {_rel(asset.fbx_path)} ({tris} tris{lod_note})")
    notes = f"{asset.kind} mesh, {tris} tris{lod_note}; Unreal: {asset.unreal_folder}/{asset.asset_name}"
    rows = [{"path": _rel(asset.fbx_path), "source": source, "licence": licence, "notes": notes}]
    for i in range(1, len(asset.lods) + 1):
        rows.append({"path": _rel(asset.lod_fbx_path(i)), "source": source, "licence": licence,
                     "notes": f"LOD{i} of {asset.asset_name}"})
    return rows


def cmd_build(args: argparse.Namespace) -> None:
    cat = catalogue.load()
    palette = catalogue.load_palette()
    spec = court_spec.load()
    wanted = set(args.ids)
    unknown = wanted - {a.id for a in cat.assets}
    if unknown:
        raise SystemExit(f"unknown asset id(s): {', '.join(sorted(unknown))}")
    rows: list[dict] = []
    generated = [(tex, "textures.py") for tex in textures.generate_all(palette)]
    generated += [(tex, "glass.py") for tex in glass.generate_textures()]
    ball = cat.get("Ball")
    generated += [(tex, "ball_textures.py (config/ball.json)")
                  for tex in ball_textures.generate_all(ball.size["diameter_m"] * 500.0, textures.TEXTURE_DIR)]
    for tex, script in generated:
        log(f"texture {_rel(tex)}")
        rows.append({"path": _rel(tex), "source": f"procedural: Tools/AssetPipeline/asset_lib/{script}",
                     "licence": "original (this repo)", "notes": "texture"})
    for tex in turf_textures.generate_all(spec, textures.TEXTURE_DIR):
        log(f"texture {_rel(tex)}")
        rows.append({"path": _rel(tex), "source": "procedural: Tools/AssetPipeline/asset_lib/turf_textures.py",
                     "licence": "original (this repo)", "notes": "texture (config/court_surface.json)"})
    for asset in cat.assets:
        if wanted and asset.id not in wanted:
            continue
        if not asset.enabled and asset.id not in wanted:
            log(f"{asset.id}: disabled in assets.json")
            continue
        built = build_asset(asset, spec, palette, args.generate)
        rows += built or []
    manifest.upsert(rows)
    log(f"manifest updated: {_rel(manifest.MANIFEST_PATH)}")
    write_gitignore(cat)


def write_gitignore(cat: catalogue.Catalogue) -> None:
    """Procedural outputs are rebuilt from the scripts by `build`, so git ignores
    them (CLAUDE.md: don't commit generated files). AI and hand-off outputs are
    committed through Git LFS, because they can't be rebuilt for free."""
    lines = ["# Written by Tools/AssetPipeline/pipeline.py build: procedural outputs, rebuilt from the scripts.",
             "/Textures/"]
    for asset in cat.assets:
        if asset.source == "procedural":
            lines.append("/" + asset.fbx_path.relative_to(SOURCE_ART_DIR).as_posix())
            lines += ["/" + asset.lod_fbx_path(i).relative_to(SOURCE_ART_DIR).as_posix()
                      for i in range(1, len(asset.lods) + 1)]
    (SOURCE_ART_DIR / ".gitignore").write_text("\n".join(lines) + "\n", encoding="utf-8")


def cmd_generate(args: argparse.Namespace) -> None:
    cat = catalogue.load()
    for asset_id in args.ids:
        asset = cat.get(asset_id)
        if asset.source != "ai":
            raise SystemExit(f"{asset_id} is {asset.source}, not an AI asset")
        generate(asset)


def unreal_editor() -> Path:
    system = platform.system()
    if system == "Darwin":
        root = Path(os.environ.get("UE_ROOT", "/Users/Shared/Epic Games/UE_5.8"))
        exe = root / "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
    elif system == "Windows":
        root = Path(os.environ.get("UE_ROOT", r"C:\Program Files\Epic Games\UE_5.8"))
        exe = root / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
    else:
        if "UE_ROOT" not in os.environ:
            raise SystemExit("Set UE_ROOT to your Unreal Engine 5.8 install")
        exe = Path(os.environ["UE_ROOT"]) / "Engine/Binaries/Linux/UnrealEditor-Cmd"
    if not exe.exists():
        raise SystemExit(f"Unreal Editor not found at {exe}; set UE_ROOT")
    return exe


def cmd_unreal(args: argparse.Namespace) -> None:
    editor = unreal_editor()
    script = UNREAL_DIR / "import_assets.py"
    log(f"importing into Unreal with {editor.name} (the editor must be closed)")
    # The Python Script Plugin's commandlet runs the import without opening the editor UI.
    proc = subprocess.run([str(editor), str(REPO_ROOT / "Padel.uproject"), "-run=pythonscript",
                           f"-script={script}", "-unattended", "-nullrhi", "-nosplash", "-nosound", "-stdout",
                           "-FullStdOutLogOutput"], text=True, capture_output=True)
    out = proc.stdout + proc.stderr
    lines = [line for line in out.splitlines() if "[padel-import]" in line]
    print("\n".join(lines) or out[-4000:])
    if proc.returncode != 0 or not any("[padel-import] done" in line for line in lines):
        raise SystemExit("Unreal import failed; see the log above")


def cmd_preview(args: argparse.Namespace) -> None:
    cat = catalogue.load()
    spec = court_spec.load()
    if not (textures.TEXTURE_DIR / "T_Turf_N.png").is_file():
        turf_textures.generate_all(spec, textures.TEXTURE_DIR)
    job = {"spec": spec, "palette": catalogue.load_palette(),
           "surface": turf_textures.load_config(), "textures_dir": str(textures.TEXTURE_DIR),
           "assets": [a.raw for a in cat.assets if a.source == "procedural"],
           "out": str(Path(args.out).resolve()), "samples": args.samples, "resolution": [1280, 720]}
    # Lit like the game: the preset's fixtures, sun and exposure, from a look-dev camera.
    name, preset = lighting.preset(args.preset)
    cameras = json.loads((REPO_ROOT / "Data" / "LookDev.json").read_text(encoding="utf-8"))["Cameras"]
    camera = next((c for c in cameras if c["Name"] == args.camera), None)
    if camera is None:
        raise SystemExit(f"no camera '{args.camera}' in Data/LookDev.json")
    job.update({"preset_name": name, "preset": preset, "fixtures": lighting.fixtures(preset),
                "rigs": lighting.venue_rigs(), "camera": camera})
    blender_runner.run("render_preview", job)
    log(f"preview written to {args.out}")


def cmd_preview_ball(args: argparse.Namespace) -> None:
    cat = catalogue.load()
    ball = cat.get("Ball")
    if not (textures.TEXTURE_DIR / "T_Ball_Masks.png").is_file():
        ball_textures.generate_all(ball.size["diameter_m"] * 500.0, textures.TEXTURE_DIR)
    job = {"ball": ball.raw, "palette": catalogue.load_palette(), "textures_dir": str(textures.TEXTURE_DIR),
           "out": str(Path(args.out).resolve()), "samples": args.samples, "resolution": [1600, 900]}
    blender_runner.run("render_ball", job)
    log(f"ball preview written to {args.out}")


def cmd_check(args: argparse.Namespace) -> None:
    cat = catalogue.load()
    catalogue.load_palette()
    court_spec.load()
    glass.load_config()
    ball_textures.load_config()
    for asset in cat.assets:
        if asset.source == "ai":
            catalogue.render_prompt(asset)
    log(f"ok: {len(cat.assets)} assets, palette, prompts and CourtSpec.h all valid")


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("build", help="build SourceArt from the catalogue (Blender)")
    p.add_argument("ids", nargs="*", help="asset ids (default: every enabled asset)")
    p.add_argument("--generate", action="store_true", help="call AI services for AI assets with no download yet")
    p.set_defaults(func=cmd_build)
    p = sub.add_parser("all", help="build, then import into Unreal")
    p.add_argument("--generate", action="store_true")
    p.set_defaults(func=lambda a: (cmd_build(argparse.Namespace(ids=[], generate=a.generate)), cmd_unreal(a)))
    p = sub.add_parser("generate", help="generate AI assets and download them to SourceArt/_raw/")
    p.add_argument("ids", nargs="+")
    p.set_defaults(func=cmd_generate)
    p = sub.add_parser("unreal", help="import SourceArt into Content/Padel (Unreal Python)")
    p.set_defaults(func=cmd_unreal)
    p = sub.add_parser("preview", help="render a Blender preview of the procedural assets")
    p.add_argument("out")
    p.add_argument("--samples", type=int, default=24)
    p.add_argument("--preset", default=None, help="Data/Lighting.json preset (default: the file's Default)")
    p.add_argument("--camera", default="Broadcast", help="Data/LookDev.json camera name")
    p.set_defaults(func=cmd_preview)
    p = sub.add_parser("preview-ball", help="render a close-up of the ball in Blender (felt, seam, mark)")
    p.add_argument("out")
    p.add_argument("--samples", type=int, default=64)
    p.set_defaults(func=cmd_preview_ball)
    p = sub.add_parser("check", help="validate the catalogue, palette, prompts and names")
    p.set_defaults(func=cmd_check)
    args = parser.parse_args(argv)
    try:
        args.func(args)
    except (catalogue.CatalogueError, providers.ProviderError, blender_runner.BlenderError) as err:
        raise SystemExit(f"[assets] error: {err}") from None


if __name__ == "__main__":
    main()
