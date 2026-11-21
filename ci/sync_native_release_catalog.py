#!/usr/bin/env python3
"""Synchronize canonical native download links with the existing GitHub Release.

The WASM archive contains only an empty native-catalog template. This script
derives the public catalog from the *actual* existing v1.0.0 Release SHA256SUMS,
never from the WASM build tree. It never creates or changes a GitHub Release.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ci.refresh_existing_release import EXPECTED_ASSETS, REPO, TAG, snapshot_now

DEST = Path("/var/www/genycaster.xyz/public/releases/native.json")
RELEASE_ID = 453701445
PREFIX = f"https://github.com/{REPO}/releases/download/{TAG}/"
INSTALLERS = {
    ("windows", "x64"): "GenyCaster-v1.0.0-windows-x86_64.exe",
    ("windows", "arm64"): "GenyCaster-v1.0.0-windows-arm64.exe",
    ("macos", "arm64"): "GenyCaster-v1.0.0-macos-arm64-adhoc-UNNOTARIZED.dmg",
    ("macos", "x64"): "GenyCaster-v1.0.0-macos-x86_64-adhoc-UNNOTARIZED.dmg",
    ("linux", "x64"): "GenyCaster-v1.0.0-linux-x86_64.AppImage",
    ("linux", "arm64"): "GenyCaster-v1.0.0-linux-arm64.AppImage",
    ("android", "arm64"): "GenyCaster-v1.0.0-android-arm64-v8a.apk",
}
HEX = re.compile(r"[0-9a-f]{64}")


def parse_checksums(text: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in text.splitlines():
        digest, separator, filename = line.partition("  ")
        if (separator != "  " or HEX.fullmatch(digest) is None
                or filename not in EXPECTED_ASSETS - {"SHA256SUMS"}
                or filename in result):
            raise ValueError("Invalid, duplicated, or unrecognized Release checksum")
        result[filename] = digest
    if set(result) != EXPECTED_ASSETS - {"SHA256SUMS"}:
        raise ValueError("Incomplete current Release SHA256SUMS inventory")
    return result


def generate(checksums: dict[str, str]) -> dict:
    if set(checksums) != EXPECTED_ASSETS - {"SHA256SUMS"}:
        raise ValueError("Unexpected Release checksum inventory")
    platforms: dict[str, dict] = {}
    for (platform, arch), filename in INSTALLERS.items():
        digest = checksums[filename]
        if HEX.fullmatch(digest) is None:
            raise ValueError("Invalid installer checksum")
        record = {
            "sha256": digest,
            "url": PREFIX + filename,
            "version": "1.0.0",
        }
        if platform == "macos":
            record["distribution"] = "ad-hoc-unnotarized"
        platforms.setdefault(platform, {})[arch] = record
    return {"schema": 1, "platforms": platforms}


def fetch_checksums() -> str:
    with tempfile.TemporaryDirectory(prefix="genycaster-sha-") as folder:
        subprocess.run(
            ["gh", "release", "download", TAG, "--repo", REPO,
             "--pattern", "SHA256SUMS", "--dir", folder],
            check=True, timeout=90,
        )
        path = Path(folder) / "SHA256SUMS"
        if not path.is_file() or path.is_symlink() or path.stat().st_size > 10000:
            raise ValueError("Invalid GitHub Release checksum file")
        return path.read_text(encoding="ascii")


def apply(payload: bytes) -> None:
    if (DEST.is_symlink() or not DEST.parent.is_dir()
            or DEST.parent.resolve() != DEST.parent):
        raise ValueError("Unsafe canonical download catalog path")
    if DEST.is_file() and DEST.read_bytes() == payload:
        print("Catalog already agrees with existing Release")
        return
    fd, tmp = tempfile.mkstemp(prefix=".native-release-", dir=DEST.parent)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(payload)
            f.flush()
            os.fsync(f.fileno())
        os.chmod(tmp, 0o644)
        os.replace(tmp, DEST)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    identity = snapshot_now()
    if identity["id"] != RELEASE_ID:
        raise ValueError("Refusing catalog for a different GitHub Release ID")
    manifest = generate(parse_checksums(fetch_checksums()))
    blob = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("ascii")
    if args.apply:
        apply(blob)
    print("Verified current v1.0.0 GitHub Release ID, latest tag, "
          f"and {len(INSTALLERS)} exact installer SHA-256 links", flush=True)
    print("Catalog SHA256", hashlib.sha256(blob).hexdigest(), flush=True)


if __name__ == "__main__":
    main()
