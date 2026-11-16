"""The powder-coat normal map for the court's steel (GDD 10).

Powder coat isn't mirror-flat: it has a fine "orange peel" ripple a few
millimetres across, which is what breaks up the reflections of the arena lights
along a post. This writes a small tiling normal map of it (overlapping soft bumps,
two sizes) with the standard library only, like textures.py.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from .textures import TEXTURE_DIR, write_png


def _bumps(size: int, count: int, r_min: float, r_max: float, rng: random.Random) -> list[list[float]]:
    """Tiling height field of overlapping soft round bumps, like cured powder coat."""
    height = [[0.0] * size for _ in range(size)]
    for _ in range(count):
        cx, cy = rng.random() * size, rng.random() * size
        r = r_min + (r_max - r_min) * rng.random()
        amp = 0.5 + rng.random()
        reach = int(math.ceil(r * 2))
        for dy in range(-reach, reach + 1):
            y = int(cy) + dy
            for dx in range(-reach, reach + 1):
                x = int(cx) + dx
                d2 = ((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2) / (r * r)
                if d2 < 4.0:
                    height[y % size][x % size] += amp * math.exp(-d2)
    return height


def powder_coat_normal(path: Path, size: int = 256, seed: int = 7, strength: float = 0.6) -> Path:
    rng = random.Random(seed)
    fine, coarse = _bumps(size, 900, 3.0, 6.0, rng), _bumps(size, 120, 10.0, 18.0, rng)
    height = [[0.7 * fine[y][x] + 0.3 * coarse[y][x] for x in range(size)] for y in range(size)]

    def pixel(x: int, y: int) -> tuple[int, int, int]:
        dx = (height[y][(x + 1) % size] - height[y][x - 1]) * strength
        dy = (height[(y + 1) % size][x] - height[y - 1][x]) * strength
        n = (-dx, -dy, 1.0)  # green points down the image (DirectX style), as Unreal expects
        length = math.sqrt(sum(c * c for c in n))
        return tuple(int(round((c / length * 0.5 + 0.5) * 255)) for c in n)  # type: ignore[return-value]

    return write_png(path, size, size, 3, pixel)


def generate_all(out_dir: Path = TEXTURE_DIR) -> list[Path]:
    return [powder_coat_normal(out_dir / "T_PowderCoat_N.png")]
