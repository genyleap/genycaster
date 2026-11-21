#!/usr/bin/env python3
"""Publish cache-busted Open Graph previews for canonical GenyCaster links.

Root URL is WASM at /genycaster.html; /download is temporarily a server-side
302 to /about/#downloads. Crawlers do not execute hash JavaScript, so /about
must carry the download preview until the Nginx rule is migrated.

Do not change API, Qt runtime files, user data or existing Release assets.
"""
from __future__ import annotations
import argparse
from html import escape
import os
from pathlib import Path
import re
import shutil
import struct
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "ui/wasm"
LIVE = Path("/var/www/genycaster.xyz/public")
ASSET_NAMES = ("og-genycaster-1-beta.png", "og-genycaster-download-v1.png")
IMAGE_URLS = {
    "home": "https://genycaster.xyz/" + ASSET_NAMES[0],
    "downloads": "https://genycaster.xyz/" + ASSET_NAMES[1],
}
META = {
    "home": {
        "url": "https://genycaster.xyz/",
        "title": "GenyCaster 1.0 Beta — Farcaster & HyperSnap",
        "description": "One local-first social client for Farcaster and HyperSnap. Explore GenyCaster on the web or download it for Windows, macOS, Linux and Android.",
        "image": IMAGE_URLS["home"],
        "alt": "GenyCaster 1.0 Beta — Farcaster and HyperSnap in one client",
    },
    "downloads": {
        "url": "https://genycaster.xyz/download",
        "title": "Download GenyCaster 1.0 Beta — Windows, macOS, Linux & Android",
        "description": "Choose your platform and architecture. Download official GenyCaster 1.0 Beta installers for Windows, macOS, Linux and Android from GitHub.",
        "image": IMAGE_URLS["downloads"],
        "alt": "Download GenyCaster 1.0 Beta for Windows, macOS, Linux and Android",
    }
}
START = "<!-- GenyCaster managed Open Graph and Twitter preview -->"
END = "<!-- End GenyCaster managed preview -->"
META_LINE = re.compile(
    r'(?m)^[ \t]*<meta\s+(?:name|property)="(?:description|og:[^"]+|twitter:[^"]+)"[^>]*>[ \t]*\r?\n?',
    re.IGNORECASE,
)
CANONICAL_LINE = re.compile(r'(?m)^[ \t]*<link rel="canonical"[^>]*>[ \t]*\r?\n?', re.IGNORECASE)


def tags(which: str) -> str:
    obj = META[which]
    attrs = [
        ("name", "description", obj["description"]),
        ("property", "og:type", "website"),
        ("property", "og:site_name", "GenyCaster"),
        ("property", "og:locale", "en_US"),
        ("property", "og:url", obj["url"]),
        ("property", "og:title", obj["title"]),
        ("property", "og:description", obj["description"]),
        ("property", "og:image", obj["image"]),
        ("property", "og:image:secure_url", obj["image"]),
        ("property", "og:image:type", "image/png"),
        ("property", "og:image:width", "1200"),
        ("property", "og:image:height", "630"),
        ("property", "og:image:alt", obj["alt"]),
        ("name", "twitter:card", "summary_large_image"),
        ("name", "twitter:title", obj["title"]),
        ("name", "twitter:description", obj["description"]),
        ("name", "twitter:image", obj["image"]),
        ("name", "twitter:image:alt", obj["alt"]),
    ]
    lines = [START, '<link rel="canonical" href="' + escape(obj["url"], quote=True) + '">']
    lines.extend(f'<meta {kind}="{name}" content="{escape(value, quote=True)}">'
                 for kind, name, value in attrs)
    lines.append(END)
    return "\n".join(lines) + "\n"


