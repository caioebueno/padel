"""Tripo (https://developers.tripo3d.com), API v3: text to model and auto-rigging.

Endpoints as documented in November 2026:
  POST /v3/generation/text-to-model  {"prompt", "negative_prompt", "model"} -> {"code": 0, "data": {"task_id"}}
  POST /v3/animations/rig            {"input": {"task_id"}, "out_format": "glb"}
  GET  /v3/tasks/{id}                data.status queued | running | success | failed | cancelled, data.output.model_url
"""

from __future__ import annotations

from typing import Any

from .base import GenerationResult, Provider, ProviderError

_DONE = {"success"}
# Legacy statuses the docs say to treat as terminal.
_FAILED = {"failed", "cancelled", "banned", "expired"}


class TripoProvider(Provider):
    name = "tripo"
    base_url = "https://openapi.tripo3d.com"
    licence = "Tripo output: owned by the account holder on a paid plan, CC BY 4.0 on the free plan (check the plan used)"

    @staticmethod
    def _data(payload: dict[str, Any]) -> dict[str, Any]:
        if payload.get("code", 0) != 0:
            raise ProviderError(f"tripo error {payload.get('code')}: {payload.get('message', payload)}")
        return payload.get("data") or {}

    def _wait(self, task_id: str, what: str) -> dict[str, Any]:
        def fetch() -> tuple[str, dict[str, Any]]:
            data = self._data(self._request("GET", f"/v3/tasks/{task_id}"))
            return data.get("status", ""), data

        return self._poll(fetch, _DONE, _FAILED, f"{what} {task_id}")

    @staticmethod
    def _model_url(data: dict[str, Any], task_id: str) -> str:
        output = data.get("output") or {}
        url = output.get("model_url") or output.get("pbr_model") or output.get("model") or ""
        if not url:
            raise ProviderError(f"tripo task {task_id} succeeded without a model url")
        return url

    def generate(self, prompt: str, negative: str, options: dict[str, Any], target_tris: int) -> GenerationResult:
        body: dict[str, Any] = {"prompt": prompt}
        if negative:
            body["negative_prompt"] = negative
        if "model" in options:
            body["model"] = options["model"]
        task_id = self._data(self._request("POST", "/v3/generation/text-to-model", body))["task_id"]
        data = self._wait(task_id, "generation")
        return GenerationResult(self.name, task_id, self._model_url(data, task_id), [task_id])

    def rig(self, result: GenerationResult, height_m: float, skeleton: str) -> GenerationResult:
        # Tripo picks the skeleton itself; height is fixed later by Blender cleanup.
        body = {"input": {"task_id": result.task_id}, "out_format": "glb"}
        rig_id = self._data(self._request("POST", "/v3/animations/rig", body))["task_id"]
        data = self._wait(rig_id, "rig")
        return GenerationResult(self.name, rig_id, self._model_url(data, rig_id), result.task_chain + [rig_id])
