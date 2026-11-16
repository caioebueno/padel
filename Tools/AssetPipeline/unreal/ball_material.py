"""The ball's master materials (GDD 2, GDD 10; docs/decisions/0007-ball.md). Built from
text by import_assets.py like every other master; never edit the assets by hand.

  M_PadelBallFelt   the ball: Cloth shading (felt sheen from the fuzz colour), the seam
                    drawn from the distance field in T_Ball_Masks.R, the printed mark from .B,
                    dye mottle from .G, the seam groove from T_Ball_N and tiling fibres from
                    T_Felt_N that fade out with distance.
  M_PadelBallFuzz   the LOD0 fuzz shell: masked loose fibres (T_Felt_Mask), denser towards
                    the outline so the silhouette goes soft, none over the rubber seam.
  M_PadelBallTrail  the speed trail: translucent, unlit, fading towards the tail and the
                    tube's edges. Its opacity comes from the component's custom primitive
                    data 0, written by APadelBall each frame, so no dynamic instances.

Seam readability (GDD 10: the seam shows spin). The seam's half width is
max(SeamHalfWidth, MinSeamPixels / 2 pixels), with the pixel's angular footprint taken
from the screen-space derivatives of the distance field, and a one-pixel box filter on
the edge. Up close the seam is its real 3 mm; from the broadcast camera, where the whole
ball is about ten pixels, it stays MinSeamPixels wide instead of vanishing, so the
stripe visibly turns with the spin. Brightness: ReadabilityLift adds felt-coloured light
relative to the exposure (EyeAdaptationInverse), a floor that keeps the ball readable in
a player's shadow without changing how it looks in the light.

Plain expressions except the engine's EyeAdaptationInverse node; no Custom HLSL, so the
graph compiles the same on D3D and Metal.
"""

from __future__ import annotations

import math

import unreal

from turf_material import Graph

MEL = unreal.MaterialEditingLibrary


def _eye_adaptation_inverse(g: Graph, fail):
    cls = getattr(unreal, "MaterialExpressionEyeAdaptationInverse", None)
    if cls is None:
        fail("ball materials: no EyeAdaptationInverse node in this engine; the readability lift is not exposure-relative")
        return 1.0
    expr = g.node(cls)
    # LightValue 1: the result is 1 / exposure, so emissive E shows like a lit surface of luminance E.
    return expr


def _vec(g: Graph, a, b, c=None):
    """AppendVector chain: (a, b) or (a, b, c)."""
    ab = g.node(unreal.MaterialExpressionAppendVector)
    g.link(a, ab, "A")
    g.link(b, ab, "B")
    if c is None:
        return ab
    abc = g.node(unreal.MaterialExpressionAppendVector)
    g.link(ab, abc, "A")
    g.link(c, abc, "B")
    return abc


def _mask(g: Graph, src, channels: str):
    expr = g.node(unreal.MaterialExpressionComponentMask, r="r" in channels, g="g" in channels,
                  b="b" in channels, a="a" in channels)
    g.link(src, expr, "")
    return expr


def _unary(g: Graph, cls, src):
    expr = g.node(cls)
    g.link(src, expr, "")
    return expr


def _max(g: Graph, a, b):
    return g._binary(unreal.MaterialExpressionMax, a, b)


def _seam_distance(g: Graph, masks, sdf_range):
    """Angular distance from the seam's centre line, radians."""
    return g.mul((masks, "R"), sdf_range)


def _facing(g: Graph):
    """1 facing the camera, 0 at the outline."""
    n = g.node(unreal.MaterialExpressionVertexNormalWS)
    v = g.node(unreal.MaterialExpressionCameraVectorWS)
    d = g.node(unreal.MaterialExpressionDotProduct)
    g.link(n, d, "A")
    g.link(v, d, "B")
    return g.sat(d)


def _distance_fade(g: Graph, start_m, end_m):
    """0 near the camera, 1 beyond end_m (PixelDepth is in centimetres)."""
    depth_m = g.mul(g.node(unreal.MaterialExpressionPixelDepth), 0.01)
    return g.sat(g.div(g.sub(depth_m, start_m), g.sub(end_m, start_m)))


