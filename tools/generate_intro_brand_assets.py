#!/usr/bin/env python3
"""Extract canonical, genuine product marks for the plain studio introduction.

These are copies of brand vectors already used by Genyleap; no geometric
substitutes are drawn and no font-dependent glyphs are substituted.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/"ui/wasm"
SOURCES={
 "intro-base.svg":Path("/var/www/genyleap.com/public_html/assets/site/brands/networks/base-mark-white.svg"),
 "intro-farcaster.svg":ROOT/"ui/assets/icons/genyleap/icons/brands/farcaster.svg",
 "intro-hypersnap.svg":ROOT/"ui/assets/icons/genyleap/icons/quilibrium/hypersnap.svg",
 "intro-openproof.svg":Path("/var/www/genyleap.com/public_html/assets/site/brand/openproof-final.svg"),
}
for target,source in SOURCES.items():
    data=source.read_text(encoding="utf-8")
    if target=="intro-openproof.svg":
        # The source is a complete OpenProof badge; retain the actual vector mark.
        # All accents are normalized to white for a single-color studio slate.
        data=re.sub(r"#[0-9A-Fa-f]{6}", "#F7F8FA",data)
    else:
        data=data.replace("currentColor","#F7F8FA")
    if not re.search(r"<svg\b[^>]*\bxmlns\s*=",data):
        data=data.replace("<svg ", '<svg xmlns="http://www.w3.org/2000/svg" ',1)
    ET.fromstring(data)
    (OUTPUT/target).write_text(data,encoding="utf-8")
    print(target,"<-",source)
