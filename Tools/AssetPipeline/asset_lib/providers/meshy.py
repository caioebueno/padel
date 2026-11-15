"""Meshy (https://docs.meshy.ai): text to 3D (preview, then textured refine) and auto-rigging.

Endpoints as documented in November 2026:
  POST /openapi/v2/text-to-3d        {"mode": "preview" | "refine", ...} -> {"result": task_id}
  GET  /openapi/v2/text-to-3d/{id}   status PENDING | IN_PROGRESS | SUCCEEDED | FAILED | CANCELED, model_urls.glb
  POST /openapi/v1/rigging           {"input_task_id", "height_meters", "skeleton_template"} -> {"result": task_id}
  GET  /openapi/v1/rigging/{id}      result.rigged_character_glb_url
"""

from __future__ import annotations

from typing import Any

from .base import GenerationResult, Provider, ProviderError

_DONE = {"SUCCEEDED"}
_FAILED = {"FAILED", "CANCELED", "EXPIRED"}
# Meshy prompts are capped at 800 characters.
_MAX_PROMPT = 800


class MeshyProvider(Provider):
    name = "meshy"
    base_url = "https://api.meshy.ai"
    licence = "Meshy output: owned by the account holder on a paid plan, CC BY 4.0 on the free plan (check the plan used)"

    def _text_task(self, task_id: str) -> tuple[str, dict[str, Any]]:
        task = self._request("GET", f"/openapi/v2/text-to-3d/{task_id}")
        return task.get("status", ""), task

    def generate(self, prompt: str, negative: str, options: dict[str, Any], target_tris: int) -> GenerationResult:
        # Meshy dropped negative_prompt, so the "avoid" list rides in the prompt itself.
        full = f"{prompt}. Avoid: {negative}" if negative else prompt
        if len(full) > _MAX_PROMPT:
            full = full[:_MAX_PROMPT]
        preview_body: dict[str, Any] = {
            "mode": "preview",
            "prompt": full,
            "ai_model": options.get("ai_model", "latest"),
            "should_remesh": options.get("should_remesh", True),
            "topology": options.get("topology", "triangle"),
            # Ask for roughly the budget; Blender cleanup enforces it exactly.
            "target_polycount": max(100, min(300000, target_tris)),
            "target_formats": ["glb"],
        }
        if "pose_mode" in options:
            preview_body["pose_mode"] = options["pose_mode"]
        preview_id = self._request("POST", "/openapi/v2/text-to-3d", preview_body)["result"]
        self._poll(lambda: self._text_task(preview_id), _DONE, _FAILED, f"preview {preview_id}")

        refine_body = {
            "mode": "refine",
            "preview_task_id": preview_id,
            "enable_pbr": options.get("enable_pbr", True),
            "target_formats": ["glb"],
        }
        refine_id = self._request("POST", "/openapi/v2/text-to-3d", refine_body)["result"]
        task = self._poll(lambda: self._text_task(refine_id), _DONE, _FAILED, f"refine {refine_id}")
        glb = (task.get("model_urls") or {}).get("glb", "")
        if not glb:
            raise ProviderError(f"meshy refine {refine_id} succeeded without a GLB url")
        return GenerationResult(self.name, refine_id, glb, [preview_id, refine_id])

    def rig(self, result: GenerationResult, height_m: float, skeleton: str) -> GenerationResult:
        body = {"input_task_id": result.task_id, "height_meters": height_m, "skeleton_template": skeleton}
        rig_id = self._request("POST", "/openapi/v1/rigging", body)["result"]

        def fetch() -> tuple[str, dict[str, Any]]:
            task = self._request("GET", f"/openapi/v1/rigging/{rig_id}")
            return task.get("status", ""), task

        task = self._poll(fetch, _DONE, _FAILED, f"rig {rig_id}")
        glb = (task.get("result") or {}).get("rigged_character_glb_url", "")
        if not glb:
            raise ProviderError(f"meshy rig {rig_id} succeeded without a rigged GLB url")
        return GenerationResult(self.name, rig_id, glb, result.task_chain + [rig_id])
