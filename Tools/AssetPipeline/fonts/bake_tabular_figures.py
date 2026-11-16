"""Bake tabular figures into copies of the Barlow number fonts (menu spec 4.2).

Slate shapes text with the font's default features only, so it can't switch on the
OpenType `tnum` feature, and Barlow's default digits are proportional: scores and
stat columns would wobble as digits change. This writes a copy of each font whose
digits map straight to the `tnum` glyphs, named "<Family> Tabular" (Barlow is OFL 1.1
with no Reserved Font Name, so a modified copy may be shipped).

Usage: python3 Tools/AssetPipeline/fonts/bake_tabular_figures.py   (needs fontTools)
"""
from pathlib import Path

from fontTools.ttLib import TTFont

FONTS_DIR = Path(__file__).resolve().parents[3] / "SourceArt" / "Fonts"
SOURCES = ["BarlowCondensed-Bold.ttf", "BarlowCondensed-ExtraBold.ttf"]
DIGITS = "0123456789"


def tnum_map(font):
    """Glyph name -> tabular glyph name, from the font's own tnum single substitutions."""
    gsub = font["GSUB"].table
    mapping = {}
    for record in gsub.FeatureList.FeatureRecord:
        if record.FeatureTag != "tnum":
            continue
        for index in record.Feature.LookupListIndex:
            for sub in gsub.LookupList.Lookup[index].SubTable:
                mapping.update(getattr(sub, "mapping", {}) or {})
    return mapping


def bake(source):
    font = TTFont(FONTS_DIR / source)
    mapping = tnum_map(font)
    for table in font["cmap"].tables:
        if not table.isUnicode():
            continue
        for char in DIGITS:
            glyph = table.cmap.get(ord(char))
            if glyph in mapping:
                table.cmap[ord(char)] = mapping[glyph]
    for record in font["name"].names:
        if record.nameID in (1, 3, 4, 6, 16):
            text = record.toUnicode()
            record.string = text.replace("Barlow Condensed", "Barlow Condensed Tabular").replace(
                "BarlowCondensed", "BarlowCondensedTabular")
    target = FONTS_DIR / source.replace("BarlowCondensed-", "BarlowCondensedTabular-")
    font.save(target)
    advances = {font["hmtx"][font.getBestCmap()[ord(c)]][0] for c in DIGITS}
    assert len(advances) == 1, f"{target.name}: digits still differ in width {advances}"
    print(f"wrote {target.relative_to(FONTS_DIR.parents[1])}")


if __name__ == "__main__":
    for name in SOURCES:
        bake(name)
