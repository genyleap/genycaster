#!/usr/bin/env python3
"""Pin Njal's 68 WebP stickers and mssgs Cream Cat's 208 WebP posters.

No third-party artwork is copied into Genycaster. Cream Cat has explicit
commercial app/hotlink permissions with credit at mssgs-stickers.com/license.

Run with --apply to update the reviewed, generated JS catalog. Without it,
validate that the checked-in catalog matches the published manifest snapshot.
"""
from __future__ import annotations

import argparse
import json
import re
import urllib.request
from pathlib import Path
from urllib.parse import quote

MANIFEST_URL = ("https://raw.githubusercontent.com/cryptonjal/njals-sticker-pack/"
                "refs/heads/main/njals-sticker-pack.json")
PUBLISHER_URL = "https://github.com/cryptonjal/njals-sticker-pack"
IMAGE_PREFIX = ("https://red-geographical-clownfish-714.mypinata.cloud/ipfs/"
                "bafybeicdae7bmjnkp3aiwdemgduv6bwfmqf4xnbhfsjiz73k5fg375yneq/")
DEST = Path(__file__).resolve().parents[1] / "ui/components/composer/StickerCatalog.js"

# The previously shipped 32 IDs must remain stable while all 68 are added.
SELECTED = {
    "02-big-smile": "reactions",
    "04-celebration": "reactions",
    "08-cloudy": "reactions",
    "10-coffee": "memes",
    "12-crying": "reactions",
    "14-doubtful": "reactions",
    "16-fire": "web3",
    "17-fireworks": "web3",
    "20-frustrated": "reactions",
    "21-gm": "web3",
    "22-grumpy": "reactions",
    "23-hamburger": "memes",
    "24-hug": "reactions",
    "25-king": "memes",
    "27-kiss": "reactions",
    "29-laughing-extreme": "reactions",
    "30-laughing": "reactions",
    "31-love": "reactions",
    "32-melting": "reactions",
    "34-neutral": "reactions",
    "39-open-smile": "reactions",
    "42-pizza": "memes",
    "45-rolling-eyes": "reactions",
    "48-salut": "web3",
    "50-satisfying": "reactions",
    "52-sleep": "reactions",
    "55-smirking": "reactions",
    "56-stareyes": "reactions",
    "58-surprised": "reactions",
    "60-tear": "reactions",
    "61-thinker": "memes",
    "68-wink": "reactions",
}

RUNTIME = r'''
function packForId(id) {
    for (var i=0;i<packs.length;++i)
        if (packs[i].id===id) return packs[i]
    return null
}

function stickersForPack(id) {
    return stickers.filter(function(sticker) { return sticker.pack===id })
}

function itemForId(id) {
    for (var i = 0; i < stickers.length; ++i)
        if (stickers[i].id === id) return stickers[i]
    return null
}

function embedUrl(id) {
    var value = itemForId(id)
    return value ? value.url : ""
}

function previewUrl(id) {
    var value = itemForId(id)
    return value ? (value.thumbnailUrl || value.url) : ""
}

function isStickerUrl(url) {
    var value = String(url || "").split("#")[0].split("?")[0]
    // Preserve already-published first-party stickers in historical casts;
    // they are intentionally no longer selectable in the new picker.
    if (/^https:\/\/genycaster\.xyz\/stickers\/v1\.0\.0\/[a-z0-9_-]+\.png$/.test(value))
        return true
    // Existing protocol casts remain readable even when a premium catalog
    // is retired; never reuse the Twemoji pack identifier.
    if (/^https:\/\/genycaster\.xyz\/stickers\/twemoji-pro-v1\/[a-f0-9]+\.png$/.test(value))
        return true
    for (var i = 0; i < stickers.length; ++i)
        if (stickers[i].url === value) return true
    return false
}

function decodeIds(json, maxLength) {
    var raw = []
    try { raw = JSON.parse(String(json || "[]")) } catch (_) { return [] }
    if (!Array.isArray(raw)) return []
    var result = []
    var limit = Math.min(stickers.length, Math.max(0, Number(maxLength) || 0))
    for (var i = 0; i < raw.length && result.length < limit; ++i) {
        var id = raw[i]
        if (typeof id === "string" && itemForId(id) && result.indexOf(id) < 0)
            result.push(id)
    }
    return result
}

function toggleFavorite(ids, id) {
    if (!itemForId(id)) return ids.slice()
    var result = ids.slice()
    var index = result.indexOf(id)
    if (index < 0) result.unshift(id)
    else result.splice(index, 1)
    return result.slice(0, stickers.length)
}

function recordRecent(ids, id) {
    if (!itemForId(id)) return ids.slice()
    var result = [id]
    for (var i = 0; i < ids.length && result.length < 12; ++i)
        if (ids[i] !== id && itemForId(ids[i])) result.push(ids[i])
    return result
}

function matches(sticker, query) {
    if (!query) return true
    var searchable = (sticker.id + " " + sticker.title + " "
        + sticker.keywords + " " + sticker.pack).toLowerCase()
    return searchable.indexOf(query) >= 0
}

function filtered(pack, query, favorites, recent) {
    var filter = String(pack || "all")
    var q = String(query || "").trim().toLowerCase().slice(0, 70)
    var ordered = stickers
    if (filter === "favorites") ordered = favorites.map(itemForId).filter(Boolean)
    else if (filter === "recent") ordered = recent.map(itemForId).filter(Boolean)
    return ordered.filter(function(item) {
        return (filter === "all" || filter === "recent" || filter === "favorites"
                || item.pack === filter) && matches(item, q)
    })
}
'''


