"""Builds M_PadelGlass, the tempered court glass (GDD 10), inside the Unreal Editor.
Called by import_assets.py; see docs/decisions/0006-court-glass.md.

What makes it read as real glass without hiding the ball:

- Surface ForwardShading translucency, so the faces get real specular and
  reflections (Lumen front-layer translucency reflections when the project turns
  them on, reflection captures otherwise). Base opacity is about 5 %.
- Fresnel: more reflective, more opaque and greener at grazing angles, like the
  side glass seen from the broadcast camera.
- The edge material (MI_Court_GlassEdge) is the same master with high opacity
  and a deep green, the iron tint of soda-lime glass seen through its edge.
- Dirt from three small procedural textures (asset_lib/glass.py): greasy smears
  and wipes that blur reflections, felt ball marks concentrated in the band of
  heights where balls hit, and sand dust fading up from the floor (world Z, so
  the court floor must be at Z = 0).
- A faint roller-wave normal map, the ripple tempering leaves in real panels,
  which bends reflections slightly.

Cost: three texture samples and a few dozen instructions on 18 panels. No
refraction (10-12 mm flat glass barely shifts the image, and the distortion pass
isn't worth it at 60 fps), no two-sided lighting, no depth writes.
"""

from __future__ import annotations

import unreal

from asset_lib import glass

MEL = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty
BLACK = "/Engine/EngineResources/Black"
FLAT_NORMAL = "/Engine/EngineMaterials/DefaultNormal"

# Palette key -> material parameter. Keys missing from a palette entry keep the master's default.
INSTANCE_SCALARS = {
    "edge_opacity": "EdgeOpacity",
    "fresnel_exponent": "FresnelExponent",
    "smudge_amount": "SmudgeAmount",
    "smudge_roughness": "SmudgeRoughness",
    "smudge_opacity": "SmudgeOpacity",
    "ball_mark_amount": "BallMarkAmount",
    "ball_mark_opacity": "BallMarkOpacity",
    "ball_mark_height_m": "BallMarkHeight",
    "ball_mark_spread_m": "BallMarkSpread",
    "dust_amount": "DustAmount",
    "dust_height_m": "DustHeight",
    "normal_strength": "NormalStrength",
}
INSTANCE_VECTORS = {"ball_mark_color": "BallMarkColor", "dust_color": "DustColor"}


def _set_first(obj, names: list[str], value) -> bool:
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:  # noqa: BLE001 - unreal raises plain Exception for unknown properties
            continue
    return False


class _Graph:
    """Small helper so the graph below reads top to bottom."""

    def __init__(self, mat: unreal.Material) -> None:
        self.mat = mat
        self.y = -600

    def node(self, cls, x: int = -400, **props):
        expr = MEL.create_material_expression(self.mat, cls, x, self.y)
        self.y += 70
        for key, value in props.items():
            expr.set_editor_property(key, value)
        return expr

    def scalar(self, name: str, default: float):
        return self.node(unreal.MaterialExpressionScalarParameter, -1100, parameter_name=name, default_value=default)

    def vector(self, name: str, color: unreal.LinearColor):
        return self.node(unreal.MaterialExpressionVectorParameter, -1100, parameter_name=name, default_value=color)

    def texture(self, name: str, texture, sampler, uvs, fallback: str):
        expr = self.node(unreal.MaterialExpressionTextureSampleParameter2D, -800, parameter_name=name,
                         sampler_type=sampler)
        # A missing texture falls back to a neutral engine one (no dirt, flat normal) so the material still compiles.
        expr.set_editor_property("texture", texture or unreal.load_asset(fallback))
        MEL.connect_material_expressions(uvs, "", expr, "UVs")
        return expr

    def op(self, cls, a, b=None, a_out: str = "", b_out: str = ""):
        expr = self.node(cls)
        if b is None:
            MEL.connect_material_expressions(a, a_out, expr, "")
        else:
            MEL.connect_material_expressions(a, a_out, expr, "A")
            MEL.connect_material_expressions(b, b_out, expr, "B")
        return expr

    def lerp(self, a, b, alpha):
        expr = self.node(unreal.MaterialExpressionLinearInterpolate)
        MEL.connect_material_expressions(a, "", expr, "A")
        MEL.connect_material_expressions(b, "", expr, "B")
        MEL.connect_material_expressions(alpha, "", expr, "Alpha")
        return expr


