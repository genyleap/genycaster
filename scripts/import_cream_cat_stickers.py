#!/usr/bin/env python3
"""Pin all 208 Cream Cat poster URLs from the publisher's JSON-LD page.

mssgs-stickers.com permits commercial use and hotlinking with attribution.
This generates metadata only; no artwork is bundled or rehosted.
The publisher's WebP posters are used as interoperable cast embeds;
animated APNG URLs are retained for future motion-aware playback.
"""
from __future__ import annotations
import argparse
import concurrent.futures
import hashlib
import json
import re
import urllib.request
from pathlib import Path
from urllib.parse import urlsplit

PAGE = "https://mssgs-stickers.com/packs/cream-cat"
LICENCE = "https://mssgs-stickers.com/license"
OUT = Path(__file__).resolve().with_name("cream_cat_pack_manifest.json")
PREFIX = "https://mssgs-stickers.com/"

def read(url: str, cap: int = 450_000) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent":"Genycaster-stickers/1.0"})
    with urllib.request.urlopen(req, timeout=22) as resp:
        if resp.status != 200:
            raise ValueError("HTTP failure " + url)
        data = resp.read(cap + 1)
    if not data or len(data) > cap:
        raise ValueError("unexpected asset size " + url)
    return data

def parse_page() -> list[dict]:
    page = read(PAGE, cap=900_000).decode("utf-8")
    payload = re.search(r'<script type="application/ld\+json">([\s\S]*?)</script>',page)
    if payload is None:
        raise ValueError("missing publisher JSON-LD")
    graph = json.loads(payload.group(1)).get("@graph", [])
    pages = [x for x in graph if x.get("@type") == "CollectionPage"]
    if len(pages) != 1:
        raise ValueError("unexpected publisher schema")
    entries = pages[0]["mainEntity"]["itemListElement"]
    if len(entries) != 208:
        raise ValueError(f"expected 208 Cream Cat stickers, got {len(entries)}")
    items = []
    seen = set()
    for position, source in enumerate(entries, start=1):
        thumb = source.get("thumbnailUrl","")
        animation = source.get("contentUrl","")
        slug = source.get("url","").removeprefix(PREFIX+"stickers/")
        if (not re.fullmatch(r"cream-cat-[a-z0-9-]+",slug) or slug in seen
            or not thumb.startswith(PREFIX+"thumbs/sticker-cream-cat-")
            or not thumb.endswith(".webp")
            or not animation.startswith(PREFIX+"files/sticker-cream-cat-")
            or not animation.endswith(".apng")
            or source.get("encodingFormat") != "image/apng"
            or (source.get("width"),source.get("height")) != (320,320)
            or source.get("license") != LICENCE
            or source.get("position") != position):
            raise ValueError(f"untrusted publisher metadata at {position}")
        seen.add(slug)
        poster = thumb.replace("/thumbs/","/files/",1)
        title = str(source.get("name","")).removesuffix(" sticker").strip()
        tags = str(source.get("keywords",""))[:250]
        if not title or len(title) > 120:
            raise ValueError("missing sticker title")
        items.append({"id":slug,"pack":"cream-cat-pro","title":title,
            "alt": title+" · Cream Cat sticker", "keywords": tags,
            "url":poster,"thumbnailUrl":thumb,"animationUrl":animation,
            "mediaType":"image/webp"})
    return items

def verify(item: dict) -> dict:
    data = read(item["url"])
    if data[:4] != b"RIFF" or data[8:12] != b"WEBP":
        raise ValueError("bad WebP poster " + item["id"])
    return dict(item, sha256=hashlib.sha256(data).hexdigest())

def main() -> None:
    arg = argparse.ArgumentParser()
    arg.add_argument("--apply", action="store_true")
    args = arg.parse_args()
    if args.apply:
        source = parse_page()
        with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
            entries = list(pool.map(verify,source))
        saved = {"source":PAGE,"licence":LICENCE,
                 "credit":"Stickers by mssgs-stickers.com",
                 "stickers":entries}
        OUT.write_text(json.dumps(saved,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        print("UPDATED",len(entries),"Cream Cat metadata and verified WebP SHA-256")
    else:
        saved = json.loads(OUT.read_text())
        assert saved["source"]==PAGE and saved["licence"]==LICENCE
        assert len(saved["stickers"])==208
        ids = [x["id"] for x in saved["stickers"]]
        assert len(set(ids))==208
        for x in saved["stickers"]:
            assert x["pack"]=="cream-cat-pro" and x["mediaType"]=="image/webp"
            assert re.fullmatch(r"[a-f0-9]{64}",x["sha256"])
            assert x["url"].startswith(PREFIX+"files/sticker-cream-cat-")
        print("PASS",len(ids),"pinned Cream Cat WebP assets")
if __name__=="__main__":
    main()
