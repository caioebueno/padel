"""Video footwork foot plants (0012): floor contacts whatever the speed, releasing a plant the
leg can't reach, and the game's planted-slide rule in the report. Needs numpy and scipy."""

from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path

HAVE_SCIPY = importlib.util.find_spec("numpy") is not None and importlib.util.find_spec("scipy") is not None
MOCAP = Path(__file__).resolve().parents[1] / "mocap"
CONFIG = Path(__file__).resolve().parents[1] / "config" / "footwork.json"
FPS = 30.0
PLANTS = {"floor_enter_m": 0.06, "floor_exit_m": 0.08, "min_s": 0.13, "release_reach": 0.95,
          "swing_lift_m": 0.06, "swing_min_travel_m": 0.05}


@unittest.skipUnless(HAVE_SCIPY, "needs numpy and scipy")
class FootPlantsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        sys.path.insert(0, str(MOCAP))
        import wham_to_fbx
        cls.w = wham_to_fbx

    def test_config_has_the_foot_plants(self) -> None:
        cfg = json.loads(CONFIG.read_text())["foot_plants"]
        self.assertEqual({k for k in cfg if not k.startswith("_")}, set(PLANTS))
        self.assertLess(cfg["floor_enter_m"], cfg["floor_exit_m"])

    def test_a_foot_at_the_floor_plants_with_hysteresis(self) -> None:
        import numpy as np
        # Down at 0.04 m, wobbling to 0.07 (under exit, over enter) stays down, lifts at 0.12.
        h = np.array([0.15] * 5 + [0.04] * 6 + [0.07] * 3 + [0.04] * 4 + [0.12] * 5)
        c = self.w.floor_mask(h, FPS, PLANTS)
        self.assertEqual(c.tolist(), [False] * 5 + [True] * 13 + [False] * 5)

    def test_a_brush_of_the_floor_is_not_a_plant(self) -> None:
        import numpy as np
        # Two frames under the floor (0.07 s) is under min_s 0.13 s.
        h = np.array([0.15] * 5 + [0.04] * 2 + [0.15] * 5)
        self.assertFalse(self.w.floor_mask(h, FPS, PLANTS).any())

    def test_a_plant_the_leg_can_no_longer_reach_is_cut_back(self) -> None:
        import numpy as np
        # Hip moving 1 m/s over an ankle held still; leg 0.9 m long, 0.86 m reach, hip 0.75 m up.
        T = 40
        hip = np.stack([np.linspace(-0.6, 0.7, T), np.full(T, 0.75), np.zeros(T)], 1)
        ankle = np.zeros((T, 3))
        contact = np.ones(T, dtype=bool)
        c = self.w.release_overreach(contact, hip, ankle, 0.9, PLANTS)
        self.assertTrue(c.any())
        held = np.median(ankle[c], 0)
        self.assertTrue((np.linalg.norm(hip[c] - held, axis=1) <= 0.95 * 0.9 + 1e-9).all())
        # The cut is at both ends: the hip is far behind at the start and far ahead at the end.
        self.assertFalse(c[0])
        self.assertFalse(c[-1])

    def _skating_body(self, frames: int = 30):
        """SMPL rest pose, the left foot sliding 0.5 m forward along the floor (hip rotation)."""
        import numpy as np
        rng = np.random.default_rng(0)
        J = np.zeros((24, 3))
        # A rough SMPL skeleton (y up, metres): pelvis 0.95, hips 0.87, knees 0.5, ankles 0.1, balls 0.03.
        J[0] = [0, 0.95, 0]; J[1] = [0.09, 0.87, 0]; J[2] = [-0.09, 0.87, 0]
        J[4] = [0.09, 0.50, 0]; J[5] = [-0.09, 0.50, 0]; J[7] = [0.09, 0.10, 0]; J[8] = [-0.09, 0.10, 0]
        J[10] = [0.09, 0.03, 0.12]; J[11] = [-0.09, 0.03, 0.12]
        for j in range(24):
            if not J[j].any():
                J[j] = J[self.w.PARENTS[j]] + [0, 0.1, 0] + rng.normal(0, 1e-3, 3)
        from scipy.spatial.transform import Rotation as R
        q = np.tile(R.identity().as_quat(), (frames, 24, 1))
        swing = np.linspace(-0.3, 0.3, frames)             # the left hip swings the leg forward
        q[:, 1] = R.from_euler("x", -swing[:, None]).as_quat()
        trans = np.zeros((frames, 3))
        return q, trans, J

    def test_a_foot_stepping_along_the_floor_is_lifted(self) -> None:
        import numpy as np
        q, trans, J = self._skating_body()
        P0 = self.w.forward_kinematics(q, trans, J)
        contacts = {"L": np.zeros(len(q), dtype=bool), "R": np.ones(len(q), dtype=bool)}
        low = P0[:, [7, 10], 1].min(1)
        plants = dict(PLANTS, floor_exit_m=float(low.max()) + 0.01)
        q2 = self.w.lift_floor_steps(q.copy(), trans, J, contacts, 30.0, plants)
        P = self.w.forward_kinematics(q2, trans, J)
        lift = P[:, 7, 1] - P0[:, 7, 1]
        self.assertGreater(lift.max(), 0.03)                  # lifted mid-step
        self.assertLess(abs(lift[0]), 0.02)                   # and on the floor at both ends
        self.assertLess(abs(lift[-1]), 0.02)
        # The right foot, planted, is left alone.
        self.assertTrue(np.allclose(P[:, 8], P0[:, 8], atol=1e-6))

    def test_a_foot_still_on_the_floor_is_not_lifted(self) -> None:
        import numpy as np
        q, trans, J = self._skating_body()
        q[:, 1] = q[0, 1]                                     # no swing: the foot stays put
        P0 = self.w.forward_kinematics(q, trans, J)
        contacts = {"L": np.zeros(len(q), dtype=bool), "R": np.ones(len(q), dtype=bool)}
        plants = dict(PLANTS, floor_exit_m=1.0)
        P = self.w.forward_kinematics(self.w.lift_floor_steps(q.copy(), trans, J, contacts, 30.0, plants), trans, J)
        self.assertTrue(np.allclose(P[:, 7], P0[:, 7], atol=1e-6))

    def test_the_game_rule_sees_a_foot_gliding_low(self) -> None:
        import numpy as np
        # 24 joints; both feet at 0.03 m (the sole on the floor) gliding at 1 m/s for 1 s.
        T = 30
        P = np.zeros((T, 24, 3))
        for a, b in self.w.FOOT_JOINTS.values():
            P[:, a, 1] = 0.08
            P[:, b, 1] = 0.03
            P[:, b, 0] = np.linspace(0.0, 1.0 * (T - 1) / FPS, T)
        slide = self.w.game_planted_slide(P, FPS)
        self.assertAlmostEqual(slide, 1.0, places=2)
        # Held still, the same feet don't slide.
        P[:, :, 0] = 0.0
        self.assertAlmostEqual(self.w.game_planted_slide(P, FPS), 0.0, places=6)


if __name__ == "__main__":
    unittest.main()
