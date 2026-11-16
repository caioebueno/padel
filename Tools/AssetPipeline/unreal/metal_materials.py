"""Master materials for the court's steel and welded mesh (GDD 10). Called by
import_assets.py inside the Unreal Editor; never edit the generated assets.

  M_PadelMetal     powder-coated or galvanised steel: palette colour and
                   roughness, a tiling orange-peel normal map in real-world
                   scale, low-frequency roughness variation, and a little dirt
                   collecting near the floor (world-space, so no extra UVs).
                   Physically based: powder coat is a paint (metallic 0), zinc
                   is a metal (metallic 1); the palette says which.
  M_PadelWireMesh  the welded mesh. The panel quads carry UVs in wire cells
                   (one UV unit = one 50 mm cell), and the material draws the
                   wires analytically with a box-filtered grid: the filter
                   width comes from the screen-space UV derivatives, so a wire
                   thinner than a pixel becomes the right fraction of coverage
                   instead of flickering on and off (no moire from the
                   broadcast camera). That coverage is dithered per pixel and
                   per frame into the opacity mask, and TSR resolves the dither
                   into a smooth, see-through haze at distance. The normal
                   rounds each wire like a cylinder up close and flattens out
                   as the wires get thinner than a pixel. Masked, two-sided,
                   no textures, so there is nothing to mip and no LODs to bake.

All shader code is in Custom nodes, so the maths is readable text in this file
rather than a node graph. It uses only plain HLSL that Unreal cross-compiles to
Metal.
"""

from __future__ import annotations

import unreal

MATERIAL_DIR = "/Game/Padel/Materials"
MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
DEFAULT_NORMAL = "/Engine/EngineMaterials/DefaultNormal"
POWDER_COAT_NORMAL = f"{MATERIAL_DIR}/T_PowderCoat_N"
MASTERS = ("M_PadelMetal", "M_PadelWireMesh")

# Value noise in metres, three octaves, used for roughness variation and dirt.
METAL_SURFACE_HLSL = r"""
float3 p = WorldPos.xyz * 0.01;
float n = 0.0;
float amp = 0.5;
float freq = 7.0;
for (int o = 0; o < 3; o++)
{
    float3 q = p * freq;
    float3 i = floor(q);
    float3 f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float v = 0.0;
    for (int k = 0; k < 8; k++)
    {
        float3 c = float3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
        float h = frac(sin(dot(i + c, float3(127.1, 311.7, 74.7))) * 43758.5453);
        float3 w = lerp(1.0 - f, f, c);
        v += h * w.x * w.y * w.z;
    }
    n += v * amp;
    amp *= 0.5;
    freq *= 2.7;
}
n /= 0.875;
float g = saturate(1.0 - p.z / max(GrimeHeight, 0.001));
g = g * g * GrimeAmount * saturate(0.35 + n);
float3 col = lerp(BaseColor.rgb, GrimeColor.rgb, g);
float rough = saturate(Roughness + (n - 0.5) * RoughnessVariation + g * 0.25);
return float4(col, rough);
"""

FLATTEN_NORMAL_HLSL = r"""
return normalize(lerp(float3(0.0, 0.0, 1.0), NormalIn.rgb, saturate(Strength)));
"""

# Box-filtered grid after Inigo Quilez's "filtered grid" (iquilezles.org, MIT-style
# public article): the exact fraction of the pixel's footprint covered by wire.
WIRE_COVERAGE_HLSL = r"""
float f = clamp(WireFraction, 0.001, 0.5);
float N = 1.0 / f;
float2 w = max(abs(ddx(UV)), abs(ddy(UV))) + 0.0001;
float2 p = UV + 0.5 * f;
float2 a = p + 0.5 * w;
float2 b = p - 0.5 * w;
float2 i = (floor(a) + min(frac(a) * N, 1.0) - floor(b) - min(frac(b) * N, 1.0)) / (N * w);
float coverage = 1.0 - (1.0 - saturate(i.x)) * (1.0 - saturate(i.y));
float2 px = Parameters.SvPosition.xy + 5.588238 * float(View.StateFrameIndexMod8);
float dither = frac(52.9829189 * frac(dot(px, float2(0.06711056, 0.00583715))));
return coverage > dither ? 1.0 : 0.0;
"""

