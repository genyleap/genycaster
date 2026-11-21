#!/usr/bin/env python3
"""Render cache-busted GenyCaster 1.0 Beta social preview cards.

Cards use the public GenyCaster mark and semantic native theme colors. Keep
both 1200x630 PNGs in ui/wasm so every future WASM package retains them.
No personal/user data and no remote image dependencies.
"""
from __future__ import annotations

import argparse
import os
import re
from pathlib import Path
import struct
import subprocess
import tempfile
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "ui/wasm"
CARDS = {
    "og-genycaster-home-v2": {
        "headline": "GenyCaster",
        "second": "Farcaster + HyperSnap",
        "detail": "One local-first social client. Web, desktop and Android.",
        "footer": "WINDOWS  /  MACOS  /  LINUX  /  ANDROID  /  WEB",
    },
    "og-genycaster-download-v2": {
        "headline": "Download GenyCaster",
        "second": "Choose your platform.",
        "detail": "Official v1.0.0 Beta installers for desktop and Android.",
        "footer": "WINDOWS  /  MACOS  /  LINUX  /  ANDROID",
    },
}
ICON_PATHS = [
    ('#101010', 'M 158 56 H 354 C 410.33 56 456 101.67 456 158 V 354 C 456 410.33 410.33 456 354 456 H 158 C 101.67 456 56 410.33 56 354 V 158 C 56 101.67 101.67 56 158 56 Z'),
    ('#FFFFFF', 'M 366 204 C 342 157 298 128 249 128 C 178.30 128 121 185.30 121 256 C 121 326.70 178.30 384 249 384 C 294 384 331.8 365.4 354.3 349.5 L 322.0 315.2 C 303.6 330.2 281.4 336 249 336 C 204.82 336 169 300.18 169 256 C 169 211.82 204.82 176 249 176 C 278 176 299.5 185 315.5 204 Z'),
    ('#BEBEBE', 'M 254 231 H 376 C 382.08 231 385 233.92 385 240 V 362 C 385 367.7 383.1 367.25 379.4 363.3 C 341.2 321.9 296.6 280.8 252.2 245.8 C 245.6 240.8 247.0 231 254 231 Z'),
]


def render_source(card: dict[str, str]) -> bytes:
    mark = "\n".join(f'<path fill="{color}" d="{path}"/>' for color, path in ICON_PATHS)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="630" viewBox="0 0 1200 630">
  <rect width="1200" height="630" fill="#0B0C0F"/>
  <rect x="38" y="36" width="1124" height="558" rx="24" fill="#121317" stroke="#292C33" stroke-width="2"/>
  <g transform="translate(66 59) scale(0.176)">{mark}</g>
  <text x="153" y="114" font-family="DejaVu Sans,sans-serif" font-weight="bold" letter-spacing="1.0" font-size="30" fill="#F7F7F8">GENYCASTER</text>
  <rect x="947" y="71" width="166" height="51" rx="14" fill="#1B1D23" stroke="#414858"/>
  <text x="1030" y="104" text-anchor="middle" font-family="DejaVu Sans,sans-serif" font-weight="bold" font-size="17" fill="#829AFF">v1.0.0 BETA</text>
  <path d="M72 151 H1128" fill="none" stroke="#343844" stroke-width="2"/>
  <rect x="74" y="203" width="8" height="43" rx="4" fill="#829AFF"/>
  <text x="105" y="302" font-family="DejaVu Sans,sans-serif" font-weight="bold" font-size="76" letter-spacing="-1.5" fill="#F7F7F8">{escape(card["headline"])}</text>
  <text x="105" y="399" font-family="DejaVu Sans,sans-serif" font-weight="bold" font-size="61" letter-spacing="-1.0" fill="#829AFF">{escape(card["second"])}</text>
  <text x="107" y="459" font-family="DejaVu Sans,sans-serif" font-size="23" fill="#AFB2BA">{escape(card["detail"])}</text>
  <path d="M72 505 H1128" fill="none" stroke="#343844" stroke-width="2"/>
  <text x="75" y="549" font-family="DejaVu Sans Mono,monospace" font-size="15" letter-spacing=".15" fill="#AFB2BA">{escape(card["footer"])}</text>
  <text x="1125" y="549" text-anchor="end" font-family="DejaVu Sans Mono,monospace" font-size="16" fill="#F7F7F8">genycaster.xyz</text>
