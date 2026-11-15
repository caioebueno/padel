"""Reads the court measurements from Source/PadelCore/Public/CourtSpec.h.

CourtSpec.h is the single source of truth for the court (GDD 2). Rather than copy
its numbers into Python, this parses the `inline constexpr double Name = <expr>;`
lines and evaluates the arithmetic, so the art court and the collision court
can't drift apart.
"""

from __future__ import annotations

import ast
import operator
import re
from pathlib import Path

from . import REPO_ROOT

COURT_SPEC_PATH = REPO_ROOT / "Source" / "PadelCore" / "Public" / "CourtSpec.h"

_CONST_RE = re.compile(r"^inline\s+constexpr\s+(?:double|float|int)\s+([A-Z][A-Za-z0-9]*)\s*=\s*([^;]+);")
_OPS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
    ast.USub: operator.neg,
    ast.UAdd: operator.pos,
}


def _eval(node: ast.AST, names: dict[str, float]) -> float:
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return float(node.value)
    if isinstance(node, ast.Name):
        if node.id not in names:
            raise ValueError(f"CourtSpec.h: unknown constant {node.id}")
        return names[node.id]
    if isinstance(node, ast.BinOp) and type(node.op) in _OPS:
        return _OPS[type(node.op)](_eval(node.left, names), _eval(node.right, names))
    if isinstance(node, ast.UnaryOp) and type(node.op) in _OPS:
        return _OPS[type(node.op)](_eval(node.operand, names))
    raise ValueError(f"CourtSpec.h: unsupported expression {ast.dump(node)}")


def parse(text: str) -> dict[str, float]:
    """Returns every numeric constant in the header, in metres."""
    names: dict[str, float] = {}
    for line in text.splitlines():
        match = _CONST_RE.match(line.strip())
        if not match:
            continue
        name, expr = match.groups()
        names[name] = _eval(ast.parse(expr.strip(), mode="eval").body, names)
    return names


def load(path: Path = COURT_SPEC_PATH) -> dict[str, float]:
    spec = parse(path.read_text(encoding="utf-8"))
    for required in ("Length", "Width", "NetHeightCentre", "NetHeightPosts", "BackGlassHeight",
                     "FenceTotalHeight", "SideGlassStepStartX", "SideGlassEndX", "ExitNearX"):
        if required not in spec:
            raise ValueError(f"CourtSpec.h is missing {required}")
    return spec


def net_height_at(spec: dict[str, float], y: float) -> float:
    """Mirror of Padel::Court::NetHeightAt: the tape sags in a parabola."""
    t = min(max(abs(y) / spec["HalfWidth"], 0.0), 1.0)
    return spec["NetHeightCentre"] + (spec["NetHeightPosts"] - spec["NetHeightCentre"]) * t * t
