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

    def test_the_top_is_broad_like_a_pro_racket(self) -> None:
        # A rounded, broad top (not a point): about 80 mm across 2 mm below the tip.
        tip = layout.tip_z(self.cfg)
        self.assertGreater(2 * layout.half_width_at(self.cfg, tip - 0.002), 0.07)

    def test_it_is_a_diamond(self) -> None:
        # Widest point high in the head (about two thirds of the way up the head), the
        # shape of a power racket, not a round one.
        x, z = layout.widest(self.cfg)
        head_bottom = max(z for x, z in layout.heart_windows(self.cfg)[0])
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

    def test_holes_stay_on_the_face_and_off_the_wordmark_band(self) -> None:
        face = layout.face_outline(self.cfg)
        r = self.cfg["holes"]["diameter_m"] / 2
        band_top = self.cfg["holes"]["keep_clear_z_m"][1]
        for p in self.holes:
            self.assertTrue(layout.point_in_polygon(p, face))
            self.assertGreaterEqual(layout.distance_to_polyline(p, face), r)
            self.assertGreater(p[1] - r, band_top)
        # A tournament racket's dense pattern, not a sprinkle.
        self.assertGreaterEqual(len(self.holes), 55)

    def test_heart_is_one_window_between_the_arms(self) -> None:
        windows = layout.heart_windows(self.cfg)
        bridge = self.cfg["heart"]["bridge_width_m"]
        # One open triangle like a pro racket's; a bridge, if configured, must be wider
        # than both windows' edge rounding.
        self.assertEqual(len(windows), 1 if bridge == 0 else 2)
        if bridge:
            self.assertGreater(bridge, 2 * self.cfg["frame"]["edge_radius_m"])
        zs = [z for x, z in windows[0]]
        self.assertAlmostEqual(min(zs), self.cfg["heart"]["bottom_z_m"], delta=0.004)
        # Its top is the bottom of the face, below the wordmark.
        self.assertLess(max(zs), self.cfg["graphics"]["model"]["centre_z_m"] - self.cfg["graphics"]["model"]["cap_m"])
        for window in windows:
            for p in window:
                self.assertTrue(layout.point_in_polygon(p, self.outline))
                # Every window point is about an arm's width inside the edge (measured across X, so a slanted arm is a little narrower).
                self.assertGreater(layout.distance_to_polyline(p, self.outline), self.cfg["heart"]["arm_width_m"] * 0.7)
                # ...and clear of the bridge.
                self.assertGreaterEqual(abs(p[0]), bridge / 2 - 1e-6)
            for x, z in self.holes:
                self.assertFalse(layout.point_in_polygon((x, z), window))

    def test_game_data_matches_the_mesh(self) -> None:
        # Data/Racket.json names this mesh and puts the placeholder swing's face centre
        # in the middle of the head (above the heart, below the tip).
        data = json.loads((Path(__file__).resolve().parents[3] / "Data" / "Racket.json").read_text())["Racket"]
        self.assertEqual(data["Mesh"], "/Game/Padel/Equipment/SM_Racket.SM_Racket")
        head_bottom = max(z for x, z in layout.heart_windows(self.cfg)[0])
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

    def test_emblem_fits_the_face(self) -> None:
        face = layout.face_outline(self.cfg)
        for stone in layout.arch_stones(self.cfg):
            for p in stone:
                self.assertTrue(layout.point_in_polygon(p, face), f"emblem point {p} off the face")

    def test_frame_and_grip_print(self) -> None:
        frame = racket_textures.draw_frame(self.cfg)
        grip = racket_textures.draw_grip(self.cfg)
        self.assertGreater(sum(v > 0.5 for v in frame.channels[1]), 500, "no lettering on the frame")
        self.assertGreater(sum(v > 0.5 for v in grip.channels[1]), 200, "no dots on the grip")
        # The lettering sits on the flat of the edge, clear of the rounded corners.
        r = self.cfg["frame"]["edge_radius_m"] / self.cfg["frame"]["thickness_m"]
        rows = [y for y in range(frame.h) if any(frame.channels[1][y * frame.w + x] > 0.5 for x in range(frame.w))]
        self.assertGreaterEqual(min(rows) / frame.h, r * 0.5)
        self.assertLessEqual(max(rows) / frame.h, 1 - r * 0.5)

    def test_edge_u_runs_round_from_the_neck(self) -> None:
        neck = self.cfg["frame"]["neck_z_m"]
        tip = layout.tip_z(self.cfg)
        # The seam is at the neck, so u there is 0 or 1.
        u = layout.edge_u(self.cfg, 0.0, neck)
        self.assertLess(min(u, 1.0 - u), 0.02)
        self.assertAlmostEqual(layout.edge_u(self.cfg, 0.0, tip), 0.5, delta=0.02)
        z = self.cfg["graphics"]["frame_text"]["centre_z_m"]
        right = layout.edge_u(self.cfg, layout.half_width_at(self.cfg, z), z)
        left = layout.edge_u(self.cfg, -layout.half_width_at(self.cfg, z), z)
        self.assertAlmostEqual(right + left, 1.0, delta=0.01)

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
        ink = sum(self.channel_at(2, -width / 2 + width * i / 200, g["centre_z_m"]) > 0.5 for i in range(200))
        self.assertGreater(ink, 20)

    def test_textures_write(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            paths = racket_textures.generate_all(Path(tmp), self.cfg)
            self.assertEqual(sorted(p.name for p in paths),
                             ["T_CarbonWeave_Mask.png", "T_RacketFrame_Masks.png", "T_RacketGrip_Masks.png",
                              "T_RacketGrit_N.png", "T_Racket_Masks.png"])
            masks = (Path(tmp) / "T_Racket_Masks.png").read_bytes()
            # RGBA (colour type 6), masks_px square.
            self.assertEqual(masks[25], 6)
            self.assertEqual(int.from_bytes(masks[16:20], "big"), self.cfg["textures"]["masks_px"])


if __name__ == "__main__":
    unittest.main()
