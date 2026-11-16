"""The racket face's master material (docs/decisions/0008-racket.md). Built from text by
import_assets.py like every other master; never edit the asset by hand. The rest of the
racket (frame, foam core, bumper, alloy, tape, overgrip, cap) uses M_PadelOpaque with
palette.json 'racket' values.

  M_PadelRacketFace  the hitting faces and the front of the rounded edges:
    carbon = BaseColor * (1 - WeaveContrast + 2 WeaveContrast * weave)   bare 2x2 twill
    base   = carbon, then PaintColor by Masks.R, AccentColor by .G,
             InkColor by .B, HoleColor by .A (only LOD2 shows .A: it has no drilled holes)
    rough  = lerp(lerp(Roughness, PaintRoughness, printed), GritRoughness, GritAmount)
    normal = T_RacketGrit_N, the raised sand finish, tiled at its real size and faded
             to flat (with the weave's contrast) between DetailFadeStart and
             DetailFadeEnd metres, where it would only shimmer

Inputs: UV0 is the face square of racket_layout.face_uv (0..1 over size_m, V down from
the tip); the tiling textures are scaled from it by WeaveTiles and GritTiles. Plain
expressions only, so it compiles the same on D3D and Metal.
"""

from __future__ import annotations

import unreal

from turf_material import Graph

MEL = unreal.MaterialEditingLibrary


def _mask(g: Graph, src, channels: str):
    expr = g.node(unreal.MaterialExpressionComponentMask, r="r" in channels, g="g" in channels,
                  b="b" in channels, a="a" in channels)
    g.link(src, expr, "")
    return expr


def _max(g: Graph, a, b):
    return g._binary(unreal.MaterialExpressionMax, a, b)


def build_face(mat: unreal.Material, material_dir: str, cfg: dict) -> None:
    P = unreal.MaterialProperty
    S = unreal.MaterialSamplerType
    tex = cfg["textures"]
    g = Graph(mat)

    carbon_color = g.vector("BaseColor", (0.008, 0.009, 0.012))
    paint_color = g.vector("PaintColor", (0.014, 0.39, 0.32))
    accent_color = g.vector("AccentColor", (1.0, 0.09, 0.04))
    ink_color = g.vector("InkColor", (0.85, 0.85, 0.82))
    hole_color = g.vector("HoleColor", (0.27, 0.25, 0.22))
    roughness = g.scalar("Roughness", 0.32)
    paint_roughness = g.scalar("PaintRoughness", 0.4)
    grit_roughness = g.scalar("GritRoughness", 0.58)
    grit_amount = g.scalar("GritAmount", 0.55)
    grit_strength = g.scalar("GritNormalStrength", 0.8)
    weave_contrast = g.scalar("WeaveContrast", 0.35)
    weave_tiles = g.scalar("WeaveTiles", tex["size_m"] / tex["weave_tile_m"])
    grit_tiles = g.scalar("GritTiles", tex["size_m"] / tex["grit_tile_m"])
    specular = g.scalar("Specular", 0.5)
    fade_start = g.scalar("DetailFadeStart", 1.5)
    fade_end = g.scalar("DetailFadeEnd", 8.0)

    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    masks = g.texture("RacketMasks", f"{material_dir}/T_Racket_Masks", S.SAMPLERTYPE_MASKS, uv0)
    weave = g.texture("CarbonWeave", f"{material_dir}/T_CarbonWeave_Mask", S.SAMPLERTYPE_LINEAR_GRAYSCALE,
                      g.mul(uv0, weave_tiles))
    grit = g.texture("GritNormal", f"{material_dir}/T_RacketGrit_N", S.SAMPLERTYPE_NORMAL, g.mul(uv0, grit_tiles))

    # Detail fades out with distance (PixelDepth is in centimetres).
    depth_m = g.mul(g.node(unreal.MaterialExpressionPixelDepth), 0.01)
    near = g.sub(1.0, g.sat(g.div(g.sub(depth_m, fade_start), g.sub(fade_end, fade_start))))

    contrast = g.mul(weave_contrast, near)
    tows = g.add(g.sub(1.0, contrast), g.mul(g.mul(contrast, 2.0), (weave, "R")))
    base = g.mul(carbon_color, tows)
    base = g.lerp(base, paint_color, (masks, "R"))
    base = g.lerp(base, accent_color, (masks, "G"))
    base = g.lerp(base, ink_color, (masks, "B"))
    base = g.lerp(base, hole_color, (masks, "A"))
    MEL.connect_material_property(base, "", P.MP_BASE_COLOR)

    printed = _max(g, _max(g, (masks, "R"), (masks, "G")), (masks, "B"))
    rough = g.lerp(g.lerp(roughness, paint_roughness, printed), grit_roughness, grit_amount)
    MEL.connect_material_property(rough, "", P.MP_ROUGHNESS)
    MEL.connect_material_property(specular, "", P.MP_SPECULAR)

    flat = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    normal = g.lerp(flat, _mask(g, grit, "rgb"), g.mul(grit_strength, near))
    MEL.connect_material_property(normal, "", P.MP_NORMAL)
    MEL.recompile_material(mat)


# palette.json keys of the racket face -> material parameters (color, roughness and
# specular are set by import_assets.build_instances like every other master).
INSTANCE_SCALARS = {"paint_roughness": "PaintRoughness", "grit_roughness": "GritRoughness", "grit_amount": "GritAmount",
                    "grit_normal_strength": "GritNormalStrength", "weave_contrast": "WeaveContrast",
                    "detail_fade_start_m": "DetailFadeStart", "detail_fade_end_m": "DetailFadeEnd"}
INSTANCE_VECTORS = {"paint_color": "PaintColor", "accent_color": "AccentColor", "ink_color": "InkColor",
                    "hole_color": "HoleColor"}


def apply_instance(mi, settings: dict, linear) -> None:
    for key, param in INSTANCE_SCALARS.items():
        if key in settings:
            MEL.set_material_instance_scalar_parameter_value(mi, param, float(settings[key]))
    for key, param in INSTANCE_VECTORS.items():
        if key in settings:
            MEL.set_material_instance_vector_parameter_value(mi, param, linear(settings[key]))
