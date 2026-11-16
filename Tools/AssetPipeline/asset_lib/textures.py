"""Procedural textures: the mask texture for the net, and the
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
        write_png(out_dir / "T_NetGrid_Mask.png", 64, 64, 1, _grid_mask(64, 6.0)),
        ball_felt(out_dir / "T_Ball_Felt.png", felt["color"], felt.get("seam_color", "#ffffff")),
        *metal_textures.generate_all(out_dir),
    ]
