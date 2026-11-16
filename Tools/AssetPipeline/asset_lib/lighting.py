"""Reads Data/Lighting.json for the venue builders (decision 0009).

The game builds its floodlights from the same rows in C++
(Padel::Presentation::Lighting::BuildFixtures); this mirrors only the positions, so the
trusses and masts the Blender scripts build carry the fixtures the game lights. The
native test Lighting.Data.OneVenueOneRig makes sure every preset of a venue agrees.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from . import REPO_ROOT

LIGHTING_PATH = REPO_ROOT / "Data" / "Lighting.json"
VENUES = ("Indoor", "Outdoor")


def load(path: Path = LIGHTING_PATH) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def fixture_positions(rows: list[dict[str, Any]]) -> list[tuple[float, float, float]]:
    """Fixture positions in metres, in BuildFixtures() order (rows, -Y side first)."""
    out = []
    for row in rows:
        count = int(row["Count"])
        for side in (-1.0, 1.0):
            for i in range(count):
                t = i / (count - 1) if count > 1 else 0.5
                x = row["StartX"] + (row["EndX"] - row["StartX"]) * t
                out.append((x, side * row["OffsetY"], row["HeightM"]))
    return out


def venue_rigs(data: dict[str, Any] | None = None) -> dict[str, list[tuple[float, float, float, str]]]:
    """Fixture (x, y, z, role) per venue, from the first preset of each venue in the file.
    Role is "Court" (floodlights on the rig or masts) or "House" (lights for the stands)."""
    data = data or load()
    rigs: dict[str, list[tuple[float, float, float, str]]] = {}
    for preset in data["Presets"].values():
        venue = preset.get("Venue", "Indoor")
        if venue not in rigs:
            rigs[venue] = [(x, y, z, row.get("Role", "Court"))
                           for row in preset.get("FixtureRows", []) for x, y, z in fixture_positions([row])]
    return rigs


def fixtures(preset: dict[str, Any]) -> list[dict[str, Any]]:
    """Every fixture of a preset with its aim, intensity and cones (for the Blender
    preview), mirroring BuildFixtures(). Directions are unit vectors."""
    out = []
    for row in preset.get("FixtureRows", []):
        for x, y, z in fixture_positions([row]):
            side = 1.0 if y > 0 else -1.0
            aim = (x * row.get("AimXScale", 1.0), side * row.get("AimOwnSideM", 0.0), 0.0)
            d = (aim[0] - x, aim[1] - y, aim[2] - z)
            length = sum(c * c for c in d) ** 0.5
            out.append({"position": (x, y, z), "direction": tuple(c / length for c in d),
                        "intensity_cd": row["IntensityCd"], "inner_deg": row["InnerConeDeg"],
                        "outer_deg": row["OuterConeDeg"], "kelvin": row["TemperatureK"],
                        "lens_nits": row.get("LensLuminanceNits", 0.0)})
    return out


def preset(name: str | None = None, data: dict[str, Any] | None = None) -> tuple[str, dict[str, Any]]:
    """(name, preset) for a preset in Data/Lighting.json, the default when name is None."""
    data = data or load()
    name = name or data["Default"]
    if name not in data["Presets"]:
        raise KeyError(f"no lighting preset '{name}' in Data/Lighting.json (have {', '.join(data['Presets'])})")
    return name, data["Presets"][name]
