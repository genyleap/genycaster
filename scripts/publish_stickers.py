#!/usr/bin/env python3
"""Publish the reviewed immutable sticker PNG catalog without touching WASM.

Usage: python3 scripts/publish_stickers.py [--public-root /var/www/genycaster.xyz/public]
Assets are owned by the repository, content is non-executable and does not
require a protocol or authenticated media-gateway change.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import struct
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "ui/assets/stickers/v1"
IDS = ("bloom", "orbit", "pulse", "spark", "love", "laugh",
       "wow", "cheer", "gm", "rocket", "onchain", "verified")


def read_png(path: Path) -> bytes:
    if path.is_symlink() or not path.is_file():
        raise RuntimeError(f"Missing/unsafe original sticker: {path}")
    data = path.read_bytes()
    if not (200 < len(data) < 350_000) or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"Invalid PNG: {path}")
    width, height, depth, color = struct.unpack(">IIBB", data[16:26])
    if (width, height) != (512, 512) or depth != 8 or color not in (2, 6):
        raise RuntimeError(f"Invalid PNG dimensions/depth/type: {path}")
    if not (ART / (path.stem + ".svg")).is_file():
        raise RuntimeError(f"Missing editable SVG source: {path}")
    return data


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--public-root", default="/var/www/genycaster.xyz/public")
    args = parser.parse_args()
    public = Path(args.public_root)
    if public.is_symlink() or not public.is_dir():
        raise RuntimeError("Untrusted public directory")
    dest = public / "stickers" / "v1.0.0"
    if dest.is_symlink():
        raise RuntimeError("Refusing symlinked sticker directory")
    items = {id_: read_png(ART / (id_ + ".png")) for id_ in IDS}
    dest.mkdir(parents=True, exist_ok=True, mode=0o755)
    # A restrictive deployment umask can otherwise leave newly created
    # directories at 0700: Nginx would return 404 (permission denied) even
    # though the image files themselves are world-readable.
    if (dest.parent.is_symlink() or dest.is_symlink()):
        raise RuntimeError("Refusing symlinked static asset path")
    os.chmod(dest.parent, 0o755)
    os.chmod(dest, 0o755)
    for id_, data in items.items():
        target = dest / (id_ + ".png")
        if target.is_symlink():
            raise RuntimeError(f"Refusing symlinked sticker asset: {target}")
        if target.is_file() and target.read_bytes() == data:
            print("ALREADY_CURRENT", id_, flush=True)
            continue
        with tempfile.NamedTemporaryFile(prefix=".sticker-", dir=dest, delete=False) as staging:
            path = Path(staging.name)
            staging.write(data)
            staging.flush()
            os.fsync(staging.fileno())
        os.chmod(path, 0o644)
        os.replace(path, target)
        print("PUBLISHED", id_, hashlib.sha256(data).hexdigest()[:16], len(data), flush=True)
    for id_, data in items.items():
        target = dest / (id_ + ".png")
        if target.read_bytes() != data:
            raise RuntimeError(f"Sticker integrity failed: {id_}")
    print("SUCCESS", len(items), "static public PNG stickers", flush=True)


if __name__ == "__main__":
    main()
