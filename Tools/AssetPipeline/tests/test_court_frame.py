"""The court's steel structure layout (asset_lib/court_frame_layout.py) against
CourtSpec.h (GDD 2) and the glass fixings. Standard library only, no Blender."""

from __future__ import annotations

import unittest

from asset_lib import court_frame_layout as layout, court_spec


def side_surface(spec: dict, x: float, z: float) -> str:
    """Python mirror of Padel::Court::SideWallSurfaceAt."""
    ax = abs(x)
    if ax > spec["HalfLength"] or z < 0 or spec["ExitNearX"] < ax < spec["ExitFarX"]:
        return "none"
    glass = (spec["SideGlassFullHeight"] if ax >= spec["SideGlassStepStartX"]
             else spec["SideGlassStepHeight"] if ax >= spec["SideGlassEndX"] else 0.0)
    if z < glass:
        return "glass"
    top = spec["FenceTotalHeight"] if ax >= spec["SideGlassStepStartX"] else spec["SideMeshHeight"]
    return "mesh" if z < top else "none"


def back_surface(spec: dict, z: float) -> str:
    """Python mirror of Padel::Court::BackWallSurfaceAt."""
    if z < 0:
        return "none"
    if z < spec["BackGlassHeight"]:
        return "glass"
    return "mesh" if z < spec["FenceTotalHeight"] else "none"


def inside(outline: list, x: float, y: float) -> bool:
    hit = False
    for (x0, y0), (x1, y1) in zip(outline, outline[1:] + outline[:1]):
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            hit = not hit
    return hit


def frange(a: float, b: float, step: float) -> list[float]:
    """a, a + step, ... up to b, never past it."""
    n = int((b - a) / step + 1e-9)
    return [a + i * step for i in range(n + 1)]


class CourtFrameLayoutTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.spec = court_spec.load()
        cls.cfg = layout.load_config()
        cls.lay = layout.build(cls.spec, cls.cfg)
        cls.hl, cls.hw = cls.spec["HalfLength"], cls.spec["HalfWidth"]

    def panels_on(self, sx: int, sy: int) -> list:
        """Panels of one wall: sy == 0 is the back wall of half sx."""
        out = []
        for p in self.lay.panels:
            if sy == 0 and p.out == (float(sx), 0.0, 0.0):
                out.append(p)
            elif sy and p.out == (0.0, float(sy), 0.0) and p.along == (float(sx), 0.0, 0.0):
                out.append(p)
        return out

    def covered(self, panels: list, a: float, z: float, axis: int) -> bool:
        for p in panels:
            s = (a - p.origin[axis]) * p.along[axis]
            if 0 <= s <= p.length and p.origin[2] <= z <= p.origin[2] + p.height:
                return True
        return False

    def test_mesh_panels_cover_exactly_the_mesh_surfaces_of_courtspec(self) -> None:
        zs = frange(0.013, 4.2, 0.1)
        for sx in (-1, 1):
            back = self.panels_on(sx, 0)
            for y in frange(-4.987, 4.987, 0.1):
                for z in zs:
                    self.assertEqual(self.covered(back, y, z, 1), back_surface(self.spec, z) == "mesh", (sx, y, z))
            for sy in (-1, 1):
                side = self.panels_on(sx, sy)
                for ax in frange(0.013, 9.987, 0.1):
                    for z in zs:
                        expected = side_surface(self.spec, sx * ax, z) == "mesh"
                        self.assertEqual(self.covered(side, sx * ax, z, 0), expected, (sx, sy, ax, z))

    def test_mesh_panels_sit_in_the_wall_plane_and_are_at_most_two_metres(self) -> None:
        self.assertEqual(len(self.lay.panels), 26)
        self.assertAlmostEqual(sum(p.length * p.height for p in self.lay.panels), 84.0)
        for p in self.lay.panels:
            self.assertLessEqual(p.length, self.cfg["panel_frame"]["max_panel_length_m"] + 1e-9)
            normal_axis = 0 if p.out[0] else 1
            limit = self.hl if normal_axis == 0 else self.hw
            self.assertAlmostEqual(abs(p.origin[normal_axis]), limit)

    def test_posts_back_every_glass_bolt(self) -> None:
        # The glass agent's fixings: 35 mm in from each panel edge (4 mm half joint),
        # through 12 mm glass and a 3 mm gasket, into the post.
        bolt = 0.035 + 0.004
        face = self.hl + self.lay.post_face_offset + 0.01
        side_face = self.hw + self.lay.post_face_offset + 0.01
        outlines = [p.outline for p in self.lay.posts]
        for sx in (-1, 1):
            for y in frange(-self.hw, self.hw, 2.0):
                for b in (-bolt, bolt):
                    if abs(y + b) > self.hw:
                        continue
                    self.assertTrue(any(inside(o, sx * face, y + b) for o in outlines), (sx, y + b))
            for sy in (-1, 1):
                for x in (self.hl, self.spec["SideGlassStepStartX"], self.spec["SideGlassEndX"]):
                    for b in (-bolt, bolt):
                        if x + b > self.hl:
                            continue
                        self.assertTrue(any(inside(o, sx * (x + b), sy * side_face) for o in outlines), (sx, sy, x + b))

    def test_post_section_matches_the_glass_fixings_contract(self) -> None:
        self.assertGreaterEqual(self.cfg["post"]["across_m"], 0.1)
        self.assertAlmostEqual(self.cfg["post"]["depth_m"], 0.05)
        self.assertAlmostEqual(self.lay.post_face_offset, 0.015)

    def test_no_steel_inside_the_court(self) -> None:
        lim_x, lim_y = self.hl + self.lay.post_face_offset, self.hw + self.lay.post_face_offset
        shapes = [p.outline for p in self.lay.posts]
        shapes += [[(b.lo[0], b.lo[1]), (b.hi[0], b.lo[1]), (b.hi[0], b.hi[1]), (b.lo[0], b.hi[1])] for b in self.lay.plates]
        for outline in shapes:
            xs, ys = [p[0] for p in outline], [p[1] for p in outline]
            for x in frange(min(xs), max(xs), (max(xs) - min(xs)) / 20):
                for y in frange(min(ys), max(ys), (max(ys) - min(ys)) / 20):
                    if inside(outline, x, y):
                        self.assertTrue(abs(x) >= lim_x - 1e-6 or abs(y) >= lim_y - 1e-6, (x, y))
        for f in self.lay.fasteners:
            if f.kind == "anchor":
                self.assertTrue(abs(f.base[0]) > lim_x or abs(f.base[1]) > lim_y, f.name)

    def test_exits_are_clear(self) -> None:
        # Nothing stands in the opening except the half-posts on the glass and mesh joints.
        half = self.cfg["post"]["across_m"] / 2
        shapes = [p.outline for p in self.lay.posts]
        for sx in (-1, 1):
            for sy in (-1, 1):
                for ax in frange(self.spec["ExitNearX"] + half + 0.001, self.spec["ExitFarX"] - half - 0.001, 0.05):
                    for ay in frange(self.hw, self.hw + 0.3, 0.01):
                        self.assertFalse(any(inside(o, sx * ax, sy * ay) for o in shapes), (sx * ax, sy * ay))
                for p in self.panels_on(sx, sy):
                    a0 = p.origin[0] * sx
                    self.assertTrue(a0 + p.length <= self.spec["ExitNearX"] + 1e-9 or a0 >= self.spec["ExitFarX"] - 1e-9)

    def test_every_panel_edge_has_a_post_reaching_its_top(self) -> None:
        face = self.lay.post_face_offset + 0.01
        for p in self.lay.panels:
            top = p.origin[2] + p.height
            for s in (0.0, p.length):
                edge = [o + s * a + face * n for o, a, n in zip(p.origin, p.along, p.out)]
                backers = [q for q in self.lay.posts if inside(q.outline, edge[0], edge[1])]
                self.assertTrue(backers, f"{p.name} edge {s}")
                self.assertGreaterEqual(max(q.z1 for q in backers), top - 1e-9, p.name)

    def test_layout_is_symmetric(self) -> None:
        def key(points):
            return sorted((round(x, 6), round(y, 6)) for x, y in points)
        all_posts = {tuple(key(p.outline)) for p in self.lay.posts}
        for p in self.lay.posts:
            for mx, my in ((-1, 1), (1, -1), (-1, -1)):
                self.assertIn(tuple(key([(mx * x, my * y) for x, y in p.outline])), all_posts, p.name)
        self.assertEqual(len(self.lay.posts), 30)


if __name__ == "__main__":
    unittest.main()
