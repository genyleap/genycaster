#!/usr/bin/env python3
"""Pin a curated selection of Njal's public Farcaster sticker manifest.

No third-party artwork is copied into Genycaster; only names, source URLs,
content hashes and tags are referenced. Publishing and caching of third-party
files require explicit permission from their respective rights holders.

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

# A small, expressive, browsable selection; original IDs remain stable.
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
function itemForId(id) {
    for (var i = 0; i < stickers.length; ++i)
        if (stickers[i].id === id) return stickers[i]
    return null
}

function embedUrl(id) {
    var value = itemForId(id)
    return value ? value.url : ""
}

function previewUrl(id) { return embedUrl(id) }

function isStickerUrl(url) {
    var value = String(url || "").split("#")[0].split("?")[0]
    // Preserve already-published first-party stickers in historical casts;
    // they are intentionally no longer selectable in the new picker.
    if (/^https:\/\/genycaster\.xyz\/stickers\/v1\.0\.0\/[a-z0-9_-]+\.png$/.test(value))
        return true
    if (value.indexOf(imageOrigin) !== 0) return false
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
        "var stickers = [",
    ]
    for item in stickers:
        metadata = {
            "id": item["id"],
            "pack": SELECTED[item["id"]],
            "title": item["name"],
            "alt": item["alt"],
            "keywords": " ".join(item.get("tags", [])),
            "url": quote(item["url"], safe=":/?=&#%-_.!~*'()"),
            "sha256": item["sha256"],
            "mediaType": "image/webp",
        }
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
        if identifier not in SELECTED:
            continue
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
    if set(found) != set(SELECTED):
        raise RuntimeError(f"Missing upstream assets: {set(SELECTED) - set(found)}")
    result = render([found[id_] for id_ in SELECTED])
    if args.apply:
        DEST.write_text(result, encoding="utf-8")
        print(f"UPDATED {len(found)} stickers: {DEST}")
    elif DEST.read_text(encoding="utf-8") != result:
        raise SystemExit("Pinned catalog changed: run with --apply after reviewing upstream")
    else:
        print(f"PASS {len(found)} pinned manifest entries, no image copies")


if __name__ == "__main__":
    main()
