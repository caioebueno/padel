"""Procedural textures: the mask texture for the knotted net (with its normal map). The
ball's textures are in ball_textures.py.

Pure Python (no numpy or Pillow) so the pipeline runs with only Blender and the
standard library. The images are small, so speed doesn't matter.
"""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path
from typing import Callable

from . import SOURCE_ART_DIR

TEXTURE_DIR = SOURCE_ART_DIR / "Textures"


def write_png(path: Path, width: int, height: int, channels: int, pixel: Callable[[int, int], tuple[int, ...]]) -> Path:
    """Writes an 8-bit grey (1 channel) or RGB (3 channel) PNG. Row 0 is the top."""
    colour_type = {1: 0, 3: 2}[channels]
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        for x in range(width):
            raw.extend(pixel(x, y))
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, colour_type, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)
    return path


def _grid_mask(size: int, wire_px: float) -> Callable[[int, int], tuple[int]]:
    # One cell per texture tile: wire along the left and bottom edges, soft 1 px edge.
    def pixel(x: int, y: int) -> tuple[int]:
        d = min(x + 0.5, size - x - 0.5, y + 0.5, size - y - 0.5)
        cover = min(max(wire_px / 2 - d + 0.5, 0.0), 1.0)
        return (int(round(255 * cover)),)
    return pixel


def _net_height(size: int, cord_px: float, knot_px: float) -> Callable[[float, float], float]:
    """Height field of one cell of knotted net, 0..1: a round cord along each tile
    edge (so tiles join seamlessly), twisted, with a knot where they cross."""
    r = cord_px / 2

    def cord(d: float, along: float) -> float:
        # Three-strand twist: the cord's edge ripples slightly along its length.
        twist = 1.0 + 0.12 * math.cos(2 * math.pi * along / (cord_px * 1.6))
        k = d / (r * twist)
        return math.sqrt(max(0.0, 1.0 - k * k))

    def height(x: float, y: float) -> float:
        dx = min(x, size - x)  # distance to the vertical cord on the tile edge
        dy = min(y, size - y)
        h = max(cord(dx, y), cord(dy, x))
        knot = math.hypot(dx, dy) / (knot_px / 2)
        if knot < 1.0:
            h = max(h, math.sqrt(1.0 - knot * knot) * 1.15)
        return min(h, 1.0)
    return height


def net_mask(size: int = 128, cord_px: float = 8.0, knot_px: float = 18.0) -> Callable[[int, int], tuple[int]]:
    """Opacity of the knotted net cell (4.5 cm, so 8 px of 128 is a 2.8 mm cord)."""
    height = _net_height(size, cord_px, knot_px)

    def pixel(x: int, y: int) -> tuple[int]:
        # 2x2 supersampling for a soft, alias-free edge.
        cover = sum(1.0 for sx in (0.25, 0.75) for sy in (0.25, 0.75) if height(x + sx, y + sy) > 0.0) / 4
        return (int(round(255 * cover)),)
    return pixel


def net_normal(size: int = 128, cord_px: float = 8.0, knot_px: float = 18.0) -> Callable[[int, int], tuple[int, int, int]]:
    """Tangent-space normal map from the same height field, so the cord looks round."""
    height = _net_height(size, cord_px, knot_px)
    strength = cord_px / 2

    def pixel(x: int, y: int) -> tuple[int, int, int]:
        cx, cy = x + 0.5, y + 0.5
        gx = (height(cx + 0.5, cy) - height(cx - 0.5, cy)) * strength
        # Row 0 is the top of the image and Unreal's V runs down, so +y is -V.
        gy = (height(cx, cy - 0.5) - height(cx, cy + 0.5)) * strength
        n = (-gx, -gy, 1.0)
        length = math.sqrt(sum(c * c for c in n))
        return tuple(int(round((c / length * 0.5 + 0.5) * 255)) for c in n)  # type: ignore[return-value]
    return pixel


def generate_all(palette: dict, out_dir: Path = TEXTURE_DIR) -> list[Path]:
    from . import metal_textures  # imported here: it builds on write_png above
    return [
        write_png(out_dir / "T_NetGrid_Mask.png", 128, 128, 1, net_mask()),
        write_png(out_dir / "T_NetGrid_Normal.png", 128, 128, 3, net_normal()),
        *metal_textures.generate_all(out_dir),
    ]
