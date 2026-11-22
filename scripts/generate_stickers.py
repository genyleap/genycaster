#!/usr/bin/env python3
"""Generate twelve original Genycaster sticker illustrations (editable SVG + PNG).

Requires ImageMagick's SVG rasterizer only when --render is passed.
All assets are created from project-owned vector paths, with no third-party art.
"""
from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "ui/assets/stickers/v1"

# Themes: original glossy enamel shapes with a sturdy white sticker outline.
STICKERS = [
    ("bloom", "#5C7AFF", "#A9BDFF", "flower"),
    ("orbit", "#6450F2", "#D69DFF", "orbit"),
    ("pulse", "#FF5585", "#FFC17C", "pulse"),
    ("spark", "#1DADCE", "#90F1EA", "spark"),
    ("love", "#FA4F96", "#FFBAD8", "heart"),
    ("laugh", "#FFAD25", "#FFEC8B", "laugh"),
    ("wow", "#B76DFC", "#ECC7FF", "wow"),
    ("cheer", "#17B987", "#AFF5BF", "cheer"),
    ("gm", "#F5A122", "#FFE482", "gm"),
    ("rocket", "#5877EE", "#B4C8FF", "rocket"),
    ("onchain", "#25A6BD", "#B8F8F4", "chain"),
    ("verified", "#37B78B", "#A4EFD4", "verified"),
]

