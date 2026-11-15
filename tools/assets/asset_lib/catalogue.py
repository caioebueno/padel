"""Loads and validates config/assets.json and the prompt templates."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from . import CONFIG_DIR, PROMPTS_DIR, REPO_ROOT, originality

CATALOGUE_PATH = CONFIG_DIR / "assets.json"
PALETTE_PATH = CONFIG_DIR / "palette.json"

CATEGORIES = ("court", "props", "characters")
SOURCES = ("procedural", "ai", "external")
BUILDERS = ("court", "ball", "racket", "character")
PROVIDERS = ("meshy", "tripo")
# CLAUDE.md: snake_case file names. Ids become file and node names.
_ID_RE = re.compile(r"^[a-z][a-z0-9_]*$")


class CatalogueError(ValueError):
    pass


@dataclass
class Lod:
    ratio: float
    begin_m: float


@dataclass
class Asset:
    id: str
    category: str
    source: str
    out: str
    budget_tris: int
    lods: list[Lod]
    raw: dict[str, Any] = field(repr=False)

    @property
    def enabled(self) -> bool:
        return bool(self.raw.get("enabled", True))

    @property
    def out_path(self) -> Path:
        return REPO_ROOT / self.out

    @property
    def size(self) -> dict[str, float]:
        return dict(self.raw.get("size", {}))

    @property
    def is_character(self) -> bool:
        return self.category == "characters"


@dataclass
class Catalogue:
    assets: list[Asset]
    lod_hysteresis_m: float

    def get(self, asset_id: str) -> Asset:
        for asset in self.assets:
            if asset.id == asset_id:
                return asset
        raise CatalogueError(f"no asset with id '{asset_id}' in {CATALOGUE_PATH.name}")


def _validate(entry: dict[str, Any], terms: list[str]) -> Asset:
    asset_id = entry.get("id", "")
    where = f"asset '{asset_id or '?'}'"
    if not _ID_RE.match(asset_id):
        raise CatalogueError(f"{where}: id must be snake_case")
    originality.check(asset_id, f"{where} id", terms)
    category = entry.get("category")
    if category not in CATEGORIES:
        raise CatalogueError(f"{where}: category must be one of {CATEGORIES}")
    source = entry.get("source")
    if source not in SOURCES:
        raise CatalogueError(f"{where}: source must be one of {SOURCES}")
    if source == "procedural" and entry.get("builder") not in BUILDERS:
        raise CatalogueError(f"{where}: procedural assets need a builder from {BUILDERS}")
    if source == "ai":
        if entry.get("provider") not in PROVIDERS:
            raise CatalogueError(f"{where}: provider must be one of {PROVIDERS}")
        if not (PROMPTS_DIR / f"{entry.get('template', '')}.json").is_file():
            raise CatalogueError(f"{where}: no prompt template prompts/{entry.get('template')}.json")
    if source == "external" and not entry.get("file"):
        raise CatalogueError(f"{where}: external assets need 'file' (a path under assets/_incoming/)")
    out = entry.get("out", "")
    expected = f"assets/models/{category}/{asset_id}.glb"
    if out != expected:
        # Consistent names: the file is always assets/models/<category>/<id>.glb.
        raise CatalogueError(f"{where}: out must be '{expected}'")
    budget = entry.get("budget_tris")
    if not isinstance(budget, int) or budget <= 0:
        raise CatalogueError(f"{where}: budget_tris must be a positive integer")
    lods: list[Lod] = []
    last_begin = 0.0
    last_ratio = 1.0
    for lod in entry.get("lods", []):
        ratio = float(lod["ratio"])
        begin = float(lod["begin_m"])
        if not 0.0 < ratio < last_ratio:
            raise CatalogueError(f"{where}: LOD ratios must fall below 1.0 and shrink with each LOD")
        if begin <= last_begin:
            raise CatalogueError(f"{where}: LOD begin_m distances must grow with each LOD")
        lods.append(Lod(ratio, begin))
        last_begin, last_ratio = begin, ratio
    if category == "characters" and "height_m" not in entry.get("size", {}):
        raise CatalogueError(f"{where}: characters need size.height_m")
    return Asset(asset_id, category, source, out, budget, lods, entry)


def load(path: Path = CATALOGUE_PATH) -> Catalogue:
    data = json.loads(path.read_text(encoding="utf-8"))
    terms = originality.load_terms()
    assets = [_validate(entry, terms) for entry in data["assets"]]
    ids = [a.id for a in assets]
    if len(ids) != len(set(ids)):
        raise CatalogueError("asset ids must be unique")
    return Catalogue(assets, float(data.get("lod_hysteresis_m", 0.0)))


def load_palette(path: Path = PALETTE_PATH) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def load_template(name: str) -> dict[str, Any]:
    return json.loads((PROMPTS_DIR / f"{name}.json").read_text(encoding="utf-8"))


def render_prompt(asset: Asset) -> tuple[str, str, dict[str, Any]]:
    """Returns (prompt, negative prompt, provider options) for an AI asset,
    after the originality check."""
    template = load_template(asset.raw["template"])
    variables = asset.raw.get("vars", {})
    try:
        prompt = template["prompt"].format(**variables)
    except KeyError as missing:
        raise CatalogueError(f"asset '{asset.id}': template needs var {missing}") from None
    negative = template.get("negative", "")
    terms = originality.load_terms()
    originality.check(prompt, f"prompt for '{asset.id}'", terms)
    originality.check(negative, f"negative prompt for '{asset.id}'", terms, allow_in_negative=True)
    options = template.get("provider_options", {}).get(asset.raw["provider"], {})
    return prompt, negative, options
