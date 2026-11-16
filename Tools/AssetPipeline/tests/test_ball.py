"""The ball's textures (GDD 2, GDD 10): the seam curve, its distance field, the printed
mark and the felt tile. Standard library only, no Blender. Runs on a small copy of
config/ball.json's texture sizes so it stays quick."""

from __future__ import annotations

import copy
import math
import tempfile
import unittest
import zlib
from pathlib import Path

from asset_lib import ball_textures, catalogue, originality

RADIUS_MM = 32.5


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    return int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")


def png_rows(path: Path, channels: int) -> list[bytes]:
    """Rows of an unfiltered 8-bit PNG as written by textures.write_png."""
    data = path.read_bytes()
    width, height = png_size(path)
    pos, idat = 8, b""
    while pos < len(data):
        length = int.from_bytes(data[pos:pos + 4], "big")
        tag = data[pos + 4:pos + 8]
        if tag == b"IDAT":
            idat += data[pos + 8:pos + 8 + length]
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = 1 + width * channels
    return [raw[y * stride + 1:(y + 1) * stride] for y in range(height)]


class SeamTest(unittest.TestCase):
    def setUp(self) -> None:
        self.cfg = ball_textures.load_config()
        self.b = self.cfg["seam"]["b"]

    def test_seam_is_a_closed_curve_on_the_sphere(self) -> None:
        curve = ball_textures.seam_curve(self.b, 720)
        for p in curve:
            self.assertAlmostEqual(math.sqrt(sum(c * c for c in p)), 1.0, places=9)
        first, last = ball_textures.seam_point(0.0, self.b), ball_textures.seam_point(2 * math.pi, self.b)
        for a, b in zip(first, last):
            self.assertAlmostEqual(a, b, places=9)

    def test_seam_crosses_the_axes_and_the_halves_bulge_at_the_diagonals(self) -> None:
        # config/ball.json seam._doc and mark.centres rely on this layout.
        field = ball_textures.SeamField(self.b, 720, (96, 48))
        # The halves bulge at the equator diagonals and the seam crosses the axes there.
        on_axis = field.distance((1.0, 0.0, 0.0), 0.0, 0.5)
        bulge = field.distance(ball_textures._norm((1.0, 1.0, 0.0)), 0.125, 0.5)
        self.assertLess(on_axis, 1e-3)
        self.assertGreater(bulge, 0.6)

    def test_seam_distance_matches_brute_force(self) -> None:
        field = ball_textures.SeamField(self.b, 1440, (96, 48))
        curve = ball_textures.seam_curve(self.b, 20000)
        far = self.cfg["textures"]["sdf_range_rad"]
        for u, v in ((0.03, 0.5), (0.2, 0.62), (0.37, 0.31), (0.5, 0.8), (0.71, 0.45), (0.9, 0.12), (0.125, 0.5)):
            d = ball_textures.uv_direction(u, v)
            exact = math.acos(min(1.0, max(sum(a * b for a, b in zip(d, q)) for q in curve)))
            self.assertAlmostEqual(field.distance(d, u, v, far), min(exact, far), delta=2e-4)

    def test_mark_is_original_and_inked_only_inside_its_outline(self) -> None:
        mark = self.cfg["mark"]
        # No text, so nothing for the originality guard to find in its description.
        originality.check(mark["_doc"], "ball mark", originality.load_terms())
        self.assertGreater(ball_textures.mark_coverage(0.0, 0.0, mark, 0.1), 0.5)  # the net line
        self.assertEqual(ball_textures.mark_coverage(mark["length_mm"], 0.0, mark, 0.1), 0.0)
        self.assertEqual(ball_textures.mark_coverage(mark["length_mm"] / 4, 0.0, mark, 0.1), 0.0)  # inside the court

    def test_groove_sits_below_the_felt_and_meets_it_smoothly(self) -> None:
        seam = self.cfg["seam"]
        self.assertLess(ball_textures.groove_height(0.0, seam), -0.8 * seam["groove_depth_mm"])
        self.assertAlmostEqual(ball_textures.groove_height(seam["half_width_mm"] + seam["shoulder_mm"], seam), 0.0)
        self.assertEqual(ball_textures.groove_height(10.0, seam), 0.0)


class BallTexturesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cfg = copy.deepcopy(ball_textures.load_config())
        cfg["textures"]["masks_px"] = [128, 64]
        cfg["textures"]["coarse_px"] = [64, 32]
        cfg["felt"]["fibre_px"] = 64
        cfg["felt"]["fibres"] = 200
        cls.cfg = cfg
        cls.tmp = tempfile.TemporaryDirectory()
        cls.paths = ball_textures.generate_all(RADIUS_MM, Path(cls.tmp.name), cfg)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    def test_writes_the_four_textures_the_material_reads(self) -> None:
        self.assertEqual([p.name for p in self.paths], ["T_Ball_Masks.png", "T_Ball_N.png", "T_Felt_N.png", "T_Felt_Mask.png"])
        self.assertEqual(png_size(self.paths[0]), (128, 64))
        self.assertEqual(png_size(self.paths[1]), (128, 64))
        self.assertEqual(png_size(self.paths[2]), (64, 64))

    def test_masks_hold_the_seam_field_the_mottle_and_the_mark(self) -> None:
        rows = png_rows(self.paths[0], 3)
        reds = [row[i] for row in rows for i in range(0, len(row), 3)]
        blues = [row[i] for row in rows for i in range(2, len(row), 3)]
        greens = [row[i] for row in rows for i in range(1, len(row), 3)]
        self.assertLessEqual(min(reds), 3)  # on the seam (pixel centres miss the line by a little)
        self.assertEqual(max(reds), 255)    # out of the field's range
        self.assertGreater(max(blues), 100)  # the mark is printed (thin strokes at this test size)
        self.assertGreater(max(greens) - min(greens), 200)  # the mottle uses the whole range
        # The seam crosses the equator at U = 0 (+X): red is near zero there.
        self.assertLess(rows[32][0], 40)  # pixel 0 is half a pixel (0.02 rad) east of +X

    def test_textures_are_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            again = ball_textures.generate_all(RADIUS_MM, Path(tmp), self.cfg)
            for a, b in zip(self.paths, again):
                self.assertEqual(a.read_bytes(), b.read_bytes(), a.name)


class BallCatalogueTest(unittest.TestCase):
    def test_ball_has_hand_built_lods_and_a_trail(self) -> None:
        cat = catalogue.load()
        ball = cat.get("Ball")
        self.assertEqual(ball.size["diameter_m"], 0.065)
        self.assertFalse(ball.nanite)
        self.assertEqual(len(ball.lods), len(ball_textures.load_config()["mesh"]["lod_segments"]) - 1)
        self.assertEqual(cat.get("BallTrail").asset_name, "SM_BallTrail")
        palette = catalogue.load_palette()["ball"]
        self.assertEqual({e["master"] for e in palette.values()}, {"BallFelt", "BallFuzz", "BallTrail"})
        self.assertEqual(catalogue.material_name("ball", "Fuzz"), "MI_Ball_Fuzz")

    def test_felt_is_bright_optic_yellow(self) -> None:
        # GDD 10: the ball is the brightest thing on screen; real felt is about 0.6-0.85 albedo.
        c = catalogue.load_palette()["ball"]["Felt"]["color"].lstrip("#")
        lin = [(v / 255 / 12.92) if v / 255 <= 0.04045 else ((v / 255 + 0.055) / 1.055) ** 2.4
               for v in (int(c[i:i + 2], 16) for i in (0, 2, 4))]
        luminance = 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2]
        self.assertGreater(luminance, 0.55)
        self.assertLess(luminance, 0.9)
        self.assertGreater(lin[1], lin[2] * 4)  # yellow-green, not white


if __name__ == "__main__":
    unittest.main()
