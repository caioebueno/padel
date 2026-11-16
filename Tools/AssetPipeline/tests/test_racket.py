"""The racket's layout and textures, without Blender (docs/decisions/0008-racket.md):
regulation sizes, the hole pattern, the open heart, the face graphic and the originality
of its names."""

from __future__ import annotations

import json
import math
import tempfile
import unittest
from pathlib import Path

from asset_lib import originality, racket_layout as layout, racket_textures


class RacketLayoutTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.cfg = layout.load_config()
        cls.outline = layout.outline(cls.cfg, 360)
        cls.holes = layout.hole_centres(cls.cfg)

    def test_silhouette_is_regulation_size_with_the_grip_centre_on_the_origin(self) -> None:
        xs = [p[0] for p in self.outline]
        zs = [p[1] for p in self.outline]
        width = max(xs) - min(xs)
        # The bumper over the tip counts towards the length.
        length = layout.tip_z(self.cfg) + self.cfg["bumper"]["proud_m"] - self.cfg["frame"]["butt_z_m"]
        self.assertLessEqual(width, layout.MAX_WIDTH_M + 1e-4)
        self.assertGreater(width, 0.255)
        self.assertLessEqual(length, layout.MAX_LENGTH_M + 1e-9)
        self.assertLessEqual(self.cfg["frame"]["thickness_m"], layout.MAX_THICKNESS_M)
        self.assertAlmostEqual(max(xs), -min(xs), delta=1e-4)
        self.assertAlmostEqual(max(zs), layout.tip_z(self.cfg), delta=1e-3)
        # The handle (butt to the neck) straddles the origin.
        self.assertLess(self.cfg["grip"]["wrap_bottom_z_m"], 0.0)
        self.assertGreater(self.cfg["grip"]["wrap_top_z_m"], 0.0)

    def test_it_is_a_diamond(self) -> None:
        # Widest point high in the head (about two thirds of the way up the head), the
        # shape of a power racket, not a round one.
        x, z = layout.widest(self.cfg)
        head_bottom = self.cfg["heart"]["top_z_m"]
        share = (z - head_bottom) / (layout.tip_z(self.cfg) - head_bottom)
        self.assertGreater(share, 0.55)
        self.assertLess(share, 0.75)

    def test_holes_are_regulation_size_symmetric_and_apart(self) -> None:
        d = self.cfg["holes"]["diameter_m"]
        lo, hi = layout.HOLE_DIAMETER_RANGE_M
        self.assertTrue(lo <= d <= hi)
        self.assertGreaterEqual(len(self.holes), 36)
        for x, z in self.holes:
            self.assertTrue(any(math.dist((-x, z), q) < 1e-9 for q in self.holes), f"no mirror for {(x, z)}")
        gap = self.cfg["holes"]["min_gap_m"]
        for i, p in enumerate(self.holes):
            for q in self.holes[i + 1:]:
                self.assertGreaterEqual(math.dist(p, q), d + gap - 1e-9)

    def test_holes_stay_on_the_face_and_off_the_wordmark_panel(self) -> None:
        face = layout.face_outline(self.cfg)
        r = self.cfg["holes"]["diameter_m"] / 2
        for p in self.holes:
            self.assertTrue(layout.point_in_polygon(p, face))
            self.assertGreaterEqual(layout.distance_to_polyline(p, face), r)
            self.assertGreater(p[1] - r, layout.panel_edge(self.cfg, p[0]))

    def test_heart_windows_leave_arms_and_a_bridge(self) -> None:
        windows = layout.heart_windows(self.cfg)
        self.assertEqual(len(windows), 2)
        bridge = self.cfg["heart"]["bridge_width_m"]
        # The bevel needs the bridge wider than both windows' edge rounding.
        self.assertGreater(bridge, 2 * self.cfg["frame"]["edge_radius_m"])
        for window in windows:
            for p in window:
                self.assertTrue(layout.point_in_polygon(p, self.outline))
                # Every window point is about an arm's width inside the edge (measured across X, so a slanted arm is a little narrower).
                self.assertGreater(layout.distance_to_polyline(p, self.outline), self.cfg["heart"]["arm_width_m"] * 0.75)
                # ...and clear of the bridge.
                self.assertGreaterEqual(abs(p[0]), bridge / 2 - 1e-6)
            for x, z in self.holes:
                self.assertFalse(layout.point_in_polygon((x, z), window))

    def test_game_data_matches_the_mesh(self) -> None:
        # Data/Racket.json names this mesh and puts the placeholder swing's face centre
        # in the middle of the head (above the heart, below the tip).
        data = json.loads((Path(__file__).resolve().parents[3] / "Data" / "Racket.json").read_text())["Racket"]
        self.assertEqual(data["Mesh"], "/Game/Padel/Equipment/SM_Racket.SM_Racket")
        head_bottom = self.cfg["heart"]["top_z_m"]
        self.assertAlmostEqual(data["FaceCentreM"], (head_bottom + layout.tip_z(self.cfg)) / 2, delta=0.01)

    def test_offset_moves_edges_by_the_distance(self) -> None:
        square = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        inner = layout.offset(square, 0.1)
        self.assertAlmostEqual(min(p[0] for p in inner), 0.1)
        self.assertAlmostEqual(max(p[1] for p in inner), 0.9)

    def test_fillet_rounds_sharp_corners_only(self) -> None:
        square = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        round_square = layout.fillet_corners(square, 0.2)
        self.assertGreater(len(round_square), 40)
        self.assertLess(max(layout._turns(round_square)), 25.0)
        self.assertFalse(layout.point_in_polygon((0.02, 0.02), round_square))
        self.assertTrue(layout.point_in_polygon((0.5, 0.02), round_square))


class RacketTextureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.cfg = layout.load_config()
        cls.canvas = racket_textures.draw_face(cls.cfg)

    def channel_at(self, ch: int, x: float, z: float) -> float:
        n = self.canvas.n
        u, v = layout.face_uv(self.cfg, x, z)
        return self.canvas.channels[ch][min(n - 1, int(v * n)) * n + min(n - 1, int(u * n))]

    def test_brand_text_can_be_drawn(self) -> None:
        for key in ("name", "model", "tag"):
            for char in self.cfg["brand"][key]:
                self.assertIn(char, racket_textures.GLYPHS, f"no stroke glyph for {char!r}")

    def test_names_are_original(self) -> None:
        terms = originality.load_terms()
        for key in ("name", "model", "tag"):
            originality.check(self.cfg["brand"][key], f"racket brand {key}", terms)
        self.assertIn("metalbone", terms)

    def test_holes_channel_matches_the_drilled_holes(self) -> None:
        r = self.cfg["holes"]["diameter_m"] / 2
        for x, z in layout.hole_centres(self.cfg):
            self.assertGreater(self.channel_at(3, x, z), 0.99)
            # Just outside the drilled hole the face is not darkened.
            self.assertLess(self.channel_at(3, x + r + 0.0008, z), 0.01)

    def test_wordmark_sits_on_the_paint_panel_and_fits_the_face(self) -> None:
        g = self.cfg["graphics"]["wordmark"]
        width = racket_textures.text_width(self.cfg["brand"]["name"], g["cap_m"], g["tracking"])
        half_face = layout.half_width_at(self.cfg, g["centre_z_m"]) - self.cfg["frame"]["face_margin_m"]
        self.assertLess(width / 2, half_face)
        self.assertGreater(self.channel_at(0, 0.0, g["centre_z_m"]), 0.99)
        ink = sum(self.channel_at(2, -width / 2 + width * i / 200, g["centre_z_m"]) > 0.5 for i in range(200))
        self.assertGreater(ink, 20)

    def test_textures_write(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            paths = racket_textures.generate_all(Path(tmp), self.cfg)
            self.assertEqual(sorted(p.name for p in paths),
                             ["T_CarbonWeave_Mask.png", "T_RacketGrit_N.png", "T_Racket_Masks.png"])
            masks = (Path(tmp) / "T_Racket_Masks.png").read_bytes()
            # RGBA (colour type 6), masks_px square.
            self.assertEqual(masks[25], 6)
            self.assertEqual(int.from_bytes(masks[16:20], "big"), self.cfg["textures"]["masks_px"])


if __name__ == "__main__":
    unittest.main()
