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

    FPS = 30.0
    WINDOWS = {"min_mps": 0.5, "min_s": 0.3, "gap_s": 0.2, "pad_s": 0.3, "max_s": 2.0, "min_window_s": 1.0}

    def _root(self, speeds_mps):
        """Root positions (y up) moving along x at each frame's speed."""
        import numpy as np
        x = np.concatenate([[0.0], np.cumsum(np.asarray(speeds_mps, dtype=float)[1:] / self.FPS)])
        return np.stack([x, np.zeros_like(x), np.zeros_like(x)], 1)

    def _speeds(self, *stretches):
        """Per-frame speeds from (seconds, m/s) stretches."""
        import numpy as np
        return np.concatenate([np.full(int(round(s * self.FPS)), v) for s, v in stretches])

    def test_config_has_the_labelled_windows(self) -> None:
        import json
        cfg = json.loads((MOCAP.parent / "config" / "footwork.json").read_text())["labelled_windows"]
        self.assertEqual({k for k in cfg if not k.startswith("_")}, set(self.WINDOWS))

    def test_a_labelled_clip_keeps_only_its_moving_window_padded(self) -> None:
        # 5 s cut around one move: 2 s standing, a 1 s shuffle, 2 s standing.
        speeds = self._speeds((2.0, 0.0), (1.0, 1.5), (2.0, 0.0))
        moves = self.moves.labelled_moves({"root_pos": self._root(speeds)}, "shuffle-03_1m40s_side", self.FPS, self.WINDOWS)
        self.assertEqual(len(moves), 1)
        s, e, label = moves[0]
        self.assertEqual(label, "shuffle")
        # The move (frames 60..90) plus 0.3 s either side, give or take the 0.1 s smoothing.
        self.assertAlmostEqual(s, 51, delta=3)
        self.assertAlmostEqual(e, 99, delta=3)

    def test_a_long_move_splits_into_equal_windows_under_the_cap(self) -> None:
        speeds = self._speeds((0.5, 0.0), (4.5, 1.2), (0.5, 0.0))
        windows = self.moves.active_windows(self._root(speeds), self.FPS, self.WINDOWS)
        self.assertGreaterEqual(len(windows), 3)
        lengths = [e - s for s, e in windows]
        self.assertLessEqual(max(lengths), 2.0 * self.FPS)
        self.assertLessEqual(max(lengths) - min(lengths), 1)
        # Contiguous: nothing of the move is lost between windows.
        self.assertTrue(all(a[1] == b[0] for a, b in zip(windows, windows[1:])))

    def test_brief_pauses_are_bridged_and_twitches_dropped(self) -> None:
        # A 0.1 s pause inside a move is bridged; a 0.1 s twitch alone is dropped.
        speeds = self._speeds((1.0, 0.0), (0.6, 1.0), (0.1, 0.0), (0.6, 1.0), (1.5, 0.0), (0.1, 2.0), (1.5, 0.0))
        windows = self.moves.active_windows(self._root(speeds), self.FPS, self.WINDOWS)
        self.assertEqual(len(windows), 1)
        self.assertLess(windows[0][1], int(3.0 * self.FPS))

    def test_a_clip_that_never_moves_gives_nothing(self) -> None:
        import numpy as np
        still = {"root_pos": np.zeros((150, 3))}
        self.assertEqual(self.moves.labelled_moves(still, "ready_stance-01_x", self.FPS, self.WINDOWS), [])
        self.assertEqual(self.moves.labelled_moves({"root_pos": np.zeros((3, 3))}, "lunge-01_x", self.FPS, self.WINDOWS), [])

    def _track(self, u: float, y: float, frames: int = 100, sway: float = 60.0, start: int = 0):
        """A WHAM-like track whose median image position is (u, y) in a 1920x1080 frame."""
        import numpy as np
        f = np.hypot(1920, 1080)
        z = 10.0
        us = u + np.linspace(-sway / 2, sway / 2, frames)
        trans = np.stack([(us - 960) * z / f, np.full(frames, (y - 540) * z / f), np.full(frames, z)], 1)
        return {"trans": trans, "frame_ids": np.arange(start, start + frames)}

    def test_default_box_matches_the_broadcast_camera(self) -> None:
        res = {0: self._track(600, 800), 1: self._track(1300, 800), 2: self._track(900, 300)}
        self.assertEqual(self.wham.pick_near_players(res, 100), {"L": 0, "R": 1})

    def test_a_looser_box_keeps_players_just_above_the_middle_and_by_the_glass(self) -> None:
        # Training-match cameras: near players at y = 530 and out at x = 1800.
        res = {0: self._track(700, 530), 1: self._track(1800, 700)}
        self.assertEqual(self.wham.pick_near_players(res, 100), {})
        self.assertEqual(self.wham.pick_near_players(res, 100, net_y=0.42, side_x=0.0), {"L": 0, "R": 1})

    def test_both_near_players_right_of_the_middle_are_both_kept(self) -> None:
        # Side camera: one partner big at 0.6 W, the other by the glass at 0.9 W.
        res = {0: self._track(1150, 700), 1: self._track(1750, 600, frames=90), 2: self._track(400, 500, frames=50)}
        self.assertEqual(self.wham.pick_near_players(res, 100, net_y=0.42, side_x=0.0), {"L": 0, "R": 1})

    def test_one_player_tracked_twice_is_not_a_pair(self) -> None:
        res = {0: self._track(900, 700, frames=60), 1: self._track(1000, 700, frames=40, start=60)}
        self.assertEqual(self.wham.pick_near_players(res, 100, net_y=0.42, side_x=0.0), {"L": 0})

    def test_still_or_brief_tracks_still_drop_out(self) -> None:
        res = {0: self._track(700, 700, sway=5.0), 1: self._track(1200, 700, frames=20)}
        self.assertEqual(self.wham.pick_near_players(res, 100, net_y=0.42, side_x=0.0), {})


if __name__ == "__main__":
    unittest.main()
