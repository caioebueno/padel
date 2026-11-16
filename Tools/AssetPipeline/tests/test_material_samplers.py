"""Material graph rules the Unreal scripts must follow. They run only inside the editor, so
these read their source. Breaking one makes a master fail to compile, and the engine then
draws its default checkerboard material in game (the court glass and the net did, on Metal)."""

from __future__ import annotations

import ast
import re
import unittest
from pathlib import Path

UNREAL_DIR = Path(__file__).resolve().parents[1] / "unreal"


class MaterialSamplerTest(unittest.TestCase):
    def test_no_srgb_grayscale_sampler(self) -> None:
        # Every *_Mask texture imports as TC_GRAYSCALE with sRGB off, which needs Linear Grayscale.
        for script in sorted(UNREAL_DIR.glob("*.py")):
            with self.subTest(script=script.name):
                self.assertIsNone(re.search(r"SAMPLERTYPE_GRAYSCALE\b", script.read_text(encoding="utf-8")))

    def test_masks_import_linear(self) -> None:
        source = (UNREAL_DIR / "import_assets.py").read_text(encoding="utf-8")
        block = source[source.index('if png.stem.endswith("_Mask")'):]
        block = block[:block.index("elif")]
        self.assertIn('"srgb", False', block)
        self.assertIn("TC_GRAYSCALE", block)

    def test_opaque_masters_allow_nanite(self) -> None:
        tree = ast.parse((UNREAL_DIR / "import_assets.py").read_text(encoding="utf-8"))
        masters = next(ast.literal_eval(node.value) for node in ast.walk(tree)
                       if isinstance(node, ast.Assign) and any(getattr(t, "id", "") == "NANITE_MASTERS" for t in node.targets))
        for master in ("M_PadelOpaque", "M_PadelTurf", "M_PadelMetal"):
            self.assertIn(master, masters)


if __name__ == "__main__":
    unittest.main()
