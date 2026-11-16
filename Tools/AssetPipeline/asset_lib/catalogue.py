"""Loads and validates config/assets.json, the palette and the prompt templates."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from . import CONFIG_DIR, PROMPTS_DIR, REPO_ROOT, SOURCE_ART_DIR, originality

CATALOGUE_PATH = CONFIG_DIR / "assets.json"
PALETTE_PATH = CONFIG_DIR / "palette.json"

# Content/Padel/<Category>/ in Unreal, SourceArt/<Category>/ on disk (CLAUDE.md layout).
CATEGORIES = ("Court", "Equipment", "Characters", "Venue")
KINDS = ("static", "skeletal")
SOURCES = ("procedural", "ai", "external")
BUILDERS = ("court_glass", "court_frame", "court_surface", "net", "ball", "racket", "character", "venue")
PROVIDERS = ("meshy", "tripo")
MASTERS = ("Opaque", "Turf", "Glass", "Masked", "Emissive", "Net", "NetSolid", "Metal", "WireMesh")
# Epic naming: SM_ static mesh, SK_ skeletal mesh.
PREFIX = {"static": "SM", "skeletal": "SK"}
_ID_RE = re.compile(r"^[A-Z][A-Za-z0-9]*$")


class CatalogueError(ValueError):
    pass


@dataclass
class Lod:
    ratio: float
    screen_size: float


@dataclass
class Asset:
    id: str
    kind: str
    category: str
    source: str
    budget_tris: int
    lods: list[Lod]
    raw: dict[str, Any] = field(repr=False)

    @property
    def enabled(self) -> bool:
        return bool(self.raw.get("enabled", True))

    @property
    def asset_name(self) -> str:
        return f"{PREFIX[self.kind]}_{self.id}"

    @property
    def fbx_path(self) -> Path:
        return SOURCE_ART_DIR / self.category / f"{self.asset_name}.fbx"

    def lod_fbx_path(self, index: int) -> Path:
        return SOURCE_ART_DIR / self.category / f"{self.asset_name}_LOD{index}.fbx"

    @property
    def unreal_folder(self) -> str:
        return f"/Game/Padel/{self.category}"

    @property
    def size(self) -> dict[str, float]:
        return dict(self.raw.get("size", {}))

    @property
    def nanite(self) -> bool:
        return self.kind == "static" and bool(self.raw.get("nanite", False))

    def repo_relative(self, path: Path) -> str:
        return path.relative_to(REPO_ROOT).as_posix()


@dataclass
class Catalogue:
    assets: list[Asset]

    def get(self, asset_id: str) -> Asset:
        for asset in self.assets:
            if asset.id == asset_id:
                return asset
        raise CatalogueError(f"no asset with id '{asset_id}' in {CATALOGUE_PATH.name}")


def _validate(entry: dict[str, Any], terms: list[str]) -> Asset:
    asset_id = entry.get("id", "")
    where = f"asset '{asset_id or '?'}'"
    if not _ID_RE.match(asset_id):
        raise CatalogueError(f"{where}: id must be PascalCase (it becomes SM_<Id> or SK_<Id>)")
    originality.check(asset_id, f"{where} id", terms)
    kind = entry.get("kind")
    if kind not in KINDS:
        raise CatalogueError(f"{where}: kind must be one of {KINDS}")
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
        raise CatalogueError(f"{where}: external assets need 'file' (a path under SourceArt/_incoming/)")
    budget = entry.get("budget_tris")
    if not isinstance(budget, int) or budget <= 0:
        raise CatalogueError(f"{where}: budget_tris must be a positive integer")
    lods: list[Lod] = []
    last_ratio, last_screen = 1.0, 1.0
    for lod in entry.get("lods", []):
        ratio, screen = float(lod["ratio"]), float(lod["screen_size"])
        if not 0.0 < ratio < last_ratio:
            raise CatalogueError(f"{where}: LOD ratios must fall below 1.0 and shrink with each LOD")
        if not 0.0 < screen < last_screen:
            raise CatalogueError(f"{where}: LOD screen sizes must fall below 1.0 and shrink with each LOD")
        lods.append(Lod(ratio, screen))
        last_ratio, last_screen = ratio, screen
    if lods and kind == "static" and entry.get("nanite"):
        raise CatalogueError(f"{where}: Nanite meshes don't need baked LODs")
    if kind == "skeletal" and "height_m" not in entry.get("size", {}):
        raise CatalogueError(f"{where}: skeletal characters need size.height_m")
    return Asset(asset_id, kind, category, source, budget, lods, entry)


def load(path: Path = CATALOGUE_PATH) -> Catalogue:
    data = json.loads(path.read_text(encoding="utf-8"))
    terms = originality.load_terms()
    assets = [_validate(entry, terms) for entry in data["assets"]]
    ids = [a.id for a in assets]
    if len(ids) != len(set(ids)):
        raise CatalogueError("asset ids must be unique")
    return Catalogue(assets)


def load_palette(path: Path = PALETTE_PATH) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    for group, materials in data.items():
        if group.startswith("_"):
            continue
        for name, settings in materials.items():
            if settings.get("master") not in MASTERS:
                raise CatalogueError(f"palette {group}.{name}: master must be one of {MASTERS}")
            if not re.fullmatch(r"#[0-9a-fA-F]{6}", settings.get("color", "")):
                raise CatalogueError(f"palette {group}.{name}: color must be #rrggbb")
    return data


def material_name(group: str, name: str) -> str:
    """Material slot name in the FBX, and the Unreal material instance it maps to."""
    pascal = "".join(part.capitalize() for part in group.split("_"))
    return f"MI_{pascal}_{name}"


def load_template(name: str) -> dict[str, Any]:
    return json.loads((PROMPTS_DIR / f"{name}.json").read_text(encoding="utf-8"))


def render_prompt(asset: Asset) -> tuple[str, str, dict[str, Any]]:
    """Returns (prompt, negative prompt, provider options) for an AI asset, after
    the originality check."""
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
