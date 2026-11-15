"""AI 3D generation services. Each provider turns a prompt into a GLB URL and,
for characters, rigs the result. Keys come from the environment only."""

from __future__ import annotations

import os

from .base import Provider, ProviderError, GenerationResult
from .meshy import MeshyProvider
from .tripo import TripoProvider

ENV_KEYS = {"meshy": "MESHY_API_KEY", "tripo": "TRIPO_API_KEY"}


def make(name: str, session=None, sleep=None) -> Provider:
    env = ENV_KEYS[name]
    key = os.environ.get(env, "")
    if not key:
        raise ProviderError(f"{env} is not set. Add it to your shell (never commit it) to generate '{name}' assets.")
    cls = {"meshy": MeshyProvider, "tripo": TripoProvider}[name]
    return cls(key, session=session, sleep=sleep)


__all__ = ["Provider", "ProviderError", "GenerationResult", "MeshyProvider", "TripoProvider", "make", "ENV_KEYS"]
