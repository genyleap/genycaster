#!/usr/bin/env python3
"""Publish eight CC-BY-4.0 Twemoji reaction images for the Pro sticker pack.

Artwork: Twemoji contributors, https://github.com/jdecked/twemoji
Graphics license: https://creativecommons.org/licenses/by/4.0/
Original sources: repository tag v17.0.3, unchanged SVG geometry.
Rasterization is an adaptation (SVG -> transparent 512px PNG). Assets
and attribution remain available independent of the app's build.
"""
from __future__ import annotations
import hashlib
import os
import subprocess
import tempfile
import urllib.request
from pathlib import Path

ART = {
    "1f973": "Party",
    "1f929": "Starstruck",
    "1f680": "Rocket",
    "1f451": "Crown",
    "1f3c6": "Trophy",
    "1f496": "Sparkling Heart",
    "1f525": "Fire",
    "1f4af": "100",
}
TAG = "v17.0.3"
ORIGIN = f"https://raw.githubusercontent.com/jdecked/twemoji/{TAG}/assets/svg/"
OUT = Path("/var/www/genycaster.xyz/public/stickers/twemoji-pro-v1")

def publish() -> None:
    if OUT.is_symlink() or OUT.parent.is_symlink():
        raise ValueError("unsafe sticker destination")
    OUT.mkdir(parents=True, exist_ok=True)
    os.chmod(OUT, 0o755)
    os.chmod(OUT.parent, 0o755)
    for code, name in ART.items():
        request = urllib.request.Request(ORIGIN + code + ".svg",
                                         headers={"User-Agent": "Genycaster-sticker-publisher"})
        with urllib.request.urlopen(request, timeout=15) as response:
            svg = response.read(100_000)
        if not svg.startswith(b"<svg ") or b"<script" in svg.lower() or len(svg) > 60_000:
            raise ValueError("unexpected upstream SVG: " + code)
        with tempfile.TemporaryDirectory(prefix="twemoji-render-") as temporary:
            source = Path(temporary) / (code + ".svg")
            image = Path(temporary) / (code + ".png")
            source.write_bytes(svg)
            subprocess.run(["convert", "-background", "none", "-density", "240",
                str(source), "-resize", "512x512", "-depth", "8",
                "-define", "png:color-type=6", "-strip", str(image)],
                check=True, capture_output=True, timeout=20)
            data = image.read_bytes()
        if len(data) < 1000 or len(data) > 500_000 or not data.startswith(b"\x89PNG\r\n\x1a\n"):
            raise ValueError("invalid raster")
        target = OUT / (code + ".png")
        if target.is_symlink():
            raise ValueError("unsafe destination file")
        if not target.is_file() or target.read_bytes() != data:
            with tempfile.NamedTemporaryFile(dir=OUT, prefix=".temp-pro-", delete=False) as tmp:
                tmp.write(data)
                tmp.flush()
                os.fsync(tmp.fileno())
                staging = Path(tmp.name)
            os.chmod(staging, 0o644)
            os.replace(staging, target)
        print(code, name, hashlib.sha256(data).hexdigest(), len(data), flush=True)

if __name__ == "__main__":
    publish()
