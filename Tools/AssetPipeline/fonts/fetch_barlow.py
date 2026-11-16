"""Download the Barlow menu fonts into SourceArt/Fonts and bake the tabular copies.

Barlow (Jeremy Tribby, SIL Open Font License 1.1, see SourceArt/Fonts/OFL.txt) is
fetched from the Google Fonts repository and checked against pinned SHA-256 hashes, so
every machine gets the same files. The .ttf files are tracked through Git LFS; run this
when they're missing (a clone without LFS, or a session that can't reach LFS). Without
them the menus fall back to the engine font.

Usage: python3 Tools/AssetPipeline/fonts/fetch_barlow.py   (the bake step needs fontTools)
"""
import hashlib
import subprocess
import sys
import urllib.request
from pathlib import Path

FONTS_DIR = Path(__file__).resolve().parents[3] / "SourceArt" / "Fonts"
BASE = "https://raw.githubusercontent.com/google/fonts/main/ofl"
SOURCES = {
    "Barlow-Medium.ttf": ("barlow", "f8906f762cb73dca441da034bc363b2d8e2e68bc10d5c05e58717646c20cc4b4"),
    "BarlowCondensed-SemiBold.ttf": ("barlowcondensed", "7b619d14bc2327509a9ef32b0890f709626f7ecc9ff61191c2a4314c5499d2d9"),
    "BarlowCondensed-Bold.ttf": ("barlowcondensed", "e476562ec9c1e16cf16475895b511f08c804f438cc9a9f80a44ea50a0eeb5b65"),
    "BarlowCondensed-ExtraBold.ttf": ("barlowcondensed", "724c9c25952d5f4a2d87185d9767aa006144c5f0d944dc05bf7d5d603551c260"),
    "BarlowCondensed-ExtraBoldItalic.ttf": ("barlowcondensed", "331783dae9ec0c398f4d32c9fd5a56d4c602caa0c1628c2b9cbfe93f48eb68c9"),
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    FONTS_DIR.mkdir(parents=True, exist_ok=True)
    for name, (family, expected) in SOURCES.items():
        target = FONTS_DIR / name
        if target.exists() and sha256(target) == expected:
            print(f"ok       {name}")
            continue
        with urllib.request.urlopen(f"{BASE}/{family}/{name}") as response:
            data = response.read()
        actual = hashlib.sha256(data).hexdigest()
        if actual != expected:
            sys.exit(f"{name}: hash {actual} doesn't match the pinned {expected}; upstream changed, check it before updating the pin")
        target.write_bytes(data)
        print(f"fetched  {name}")
    subprocess.run([sys.executable, str(Path(__file__).with_name("bake_tabular_figures.py"))], check=True)


if __name__ == "__main__":
    main()
