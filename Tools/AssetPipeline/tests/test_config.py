"""Catalogue, palette, prompts, CourtSpec parsing, originality and the manifest.
Standard library only: `python3 -m unittest discover -s Tools/AssetPipeline/tests -t Tools/AssetPipeline`."""

from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path

from asset_lib import REPO_ROOT, catalogue, court_spec, lighting, manifest, originality, textures


class CourtSpecTest(unittest.TestCase):
    def test_reads_the_regulation_court_from_the_header(self) -> None:
        spec = court_spec.load()
        self.assertEqual(spec["Length"], 20.0)
        self.assertEqual(spec["Width"], 10.0)
        self.assertEqual(spec["HalfLength"], 10.0)
        self.assertAlmostEqual(spec["ServiceLineX"], 6.95)
        self.assertEqual(spec["SideGlassStepStartX"], 8.0)
        self.assertEqual(spec["ExitNearX"], 4.0)

    def test_net_height_matches_gdd_2(self) -> None:
        spec = court_spec.load()
        self.assertAlmostEqual(court_spec.net_height_at(spec, 0.0), 0.88)
        self.assertAlmostEqual(court_spec.net_height_at(spec, 5.0), 0.92)
        self.assertAlmostEqual(court_spec.net_height_at(spec, -7.0), 0.92)

    def test_parser_evaluates_expressions_and_rejects_unknown_names(self) -> None:
        parsed = court_spec.parse("inline constexpr double A = 2.0;\n\tinline constexpr double B = A / 4.0 + 1.0;")
        self.assertEqual(parsed, {"A": 2.0, "B": 1.5})
        with self.assertRaises(ValueError):
            court_spec.parse("inline constexpr double C = Missing * 2.0;")


class CatalogueTest(unittest.TestCase):
    def setUp(self) -> None:
        self.data = json.loads(catalogue.CATALOGUE_PATH.read_text(encoding="utf-8"))

    def _load(self, data: dict) -> catalogue.Catalogue:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "assets.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            return catalogue.load(path)

    def _with(self, index: int, **changes) -> dict:
        data = copy.deepcopy(self.data)
        data["assets"][index].update(changes)
        return data

    def test_shipped_catalogue_is_valid(self) -> None:
        cat = catalogue.load()
        ids = [a.id for a in cat.assets]
        for required in ("CourtGlass", "CourtFrame", "CourtMesh", "CourtNet", "CourtSurface", "Ball", "RacketPlaceholder",
                         "PlayerPlaceholderA", "PlayerPlaceholderB"):
            self.assertIn(required, ids)

    def test_names_follow_unreal_conventions(self) -> None:
        cat = catalogue.load()
        self.assertEqual(cat.get("Ball").asset_name, "SM_Ball")
        self.assertEqual(cat.get("PlayerPlaceholderA").asset_name, "SK_PlayerPlaceholderA")
        self.assertEqual(cat.get("Ball").unreal_folder, "/Game/Padel/Equipment")
        self.assertEqual(cat.get("PlayerPlaceholderA").lod_fbx_path(1).name, "SK_PlayerPlaceholderA_LOD1.fbx")

    def test_ball_and_racket_sizes_match_gdd_2(self) -> None:
        cat = catalogue.load()
        self.assertEqual(cat.get("Ball").size["diameter_m"], 0.065)
        racket = cat.get("RacketPlaceholder").size
        self.assertLessEqual(racket["length_m"], 0.455)
        self.assertLessEqual(racket["width_m"], 0.26)

    def test_rejects_bad_ids_budgets_and_lods(self) -> None:
        index = next(i for i, a in enumerate(self.data["assets"]) if a["id"] == "PlayerPlaceholderA")
        bad = [
            self._with(index, id="player_a"),
            self._with(index, budget_tris=0),
            self._with(index, lods=[{"ratio": 0.2, "screen_size": 0.3}, {"ratio": 0.5, "screen_size": 0.1}]),
            self._with(index, lods=[{"ratio": 0.5, "screen_size": 0.1}, {"ratio": 0.2, "screen_size": 0.3}]),
            self._with(index, category="Props"),
            self._with(index, size={}),
        ]
        for data in bad:
            with self.assertRaises(catalogue.CatalogueError):
                self._load(data)

    def test_rejects_duplicate_ids(self) -> None:
        data = copy.deepcopy(self.data)
        data["assets"].append(copy.deepcopy(data["assets"][0]))
        with self.assertRaises(catalogue.CatalogueError):
            self._load(data)

    def test_ids_go_through_the_originality_guard(self) -> None:
        with self.assertRaises(originality.OriginalityError):
            self._load(self._with(0, id="BullpadelCourt"))

    def test_palette_is_valid_and_has_both_kits(self) -> None:
        palette = catalogue.load_palette()
        for group in ("court", "ball", "team_a", "team_b"):
            self.assertIn(group, palette)
        self.assertNotEqual(palette["team_a"]["Shirt"]["color"], palette["team_b"]["Shirt"]["color"])
        self.assertEqual(catalogue.material_name("team_a", "Shirt"), "MI_TeamA_Shirt")

    def test_venue_palette_is_physically_plausible(self) -> None:
        # Realistic lighting needs real albedos (decision 0009): nothing blacker than
        # charcoal or whiter than fresh paint, and only bare metal is metallic.
        venue = catalogue.load_palette()["venue"]
        for name, settings in venue.items():
            if settings["master"] == "Emissive":
                continue
            value = settings["color"].lstrip("#")
            linear = [(c / 255 / 12.92) if c / 255 <= 0.04045 else ((c / 255 + 0.055) / 1.055) ** 2.4
                      for c in (int(value[i:i + 2], 16) for i in (0, 2, 4))]
            luminance = 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]
            self.assertGreaterEqual(luminance, 0.015, name)
            self.assertLessEqual(luminance, 0.85, name)
            self.assertEqual(settings.get("metallic", 0.0) > 0.0, name == "Aluminium", name)

    def test_venue_rigs_come_from_the_lighting_presets(self) -> None:
        rigs = lighting.venue_rigs()
        spec = court_spec.load()
        self.assertEqual(set(rigs), {"Indoor", "Outdoor"})
        for venue, positions in rigs.items():
            self.assertTrue(positions, venue)
            # Mirrored across the court's width, so both sides are lit alike.
            self.assertEqual(sorted((x, -y, z, r) for x, y, z, r in positions), sorted(positions))
            for x, y, z, role in positions:
                self.assertIn(role, ("Court", "House"))
                self.assertGreater(abs(y), spec["HalfWidth"])
                if venue == "Indoor":
                    self.assertGreaterEqual(z, spec["CeilingHeight"])
        # The look-dev stage lists every venue mesh the catalogue builds.
        lookdev = json.loads((REPO_ROOT / "Data" / "LookDev.json").read_text(encoding="utf-8"))
        listed = {m.rsplit("/", 1)[-1] for part in lookdev["Parts"] for m in part["Meshes"]}
        built = {a.asset_name for a in catalogue.load().assets if a.category == "Venue" and a.raw.get("part") != "floodlight"}
        self.assertLessEqual(built, listed)
        data = lighting.load()
        self.assertTrue(data["FixtureMesh"].endswith("/" + catalogue.load().get("FloodlightHead").asset_name))

    def test_ai_prompts_render_and_ask_for_realism_without_branding(self) -> None:
        cat = catalogue.load()
        for asset in cat.assets:
            if asset.source != "ai":
                continue
            prompt, negative, _ = catalogue.render_prompt(asset)
            self.assertIn("Photorealistic", prompt)
            self.assertNotIn("{", prompt)
            self.assertIn("logo", negative)


