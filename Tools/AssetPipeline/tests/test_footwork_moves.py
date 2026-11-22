"""Video footwork: move labels from cut clip names and the near-player picker (0012).
Needs numpy and scipy; skipped without them."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

HAVE_SCIPY = importlib.util.find_spec("numpy") is not None and importlib.util.find_spec("scipy") is not None
MOCAP = Path(__file__).resolve().parents[1] / "mocap"


@unittest.skipUnless(HAVE_SCIPY, "needs numpy and scipy")
class FootworkMovesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        sys.path.insert(0, str(MOCAP))
        import footwork_moves
        import wham_to_fbx
        cls.moves, cls.wham = footwork_moves, wham_to_fbx

    def test_label_comes_from_the_front_of_a_cut_clip_name(self) -> None:
        self.assertEqual(self.moves.label_from_clip("lunge-01_6m49s_side"), "lunge")
        self.assertEqual(self.moves.label_from_clip("split_step-04_7m46s_end"), "split_step")
        self.assertEqual(self.moves.label_from_clip("volley_back-02_3m36s_side"), "volley_back")
        self.assertEqual(self.moves.label_from_clip("rally_07"), "move")

    def test_labelled_moves_keep_the_whole_track(self) -> None:
        import numpy as np
        rig = {"root_pos": np.zeros((40, 3))}
        self.assertEqual(self.moves.labelled_moves(rig, "crossover-03_1m40s_side"), [(0, 40, "crossover")])
        self.assertEqual(self.moves.labelled_moves({"root_pos": np.zeros((3, 3))}, "lunge-01_x"), [])

    def _track(self, u: float, y: float, frames: int = 100, sway: float = 60.0):
        """A WHAM-like track whose median image position is (u, y) in a 1920x1080 frame."""
        import numpy as np
        f = np.hypot(1920, 1080)
        z = 10.0
        us = u + np.linspace(-sway / 2, sway / 2, frames)
        trans = np.stack([(us - 960) * z / f, np.full(frames, (y - 540) * z / f), np.full(frames, z)], 1)
        return {"trans": trans, "frame_ids": np.arange(frames)}

    def test_default_box_matches_the_broadcast_camera(self) -> None:
        res = {0: self._track(600, 800), 1: self._track(1300, 800), 2: self._track(900, 300)}
        self.assertEqual(self.wham.pick_near_players(res, 100), {"L": 0, "R": 1})

    def test_a_looser_box_keeps_players_just_above_the_middle_and_by_the_glass(self) -> None:
        # Training-match cameras: near players at y = 530 and out at x = 1800.
        res = {0: self._track(700, 530), 1: self._track(1800, 700)}
        self.assertEqual(self.wham.pick_near_players(res, 100), {})
        self.assertEqual(self.wham.pick_near_players(res, 100, net_y=0.4, side_x=0.03), {"L": 0, "R": 1})

    def test_still_or_brief_tracks_still_drop_out(self) -> None:
        res = {0: self._track(700, 700, sway=5.0), 1: self._track(1200, 700, frames=20)}
        self.assertEqual(self.wham.pick_near_players(res, 100, net_y=0.4, side_x=0.03), {})


if __name__ == "__main__":
    unittest.main()
