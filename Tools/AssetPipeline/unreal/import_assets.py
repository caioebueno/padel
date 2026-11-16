"""Imports SourceArt/ into Content/Padel/ and builds the materials. Runs inside the
Unreal Editor (Python Editor Script Plugin), usually through
`python3 Tools/AssetPipeline/pipeline.py unreal`, or from the editor's
Output Log (Python): `exec(open("Tools/AssetPipeline/unreal/import_assets.py").read())`.

Everything it creates is generated from text (config/assets.json,
config/palette.json and the FBX/PNG files), like the input actions in
PadelInputConfig.cpp, so never edit these assets by hand: change the config and
run it again.

  /Game/Padel/Materials/M_Padel<Master>     master materials (Opaque, Turf, Glass, Masked, Emissive, Net, NetSolid);
                                            M_PadelGlass is built by glass_material.py
  /Game/Padel/Materials/MPC_NetReaction     the net hit reaction's parameters (written by UPadelNetReactionSubsystem)
  /Game/Padel/Materials/MI_<Group>_<Name>   one instance per palette entry
  /Game/Padel/Materials/T_*                 textures
  /Game/Padel/<Category>/SM_* and SK_*      meshes, with their slots set to the MIs above

Log lines start with [padel-import]; the last one is "[padel-import] done".
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import unreal

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

from asset_lib import SOURCE_ART_DIR, catalogue  # noqa: E402
import glass_material  # noqa: E402

MATERIAL_DIR = "/Game/Padel/Materials"
MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
WHITE = "/Engine/EngineResources/WhiteSquareTexture"
FLAT_NORMAL = "/Engine/EngineMaterials/FlatNormal"
NET_MPC = "MPC_NetReaction"
# Vector parameters of MPC_NetReaction, in the order the HLSL takes them (see NetReaction.hlsl).
NET_MPC_VECTORS = ("Impact0", "Impact1", "Shape", "Motion", "Net", "Extra")
failures: list[str] = []


def log(msg: str) -> None:
    unreal.log(f"[padel-import] {msg}")
    print(f"[padel-import] {msg}", flush=True)


def fail(msg: str) -> None:
    failures.append(msg)
    unreal.log_error(f"[padel-import] FAILED {msg}")
    print(f"[padel-import] FAILED {msg}", flush=True)


def linear(hex_color: str, alpha: float = 1.0) -> unreal.LinearColor:
    value = hex_color.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(value[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return unreal.LinearColor(out[0], out[1], out[2], alpha)


def set_first(obj, names: list[str], value) -> bool:
    """Sets the first editor property that exists. Property names have moved
    between engine versions, so the import tries each known spelling."""
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:  # noqa: BLE001 - unreal raises plain Exception for unknown properties
            continue
    return False


# --- Textures ----------------------------------------------------------------

def import_file(filename: Path, destination: str, name: str) -> unreal.Object | None:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    TOOLS.import_asset_tasks([task])
    path = f"{destination}/{name}"
    return unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None


def import_textures() -> None:
    for png in sorted((SOURCE_ART_DIR / "Textures").glob("T_*.png")):
        tex = import_file(png, MATERIAL_DIR, png.stem)
        if tex is None:
            fail(f"texture {png.name}")
            continue
        if png.stem.endswith("_Mask"):
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        elif png.stem.endswith(("_N", "_Normal")):
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        ASSETS.save_loaded_asset(tex)
        log(f"texture {MATERIAL_DIR}/{png.stem}")


# --- Master materials -----------------------------------------------------------

def _material(name: str) -> unreal.Material:
    path = f"{MATERIAL_DIR}/{name}"
    if ASSETS.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
        return mat
    return TOOLS.create_asset(name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())


def _param(mat, cls, name: str, x: int, y: int, default=None):
    expr = MEL.create_material_expression(mat, cls, x, y)
    expr.set_editor_property("parameter_name", name)
    if default is not None:
        expr.set_editor_property("default_value", default)
    return expr


def _node(mat, cls, x: int, y: int):
    return MEL.create_material_expression(mat, cls, x, y)


def _base_color(mat, y: int = -300):
    """BaseColor parameter times an optional BaseTexture, shared by most masters."""
    color = _param(mat, unreal.MaterialExpressionVectorParameter, "BaseColor", -700, y, unreal.LinearColor(1, 1, 1, 1))
    tex = _param(mat, unreal.MaterialExpressionTextureSampleParameter2D, "BaseTexture", -700, y + 150)
    tex.set_editor_property("texture", unreal.load_asset(WHITE))
    mul = _node(mat, unreal.MaterialExpressionMultiply, -400, y)
    MEL.connect_material_expressions(color, "", mul, "A")
    MEL.connect_material_expressions(tex, "RGB", mul, "B")
    return color, mul


def _scalars(mat, names_defaults: dict[str, float], props: dict[str, unreal.MaterialProperty], y: int = 200) -> dict:
    out = {}
    for i, (name, default) in enumerate(names_defaults.items()):
        expr = _param(mat, unreal.MaterialExpressionScalarParameter, name, -700, y + 90 * i, default)
        out[name] = expr
        if name in props:
            MEL.connect_material_property(expr, "", props[name])
    return out


def build_net_mpc():
    """MPC_NetReaction: six vectors the net materials read for the hit reaction. All
    zero (no hit) until UPadelNetReactionSubsystem writes them."""
    path = f"{MATERIAL_DIR}/{NET_MPC}"
    if ASSETS.does_asset_exist(path):
        mpc = unreal.load_asset(path)
    else:
        mpc = TOOLS.create_asset(NET_MPC, MATERIAL_DIR, unreal.MaterialParameterCollection,
                                 unreal.MaterialParameterCollectionFactoryNew())
    vectors = []
    for name in NET_MPC_VECTORS:
        param = unreal.CollectionVectorParameter()
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
        vectors.append(param)
    mpc.set_editor_property("vector_parameters", vectors)
    ASSETS.save_loaded_asset(mpc)
    log(f"{path}")
    return mpc


def _net_reaction(mat, mpc) -> None:
    """World Position Offset for the net hit reaction: the HLSL in NetReaction.hlsl,
    fed by MPC_NetReaction, gives an offset along the net's local X."""
    P = unreal.MaterialProperty
    custom = _node(mat, unreal.MaterialExpressionCustom, -400, 700)
    custom.set_editor_property("code", (HERE / "NetReaction.hlsl").read_text(encoding="utf-8"))
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom.set_editor_property("description", "NetReaction")
    inputs = []
    for name in ("P",) + NET_MPC_VECTORS:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", name)
        inputs.append(item)
    custom.set_editor_property("inputs", inputs)
    # Local position in cm: the net is built in court space, so this is court Y and Z.
    local_cls = getattr(unreal, "MaterialExpressionLocalPosition", None)
    if local_cls is not None:
        local = _node(mat, local_cls, -900, 650)
    else:
        world = _node(mat, unreal.MaterialExpressionWorldPosition, -1100, 650)
        local = _node(mat, unreal.MaterialExpressionTransformPosition, -900, 650)
        local.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
        local.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        MEL.connect_material_expressions(world, "", local, "")
    MEL.connect_material_expressions(local, "", custom, "P")
    for i, name in enumerate(NET_MPC_VECTORS):
        param = _node(mat, unreal.MaterialExpressionCollectionParameter, -900, 750 + 70 * i)
        param.set_editor_property("collection", mpc)
        param.set_editor_property("parameter_name", name)
        MEL.connect_material_expressions(param, "", custom, name)
    zeros = _node(mat, unreal.MaterialExpressionConstant2Vector, -400, 900)
    offset = _node(mat, unreal.MaterialExpressionAppendVector, -200, 750)
    MEL.connect_material_expressions(custom, "", offset, "A")
    MEL.connect_material_expressions(zeros, "", offset, "B")
    to_world = _node(mat, unreal.MaterialExpressionTransform, -50, 750)
    to_world.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    to_world.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    MEL.connect_material_expressions(offset, "", to_world, "")
    MEL.connect_material_property(to_world, "", P.MP_WORLD_POSITION_OFFSET)


