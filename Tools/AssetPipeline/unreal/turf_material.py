"""M_PadelTurf, the playing-surface master material (GDD 10: blue artificial grass
with visible sand, white lines). Built from text by import_assets.py, like every
other master; never edit the asset by hand.

Inputs (from Blender, build_court_surface.py):
  UV0  metres, for the tiling fibre textures (T_Turf_N, T_Turf_Masks)
  UV1  the 20 x 10 playing area as 0..1, for T_CourtSurface_Masks (wear, drift, roll)

Per-pixel model, the same one the Blender preview material uses:
  wear  = CourtMasks.R * WearStrength                  flattened fibres from play
  drift = CourtMasks.G * DriftStrength                 loose sand against walls and under the net
  sand  = sat((1 - FibreMasks.R) * 2 SandAmount + 0.35 wear + drift)
  fibre = lerp(BaseColor, WornColor, wear) * blade tint * clump tint * roll tint
  base  = lerp(fibre, SandColor, sand) * FibreMasks.G (cavity)
  rough = lerp(lerp(Roughness, WornRoughness, wear), SandRoughness, sand)
  normal: T_Turf_N, flattened by wear and faded to flat between DetailFadeStart and
          DetailFadeEnd (metres from the camera), so the far court doesn't shimmer
          from the broadcast camera and distant pixels cost less.

Only plain expressions (no Custom HLSL, no engine material functions) so it
compiles the same on D3D and Metal.
"""

from __future__ import annotations

import unreal

MEL = unreal.MaterialEditingLibrary
WHITE = "/Engine/EngineResources/WhiteSquareTexture"


class Graph:
    """Small helper so the graph reads like the formula above. Unreal lays the
    nodes out left to right by the order they are made."""

    def __init__(self, mat: unreal.Material) -> None:
        self.mat = mat
        self.n = 0

    def node(self, cls, **props):
        self.n += 1
        expr = MEL.create_material_expression(self.mat, cls, -200 * (1 + self.n // 12), 120 * (self.n % 12) - 600)
        for key, value in props.items():
            expr.set_editor_property(key, value)
        return expr

    def link(self, src, dst, pin: str, out: str = "") -> None:
        if isinstance(src, tuple):
            src, out = src
        MEL.connect_material_expressions(src, out, dst, pin)

    def scalar(self, name: str, default: float):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default)

    def vector(self, name: str, rgb: tuple[float, float, float]):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def texture(self, name: str, asset_path: str, sampler, uvs):
        tex = unreal.load_asset(asset_path) or unreal.load_asset(WHITE)
        expr = self.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name,
                         texture=tex, sampler_type=sampler)
        self.link(uvs, expr, "UVs")
        return expr

    def _binary(self, cls, a, b):
        expr = self.node(cls)
        for pin, value in (("A", a), ("B", b)):
            if isinstance(value, (int, float)):
                expr.set_editor_property(f"const_{pin.lower()}", float(value))
            else:
                self.link(value, expr, pin)
        return expr

    def add(self, a, b):
        return self._binary(unreal.MaterialExpressionAdd, a, b)

    def sub(self, a, b):
        return self._binary(unreal.MaterialExpressionSubtract, a, b)

    def mul(self, a, b):
        return self._binary(unreal.MaterialExpressionMultiply, a, b)

    def div(self, a, b):
        return self._binary(unreal.MaterialExpressionDivide, a, b)

    def sat(self, a):
        # Max(Min(a, 1), 0): Min/Max take constants on both pins, Saturate's pin name varies.
        return self._binary(unreal.MaterialExpressionMax, self._binary(unreal.MaterialExpressionMin, a, 1.0), 0.0)

    def lerp(self, a, b, alpha):
        expr = self.node(unreal.MaterialExpressionLinearInterpolate)
        for pin, value in (("A", a), ("B", b), ("Alpha", alpha)):
            if isinstance(value, (int, float)):
                expr.set_editor_property(f"const_{pin.lower()}", float(value))
            else:
                self.link(value, expr, pin)
        return expr

    def tint(self, amount, mask):
        """1 - amount + 2 amount mask: mask 0.5 leaves the colour as it is."""
        return self.add(self.sub(1.0, amount), self.mul(self.mul(amount, 2.0), mask))