class OriginalityTest(unittest.TestCase):
    def test_blocks_real_names_regardless_of_accents_and_case(self) -> None:
        terms = originality.load_terms()
        for text in ("a racket like the BULLPADEL one", "player who looks like Galán", "World Padel Tour court"):
            with self.assertRaises(originality.OriginalityError, msg=text):
                originality.check(text, "test", terms)

    def test_matches_whole_words_only(self) -> None:
        terms = originality.load_terms()
        # "pes" and "nox" are blocked; words that merely contain them are not.
        originality.check("a pair of espadrilles near the equinox", "test", terms)

    def test_negative_prompts_may_say_logo_but_not_brands(self) -> None:
        terms = originality.load_terms()
        originality.check("no logo, no watermark, no sponsor", "negative", terms, allow_in_negative=True)
        with self.assertRaises(originality.OriginalityError):
            originality.check("no adidas", "negative", terms, allow_in_negative=True)


class ManifestTest(unittest.TestCase):
    def test_upsert_replaces_by_path_and_keeps_other_rows(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.csv"
            manifest.upsert([{"path": "a.fbx", "source": "x", "licence": "l", "notes": "1"},
                             {"path": "font.ttf", "source": "hand", "licence": "OFL", "notes": ""}], path)
            manifest.upsert([{"path": "a.fbx", "source": "y", "licence": "l", "notes": "2, with comma"}], path)
            rows = manifest.read(path)
            self.assertEqual([r["path"] for r in rows], ["a.fbx", "font.ttf"])
            self.assertEqual(rows[0]["source"], "y")
            self.assertEqual(rows[0]["notes"], "2, with comma")
            self.assertEqual(rows[1]["licence"], "OFL")


class TexturesTest(unittest.TestCase):
    def test_writes_valid_pngs_of_the_right_size(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            paths = textures.generate_all(catalogue.load_palette(), Path(tmp))
            # The welded mesh needs no mask: its material draws the wires (unreal/metal_materials.py).
            self.assertEqual([p.name for p in paths], ["T_NetGrid_Mask.png", "T_NetGrid_Normal.png", "T_Ball_Felt.png", "T_PowderCoat_N.png"])
            for path in paths:
                data = path.read_bytes()
                self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
            felt = paths[2].read_bytes()
            self.assertEqual(int.from_bytes(felt[16:20], "big"), 512)
            self.assertEqual(int.from_bytes(felt[20:24], "big"), 256)
            normal = paths[3].read_bytes()
            self.assertEqual(int.from_bytes(normal[16:20], "big"), 256)

    def test_net_cell_is_open_with_a_knotted_cord_on_its_edges(self) -> None:
        size = 128
        mask = textures.net_mask(size)
        # A real net is mostly hole: the cord covers well under a third of the cell.
        cover = sum(mask(x, y)[0] for x in range(0, size, 2) for y in range(0, size, 2)) / (255 * (size // 2) ** 2)
        self.assertGreater(cover, 0.08)
        self.assertLess(cover, 0.3)
        self.assertEqual(mask(size // 2, size // 2), (0,))
        self.assertEqual(mask(0, size // 2), (255,))
        # The knot at the corner is thicker than the cord between knots.
        self.assertEqual(mask(4, 4), (255,))
        self.assertEqual(mask(5, size // 2), (0,))
        normal = textures.net_normal(size)
        self.assertEqual(normal(size // 2, size // 2), (128, 128, 255))
        # The cord's flank leans away from its centre line.
        self.assertGreater(normal(2, size // 2)[0], 140)
        self.assertLess(normal(size - 3, size // 2)[0], 116)


if __name__ == "__main__":
    unittest.main()