def build_masters() -> None:
    P = unreal.MaterialProperty
    standard = {"Roughness": P.MP_ROUGHNESS, "Metallic": P.MP_METALLIC, "Specular": P.MP_SPECULAR}

    # Opaque: painted metal, lines, fabric, skin placeholders, the ball's felt.
    mat = _material("M_PadelOpaque")
    color, mul = _base_color(mat)
    MEL.connect_material_property(mul, "", P.MP_BASE_COLOR)
    s = _scalars(mat, {"Roughness": 0.5, "Metallic": 0.0, "Specular": 0.5, "EmissiveStrength": 0.0}, standard)
    emissive = _node(mat, unreal.MaterialExpressionMultiply, -300, 500)
    MEL.connect_material_expressions(mul, "", emissive, "A")
    MEL.connect_material_expressions(s["EmissiveStrength"], "", emissive, "B")
    MEL.connect_material_property(emissive, "", P.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)

    # Turf: blue artificial grass with sand showing through (GDD 10), broken up by
    # world-space noise so the surface doesn't look like flat paint from the high camera.
    mat = _material("M_PadelTurf")
    color, mul = _base_color(mat)
    sand = _param(mat, unreal.MaterialExpressionVectorParameter, "SandColor", -700, 0, unreal.LinearColor(0.6, 0.5, 0.35, 1))
    amount = _param(mat, unreal.MaterialExpressionScalarParameter, "SandAmount", -700, 100, 0.15)
    noise = _node(mat, unreal.MaterialExpressionNoise, -700, 200)
    set_first(noise, ["scale"], 0.08)
    weight = _node(mat, unreal.MaterialExpressionMultiply, -450, 150)
    MEL.connect_material_expressions(noise, "", weight, "A")
    MEL.connect_material_expressions(amount, "", weight, "B")
    lerp = _node(mat, unreal.MaterialExpressionLinearInterpolate, -250, -100)
    MEL.connect_material_expressions(mul, "", lerp, "A")
    MEL.connect_material_expressions(sand, "", lerp, "B")
    MEL.connect_material_expressions(weight, "", lerp, "Alpha")
    MEL.connect_material_property(lerp, "", P.MP_BASE_COLOR)
    _scalars(mat, {"Roughness": 0.9, "Specular": 0.3}, standard, 400)
    MEL.recompile_material(mat)

    # Glass: translucent tempered glass with reflections, edge tint and dirt (unreal/glass_material.py).
    def glass_texture(name: str):
        path = f"{MATERIAL_DIR}/{name}"
        if not ASSETS.does_asset_exist(path):
            fail(f"M_PadelGlass: texture {name} not imported")
            return None
        return unreal.load_asset(path)

    glass_material.build(_material("M_PadelGlass"), glass_texture, linear)

    # Masked: welded mesh and the net, cut out by a mask texture. UVs are in
    # metres divided by the cell size (Blender sets them), so one tile = one cell.
    mat = _material("M_PadelMasked")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    color, mul = _base_color(mat)
    MEL.connect_material_property(mul, "", P.MP_BASE_COLOR)
    mask = _param(mat, unreal.MaterialExpressionTextureSampleParameter2D, "Mask", -700, 0)
    mask.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE)
    mask.set_editor_property("texture", unreal.load_asset(WHITE))
    MEL.connect_material_property(mask, "R", P.MP_OPACITY_MASK)
    _scalars(mat, {"Roughness": 0.5, "Metallic": 1.0}, standard, 200)
    MEL.recompile_material(mat)

    # Net: the knotted cord, cut out by its mask, with a normal map so the cord reads
    # round, and the hit reaction. Two-sided: it is seen from both ends of the court.
    mpc = build_net_mpc()
    mat = _material("M_PadelNet")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    color, mul = _base_color(mat)
    MEL.connect_material_property(mul, "", P.MP_BASE_COLOR)
    mask = _param(mat, unreal.MaterialExpressionTextureSampleParameter2D, "Mask", -700, 0)
    mask.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE)
    mask.set_editor_property("texture", unreal.load_asset(WHITE))
    MEL.connect_material_property(mask, "R", P.MP_OPACITY_MASK)
    normal = _param(mat, unreal.MaterialExpressionTextureSampleParameter2D, "NormalMap", -700, 450)
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normal.set_editor_property("texture", unreal.load_asset(FLAT_NORMAL))
    MEL.connect_material_property(normal, "RGB", P.MP_NORMAL)
    _scalars(mat, {"Roughness": 0.8, "Metallic": 0.0, "Specular": 0.5}, standard, 150)
    _net_reaction(mat, mpc)
    MEL.recompile_material(mat)

    # NetSolid: the tape and bands, opaque fabric that moves with the net.
    mat = _material("M_PadelNetSolid")
    color, mul = _base_color(mat)
    MEL.connect_material_property(mul, "", P.MP_BASE_COLOR)
    _scalars(mat, {"Roughness": 0.75, "Metallic": 0.0, "Specular": 0.5}, standard, 200)
    _net_reaction(mat, mpc)
    MEL.recompile_material(mat)

    # Emissive: the arena light panels.
    mat = _material("M_PadelEmissive")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = _param(mat, unreal.MaterialExpressionVectorParameter, "BaseColor", -700, -100, unreal.LinearColor(1, 1, 1, 1))
    strength = _param(mat, unreal.MaterialExpressionScalarParameter, "EmissiveStrength", -700, 50, 10.0)
    emissive = _node(mat, unreal.MaterialExpressionMultiply, -350, 0)
    MEL.connect_material_expressions(color, "", emissive, "A")
    MEL.connect_material_expressions(strength, "", emissive, "B")
    MEL.connect_material_property(emissive, "", P.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)

    for name in ("M_PadelOpaque", "M_PadelTurf", "M_PadelGlass", "M_PadelMasked", "M_PadelEmissive",
                 "M_PadelNet", "M_PadelNetSolid"):
        ASSETS.save_asset(f"{MATERIAL_DIR}/{name}", only_if_is_dirty=False)
    log("master materials built")


