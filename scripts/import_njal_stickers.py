#!/usr/bin/env python3
"""Pin Njal's full 68-sticker pack plus eight CC-BY Twemoji Pro reactions.

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

function previewUrl(id) { return embedUrl(id) }

function isStickerUrl(url) {
    var value = String(url || "").split("#")[0].split("?")[0]
    // Preserve already-published first-party stickers in historical casts;
    // they are intentionally no longer selectable in the new picker.
    if (/^https:\/\/genycaster\.xyz\/stickers\/v1\.0\.0\/[a-z0-9_-]+\.png$/.test(value))
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
        '    {id:"twemoji-pro",title:"Pro Reactions",author:"Twemoji contributors",count:8,tier:"pro",credit:"https://github.com/jdecked/twemoji"},',
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
    for code, title, digest in [
        ("1f973","Party","9c25d18042e707930de1e0963a8de3e0e7a86af4e5c725798016773937586ee1"),
        ("1f929","Starstruck","97a48d3f97dd04e9885338ea3d5385e891731da5ae9acec42a218ac9348c145d"),
        ("1f680","Rocket","28a3baac8555651cc937ff638155dbe6da285dafbd37d2ecc20af52a3843cb2c"),
        ("1f451","Crown","159d2d13ebb634b06d9ff84d73fa4a9af5d9d10a5846829e2eaad28c952e2516"),
        ("1f3c6","Trophy","2dc45c8d6f623f55fdc0a8b8da7bb93b8f9595b680286f9601c0e4bf96a38036"),
        ("1f496","Sparkling Heart","e935fa571e5bfff90a882867de0a201c747eaa9642576320c146e5e4a88592ef"),
        ("1f525","Fire","68f3b36cdd92062c09e600f9f930d4cfceb59d1eaecd18169a5f0476cf33c71a"),
        ("1f4af","100","b300720ed1d9262bbdc7d52d30368ac1127f8fc38d118a8eeb4aeb9566d5aef7")
    ]:
        metadata = {
            "id": "twemoji-" + code, "pack": "twemoji-pro",
            "title": title, "alt": title + " reaction sticker",
            "keywords": title.lower() + " premium pro emoji",
            "url": "https://genycaster.xyz/stickers/twemoji-pro-v1/" + code + ".png",
            "sha256": digest, "mediaType": "image/png"
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