def with_tags(data: str, which: str) -> str:
    # Idempotent and strict: a broken section must never accidentally delete
    # arbitrary script or application HTML.
    if data.count(START) != data.count(END) or data.count(START) > 1:
        raise ValueError("Unexpected managed social metadata markers")
    if START in data:
        begin, end = data.index(START), data.index(END) + len(END)
        if begin > 0 and data[begin - 1] == chr(10):
            begin -= 1
        if end < len(data) and data[end] == chr(10):
            end += 1
        data = data[:begin] + data[end:]
    data = META_LINE.sub("", data)
    data = CANONICAL_LINE.sub("", data)
    needle = '<meta charset="utf-8">'
    if data.count(needle) != 1 or data.count("</head>") != 1:
        raise ValueError("Unexpected HTML head layout")
    return data.replace(needle, needle + "\n" + tags(which), 1)


def png_check(path: Path) -> None:
    if not path.is_file() or path.is_symlink():
        raise ValueError("Missing or redirected social asset " + str(path))
    with path.open("rb") as stream:
        header = stream.read(24)
    if (len(header) < 24 or header[:8] != bytes.fromhex("89504e470d0a1a0a")
            or struct.unpack(">II", header[16:24]) != (1200, 630)
            or path.stat().st_size > 3_000_000):
        raise ValueError("Invalid 1200x630 OG PNG " + str(path))


def atomic(path: Path, payload: bytes) -> None:
    if path.is_symlink() or not path.parent.is_dir():
        raise ValueError("Unsafe destination " + str(path))
    fd, tmp = tempfile.mkstemp(prefix=".genycaster-og-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(payload)
            output.flush()
            os.fsync(output.fileno())
        os.chmod(tmp, 0o644)
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def collect() -> dict[Path, bytes]:
    files = {
        SRC / "genycaster-shell.html": "home",
        SRC / "download/index.html": "downloads",
        LIVE / "genycaster.html": "home",
        LIVE / "download/index.html": "downloads",
        # /download currently 302s here. Crawlers don't run the JS forwarder.
        LIVE / "about/index.html": "downloads",
    }
    payloads = {}
    for file, which in files.items():
        if not file.is_file() or file.is_symlink():
            raise ValueError("Required HTML unavailable: " + str(file))
        edited = with_tags(file.read_text(encoding="utf-8"), which)
        if which == "downloads" and file == LIVE / "about/index.html":
            edited = re.sub(
                r"<title>[^<]+</title>",
                "<title>Download GenyCaster 1.0 Beta — Official Apps</title>",
                edited, count=1,
            )
        payloads[file] = edited.encode("utf-8")
    for filename in ASSET_NAMES:
        source = SRC / filename
        png_check(source)
        payloads[LIVE / filename] = source.read_bytes()
    return payloads


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    files = collect()
    if not args.apply:
        print("PASS social metadata and new 1200x630 preview assets")
        print("Files:", len(files))
        return
    backup = Path(tempfile.mkdtemp(prefix=".genycaster-social-backup.", dir=LIVE.parent))
    updated = []
    try:
        for target, data in files.items():
            if target.read_bytes() == data if target.exists() else False:
                continue
            was_present = target.exists()
            original = backup / str(len(updated))
            if was_present:
                shutil.copy2(target, original)
            atomic(target, data)
            updated.append((target, original if was_present else None))
        for name in ASSET_NAMES:
            png_check(LIVE / name)
        for path, which in (
            (LIVE / "genycaster.html", "home"),
            (LIVE / "download/index.html", "downloads"),
            (LIVE / "about/index.html", "downloads"),
        ):
            html = path.read_text(encoding="utf-8")
            if html.count('<meta property="og:image" content="' + META[which]["image"] + '">') != 1:
                raise ValueError("OG image not inserted exactly once")
        print("PUBLISHED canonical GenyCaster Open Graph previews:", len(updated),
              "updates, backup:", backup)
    except Exception:
        for target, original in reversed(updated):
            if original:
                atomic(target, original.read_bytes())
            else:
                target.unlink(missing_ok=True)
        shutil.rmtree(backup)
        raise


if __name__ == "__main__":
    main()
