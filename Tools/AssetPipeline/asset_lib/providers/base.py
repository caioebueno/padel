"""Shared HTTP and polling logic for the AI providers."""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

import requests


class ProviderError(RuntimeError):
    pass


@dataclass
class GenerationResult:
    provider: str
    task_id: str
    glb_url: str
    # Every task in the chain (preview, refine, rig), for the manifest.
    task_chain: list[str] = field(default_factory=list)


class Provider:
    name = "base"
    base_url = ""
    # Licence note written to the manifest. Both services give ownership to
    # paying accounts and CC BY 4.0 on free plans, so the plan must be checked.
    licence = ""
    poll_interval_s = 10.0
    timeout_s = 30 * 60.0

    def __init__(self, api_key: str, session: Any = None, sleep: Callable[[float], None] | None = None) -> None:
        self.api_key = api_key
        self.session = session or requests.Session()
        self.sleep = sleep or time.sleep

    def _headers(self) -> dict[str, str]:
        return {"Authorization": f"Bearer {self.api_key}", "Content-Type": "application/json"}

    def _request(self, method: str, path: str, body: dict[str, Any] | None = None) -> dict[str, Any]:
        url = path if path.startswith("http") else self.base_url + path
        response = self.session.request(method, url, json=body, headers=self._headers(), timeout=60)
        if response.status_code >= 400:
            raise ProviderError(f"{self.name} {method} {path} failed: HTTP {response.status_code} {response.text[:300]}")
        return response.json()

    def _poll(self, fetch: Callable[[], tuple[str, dict[str, Any]]], done: set[str], failed: set[str], what: str) -> dict[str, Any]:
        waited = 0.0
        while True:
            status, task = fetch()
            if status in done:
                return task
            if status in failed:
                raise ProviderError(f"{self.name} {what} ended with status {status}: {task}")
            if waited >= self.timeout_s:
                raise ProviderError(f"{self.name} {what} still {status} after {int(waited)} s")
            self.sleep(self.poll_interval_s)
            waited += self.poll_interval_s

    def download(self, url: str, dest: Path) -> Path:
        dest.parent.mkdir(parents=True, exist_ok=True)
        # Signed download URLs must not get our API key.
        with self.session.get(url, stream=True, timeout=300) as response:
            if response.status_code >= 400:
                raise ProviderError(f"{self.name} download failed: HTTP {response.status_code}")
            with dest.open("wb") as handle:
                for chunk in response.iter_content(chunk_size=1 << 20):
                    handle.write(chunk)
        return dest

    # Implemented by each provider.
    def generate(self, prompt: str, negative: str, options: dict[str, Any], target_tris: int) -> GenerationResult:
        raise NotImplementedError

    def rig(self, result: GenerationResult, height_m: float, skeleton: str) -> GenerationResult:
        raise NotImplementedError
