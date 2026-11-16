"""pipeline.run_editor must return when the editor exits, even if a child it
started (UnrealTraceServer on macOS) keeps running with the editor's stdout."""

from __future__ import annotations

import sys
import tempfile
import textwrap
import time
import unittest
from pathlib import Path

import pipeline

# A stand-in editor: starts a child that inherits stdout and sleeps, prints the
# import's last line, then exits at once.
FAKE_EDITOR = textwrap.dedent("""
    import subprocess, sys
    subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
    print("[padel-import] done (0 failure(s))", flush=True)
    sys.exit(0)
""")


class RunEditorTest(unittest.TestCase):
    def test_returns_when_the_editor_exits_even_if_a_child_keeps_stdout(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            script = Path(tmp) / "fake_editor.py"
            script.write_text(FAKE_EDITOR, encoding="utf-8")
            start = time.monotonic()
            code, out = pipeline.run_editor([sys.executable, str(script)], Path(tmp) / "Logs" / "import.log", timeout_s=20)
            self.assertLess(time.monotonic() - start, 10, "waited on the lingering child")
            self.assertEqual(code, 0)
            self.assertIn("[padel-import] done", out)

    def test_times_out_instead_of_hanging(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit):
                pipeline.run_editor([sys.executable, "-c", "import time; time.sleep(30)"], Path(tmp) / "l.log", timeout_s=1)


if __name__ == "__main__":
    unittest.main()