# white-lined, dimensional motifs read at both 48dp and 144dp.
MOTIFS = {
    "flower": """
      <g fill="#fff" stroke="#fff" stroke-width="9" stroke-linejoin="round">
        <ellipse cx="256" cy="164" rx="43" ry="74"/>
        <ellipse cx="256" cy="348" rx="43" ry="74"/>
        <ellipse cx="164" cy="256" rx="74" ry="43"/>
        <ellipse cx="348" cy="256" rx="74" ry="43"/>
      </g>
      <g fill="#F6A5DD" opacity=".94">
        <ellipse cx="256" cy="177" rx="34" ry="55"/>
        <ellipse cx="256" cy="335" rx="34" ry="55"/>
        <ellipse cx="177" cy="256" rx="55" ry="34"/>
        <ellipse cx="335" cy="256" rx="55" ry="34"/>
      </g>
      <circle cx="256" cy="256" r="63" fill="#FFDB62" stroke="#fff" stroke-width="12"/>
      <circle cx="239" cy="248" r="7" fill="#5A4372"/><circle cx="273" cy="248" r="7" fill="#5A4372"/>
      <path d="M237 269q19 23 38 0" fill="none" stroke="#5A4372" stroke-width="8" stroke-linecap="round"/>
    """,
    "orbit": """
      <circle cx="256" cy="256" r="98" fill="#F6DAFF" stroke="#fff" stroke-width="17"/>
      <circle cx="256" cy="256" r="62" fill="#855BFF"/>
      <ellipse cx="256" cy="258" rx="176" ry="58" transform="rotate(-31 256 258)"
        fill="none" stroke="#fff" stroke-width="32"/>
      <ellipse cx="256" cy="258" rx="176" ry="58" transform="rotate(-31 256 258)"
        fill="none" stroke="#F9C04A" stroke-width="18"/>
      <circle cx="359" cy="184" r="22" fill="#fff"/>
      <path d="M250 219l10 24 26 3-20 17 6 25-22-13-22 13 6-25-20-17 26-3z"
        fill="#fff"/>
    """,
    "pulse": """
      <path d="M258 98c48 80-6 95 47 138 23-49 9-72 16-91 92 95 86 157 65 209-24 60-84 93-142 84-78-13-135-76-121-152 7-47 52-86 78-111-4 60 30 72 57 80-11-64 22-115 0-157z"
       fill="#FFF0A0" stroke="#fff" stroke-width="19" stroke-linejoin="round"/>
      <path d="M246 233c31 52-1 75 27 104 17-24 19-43 17-52 44 48 32 114-25 128-50 13-103-27-98-77 4-38 28-58 48-78-2 38 13 49 31 53-7-32 8-54 0-78z"
       fill="#FF628D"/>
      <circle cx="235" cy="348" r="6" fill="#633353"/><circle cx="271" cy="348" r="6" fill="#633353"/>
      <path d="M236 369q18 13 36 0" fill="none" stroke="#633353" stroke-width="7" stroke-linecap="round"/>
    """,
    "spark": """
      <path d="M256 93l44 116 117 47-117 47-44 116-47-116-116-47 116-47z"
       fill="#DFFCF7" stroke="#fff" stroke-width="18" stroke-linejoin="round"/>
      <path d="M256 153l30 81 72 22-72 27-30 75-30-75-72-27 72-22z"
       fill="#6A93FF"/>
      <path d="M361 97l14 36 38 16-38 14-14 38-16-38-37-14 37-16z" fill="#FFE69E" stroke="#fff" stroke-width="9"/>
      <path d="M133 305l15 40 40 13-40 14-15 41-13-41-41-14 41-13z" fill="#FFE69E" stroke="#fff" stroke-width="9"/>
    """,
    "heart": """
      <path d="M257 408l-27-20C122 305 87 260 95 194c8-65 82-109 142-62l20 19 20-19c61-47 135-3 143 62 8 66-27 111-135 194z"
       fill="#FF7CA4" stroke="#fff" stroke-width="21" stroke-linejoin="round"/>
      <path d="M163 169c29-25 55-7 68 8" fill="none" stroke="#FFE8EF" stroke-width="21" stroke-linecap="round"/>
      <path d="M239 278q18 16 36 0" fill="none" stroke="#9D3658" stroke-width="8" stroke-linecap="round"/>
      <circle cx="216" cy="258" r="7" fill="#9D3658"/><circle cx="298" cy="258" r="7" fill="#9D3658"/>
    """,
    "laugh": """
      <circle cx="256" cy="256" r="149" fill="#FFE881" stroke="#fff" stroke-width="20"/>
      <path d="M170 234q28-36 57 0M281 234q29-36 57 0" fill="none" stroke="#80573B" stroke-width="14" stroke-linecap="round"/>
      <path d="M174 294q82 118 164 0z" fill="#8F414C" stroke="#fff" stroke-width="9" stroke-linejoin="round"/>
      <path d="M216 353q44-35 83 0" fill="none" stroke="#F58FA0" stroke-width="25" stroke-linecap="round"/>
      <path d="M127 282q-12 27-1 48M385 282q12 27 1 48" stroke="#84CFFF" stroke-width="18" fill="none" stroke-linecap="round"/>
    """,
    "wow": """
      <circle cx="256" cy="256" r="148" fill="#E7B7FF" stroke="#fff" stroke-width="19"/>
      <ellipse cx="201" cy="233" rx="31" ry="47" fill="#fff"/>
      <ellipse cx="311" cy="233" rx="31" ry="47" fill="#fff"/>
      <ellipse cx="204" cy="247" rx="14" ry="24" fill="#4F3879"/>
      <ellipse cx="307" cy="247" rx="14" ry="24" fill="#4F3879"/>
      <ellipse cx="256" cy="340" rx="31" ry="44" fill="#694275"/>
      <circle cx="215" cy="191" r="9" fill="#fff" opacity=".85"/>
    """,
    "cheer": """
      <path d="M156 180l-31-50M355 164l34-50M254 135V83" stroke="#fff" stroke-width="18" stroke-linecap="round"/>
      <path d="M156 180l-31-50M355 164l34-50M254 135V83" stroke="#FDEB9A" stroke-width="10" stroke-linecap="round"/>
      <path d="M149 257l-38 47 76 79 48-48-62-67z" fill="#A8E6FE" stroke="#fff" stroke-width="15"/>
      <path d="M363 257l38 47-76 79-48-48 62-67z" fill="#F5B3E3" stroke="#fff" stroke-width="15"/>
      <path d="M177 248l55 58 24-26-55-57q-17-15-28 0z" fill="#FFDB9B" stroke="#fff" stroke-width="13"/>
      <path d="M335 248l-55 58-24-26 55-57q17-15 28 0z" fill="#FFDB9B" stroke="#fff" stroke-width="13"/>
      <path d="M256 203l18 37 43 6-31 30 7 43-37-20-37 20 7-43-31-30 43-6z" fill="#FFDF67" stroke="#fff" stroke-width="10"/>
    """,
    "gm": """
      <circle cx="256" cy="256" r="120" fill="#FFE68B" stroke="#fff" stroke-width="18"/>
      <g stroke="#fff" stroke-width="20" stroke-linecap="round">
       <path d="M256 85v30M256 397v30M85 256h30M397 256h30M136 136l21 21M355 355l21 21M377 136l-21 21M157 355l-21 21"/>
      </g>
      <g stroke="#FFBC48" stroke-width="10" stroke-linecap="round">
       <path d="M256 85v30M256 397v30M85 256h30M397 256h30M136 136l21 21M355 355l21 21M377 136l-21 21M157 355l-21 21"/>
      </g>
      <text x="256" y="295" font-family="DejaVu Sans, sans-serif" font-size="108" font-weight="bold"
       letter-spacing="-10" text-anchor="middle" fill="#855A39">GM</text>
    """,
    "rocket": """
      <path d="M190 311q0-150 154-208 40 133-63 224z" fill="#E1EDFF" stroke="#fff" stroke-width="20" stroke-linejoin="round"/>
      <path d="M296 133q33-21 48-30 15 47 6 72z" fill="#F897AD"/>
      <circle cx="268" cy="227" r="35" fill="#8ED5F1" stroke="#6E8EEA" stroke-width="13"/>
      <path d="M193 275l-68 10-25 65 106-20M273 334l-11 69-62 25-9-105" fill="#F694BF" stroke="#fff" stroke-width="14" stroke-linejoin="round"/>
      <path d="M188 331q-50 9-63 66 56-9 70-60" fill="#FFE18A" stroke="#fff" stroke-width="12"/>
      <path d="M150 374l-14 28" stroke="#FFB352" stroke-width="16" stroke-linecap="round"/>
    """,
    "chain": """
      <g fill="none" stroke-linecap="round">
       <rect x="114" y="186" width="171" height="115" rx="57" transform="rotate(-37 199 243)" stroke="#fff" stroke-width="33"/>
       <rect x="226" y="211" width="171" height="115" rx="57" transform="rotate(-37 312 268)" stroke="#fff" stroke-width="33"/>
       <rect x="114" y="186" width="171" height="115" rx="57" transform="rotate(-37 199 243)" stroke="#A4EAFF" stroke-width="19"/>
       <rect x="226" y="211" width="171" height="115" rx="57" transform="rotate(-37 312 268)" stroke="#FFE1AC" stroke-width="19"/>
       <path d="M220 287l69-51" stroke="#fff" stroke-width="23"/>
       <path d="M220 287l69-51" stroke="#637DE5" stroke-width="13"/>
      </g>
      <path d="M358 113l11 28 30 10-30 11-11 29-10-29-29-11 29-10z" fill="#fff"/>
    """,
    "verified": """
      <path d="M256 96l130 43v111q0 105-130 166-130-61-130-166V139z"
       fill="#B5F6DB" stroke="#fff" stroke-width="22" stroke-linejoin="round"/>
      <path d="M256 133l96 33v84q0 74-96 127-96-53-96-127v-84z" fill="#45BB99"/>
      <path d="M193 259l44 45 92-98" fill="none" stroke="#fff" stroke-width="29" stroke-linecap="round" stroke-linejoin="round"/>
      <path d="M194 175l18 3M185 208l12 0" fill="none" stroke="#C8F9EB" stroke-width="12" stroke-linecap="round"/>
    """,
}