WIRE_NORMAL_HLSL = r"""
float f = clamp(WireFraction, 0.001, 0.5);
float2 d = UV - floor(UV + 0.5);
float2 w = max(abs(ddx(UV)), abs(ddy(UV))) + 0.0001;
float2 s = clamp(d / (0.5 * f), -0.95, 0.95);
float3 n = float3(0.0, 0.0, 1.0);
if (abs(d.y) < 0.5 * f)
{
    n = float3(0.0, s.y, sqrt(1.0 - s.y * s.y));
}
else if (abs(d.x) < 0.5 * f)
{
    n = float3(s.x, 0.0, sqrt(1.0 - s.x * s.x));
}
float fade = saturate(2.0 * f / max(w.x, w.y) - 2.0);
return normalize(lerp(float3(0.0, 0.0, 1.0), n, fade));
"""


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


def _scalar(mat, name: str, default: float, x: int, y: int):
    return _param(mat, unreal.MaterialExpressionScalarParameter, name, x, y, default)


def _custom(mat, description: str, code: str, inputs: list[str], output, x: int, y: int):
    node = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("description", description)
    node.set_editor_property("code", code.strip())
    node.set_editor_property("output_type", output)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    return node


def _mask(mat, source, channels: str, x: int, y: int):
    node = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x, y)
    for c in "rgba":
        node.set_editor_property(c, c in channels)
    MEL.connect_material_expressions(source, "", node, "")
    return node


def _prepare_normal_texture(log, fail) -> unreal.Texture | None:
    if not ASSETS.does_asset_exist(POWDER_COAT_NORMAL):
        fail("T_PowderCoat_N not imported; the steel falls back to a flat normal")
        return None
    tex = unreal.load_asset(POWDER_COAT_NORMAL)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    ASSETS.save_loaded_asset(tex)
    log("texture T_PowderCoat_N set up as a normal map")
    return tex


def _build_metal(normal_tex) -> None:
    P = unreal.MaterialProperty
    F = unreal.CustomMaterialOutputType
    mat = _material("M_PadelMetal")
    color = _param(mat, unreal.MaterialExpressionVectorParameter, "BaseColor", -1100, -400, unreal.LinearColor(0.04, 0.045, 0.05, 1))
    grime = _param(mat, unreal.MaterialExpressionVectorParameter, "GrimeColor", -1100, -250, unreal.LinearColor(0.07, 0.06, 0.05, 1))
    world = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, -100)
    scalars = {name: _scalar(mat, name, default, -1100, 0 + 80 * i) for i, (name, default) in enumerate(
        (("GrimeHeight", 0.4), ("GrimeAmount", 0.3), ("Roughness", 0.42), ("RoughnessVariation", 0.08)))}
    surface = _custom(mat, "PadelMetalSurface", METAL_SURFACE_HLSL,
                      ["BaseColor", "GrimeColor", "GrimeHeight", "GrimeAmount", "Roughness", "RoughnessVariation", "WorldPos"],
                      F.CMOT_FLOAT4, -700, -200)
    for src, pin in ((color, "BaseColor"), (grime, "GrimeColor"), (world, "WorldPos")):
        MEL.connect_material_expressions(src, "", surface, pin)
    for name, expr in scalars.items():
        MEL.connect_material_expressions(expr, "", surface, name)
    MEL.connect_material_property(_mask(mat, surface, "rgb", -400, -250), "", P.MP_BASE_COLOR)
    MEL.connect_material_property(_mask(mat, surface, "a", -400, -150), "", P.MP_ROUGHNESS)
    MEL.connect_material_property(_scalar(mat, "Metallic", 0.0, -400, 0), "", P.MP_METALLIC)
    MEL.connect_material_property(_scalar(mat, "Specular", 0.5, -400, 80), "", P.MP_SPECULAR)

    # Orange peel: UVs are in metres (the Blender builder sets them), divided by the tile size.
    coords = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 400)
    tiling = _scalar(mat, "NormalTiling", 0.08, -1100, 480)
    uv = MEL.create_material_expression(mat, unreal.MaterialExpressionDivide, -900, 420)
    MEL.connect_material_expressions(coords, "", uv, "A")
    MEL.connect_material_expressions(tiling, "", uv, "B")
    normal = _param(mat, unreal.MaterialExpressionTextureSampleParameter2D, "NormalMap", -750, 400)
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normal.set_editor_property("texture", normal_tex or unreal.load_asset(DEFAULT_NORMAL))
    MEL.connect_material_expressions(uv, "", normal, "UVs")
    strength = _scalar(mat, "NormalStrength", 0.35, -750, 600)
    flatten = _custom(mat, "PadelFlattenNormal", FLATTEN_NORMAL_HLSL, ["NormalIn", "Strength"], F.CMOT_FLOAT3, -400, 450)
    MEL.connect_material_expressions(normal, "RGB", flatten, "NormalIn")
    MEL.connect_material_expressions(strength, "", flatten, "Strength")
    MEL.connect_material_property(flatten, "", P.MP_NORMAL)
    MEL.recompile_material(mat)