def build(mat: unreal.Material, load_texture, linear) -> None:
    """Fills an empty material with the glass graph. load_texture(name) returns an
    imported texture or None; linear(hex) converts an sRGB colour."""
    cfg = glass.load_config()
    tex_cfg = cfg["textures"]
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", False)
    # "Surface ForwardShading" (C++ TLM_Surface); the Python spelling has varied between versions.
    modes = unreal.TranslucencyLightingMode
    mode = (getattr(modes, "TLM_SURFACE", None) or getattr(modes, "TLM_SURFACE_FORWARD_SHADING", None)
            or modes.TLM_SURFACE_PER_PIXEL_LIGHTING)
    _set_first(mat, ["translucency_lighting_mode"], mode)
    # Reflections on the glass: SSR for forward-shaded translucency, and Lumen's
    # front-layer reflections where the engine exposes the switch on the material.
    _set_first(mat, ["screen_space_reflections"], True)
    _set_first(mat, ["lumen_front_layer_translucency_reflections", "enable_lumen_front_layer_translucency_reflections"], True)
    g = _Graph(mat)

    # Inputs.
    base = g.vector("BaseColor", linear("#eef5f3"))
    edge_tint = g.vector("EdgeTint", linear("#6fa595"))
    mark_color = g.vector("BallMarkColor", linear("#d4d6c2"))
    dust_color = g.vector("DustColor", linear("#c9b48a"))
    s = {name: g.scalar(name, default) for name, default in (
        ("Opacity", 0.05), ("EdgeOpacity", 0.32), ("Roughness", 0.03), ("Specular", 0.5), ("FresnelExponent", 4.0),
        ("SmudgeAmount", 0.55), ("SmudgeRoughness", 0.22), ("SmudgeOpacity", 0.03),
        ("BallMarkAmount", 0.6), ("BallMarkOpacity", 0.18), ("BallMarkHeight", 1.3), ("BallMarkSpread", 1.4),
        ("DustAmount", 0.35), ("DustHeight", 0.4), ("NormalStrength", 0.12),
        # Tiling comes from glass.json, the same numbers that sized the textures.
        ("SmudgeTiling", 1.0 / tex_cfg["smudge"]["tile_m"]), ("BallMarkTiling", 1.0 / tex_cfg["ball_marks"]["tile_m"]),
        ("RollerWaveTiling", 1.0 / tex_cfg["roller_wave"]["period_m"]))}

    # UVs are metres on the panel face plus a per-panel offset (blender/build_court_glass.py).
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, -1100, coordinate_index=0)
    M = unreal.MaterialExpressionMultiply
    grey = unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE
    smudge_tex = g.texture("SmudgeMask", load_texture(glass.SMUDGE_TEXTURE), grey, g.op(M, uv, s["SmudgeTiling"]), BLACK)
    mark_tex = g.texture("BallMarkMask", load_texture(glass.BALL_MARK_TEXTURE), grey, g.op(M, uv, s["BallMarkTiling"]), BLACK)
    wave_tex = g.texture("RollerWave", load_texture(glass.ROLLER_WAVE_TEXTURE),
                         unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, g.op(M, uv, s["RollerWaveTiling"]),
                         FLAT_NORMAL)

    # Height above the court floor in metres (world is in centimetres).
    world = g.node(unreal.MaterialExpressionWorldPosition, -1100)
    z = g.node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
    MEL.connect_material_expressions(world, "", z, "")
    z_m = g.op(M, z, g.node(unreal.MaterialExpressionConstant, -1100, r=0.01))

    # Ball marks: strongest around BallMarkHeight, gone BallMarkSpread above or below it.
    offset = g.op(unreal.MaterialExpressionSubtract, z_m, s["BallMarkHeight"])
    band = g.op(unreal.MaterialExpressionSaturate,
                g.op(unreal.MaterialExpressionOneMinus,
                     g.op(unreal.MaterialExpressionDivide, g.op(unreal.MaterialExpressionAbs, offset), s["BallMarkSpread"])))
    marks = g.op(M, g.op(M, mark_tex, s["BallMarkAmount"], a_out="R"), band)
    # Sand dust: kicked up by feet and balls, fading out by DustHeight, squared for a soft top edge.
    dust_ramp = g.op(unreal.MaterialExpressionSaturate,
                     g.op(unreal.MaterialExpressionOneMinus, g.op(unreal.MaterialExpressionDivide, z_m, s["DustHeight"])))
    dust = g.op(M, g.op(M, dust_ramp, dust_ramp), s["DustAmount"])
    smear = g.op(M, smudge_tex, s["SmudgeAmount"], a_out="R")

    fresnel = g.node(unreal.MaterialExpressionFresnel, -800, base_reflect_fraction=0.04)
    MEL.connect_material_expressions(s["FresnelExponent"], "", fresnel, "ExponentIn")

    # Base colour: clear, greener at grazing angles, then felt and sand on top.
    tinted = g.lerp(base, edge_tint, fresnel)
    marked = g.lerp(tinted, mark_color, g.op(unreal.MaterialExpressionSaturate, marks))
    MEL.connect_material_property(g.lerp(marked, dust_color, g.op(unreal.MaterialExpressionSaturate, dust)), "", P.MP_BASE_COLOR)

    # Opacity: nearly clear head-on so the ball always reads (GDD 10).
    A = unreal.MaterialExpressionAdd
    opacity = g.lerp(s["Opacity"], s["EdgeOpacity"], fresnel)
    opacity = g.op(A, opacity, g.op(M, smear, s["SmudgeOpacity"]))
    opacity = g.op(A, opacity, g.op(M, marks, s["BallMarkOpacity"]))
    opacity = g.op(A, opacity, g.op(M, dust, g.node(unreal.MaterialExpressionConstant, -1100, r=0.5)))
    MEL.connect_material_property(g.op(unreal.MaterialExpressionSaturate, opacity), "", P.MP_OPACITY)

    # Roughness: smears blur reflections far more than they cloud the glass.
    rough = g.op(A, s["Roughness"], g.op(M, smear, s["SmudgeRoughness"]))
    rough = g.op(A, rough, g.op(M, marks, g.node(unreal.MaterialExpressionConstant, -1100, r=0.4)))
    rough = g.op(A, rough, g.op(M, dust, g.node(unreal.MaterialExpressionConstant, -1100, r=0.6)))
    MEL.connect_material_property(g.op(unreal.MaterialExpressionSaturate, rough), "", P.MP_ROUGHNESS)
    MEL.connect_material_property(s["Specular"], "", P.MP_SPECULAR)

    flat = g.node(unreal.MaterialExpressionConstant3Vector, -800, constant=unreal.LinearColor(0, 0, 1, 1))
    normal = g.node(unreal.MaterialExpressionLinearInterpolate)
    MEL.connect_material_expressions(flat, "", normal, "A")
    MEL.connect_material_expressions(wave_tex, "RGB", normal, "B")
    MEL.connect_material_expressions(s["NormalStrength"], "", normal, "Alpha")
    MEL.connect_material_property(normal, "", P.MP_NORMAL)
    MEL.recompile_material(mat)


def apply_instance(mi: unreal.MaterialInstanceConstant, settings: dict, linear) -> None:
    """Sets the glass-only parameters of a palette entry whose master is Glass."""
    for key, param in INSTANCE_SCALARS.items():
        if key in settings:
            MEL.set_material_instance_scalar_parameter_value(mi, param, float(settings[key]))
    for key, param in INSTANCE_VECTORS.items():
        if key in settings:
            MEL.set_material_instance_vector_parameter_value(mi, param, linear(settings[key]))