def render_svg(accent: str, light: str, motif: str) -> str:
    raw = f'''<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
  <defs>
    <linearGradient id="enamel" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="{light}"/>
      <stop offset=".65" stop-color="{accent}"/>
      <stop offset="1" stop-color="{accent}"/>
    </linearGradient>
  </defs>
  <path d="M256 41c124 0 215 91 215 215S380 471 256 471 41 380 41 256 132 41 256 41z"
    fill="#13172b" fill-opacity=".11" transform="translate(0 7)"/>
  <circle cx="256" cy="256" r="210" fill="url(#enamel)" stroke="#fff" stroke-width="18"/>
  <ellipse cx="201" cy="131" rx="105" ry="45" fill="#fff" opacity=".26" transform="rotate(-18 201 131)"/>
  <circle cx="414" cy="121" r="13" fill="#fff" opacity=".84"/>
  <circle cx="110" cy="370" r="9" fill="#fff" opacity=".75"/>
  {MOTIFS[motif]}
</svg>
'''
    return "\n".join(line.rstrip() for line in raw.splitlines()) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--render", action="store_true", help="Render PNGs via ImageMagick")
    args = parser.parse_args()
    ROOT.mkdir(parents=True, exist_ok=True)
    for name, accent, light, motif in STICKERS:
        svg = ROOT / f"{name}.svg"
        svg.write_text(render_svg(accent, light, motif), encoding="utf-8")
        if args.render:
            png = ROOT / f"{name}.png"
            subprocess.run([
                "convert", "-background", "none", "-density", "96",
                str(svg), "-resize", "512x512", "-depth", "8",
                "-define", "png:color-type=6", "-strip", str(png),
            ], check=True)
            if png.stat().st_size > 350_000:
                raise RuntimeError(f"Unexpectedly large sticker: {png}")
            print(name, png.stat().st_size, flush=True)


if __name__ == "__main__":
    main()