def build_felt(mat: unreal.Material, material_dir: str, cfg: dict, radius_mm: float, fail) -> None:
    P = unreal.MaterialProperty
    S = unreal.MaterialSamplerType
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLOTH)
    g = Graph(mat)
    felt_tile = cfg["felt"]["tile_mm"]

    base_color = g.vector("BaseColor", (0.69, 0.89, 0.04))
    seam_color = g.vector("SeamColor", (0.84, 0.83, 0.75))
    ink_color = g.vector("InkColor", (0.017, 0.02, 0.026))
    fuzz_color = g.vector("FuzzColor", (0.9, 1.0, 0.43))
    roughness = g.scalar("Roughness", 0.9)
    seam_roughness = g.scalar("SeamRoughness", 0.55)
    ink_roughness = g.scalar("InkRoughness", 0.6)
    mottle_amount = g.scalar("MottleAmount", 0.12)
    lift = g.scalar("ReadabilityLift", 0.06)
    min_seam_px = g.scalar("MinSeamPixels", 1.4)
    seam_half = g.scalar("SeamHalfWidth", cfg["seam"]["half_width_mm"] / radius_mm)
    sdf_range = g.scalar("SdfRange", cfg["textures"]["sdf_range_rad"])
    tiles_u = g.scalar("FibreTilesU", round(2 * math.pi * radius_mm / felt_tile))
    tiles_v = g.scalar("FibreTilesV", round(math.pi * radius_mm / felt_tile))
    fibre_strength = g.scalar("FibreNormalStrength", 1.0)
    fade_start = g.scalar("DetailFadeStart", 0.6)
    fade_end = g.scalar("DetailFadeEnd", 4.0)

    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    masks = g.texture("BallMasks", f"{material_dir}/T_Ball_Masks", S.SAMPLERTYPE_LINEAR_COLOR, uv0)
    seam_n = g.texture("SeamNormal", f"{material_dir}/T_Ball_N", S.SAMPLERTYPE_NORMAL, uv0)
    fibre_uv = g.mul(uv0, _vec(g, tiles_u, tiles_v))
    fibre_n = g.texture("FibreNormal", f"{material_dir}/T_Felt_N", S.SAMPLERTYPE_NORMAL, fibre_uv)

    # The seam: a box-filtered edge on the distance field, never thinner than MinSeamPixels.
    d = _seam_distance(g, masks, sdf_range)
    footprint = g.add(_unary(g, unreal.MaterialExpressionAbs, _unary(g, unreal.MaterialExpressionDDX, d)),
                      _unary(g, unreal.MaterialExpressionAbs, _unary(g, unreal.MaterialExpressionDDY, d)))
    footprint = _max(g, footprint, 1e-5)
    half = _max(g, seam_half, g.mul(g.mul(min_seam_px, 0.5), footprint))
    seam = g.sat(g.add(g.div(g.sub(half, d), footprint), 0.5))
    felt_share = g.sub(1.0, seam)
    ink = g.mul((masks, "B"), felt_share)

    # Dye mottle around 1.0: G = 0.5 leaves the felt colour as it is.
    mottle = g.add(1.0, g.mul(mottle_amount, g.sub(g.mul((masks, "G"), 2.0), 1.0)))
    felt = g.mul(base_color, mottle)
    base = g.lerp(g.lerp(felt, ink_color, ink), seam_color, seam)
    MEL.connect_material_property(base, "", P.MP_BASE_COLOR)
    rough = g.lerp(g.lerp(roughness, ink_roughness, ink), seam_roughness, seam)
    MEL.connect_material_property(rough, "", P.MP_ROUGHNESS)
    # Cloth: the felt gets the fuzz sheen, the rubber strip doesn't.
    MEL.connect_material_property(fuzz_color, "", P.MP_SUBSURFACE_COLOR)
    _connect_cloth_amount(felt_share)

    # Normal: the seam groove, plus the fibres (off the seam, faded out with distance),
    # combined by adding slopes (whiteout blend).
    fade = _distance_fade(g, fade_start, fade_end)
    strength = g.mul(g.mul(fibre_strength, g.sub(1.0, fade)), felt_share)
    flat = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    fibres = g.lerp(flat, (fibre_n, "RGB"), strength)
    seam_rgb = _mask(g, seam_n, "rgb")
    xy = g.add(_mask(g, seam_rgb, "rg"), _mask(g, fibres, "rg"))
    z = g.mul(_mask(g, seam_rgb, "b"), _mask(g, fibres, "b"))
    normal = _unary(g, unreal.MaterialExpressionNormalize, _vec(g, xy, z))
    MEL.connect_material_property(normal, "", P.MP_NORMAL)

    emissive = g.mul(g.mul(base, lift), _eye_adaptation_inverse(g, fail))
    MEL.connect_material_property(emissive, "", P.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)


