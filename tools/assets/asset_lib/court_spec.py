"""Reads the court measurements from scripts/engine/court_spec.gd.

CourtSpec is the single source of truth for the court (GDD 2). Rather than copy
its numbers into Python, this parses the `const NAME: float = <expr>` lines and
evaluates the arithmetic, so the art court and the collision court can't drift.
"""

from __future__ import annotations

import ast
import operator
import re
from pathlib import Path

from . import REPO_ROOT

COURT_SPEC_PATH = REPO_ROOT / "scripts" / "engine" / "court_spec.gd"

_CONST_RE = re.compile(r"^const\s+([A-Z][A-Z0-9_]*)\s*:\s*(?:float|int)\s*=\s*(.+?)\s*(?:#.*)?$")
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
            raise ValueError(f"court_spec.gd: unknown constant {node.id}")
        return names[node.id]
    if isinstance(node, ast.BinOp) and type(node.op) in _OPS:
        return _OPS[type(node.op)](_eval(node.left, names), _eval(node.right, names))
    if isinstance(node, ast.UnaryOp) and type(node.op) in _OPS:
        return _OPS[type(node.op)](_eval(node.operand, names))
    raise ValueError(f"court_spec.gd: unsupported expression {ast.dump(node)}")


def parse(text: str) -> dict[str, float]:
    """Returns every numeric constant in the GDScript source, in metres."""
    names: dict[str, float] = {}
    for line in text.splitlines():
        match = _CONST_RE.match(line.strip())
        if not match:
            continue
        name, expr = match.groups()
        names[name] = _eval(ast.parse(expr, mode="eval").body, names)
    return names


def load(path: Path = COURT_SPEC_PATH) -> dict[str, float]:
    spec = parse(path.read_text(encoding="utf-8"))
    for required in ("LENGTH", "WIDTH", "NET_HEIGHT_CENTRE", "NET_HEIGHT_POSTS", "BACK_GLASS_HEIGHT"):
        if required not in spec:
            raise ValueError(f"court_spec.gd is missing {required}")
    return spec