# --- Material instances ------------------------------------------------------------

def build_instances(palette: dict) -> None:
    for group, entries in palette.items():
        if group.startswith("_"):
            continue
        for name, settings in entries.items():
            mi_name = catalogue.material_name(group, name)
            path = f"{MATERIAL_DIR}/{mi_name}"
            if ASSETS.does_asset_exist(path):
                mi = unreal.load_asset(path)
            else:
                mi = TOOLS.create_asset(mi_name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
                                        unreal.MaterialInstanceConstantFactoryNew())
            master = unreal.load_asset(f"{MATERIAL_DIR}/M_Padel{settings['master']}")
            MEL.set_material_instance_parent(mi, master)
            MEL.set_material_instance_vector_parameter_value(mi, "BaseColor", linear(settings["color"]))
            scalars = {"roughness": "Roughness", "metallic": "Metallic", "specular": "Specular",
                       "opacity": "Opacity", "emission": "EmissiveStrength", "sand_amount": "SandAmount"}
            for key, param in scalars.items():
                if key in settings:
                    MEL.set_material_instance_scalar_parameter_value(mi, param, float(settings[key]))
            if "sand_color" in settings:
                MEL.set_material_instance_vector_parameter_value(mi, "SandColor", linear(settings["sand_color"]))
            if "edge_tint" in settings:
                MEL.set_material_instance_vector_parameter_value(mi, "EdgeTint", linear(settings["edge_tint"]))
            if settings["master"] == "Glass":
                glass_material.apply_instance(mi, settings, linear)
            for key, param in (("mask", "Mask"), ("texture", "BaseTexture"), ("normal", "NormalMap")):
                if key in settings:
                    tex = unreal.load_asset(f"{MATERIAL_DIR}/{settings[key]}")
                    if tex is None:
                        fail(f"{mi_name}: texture {settings[key]} not imported")
                        continue
                    MEL.set_material_instance_texture_parameter_value(mi, param, tex)
            if "texture" in settings:
                # The felt texture carries the colour; keep the tint white so it isn't applied twice.
                MEL.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(1, 1, 1, 1))
            ASSETS.save_loaded_asset(mi)
    log("material instances built")