</svg>
""".encode("utf-8")



# ImageMagick's older SVG text delegate renders some font glyphs at the same
# x-coordinate (visibly scrambled in Telegram). It is safe for the icon and
# geometric background, but NEVER for text. Draw all text with the native
# raster annotation engine and measure text width before rendering.
TEXT_FONT = "DejaVu-Sans-Bold"
BODY_FONT = "DejaVu-Sans"
MONO_FONT = "DejaVu-Sans-Mono"


def measure(text: str, font: str, point_size: int) -> tuple[int, int]:
    run = subprocess.run(
        ["/usr/bin/convert", "-font", font, "-pointsize", str(point_size),
         "label:" + text, "-format", "%w %h", "info:"],
        check=True, capture_output=True, text=True, timeout=15,
    )
    width, height = map(int, run.stdout.split())
    return width, height


def rasterize(card: dict[str, str], svg: bytes, target: Path) -> bytes:
    # Only vector shapes reach SVG delegates. A text-free SVG cannot
    # reproduce the overlapping glyph issue in the released PNG.
    background = re.sub(rb"(?m)^  <text.*?</text>\r?\n", b"", svg)
    if b"<text " in background:
        raise ValueError("SVG text not fully excluded from raster renderer")
    with tempfile.TemporaryDirectory(prefix="genycaster-og-") as work:
        base = Path(work) / "shapes.svg"
        base.write_bytes(background)
        command = ["/usr/bin/convert", "-background", "none",
                   "-density", "96", str(base), "-gravity", "NorthWest"]
        layers = [
            (card["headline"], TEXT_FONT,
             57 if card["headline"].startswith("Download") else 74,
             "#F7F7F8", 105, 226, 985),
            (card["second"], TEXT_FONT,
             55 if card["headline"].startswith("Download") else 57,
             "#829AFF", 105, 330, 985),
            (card["detail"], BODY_FONT, 23, "#AFB2BA", 107, 432, 1000),
            ("GENYCASTER", TEXT_FONT, 30, "#F7F7F8", 153, 72, 500),
            ("v1.0.0 BETA", TEXT_FONT, 17, "#829AFF", 959, 84, 160),
            (card["footer"], MONO_FONT, 15, "#AFB2BA", 75, 523, 870),
            ("genycaster.xyz", MONO_FONT, 15, "#F7F7F8", 960, 523, 170),
        ]
        for text, font, size, color, left, top, limit in layers:
            width, height = measure(text, font, size)
            # Raster annotation may have a larger advance than label:
            # allow 30% margin for the delegate's font metrics.
            if width * 1.3 > limit or left + width * 1.3 > 1145 or top + height > 590:
                raise ValueError(f"Social text exceeds safe bounding box: {text!r} ({width}x{height})")
            command.extend(["-font", font, "-pointsize", str(size),
                            "-fill", color, "-annotate", f"+{left}+{top}", text])
        command.extend(["-strip", "-define", "png:compression-level=9",
                        str(target)])
        subprocess.run(command, check=True, timeout=60, capture_output=True)
        return target.read_bytes()


def png_dimensions(payload: bytes) -> tuple[int, int]:
    if len(payload) < 24 or payload[:8] != bytes.fromhex("89504e470d0a1a0a"):
        raise ValueError("ImageMagick did not produce a PNG")
    return struct.unpack(">II", payload[16:24])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    for name, card in CARDS.items():
        svg = render_source(card)
        path = OUT / f"{name}.svg"
        png = OUT / f"{name}.png"
        if args.apply:
            path.write_bytes(svg)
            result = rasterize(card, svg, png)
            if png_dimensions(result) != (1200, 630):
                raise ValueError("Unexpected OG preview dimensions")
            if len(result) > 3_000_000:
                raise ValueError("Preview image too large")
            png.write_bytes(result)
            print(name, "1200x630", len(result), "bytes")
        else:
            print("Would create",path,png,len(svg),"SVG bytes")


if __name__ == "__main__":
    main()