def render(stickers: list[dict]) -> str:
    lines = [
        ".pragma library",
        "",
        "// Pinned from Njal's public Farcaster manifest. No third-party art",
        "// is bundled, modified, renamed or self-hosted by Genycaster.",
        "// The upstream repository does not currently declare a license.",
        f"var publisherUrl = {json.dumps(PUBLISHER_URL)}",
        f"var imageOrigin = {json.dumps(IMAGE_PREFIX)}",
        "var packs = [",
        '    {id:"njal",title:"Njal\\u0027s Sticker Pack",author:"Njal",count:68,tier:"free",credit:"https://github.com/cryptonjal/njals-sticker-pack"},',
        '    {id:"cream-cat-pro",title:"Cream Cat",author:"mssgs-stickers.com",count:208,tier:"pro",credit:"https://mssgs-stickers.com/license"},',
        "];",
        "var stickers = [",
    ]
    for item in stickers:
        metadata = {
            "id": item["id"],
            "pack": "njal",
            "title": item["name"],
            "alt": item["alt"],
            "keywords": " ".join(item.get("tags", [])),
            "url": quote(item["url"], safe=":/?=&#%-_.!~*'()"),
            "sha256": item["sha256"],
            "mediaType": "image/webp",
        }
        lines.append("    " + json.dumps(metadata, ensure_ascii=False, separators=(",", ":")) + ",")
    cream_cat = json.loads(
        Path(__file__).resolve().with_name("cream_cat_pack_manifest.json").read_text(encoding="utf-8"))
    if (cream_cat["source"] != "https://mssgs-stickers.com/packs/cream-cat"
        or cream_cat["credit"] != "Stickers by mssgs-stickers.com"
        or len(cream_cat["stickers"]) != 208):
        raise RuntimeError("Invalid pinned Cream Cat manifest")
    for metadata in cream_cat["stickers"]:
        if metadata["pack"] != "cream-cat-pro" or not metadata["url"].startswith(
            "https://mssgs-stickers.com/files/sticker-cream-cat-"):
            raise RuntimeError("Untrusted Cream Cat media URL")
        if not re.fullmatch(r"[a-f0-9]{64}", metadata["sha256"]):
            raise RuntimeError("Invalid Cream Cat poster digest")
        lines.append("    " + json.dumps(metadata, ensure_ascii=False, separators=(",", ":")) + ",")
    lines.extend(["]", RUNTIME.strip(), ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    request = urllib.request.Request(MANIFEST_URL, headers={"User-Agent": "Genycaster-sticker-catalog"})
    with urllib.request.urlopen(request, timeout=20) as response:
        document = json.load(response)
    if document.get("format") != "farcaster-sticker-pack" or document.get("version") != 1:
        raise RuntimeError("Unsupported upstream sticker pack")
    if document.get("id") != "njals-sticker-pack" or document.get("author", {}).get("name") != "Njal":
        raise RuntimeError("Unexpected attribution")
    found = {}
    for item in document.get("stickers", []):
        identifier = item.get("id")
        if not isinstance(identifier, str) or not re.fullmatch(r"[0-9]{2}-[a-z0-9-]+", identifier):
            raise RuntimeError("Unexpected upstream sticker identifier")
        if identifier in found:
            raise RuntimeError("Duplicate upstream ID")
        if (item.get("width"), item.get("height"), item.get("mimeType")) != (512, 512, "image/webp"):
            raise RuntimeError(f"Invalid image metadata: {identifier}")
        if (not isinstance(item.get("url"), str) or
                not item["url"].startswith(IMAGE_PREFIX) or
                not item["url"].lower().endswith(".webp")):
            raise RuntimeError(f"Unexpected image host or format: {identifier}")
        if not re.fullmatch(r"[a-f0-9]{64}", item.get("sha256", "")):
            raise RuntimeError(f"Invalid asset digest: {identifier}")
        found[identifier] = item
    if len(found) != 68 or not set(SELECTED).issubset(found):
        raise RuntimeError("Expected all 68 upstream Njal stickers with stable IDs")
    result = render(list(found.values()))
    if args.apply:
        DEST.write_text(result, encoding="utf-8")
        print(f"UPDATED {len(found)} stickers: {DEST}")
    elif DEST.read_text(encoding="utf-8") != result:
        raise SystemExit("Pinned catalog changed: run with --apply after reviewing upstream")
    else:
        print(f"PASS {len(found)} pinned manifest entries, no image copies")


if __name__ == "__main__":
    main()