# --- Meshes -----------------------------------------------------------------------

def _assign_materials(mesh) -> None:
    if isinstance(mesh, unreal.StaticMesh):
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            mi = unreal.load_asset(f"{MATERIAL_DIR}/{slot.get_editor_property('material_slot_name')}")
            if mi is not None:
                mesh.set_material(index, mi)
    else:
        materials = list(mesh.get_editor_property("materials"))
        for slot in materials:
            mi = unreal.load_asset(f"{MATERIAL_DIR}/{slot.get_editor_property('material_slot_name')}")
            if mi is not None:
                slot.set_editor_property("material_interface", mi)
        mesh.set_editor_property("materials", materials)


def _delete_unused_imported_materials(folder: str) -> None:
    """The importer makes its own copies of the FBX materials. Procedural meshes
    now point at the instances in MATERIAL_DIR, so their copies are unused and go.
    AI models keep theirs (they carry the model's own textures), because those are
    still referenced."""
    for path in ASSETS.list_assets(folder, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        if not isinstance(asset, (unreal.MaterialInterface, unreal.Texture)):
            continue
        package = path.split(".")[0]
        if not ASSETS.find_package_referencers_for_asset(package, load_assets_to_confirm=False):
            ASSETS.delete_asset(path)


def _set_nanite(mesh: unreal.StaticMesh, enabled: bool) -> None:
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", enabled)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if hasattr(subsystem, "set_nanite_settings"):
        subsystem.set_nanite_settings(mesh, settings, apply_changes=True)
    else:
        mesh.set_editor_property("nanite_settings", settings)


def _set_bounds_extension(mesh: unreal.StaticMesh, metres) -> None:
    """Grows the bounds of a mesh whose material moves its vertices (the net), so it
    isn't culled or shadow-clipped while it bulges."""
    if not metres:
        return
    extent = unreal.Vector(*(float(m) * 100.0 for m in metres))
    mesh.set_editor_property("positive_bounds_extension", extent)
    mesh.set_editor_property("negative_bounds_extension", extent)


def _import_skeletal_lods(asset: catalogue.Asset, mesh) -> None:
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    importer = getattr(subsystem, "import_lod", None) or getattr(unreal.EditorSkeletalMeshLibrary, "import_lod", None)
    if importer is None:
        fail(f"{asset.asset_name}: no import_lod in this engine version; LODs skipped")
        return
    for i, lod in enumerate(asset.lods, start=1):
        if importer(mesh, i, str(asset.lod_fbx_path(i))) < 0:
            fail(f"{asset.asset_name}: LOD{i} import")
    lod_info = list(mesh.get_editor_property("lod_info"))
    for i, lod in enumerate(asset.lods, start=1):
        if i < len(lod_info):
            screen = unreal.PerPlatformFloat()
            screen.set_editor_property("default", lod.screen_size)
            lod_info[i].set_editor_property("screen_size", screen)
    mesh.set_editor_property("lod_info", lod_info)


def import_mesh(asset: catalogue.Asset) -> None:
    if not asset.fbx_path.is_file():
        log(f"{asset.asset_name}: no {asset.fbx_path.name} yet, skipped")
        return
    mesh = import_file(asset.fbx_path, asset.unreal_folder, asset.asset_name)
    if mesh is None:
        fail(f"{asset.asset_name}: import")
        return
    expected = unreal.StaticMesh if asset.kind == "static" else unreal.SkeletalMesh
    if not isinstance(mesh, expected):
        fail(f"{asset.asset_name}: imported as {type(mesh).__name__}, expected {expected.__name__}")
        return
    _assign_materials(mesh)
    if asset.kind == "static":
        _set_nanite(mesh, asset.nanite)
        _set_bounds_extension(mesh, asset.raw.get("bounds_extension_m"))
    elif asset.lods:
        _import_skeletal_lods(asset, mesh)
    ASSETS.save_loaded_asset(mesh)
    log(f"{asset.unreal_folder}/{asset.asset_name}")


def main() -> None:
    cat = catalogue.load()
    palette = catalogue.load_palette()
    import_textures()
    build_masters()
    build_instances(palette)
    for asset in cat.assets:
        if asset.enabled:
            try:
                import_mesh(asset)
            except Exception as err:  # noqa: BLE001 - keep going so one bad asset doesn't hide the rest
                fail(f"{asset.asset_name}: {err}")
    for folder in sorted({a.unreal_folder for a in cat.assets}):
        if ASSETS.does_directory_exist(folder):
            # Twice: textures only become unreferenced once their materials are gone.
            _delete_unused_imported_materials(folder)
            _delete_unused_imported_materials(folder)
    log(f"done ({len(failures)} failure(s))")
    if failures and os.environ.get("PADEL_IMPORT_STRICT", "1") == "1":
        raise SystemExit(1)


main()
