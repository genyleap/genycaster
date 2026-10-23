#!/usr/bin/env python3
"""Create a smaller full-Unicode JP/KR/SC Noto CJK collection.

Preserves the *entire cmap* of each of the three language faces, the original
outlines, shaping tables and correct regional glyph variants. Removes the five
unused mono faces and TC/HK faces from the release's unified TTC.
Requires fontTools (pinned by the release build environment).
"""
import argparse
import hashlib
from pathlib import Path
from tempfile import NamedTemporaryFile

SOURCE_SHA256 = "b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a"
# Produced from the pinned source by fontTools 4.67.0 with the three
# *unmodified* regional faces. Hash-cached between CMake configure runs.
EXPECTED_THREE_FACE_SHA256 = "ec6bdb78be316fd4741105ba4f824ff9da04a7cf17d77c0e9959aab45ecf2a6e"
FAMILIES = ("Noto Sans CJK JP", "Noto Sans CJK KR", "Noto Sans CJK SC")

def optimize(source: Path, destination: Path):
    from fontTools.ttLib import TTCollection

    if hashlib.sha256(source.read_bytes()).hexdigest() != SOURCE_SHA256:
        raise ValueError("Noto CJK source integrity does not match pinned upstream")
    if destination.is_file() and hashlib.sha256(destination.read_bytes()).hexdigest() == EXPECTED_THREE_FACE_SHA256:
        print("Using verified prebuilt three-region CJK font:", destination.stat().st_size, "bytes")
        return
    collection = TTCollection(str(source), lazy=True)
    if len(collection.fonts) != 10:
        raise ValueError("unexpected Noto CJK source face count")
    selected = collection.fonts[:3]
    original_cmaps = []
    for font, family in zip(selected, FAMILIES):
        names = {record.toUnicode() for record in font["name"].names if record.nameID == 1}
        if family not in names:
            raise ValueError(f"missing expected font face {family}")
        cmap = frozenset(font.getBestCmap())
        if len(cmap) < 44000:
            raise ValueError(f"incomplete Unicode coverage for {family}")
        original_cmaps.append(cmap)
    collection.fonts = selected
    destination.parent.mkdir(parents=True, exist_ok=True)
    with NamedTemporaryFile(suffix=".ttc", dir=destination.parent, delete=False) as handle:
        staging = Path(handle.name)
    try:
        collection.save(str(staging))
        verify = TTCollection(str(staging), lazy=True)
        if len(verify.fonts) != 3:
            raise ValueError("optimized collection has unexpected face count")
        for font, expected in zip(verify.fonts, original_cmaps):
            if frozenset(font.getBestCmap()) != expected:
                raise ValueError("optimized collection lost Unicode glyph coverage")
        if staging.stat().st_size >= source.stat().st_size:
            raise ValueError("optimized CJK TTC did not shrink")
        if destination.is_file() and hashlib.sha256(destination.read_bytes()).digest() == hashlib.sha256(staging.read_bytes()).digest():
            staging.unlink()
        else:
            staging.replace(destination)
        print("Noto CJK JP/KR/SC:", source.stat().st_size, "->",
              destination.stat().st_size, "bytes; full original cmap retained")
    finally:
        staging.unlink(missing_ok=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    options = parser.parse_args()
    optimize(options.source, options.destination)