def build_fuzz(mat: unreal.Material, material_dir: str, cfg: dict, radius_mm: float) -> None:
    P = unreal.MaterialProperty
    S = unreal.MaterialSamplerType
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLOTH)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    g = Graph(mat)
    felt_tile = cfg["felt"]["tile_mm"]

    color = g.vector("BaseColor", (0.75, 0.93, 0.11))
    fuzz_color = g.vector("FuzzColor", (0.9, 1.0, 0.43))
    roughness = g.scalar("Roughness", 1.0)
    density = g.scalar("Density", 0.35)
    rim = g.scalar("Rim", 0.9)
    seam_half = g.scalar("SeamHalfWidth", cfg["seam"]["half_width_mm"] / radius_mm)
    sdf_range = g.scalar("SdfRange", cfg["textures"]["sdf_range_rad"])
    # Finer than the felt's fibre tiling, so the loose strands don't line up with it.
    tiles_u = g.scalar("StrandTilesU", round(2 * math.pi * radius_mm / felt_tile * 1.7))
    tiles_v = g.scalar("StrandTilesV", round(math.pi * radius_mm / felt_tile * 1.7))
    fade_start = g.scalar("DetailFadeStart", 0.6)
    fade_end = g.scalar("DetailFadeEnd", 4.0)

    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    masks = g.texture("BallMasks", f"{material_dir}/T_Ball_Masks", S.SAMPLERTYPE_LINEAR_COLOR, uv0)
    strands = g.texture("Strands", f"{material_dir}/T_Felt_Mask", S.SAMPLERTYPE_LINEAR_GRAYSCALE,
                        g.mul(uv0, _vec(g, tiles_u, tiles_v)))
    outline = g.sub(1.0, _facing(g))
    cover = g.mul((strands, "R"), g.add(density, g.mul(rim, g.mul(outline, outline))))
    # No loose fibres on the rubber strip or right at its edge.
    off_seam = g.sat(g.div(g.sub(_seam_distance(g, masks, sdf_range), seam_half), seam_half))
    cover = g.mul(g.mul(cover, off_seam), g.sub(1.0, _distance_fade(g, fade_start, fade_end)))
    # The mask clips at 0.5: scale so a strand at full coverage passes.
    MEL.connect_material_property(g.mul(cover, 1.6), "", P.MP_OPACITY_MASK)
    MEL.connect_material_property(color, "", P.MP_BASE_COLOR)
    MEL.connect_material_property(roughness, "", P.MP_ROUGHNESS)
    MEL.connect_material_property(fuzz_color, "", P.MP_SUBSURFACE_COLOR)
    _connect_cloth_amount(g.node(unreal.MaterialExpressionConstant, r=1.0))
    MEL.recompile_material(mat)


def build_trail(mat: unreal.Material, fail) -> None:
    P = unreal.MaterialProperty
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = Graph(mat)
    color = g.vector("BaseColor", (0.87, 1.0, 0.2))
    brightness = g.scalar("Brightness", 0.55)
    # Written per component by APadelBall (custom primitive data 0): no dynamic instances.
    opacity = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="TrailOpacity", default_value=0.0,
                     use_custom_primitive_data=True, primitive_data_index=0)
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    head = g.sub(1.0, _mask(g, uv0, "r"))
    facing = _facing(g)
    alpha = g.mul(g.mul(opacity, g.mul(head, head)), g.mul(facing, facing))
    MEL.connect_material_property(alpha, "", P.MP_OPACITY)
    MEL.connect_material_property(g.mul(g.mul(color, brightness), _eye_adaptation_inverse(g, fail)), "", P.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)


# palette.json keys of the ball entries -> material parameters.
INSTANCE_SCALARS = {"seam_roughness": "SeamRoughness", "mottle_amount": "MottleAmount",
                    "readability_lift": "ReadabilityLift", "min_seam_pixels": "MinSeamPixels",
                    "density": "Density", "rim": "Rim", "brightness": "Brightness"}
INSTANCE_VECTORS = {"seam_color": "SeamColor", "ink_color": "InkColor", "fuzz_color": "FuzzColor"}


def apply_instance(mi, settings: dict, linear) -> None:
    for key, param in INSTANCE_SCALARS.items():
        if key in settings:
            MEL.set_material_instance_scalar_parameter_value(mi, param, float(settings[key]))
    for key, param in INSTANCE_VECTORS.items():
        if key in settings:
            MEL.set_material_instance_vector_parameter_value(mi, param, linear(settings[key]))


def _connect_cloth_amount(expression) -> None:
    """The Cloth shading model's "Cloth" input (how much fuzz, CustomData0). UE 5.8's
    Python MaterialProperty enum has no MP_CUSTOM_DATA0, so where it's missing the input
    stays unconnected and the whole surface gets the fuzz sheen."""
    prop = getattr(unreal.MaterialProperty, "MP_CUSTOM_DATA0", None)
    if prop is not None:
        MEL.connect_material_property(expression, "", prop)
