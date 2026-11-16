"""Procedural textures: the mask texture for the knotted net (with its normal map), and the
ball's felt with its seam (the seam is what makes spin readable, GDD 10).

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


def _hex(color: str) -> tuple[int, int, int]:
    c = color.lstrip("#")
    return tuple(int(c[i:i + 2], 16) for i in (0, 2, 4))  # type: ignore[return-value]


def ball_felt(path: Path, felt: str, seam: str, width: int = 512, height: int = 256) -> Path:
    """Equirectangular felt texture with the classic two-lobe seam."""
    # Seam curve on the unit sphere: x = a cos t + b cos 3t, y = a sin t - b sin 3t,
    # z = 2 sqrt(ab) sin 2t, with a + b = 1 (it closes, and splits the sphere in two equal halves).
    a, b = 0.75, 0.25
    c = 2 * math.sqrt(a * b)
    curve = []
    for i in range(720):
        t = 2 * math.pi * i / 720
        p = (a * math.cos(t) + b * math.cos(3 * t), a * math.sin(t) - b * math.sin(3 * t), c * math.sin(2 * t))
        n = math.sqrt(sum(v * v for v in p))
        curve.append(tuple(v / n for v in p))
    felt_rgb, seam_rgb = _hex(felt), _hex(seam)
    half_width = 0.035  # radians, about 2 mm on a 65 mm ball

    def pixel(x: int, y: int) -> tuple[int, int, int]:
        lon = 2 * math.pi * (x + 0.5) / width
        lat = math.pi / 2 - math.pi * (y + 0.5) / height
        d = (math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat))
        best = max(d[0] * q[0] + d[1] * q[1] + d[2] * q[2] for q in curve)
        angle = math.acos(min(1.0, best))
        k = min(max((half_width - angle) / 0.01 + 0.5, 0.0), 1.0)
        return tuple(int(round(f + (s - f) * k)) for f, s in zip(felt_rgb, seam_rgb))  # type: ignore[return-value]

    return write_png(path, width, height, 3, pixel)


def generate_all(palette: dict, out_dir: Path = TEXTURE_DIR) -> list[Path]:
    from . import metal_textures  # imported here: it builds on write_png above
    felt = palette["ball"]["Felt"]
    return [
        write_png(out_dir / "T_NetGrid_Mask.png", 128, 128, 1, net_mask()),
        write_png(out_dir / "T_NetGrid_Normal.png", 128, 128, 3, net_normal()),
        ball_felt(out_dir / "T_Ball_Felt.png", felt["color"], felt.get("seam_color", "#ffffff")),
        *metal_textures.generate_all(out_dir),
    ]
