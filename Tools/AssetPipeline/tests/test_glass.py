"""The court's glass walls: panel layout against CourtSpec.h (GDD 2), the bolt
fixings and the dirt textures. Standard library only, no Blender."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from asset_lib import catalogue, court_spec, glass


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


class GlassLayoutTest(unittest.TestCase):
    def setUp(self) -> None:
        self.spec = court_spec.load()
        self.cfg = glass.load_config()
        self.panels = glass.panels(self.spec, self.cfg)
        self.joint = self.cfg["panel"]["joint_m"]

    def test_eighteen_panels_of_regulation_height(self) -> None:
        self.assertEqual(len(self.panels), 18)
        back = [p for p in self.panels if p["wall"] == "back"]
        side = [p for p in self.panels if p["wall"] == "side"]
        self.assertEqual(len(back), 10)
        self.assertEqual(len(side), 8)
        self.assertTrue(all(p["height"] == self.spec["BackGlassHeight"] for p in back))
        self.assertEqual(sorted(p["height"] for p in side),
                         [self.spec["SideGlassStepHeight"]] * 4 + [self.spec["SideGlassFullHeight"]] * 4)
        self.assertEqual(len({p["name"] for p in self.panels}), 18)

    def test_inner_faces_sit_on_the_court_limits_and_face_out(self) -> None:
        for p in self.panels:
            n = p["normal"]
            # u x up points out of the court, so the slab grows outward.
            self.assertEqual(cross(p["u"], (0.0, 0.0, 1.0)), n, p["name"])
            for u in (0.0, p["width"]):
                corner = glass.point_on(p, u, 0.0)
                if p["wall"] == "back":
                    self.assertAlmostEqual(corner[0], n[0] * self.spec["HalfLength"])
                    self.assertLessEqual(abs(corner[1]), self.spec["HalfWidth"])
                else:
                    self.assertAlmostEqual(corner[1], n[1] * self.spec["HalfWidth"])
                    self.assertLessEqual(abs(corner[0]), self.spec["HalfLength"])

    def test_panels_cover_the_glass_wall_exactly_with_joints_on_post_lines(self) -> None:
        for p in self.panels:
            along = 1 if p["wall"] == "back" else 0
            ends = sorted(glass.point_on(p, u, 0.0)[along] for u in (0.0, p["width"]))
            # Each edge is half a joint off a post line (every 2 m).
            # Post lines: every 2 m from the side wall across the back, every 2 m from the net along the sides.
            base = -self.spec["HalfWidth"] if p["wall"] == "back" else 0.0
            for end in ends:
                nearest = round((end - base) / 2.0) * 2.0 + base
                self.assertAlmostEqual(abs(end - nearest), self.joint / 2, places=9, msg=p["name"])
            if p["wall"] == "side":
                lo, hi = min(abs(ends[0]), abs(ends[1])), max(abs(ends[0]), abs(ends[1]))
                expected_h = self.spec["SideGlassFullHeight"] if lo > self.spec["SideGlassStepStartX"] else self.spec["SideGlassStepHeight"]
                self.assertEqual(p["height"], expected_h)
                # The glass never reaches into the exit opening.
                self.assertGreater(lo, self.spec["ExitFarX"])
                self.assertLess(hi, self.spec["HalfLength"])

    def test_glass_height_matches_courtspec_side_wall_profile(self) -> None:
        # Mirror of Padel::Court::SideGlassHeightAt, sampled through the panel centres.
        for p in (q for q in self.panels if q["wall"] == "side"):
            centre_x = glass.point_on(p, p["width"] / 2, 0.0)[0]
            ax = abs(centre_x)
            expected = self.spec["SideGlassFullHeight"] if ax >= self.spec["SideGlassStepStartX"] else self.spec["SideGlassStepHeight"]
            self.assertEqual(p["height"], expected, p["name"])

    def test_fixings_are_on_the_posts_and_drillable(self) -> None:
        fix = self.cfg["fixings"]
        t = self.cfg["panel"]["thickness_m"]
        total = 0
        for p in self.panels:
            bolts = glass.fixings(p, self.cfg)
            total += len(bolts)
            for u, z in bolts:
                edge = min(u, p["width"] - u)
                # Hole edge at least twice the thickness from the glass edge (tempered glass rule).
                self.assertGreaterEqual(edge - fix["bolt_radius_m"], 2 * t)
                # The washer lands on the 100 mm post centred on the joint.
                self.assertLessEqual(edge + self.joint / 2 + fix["washer_radius_m"], 0.05 + 1e-9)
                self.assertGreater(z, 0.1)
                self.assertLess(z, p["height"] - 0.1)
        self.assertEqual(total, 100)

    def test_heads_stay_flush_enough_for_play(self) -> None:
        # Nothing may stick out into the court: the heads are under a millimetre proud.
        self.assertLess(self.cfg["fixings"]["head_proud_m"], 0.001)
        self.assertLess(self.cfg["fixings"]["bush_proud_m"], self.cfg["fixings"]["head_proud_m"])

    def test_panels_get_different_texture_windows_and_are_deterministic(self) -> None:
        offsets = [p["uv_offset"] for p in self.panels]
        self.assertEqual(len(set(offsets)), len(offsets))
        self.assertEqual(offsets, [p["uv_offset"] for p in glass.panels(self.spec, self.cfg)])

    def test_rejects_unbuildable_configs(self) -> None:
        data = json.loads(glass.GLASS_CONFIG_PATH.read_text(encoding="utf-8"))
        bad = []
        for section, key, value in (("panel", "thickness_m", 0.004), ("panel", "arris_m", 0.01),
                                    ("fixings", "edge_inset_m", 0.02)):
            copy = json.loads(json.dumps(data))
            copy[section][key] = value
            bad.append(copy)
        with tempfile.TemporaryDirectory() as tmp:
            for i, data in enumerate(bad):
                path = Path(tmp) / f"glass{i}.json"
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.assertRaises(ValueError):
                    glass.load_config(path)


class GlassCatalogueTest(unittest.TestCase):
    def test_glass_asset_is_translucent_safe(self) -> None:
        asset = catalogue.load().get("CourtGlass")
        self.assertEqual(asset.asset_name, "SM_CourtGlass")
        self.assertEqual(asset.raw["builder"], "court_glass")
        # Nanite can't render translucent materials.
        self.assertFalse(asset.nanite)

    def test_palette_has_every_glass_slot(self) -> None:
        court = catalogue.load_palette()["court"]
        self.assertEqual(court["Glass"]["master"], "Glass")
        self.assertEqual(court["GlassEdge"]["master"], "Glass")
        # GDD 10: the glass never hides the ball.
        self.assertLessEqual(court["Glass"]["opacity"], 0.1)
        self.assertGreater(court["GlassEdge"]["opacity"], court["Glass"]["opacity"])
        for name in ("GlassBolt", "GlassGasket"):
            self.assertEqual(court[name]["master"], "Opaque")


class GlassTextureTest(unittest.TestCase):
    def test_textures_are_valid_deterministic_pngs(self) -> None:
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            first = glass.generate_textures(out_dir=Path(a))
            second = glass.generate_textures(out_dir=Path(b))
            self.assertEqual([p.stem for p in first],
                             [glass.SMUDGE_TEXTURE, glass.BALL_MARK_TEXTURE, glass.ROLLER_WAVE_TEXTURE])
            for x, y in zip(first, second):
                data = x.read_bytes()
                self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
                self.assertEqual(data, y.read_bytes(), x.name)
            cfg = glass.load_config()["textures"]
            smudge = first[0].read_bytes()
            self.assertEqual(int.from_bytes(smudge[16:20], "big"), cfg["smudge"]["size_px"])
            wave = first[2].read_bytes()
            self.assertEqual(int.from_bytes(wave[20:24], "big"), cfg["roller_wave"]["size_px"][1])
            # Normal maps are RGB, masks are grey (PNG colour type byte).
            self.assertEqual(wave[25], 2)
            self.assertEqual(smudge[25], 0)


if __name__ == "__main__":
    unittest.main()