def _build_wire_mesh() -> None:
    P = unreal.MaterialProperty
    F = unreal.CustomMaterialOutputType
    mat = _material("M_PadelWireMesh")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    color = _param(mat, unreal.MaterialExpressionVectorParameter, "BaseColor", -700, -400, unreal.LinearColor(0.04, 0.045, 0.05, 1))
    MEL.connect_material_property(color, "", P.MP_BASE_COLOR)
    MEL.connect_material_property(_scalar(mat, "Roughness", 0.45, -700, -250), "", P.MP_ROUGHNESS)
    MEL.connect_material_property(_scalar(mat, "Metallic", 0.0, -700, -170), "", P.MP_METALLIC)
    MEL.connect_material_property(_scalar(mat, "Specular", 0.5, -700, -90), "", P.MP_SPECULAR)
    coords = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1000, 100)
    wire = _scalar(mat, "WireFraction", 0.08, -1000, 200)
    coverage = _custom(mat, "PadelWireCoverage", WIRE_COVERAGE_HLSL, ["UV", "WireFraction"], F.CMOT_FLOAT1, -600, 100)
    normal = _custom(mat, "PadelWireNormal", WIRE_NORMAL_HLSL, ["UV", "WireFraction"], F.CMOT_FLOAT3, -600, 300)
    for node in (coverage, normal):
        MEL.connect_material_expressions(coords, "", node, "UV")
        MEL.connect_material_expressions(wire, "", node, "WireFraction")
    MEL.connect_material_property(coverage, "", P.MP_OPACITY_MASK)
    MEL.connect_material_property(normal, "", P.MP_NORMAL)
    MEL.recompile_material(mat)


def build_masters(log, fail) -> None:
    """Builds M_PadelMetal and M_PadelWireMesh. Run after the textures are imported."""
    _build_metal(_prepare_normal_texture(log, fail))
    _build_wire_mesh()
    for name in MASTERS:
        ASSETS.save_asset(f"{MATERIAL_DIR}/{name}", only_if_is_dirty=False)
    log("metal and wire mesh masters built")


def _linear(hex_color: str) -> unreal.LinearColor:
    value = hex_color.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(value[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return unreal.LinearColor(out[0], out[1], out[2], 1.0)


def apply_instance_params(palette: dict, material_name, fail) -> None:
    """Sets the parameters build_instances() doesn't know about on the Metal and
    WireMesh instances. material_name is catalogue.material_name."""
    for group, entries in palette.items():
        if group.startswith("_"):
            continue
        for name, settings in entries.items():
            if settings.get("master") not in ("Metal", "WireMesh"):
                continue
            path = f"{MATERIAL_DIR}/{material_name(group, name)}"
            mi = unreal.load_asset(path)
            if mi is None:
                fail(f"{path}: instance missing")
                continue
            if settings["master"] == "Metal":
                for key, param in (("normal_strength", "NormalStrength"), ("normal_tiling_m", "NormalTiling"),
                                   ("grime_height_m", "GrimeHeight"), ("grime_amount", "GrimeAmount"),
                                   ("roughness_variation", "RoughnessVariation")):
                    if key in settings:
                        MEL.set_material_instance_scalar_parameter_value(mi, param, float(settings[key]))
                if "grime_color" in settings:
                    MEL.set_material_instance_vector_parameter_value(mi, "GrimeColor", _linear(settings["grime_color"]))
            else:
                MEL.set_material_instance_scalar_parameter_value(
                    mi, "WireFraction", float(settings["wire_m"]) / float(settings["cell_m"]))
            ASSETS.save_loaded_asset(mi)
