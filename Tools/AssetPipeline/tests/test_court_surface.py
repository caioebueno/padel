"""The playing surface (GDD 2, GDD 10): turf textures, tuning, palette and the
SM_CourtSurface mesh. The texture tests are standard library only; the mesh
test needs Blender (set BLENDER or BLENDER_PYTHON), like test_blender_builds.py."""

from __future__ import annotations

import copy
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from asset_lib import blender_runner, catalogue, court_spec, turf_textures

try:
    blender_runner.command(Path("x.py"), Path("job.json"))
    HAVE_BLENDER = True
except blender_runner.BlenderError:
    HAVE_BLENDER = False

TURF_KEYS = ("worn_color", "sand_color", "sand_amount", "worn_roughness", "sand_roughness", "blade_tint",
             "roll_tint", "wear_strength", "drift_strength", "normal_strength", "fibre_tile_m",
             "detail_fade_start_m", "detail_fade_end_m")


def small_config() -> dict:
    """The shipped tuning at a lower resolution, so the tests stay quick."""
    cfg = copy.deepcopy(turf_textures.load_config())
    cfg["fibre"]["texture_px"] = 128
    cfg["court_masks"]["texture_px"] = [256, 128]
    return cfg


def read_png(path: Path) -> tuple[int, int, int, bytes]:
    """Width, height, channels and raw pixels of an unfiltered 8-bit PNG from textures.write_png."""
    data = path.read_bytes()
    width, height, _, colour = struct.unpack(">IIBB", data[16:26])
    channels = {0: 1, 2: 3}[colour]
    pos, idat = 8, b""
    while pos < len(data):
        length = int.from_bytes(data[pos:pos + 4], "big")
        if data[pos + 4:pos + 8] == b"IDAT":
            idat += data[pos + 8:pos + 8 + length]
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = width * channels + 1
    pixels = b"".join(raw[r * stride + 1:(r + 1) * stride] for r in range(height))
    return width, height, channels, pixels


class SurfaceConfigTest(unittest.TestCase):
    def test_fibre_tile_is_a_whole_number_of_rows(self) -> None:
        # Otherwise the tufting rows break at the tile edge and the repeat shows.
        f = turf_textures.load_config()["fibre"]
        palette = catalogue.load_palette()["court"]
        tile_m = f["rows_per_tile"] * f["gauge_mm"] / 1000.0
        for name in ("Turf", "Line", "Runoff"):
            self.assertAlmostEqual(palette[name]["fibre_tile_m"], tile_m, places=6)
        self.assertLess(f["sand_level_mm"], f["pile_height_mm"])

    def test_turf_palette_entries_set_every_turf_parameter(self) -> None:
        palette = catalogue.load_palette()["court"]
        for name in ("Turf", "Line", "Runoff"):
            entry = palette[name]
            self.assertEqual(entry["master"], "Turf")
            for key in TURF_KEYS:
                self.assertIn(key, entry, f"court.{name}")
            self.assertLess(entry["detail_fade_start_m"], entry["detail_fade_end_m"])
        # GDD 10: the court is blue, and the lines are white and brighter than it.
        r, g, b = (int(palette["Turf"]["color"][i:i + 2], 16) for i in (1, 3, 5))
        self.assertGreater(b, max(r, g))
        self.assertGreater(sum(int(palette["Line"]["color"][i:i + 2], 16) for i in (1, 3, 5)), 3 * 200)

    def test_surface_is_in_the_catalogue_on_its_own(self) -> None:
        asset = catalogue.load().get("CourtSurface")
        self.assertEqual(asset.asset_name, "SM_CourtSurface")
        self.assertEqual(asset.unreal_folder, "/Game/Padel/Court")
        self.assertTrue(asset.nanite)


class TurfTexturesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name)
        cls.cfg = small_config()
        cls.spec = court_spec.load()
        cls.paths = turf_textures.generate_all(cls.spec, cls.out, cls.cfg)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    def test_writes_the_three_textures(self) -> None:
        self.assertEqual([p.name for p in self.paths], ["T_Turf_N.png", "T_Turf_Masks.png", "T_CourtSurface_Masks.png"])
        w, h, c, _ = read_png(self.paths[2])
        self.assertEqual((w, h, c), (256, 128, 3))

    def test_same_seed_same_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            again = turf_textures.generate_all(self.spec, Path(tmp), self.cfg)
            for a, b in zip(self.paths, again):
                self.assertEqual(a.read_bytes(), b.read_bytes(), a.name)

    def test_normal_map_tiles_without_a_seam(self) -> None:
        w, h, c, px = read_png(self.paths[0])

        def at(x: int, y: int) -> tuple[int, ...]:
            i = ((y % h) * w + (x % w)) * c
            return tuple(px[i:i + c])

        def step(pairs) -> float:
            return sum(sum(abs(p - q) for p, q in zip(at(*a), at(*b))) for a, b in pairs) / len(pairs)

        across_seam = step([((w - 1, y), (0, y)) for y in range(h)] + [((x, h - 1), (x, 0)) for x in range(w)])
        inside = step([((w // 2 - 1, y), (w // 2, y)) for y in range(h)] + [((x, h // 2 - 1), (x, h // 2)) for x in range(w)])
        self.assertLess(across_seam, inside * 1.5)
        # Mostly facing up: the average normal's blue channel is high.
        blue = sum(px[2::3]) / (w * h)
        self.assertGreater(blue, 200)

    def test_fibres_cover_most_of_the_sand(self) -> None:
        w, h, _, px = read_png(self.paths[1])
        cover = sum(px[0::3]) / (255 * w * h)
        self.assertGreater(cover, 0.4)
        self.assertLess(cover, 0.95)  # some sand must show (GDD 10)

    def test_wear_follows_where_players_stand(self) -> None:
        fields = turf_textures.court_fields(self.cfg, self.spec, int(self.cfg["seed"]))
        for sx in (-1, 1):
            for sy in (-1, 1):
                # Back defence and the volley spot wear more than the middle of the service box near the net.
                back = turf_textures.wear_at(fields, self.spec, sx * 8.3, sy * 2.5)
                volley = turf_textures.wear_at(fields, self.spec, sx * 2.8, sy * 2.4)
                quiet = turf_textures.wear_at(fields, self.spec, sx * 0.6, sy * 4.4)
                self.assertGreater(back, 0.3)
                self.assertGreater(volley, quiet + 0.1)
                self.assertGreater(back, quiet + 0.2)

    def test_sand_drifts_against_the_walls_not_in_the_middle(self) -> None:
        fields = turf_textures.court_fields(self.cfg, self.spec, int(self.cfg["seed"]))
        w, h = fields["size"]

        def drift(x: float, y: float) -> float:
            px = min(w - 1, int((x + 10.0) / fields["metres_per_px"]))
            py = min(h - 1, int((5.0 - y) / fields["metres_per_px"]))
            return fields["drift"][py * w + px]

        self.assertGreater(drift(9.98, 4.98), 0.6)  # back corner
        self.assertGreater(drift(5.0, 4.98), drift(5.0, 2.5) + 0.3)  # side wall vs open court
        self.assertLess(drift(5.0, 2.5), 0.1)


@unittest.skipUnless(HAVE_BLENDER, "Blender not available (set BLENDER or BLENDER_PYTHON)")
class CourtSurfaceMeshTest(unittest.TestCase):
    def test_surface_matches_courtspec(self) -> None:
        spec = court_spec.load()
        asset = catalogue.load().get("CourtSurface")
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "SM_CourtSurface.fbx"
            job = {"id": asset.asset_name, "spec": spec, "palette": catalogue.load_palette(),
                   "surface": turf_textures.load_config(), "budget_tris": asset.budget_tris, "out": str(out)}
            report = blender_runner.run("build_court_surface", job)
            fbx = blender_runner.run("inspect_fbx", {"file": str(out)})
        self.assertLessEqual(report["triangles"], asset.budget_tris)
        self.assertEqual(fbx["materials"], ["MI_Court_Line", "MI_Court_Runoff", "MI_Court_Turf"])
        self.assertEqual(fbx["uv_layers"], ["CourtUV", "UVMap"])
        # Run-off reaches the cage's outer floor (CourtSpec RunoffBack/RunoffSide).
        self.assertAlmostEqual(fbx["bounds_max_m"][0], spec["HalfLength"] + spec["RunoffBack"], delta=0.01)
        self.assertAlmostEqual(fbx["bounds_min_m"][1], -(spec["HalfWidth"] + spec["RunoffSide"]), delta=0.01)
        # The ball bounces on Z = 0: the visible surface stays within a few millimetres of it.
        self.assertLess(fbx["bounds_max_m"][2], 0.004)
        self.assertAlmostEqual(fbx["bounds_min_m"][2], -turf_textures.load_config()["mesh"]["slab_thickness"], delta=0.001)


if __name__ == "__main__":
    unittest.main()
