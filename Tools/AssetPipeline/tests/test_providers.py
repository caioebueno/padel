"""Meshy and Tripo clients against a fake HTTP session: no network, no credits."""

from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from asset_lib import providers
from asset_lib.providers import MeshyProvider, ProviderError, TripoProvider


class FakeResponse:
    def __init__(self, payload=None, status: int = 200, body: bytes = b"") -> None:
        self.payload, self.status_code, self.body = payload, status, body
        self.text = str(payload)

    def json(self):
        return self.payload

    def iter_content(self, chunk_size: int):
        yield self.body

    def __enter__(self):
        return self

    def __exit__(self, *exc) -> None:
        return None


class FakeSession:
    """Answers requests from a script of (method, url fragment) -> responses."""

    def __init__(self, routes: list[tuple[str, str, list]]) -> None:
        self.routes = routes
        self.calls: list[tuple[str, str, dict | None, dict]] = []

    def request(self, method, url, json=None, headers=None, timeout=None):
        self.calls.append((method, url, json, headers or {}))
        for m, fragment, queue in self.routes:
            if m == method and fragment in url and queue:
                return queue.pop(0) if len(queue) > 1 else queue[0]
        raise AssertionError(f"unexpected {method} {url}")

    def get(self, url, stream=False, timeout=None):
        self.calls.append(("DOWNLOAD", url, None, {}))
        return FakeResponse(body=b"glTF-bytes")


def no_sleep(_: float) -> None:
    return None


class MeshyTest(unittest.TestCase):
    def test_preview_refine_and_rig(self) -> None:
        session = FakeSession([
            ("POST", "/openapi/v2/text-to-3d", [FakeResponse({"result": "prev1"}), FakeResponse({"result": "ref1"})]),
            ("GET", "/text-to-3d/prev1", [FakeResponse({"status": "IN_PROGRESS"}), FakeResponse({"status": "SUCCEEDED"})]),
            ("GET", "/text-to-3d/ref1", [FakeResponse({"status": "SUCCEEDED", "model_urls": {"glb": "https://cdn/x.glb"}})]),
            ("POST", "/openapi/v1/rigging", [FakeResponse({"result": "rig1"})]),
            ("GET", "/rigging/rig1", [FakeResponse({"status": "SUCCEEDED", "result": {"rigged_character_glb_url": "https://cdn/r.glb"}})]),
        ])
        meshy = MeshyProvider("key", session=session, sleep=no_sleep)
        result = meshy.generate("a player", "logo", {"pose_mode": "t-pose"}, 40000)
        self.assertEqual(result.glb_url, "https://cdn/x.glb")
        preview = session.calls[0]
        self.assertEqual(preview[2]["mode"], "preview")
        self.assertEqual(preview[2]["target_polycount"], 40000)
        self.assertIn("Avoid: logo", preview[2]["prompt"])
        self.assertEqual(preview[3]["Authorization"], "Bearer key")
        rigged = meshy.rig(result, 1.82, "ue5_mannequin")
        self.assertEqual(rigged.glb_url, "https://cdn/r.glb")
        self.assertEqual(rigged.task_chain, ["prev1", "ref1", "rig1"])
        rig_call = next(c for c in session.calls if "rigging" in c[1] and c[0] == "POST")
        self.assertEqual(rig_call[2], {"input_task_id": "ref1", "height_meters": 1.82, "skeleton_template": "ue5_mannequin"})

    def test_failed_task_raises(self) -> None:
        session = FakeSession([
            ("POST", "/openapi/v2/text-to-3d", [FakeResponse({"result": "p"})]),
            ("GET", "/text-to-3d/p", [FakeResponse({"status": "FAILED", "task_error": {"message": "nope"}})]),
        ])
        with self.assertRaises(ProviderError):
            MeshyProvider("key", session=session, sleep=no_sleep).generate("x", "", {}, 1000)

    def test_gives_up_after_the_timeout(self) -> None:
        session = FakeSession([
            ("POST", "/openapi/v2/text-to-3d", [FakeResponse({"result": "p"})]),
            ("GET", "/text-to-3d/p", [FakeResponse({"status": "PENDING"})]),
        ])
        meshy = MeshyProvider("key", session=session, sleep=no_sleep)
        meshy.timeout_s = 30
        with self.assertRaises(ProviderError):
            meshy.generate("x", "", {}, 1000)

    def test_http_errors_raise_with_the_status(self) -> None:
        session = FakeSession([("POST", "/openapi/v2/text-to-3d", [FakeResponse({"message": "no credits"}, status=402)])])
        with self.assertRaisesRegex(ProviderError, "402"):
            MeshyProvider("key", session=session, sleep=no_sleep).generate("x", "", {}, 1000)


class TripoTest(unittest.TestCase):
    def test_generate_and_rig(self) -> None:
        session = FakeSession([
            ("POST", "/v3/generation/text-to-model", [FakeResponse({"code": 0, "data": {"task_id": "t1"}})]),
            ("GET", "/v3/tasks/t1", [FakeResponse({"code": 0, "data": {"status": "running"}}),
                                     FakeResponse({"code": 0, "data": {"status": "success", "output": {"model_url": "https://cdn/m.glb"}}})]),
            ("POST", "/v3/animations/rig", [FakeResponse({"code": 0, "data": {"task_id": "t2"}})]),
            ("GET", "/v3/tasks/t2", [FakeResponse({"code": 0, "data": {"status": "success", "output": {"model_url": "https://cdn/r.glb"}}})]),
        ])
        tripo = TripoProvider("key", session=session, sleep=no_sleep)
        result = tripo.generate("a bench", "logo", {"model": "v3"}, 2000)
        self.assertEqual(result.glb_url, "https://cdn/m.glb")
        self.assertEqual(session.calls[0][2], {"prompt": "a bench", "negative_prompt": "logo", "model": "v3"})
        rigged = tripo.rig(result, 1.8, "ignored")
        self.assertEqual(rigged.task_chain, ["t1", "t2"])

    def test_api_error_codes_raise(self) -> None:
        session = FakeSession([("POST", "/v3/generation/text-to-model", [FakeResponse({"code": 2010, "message": "bad"})])])
        with self.assertRaises(ProviderError):
            TripoProvider("key", session=session, sleep=no_sleep).generate("x", "", {}, 1000)


class FactoryTest(unittest.TestCase):
    def test_missing_key_explains_what_to_set(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(ProviderError, "MESHY_API_KEY"):
                providers.make("meshy")

    def test_download_writes_the_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            meshy = MeshyProvider("key", session=FakeSession([]), sleep=no_sleep)
            path = meshy.download("https://cdn/x.glb", Path(tmp) / "a" / "model.glb")
            self.assertEqual(path.read_bytes(), b"glTF-bytes")


if __name__ == "__main__":
    unittest.main()
