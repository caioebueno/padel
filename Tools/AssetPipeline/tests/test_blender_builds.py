"""End-to-end Blender tests: build each procedural asset, export the FBX, import
it again and check what Unreal will get (GDD 2 sizes, budgets, Unreal bone names).

Needs Blender: set BLENDER or BLENDER_PYTHON (see README). Skipped otherwise, so
the config tests still run anywhere.
"""

from __future__ import annotations

import math
import tempfile
import unittest
from pathlib import Path

from asset_lib import blender_runner, catalogue, court_spec, lighting

try:
    blender_runner.command(Path("x.py"), Path("job.json"))
    HAVE_BLENDER = True
except blender_runner.BlenderError:
    HAVE_BLENDER = False

UE_BONES = {"root", "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head",
            "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "thigh_l", "calf_l", "foot_l", "ball_l",
            "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "thigh_r", "calf_r", "foot_r", "ball_r"}


@unittest.skipUnless(HAVE_BLENDER, "Blender not available (set BLENDER or BLENDER_PYTHON)")
class BlenderBuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name)
        cls.cat = catalogue.load()
        cls.palette = catalogue.load_palette()
        cls.spec = court_spec.load()

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    def build(self, asset_id: str, script: str, **extra) -> tuple[dict, dict]:
        asset = self.cat.get(asset_id)
        out = self.out / f"{asset.asset_name}.fbx"
        job = {"id": asset.asset_name, "kind": asset.kind, "builder": asset.raw.get("builder"),
               "size": asset.size, "budget_tris": asset.budget_tris, "spec": self.spec, "palette": self.palette,
               "palette_group": asset.raw.get("palette", ""), "part": asset.raw.get("part", "solid"),
               "body": asset.raw.get("body", {}), "lods": [{"ratio": l.ratio} for l in asset.lods],
               "out": str(out), "lod_outs": [str(self.out / f"{asset.asset_name}_LOD{i}.fbx")
                                             for i in range(1, len(asset.lods) + 1)]}
        if asset.raw.get("builder") == "venue":
            job["rigs"] = lighting.venue_rigs()
        job.update(extra)
        report = blender_runner.run(script, job)
        inspected = blender_runner.run("inspect_fbx", {"file": str(out)})
        return report, inspected

    def extent(self, inspected: dict) -> list[float]:
        return [hi - lo for lo, hi in zip(inspected["bounds_min_m"], inspected["bounds_max_m"])]

    def test_ball_is_65_mm(self) -> None:
        report, fbx = self.build("Ball", "build_equipment")
        for size in self.extent(fbx):
            self.assertAlmostEqual(size, 0.065, delta=0.001)
        self.assertLessEqual(report["triangles"], self.cat.get("Ball").budget_tris)
        self.assertEqual(fbx["materials"], ["MI_Ball_Felt"])

    def test_racket_is_within_the_regulation_size(self) -> None:
        report, fbx = self.build("RacketPlaceholder", "build_equipment")
        x, y, z = self.extent(fbx)
        self.assertLessEqual(z, 0.455 + 1e-3)
        self.assertGreater(z, 0.44)
        self.assertLessEqual(x, 0.26 + 1e-3)
        self.assertLessEqual(y, 0.038 + 1e-3)
        self.assertLessEqual(report["triangles"], self.cat.get("RacketPlaceholder").budget_tris)
        self.assertIn("MI_RacketA_Face", fbx["materials"])

    def test_court_matches_courtspec(self) -> None:
        _, solid = self.build("CourtIndoor", "build_court")
        _, glass = self.build("CourtIndoorGlass", "build_court")
        # Glass inner faces sit on the playing-area limits; its height is the back glass.
        self.assertAlmostEqual(glass["bounds_max_m"][0], 10.0 + 0.012, delta=0.002)
        self.assertAlmostEqual(glass["bounds_max_m"][1], 5.0 + 0.012, delta=0.002)
        self.assertAlmostEqual(glass["bounds_max_m"][2], 3.0, delta=0.002)
        self.assertEqual(glass["materials"], ["MI_Court_Glass"])
        self.assertNotIn("MI_Court_Glass", solid["materials"])
        # Floor top at Z = 0, run-off around the court.
        self.assertAlmostEqual(solid["bounds_max_m"][0], 12.0, delta=0.01)
        self.assertLess(solid["bounds_min_m"][2], 0.0)
        for slot in ("MI_Court_Turf", "MI_Court_Line", "MI_Court_Net", "MI_Court_Mesh", "MI_Court_Frame"):
            self.assertIn(slot, solid["materials"])

    def test_court_has_no_lights_of_its_own(self) -> None:
        # The venue owns the lighting (decision 0006); the court mesh has no light panels.
        _, solid = self.build("CourtIndoor", "build_court")
        self.assertFalse([m for m in solid["materials"] if "Light" in m])
        self.assertLess(solid["bounds_max_m"][2], self.spec["CeilingHeight"])

    def test_hall_holds_the_broadcast_camera_and_clears_the_ceiling(self) -> None:
        report, fbx = self.build("VenueHall", "build_venue")
        # GDD 9: the camera is 6-8 m behind each back glass and 7-9 m up.
        self.assertGreater(fbx["bounds_max_m"][0], self.spec["HalfLength"] + 8.0 + 1.0)
        self.assertLess(fbx["bounds_min_m"][0], -(self.spec["HalfLength"] + 8.0 + 1.0))
        self.assertGreater(fbx["bounds_max_m"][2], 9.0 + 1.0)
        # The floor sits just under the court's run-off.
        self.assertLess(fbx["bounds_min_m"][2], -0.02)
        self.assertLessEqual(report["triangles"], self.cat.get("VenueHall").budget_tris)
        for slot in ("MI_Venue_Concrete", "MI_Venue_WallLower", "MI_Venue_Steel"):
            self.assertIn(slot, fbx["materials"])

    def test_light_rigs_carry_the_fixtures_out_of_play(self) -> None:
        rigs = lighting.venue_rigs()
        truss, truss_fbx = self.build("VenueLightTruss", "build_venue")
        # ARENA_SPEC 6.1: the rig's underside is the 8 m ceiling plane (GDD 2), nothing lower.
        self.assertGreaterEqual(truss_fbx["bounds_min_m"][2], self.spec["CeilingHeight"])
        self.assertLess(truss_fbx["bounds_min_m"][2], self.spec["CeilingHeight"] + 0.01)
        # It reaches every court fixture.
        xs = [x for x, _, _, role in rigs["Indoor"] if role == "Court"]
        self.assertLessEqual(truss_fbx["bounds_min_m"][0], min(xs))
        self.assertGreaterEqual(truss_fbx["bounds_max_m"][0], max(xs))
        self.assertLessEqual(truss["triangles"], self.cat.get("VenueLightTruss").budget_tris)
        masts, masts_fbx = self.build("VenueMasts", "build_venue")
        self.assertGreater(masts["min_abs_y_m"], self.spec["HalfWidth"] + 0.3)
        top = max(z for _, _, z, _ in rigs["Outdoor"])
        self.assertGreater(masts_fbx["bounds_max_m"][2], top)

    def test_stands_and_boards_stay_outside_the_run_off(self) -> None:
        for asset_id in ("VenueStands", "VenueBoards"):
            report, fbx = self.build(asset_id, "build_venue")
            self.assertGreater(report["outside_runoff_m"], 0.0, asset_id)
            self.assertLessEqual(report["triangles"], self.cat.get(asset_id).budget_tris)
        _, boards = self.build("VenueBoards", "build_venue")
        # Sponsor-free: club colours only, no texture with text or logos.
        self.assertEqual(boards["materials"], ["MI_Venue_Board", "MI_Venue_BoardStripe", "MI_Venue_Steel"])

    def test_floodlight_head_shines_along_plus_x(self) -> None:
        report, fbx = self.build("FloodlightHead", "build_venue")
        self.assertIn("MI_Venue_FixtureLens", fbx["materials"])
        # Lens at the origin, housing behind it, only the visor in front.
        self.assertLess(fbx["bounds_max_m"][0], 0.2)
        self.assertLess(fbx["bounds_min_m"][0], -0.15)
        for size in self.extent(fbx):
            self.assertLess(size, 0.8)
        self.assertLessEqual(report["triangles"], self.cat.get("FloodlightHead").budget_tris)

    def test_placeholder_player_is_rigged_for_unreal_and_sized(self) -> None:
        asset = self.cat.get("PlayerPlaceholderA")
        report, fbx = self.build("PlayerPlaceholderA", "build_character")
        self.assertAlmostEqual(self.extent(fbx)[2], asset.size["height_m"], delta=0.03)
        self.assertEqual(fbx["armatures"], 1)
        self.assertEqual(set(fbx["bones"]), UE_BONES)
        # One root, named root: Unreal must not add its own.
        self.assertEqual(fbx["root_bones"], ["root"])
        self.assertLessEqual(report["triangles"], asset.budget_tris)
        self.assertEqual(len(report["lod_triangles"]), len(asset.lods))
        for lod_tris, lod in zip(report["lod_triangles"], asset.lods):
            self.assertLessEqual(lod_tris, math.ceil(report["triangles"] * lod.ratio) + 50)
        # Left is +X (the character faces -Y), and the hip is roughly half its height.
        heads = fbx["bone_heads_m"]
        self.assertGreater(heads["hand_l"][0], 0.5)
        self.assertLess(heads["hand_r"][0], -0.5)
        self.assertAlmostEqual(heads["pelvis"][2] / asset.size["height_m"], 0.53, delta=0.03)
        lod1 = blender_runner.run("inspect_fbx", {"file": str(self.out / "SK_PlayerPlaceholderA_LOD1.fbx")})
        self.assertEqual(set(lod1["bones"]), UE_BONES)

    def test_cleanup_rescales_a_model_and_keeps_its_rig(self) -> None:
        self.build("PlayerPlaceholderB", "build_character")
        source = self.out / "SK_PlayerPlaceholderB.fbx"
        out = self.out / "SK_Cleaned.fbx"
        report = blender_runner.run("cleanup", {
            "id": "SK_Cleaned", "kind": "skeletal", "source_file": str(source), "size": {"height_m": 1.6},
            "budget_tris": 1500, "lods": [{"ratio": 0.5}], "out": str(out),
            "lod_outs": [str(self.out / "SK_Cleaned_LOD1.fbx")]})
        self.assertLessEqual(report["triangles"], 1500)
        fbx = blender_runner.run("inspect_fbx", {"file": str(out)})
        self.assertAlmostEqual(self.extent(fbx)[2], 1.6, delta=0.01)
        self.assertAlmostEqual(fbx["bounds_min_m"][2], 0.0, delta=0.01)
        self.assertEqual(set(fbx["bones"]), UE_BONES)
        # Bones scaled with the mesh: the head bone is near the top, not left at the old size.
        self.assertAlmostEqual(fbx["bone_heads_m"]["head"][2] / 1.6, 1.56 / 1.80, delta=0.04)


if __name__ == "__main__":
    unittest.main()