def build(mat: unreal.Material, material_dir: str) -> None:
    P = unreal.MaterialProperty
    S = unreal.MaterialSamplerType
    g = Graph(mat)

    # Parameters (palette.json court.Turf/Line/Runoff override every one of them).
    base_color = g.vector("BaseColor", (0.012, 0.078, 0.33))
    worn_color = g.vector("WornColor", (0.047, 0.114, 0.29))
    sand_color = g.vector("SandColor", (0.48, 0.37, 0.2))
    sand_amount = g.scalar("SandAmount", 0.18)
    blade_tint = g.scalar("BladeTint", 0.12)
    roll_tint = g.scalar("RollTint", 0.05)
    wear_strength = g.scalar("WearStrength", 0.7)
    drift_strength = g.scalar("DriftStrength", 0.45)
    normal_strength = g.scalar("NormalStrength", 1.0)
    roughness = g.scalar("Roughness", 0.78)
    worn_roughness = g.scalar("WornRoughness", 0.62)
    sand_roughness = g.scalar("SandRoughness", 0.96)
    specular = g.scalar("Specular", 0.4)
    tile = g.scalar("FibreTile", 0.2375)
    fade_start = g.scalar("DetailFadeStart", 12.0)
    fade_end = g.scalar("DetailFadeEnd", 40.0)

    # Coordinates.
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    uv1 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=1)
    detail_uv = g.div(uv0, tile)
    # A second, larger and offset lookup of the masks gives clumps a few
    # decimetres across, so the 24 cm tile never reads as a grid from far away.
    clump_uv = g.add(g.mul(detail_uv, 0.137), 0.31)

    fibre_n = g.texture("FibreNormal", f"{material_dir}/T_Turf_N", S.SAMPLERTYPE_NORMAL, detail_uv)
    fibre_m = g.texture("FibreMasks", f"{material_dir}/T_Turf_Masks", S.SAMPLERTYPE_MASKS, detail_uv)
    clump_m = g.texture("FibreClumps", f"{material_dir}/T_Turf_Masks", S.SAMPLERTYPE_MASKS, clump_uv)
    court_m = g.texture("CourtMasks", f"{material_dir}/T_CourtSurface_Masks", S.SAMPLERTYPE_MASKS, uv1)

    wear = g.mul((court_m, "R"), wear_strength)
    drift = g.mul((court_m, "G"), drift_strength)
    roll = g.tint(roll_tint, (court_m, "B"))

    gaps = g.sub(1.0, (fibre_m, "R"))
    sand = g.sat(g.add(g.add(g.mul(gaps, g.mul(sand_amount, 2.0)), g.mul(wear, 0.35)), drift))

    fibre = g.lerp(base_color, worn_color, wear)
    fibre = g.mul(fibre, g.tint(blade_tint, (fibre_m, "B")))
    fibre = g.mul(fibre, g.tint(blade_tint, (clump_m, "B")))
    fibre = g.mul(fibre, roll)
    base = g.mul(g.lerp(fibre, sand_color, sand), (fibre_m, "G"))
    MEL.connect_material_property(base, "", P.MP_BASE_COLOR)

    rough = g.lerp(g.lerp(roughness, worn_roughness, wear), sand_roughness, sand)
    MEL.connect_material_property(rough, "", P.MP_ROUGHNESS)
    MEL.connect_material_property(specular, "", P.MP_SPECULAR)

    # Detail fade: PixelDepth is in centimetres, the parameters in metres.
    depth_m = g.mul(g.node(unreal.MaterialExpressionPixelDepth), 0.01)
    fade = g.sat(g.div(g.sub(depth_m, fade_start), g.sub(fade_end, fade_start)))
    detail = g.mul(g.mul(normal_strength, g.sub(1.0, fade)), g.sub(1.0, g.mul(wear, 0.5)))
    flat = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    normal = g.lerp(flat, (fibre_n, "RGB"), detail)
    MEL.connect_material_property(normal, "", P.MP_NORMAL)

    # The cavity is already in the base colour; feeding it to AO too darkens
    # Lumen's indirect light in the gaps between blades, where sand sits in shade.
    MEL.connect_material_property(fibre_m, "G", P.MP_AMBIENT_OCCLUSION)
    MEL.recompile_material(mat)
