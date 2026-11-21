#!/usr/bin/env python3
"""Generate *both* browser and preserved landing download catalogs.

Run only against the exact verified release/ directory AFTER its GitHub Release
is publicly published. No data is fetched from GitHub, no links are enabled
without matching file bytes and SHA256SUMS, and no private GitHub token exists
in either output. Do not run this for unshipped CI test artifacts.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import quote

if __package__:
    from .release_artifacts import EXPECTED
else:
    from release_artifacts import EXPECTED

SUPPORTED = {
    "GenyCaster-windows-x86_64": ("windows", "x64"),
    "GenyCaster-windows-arm64": ("windows", "arm64"),
    "GenyCaster-macos-x86_64": ("macos", "x64"),
    "GenyCaster-macos-arm64": ("macos", "arm64"),
    "GenyCaster-linux-x86_64": ("linux", "x64"),
    "GenyCaster-linux-arm64": ("linux", "arm64"),
    "GenyCaster-android-arm64-v8a": ("android", "arm64"),
}
TAG = re.compile(r"^v\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)[1-9]\d*)?$")
FILE = re.compile(r"^GenyCaster-[A-Za-z0-9._+-]+$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def generate(directory: Path, tag: str) -> tuple[dict, dict]:
    if not TAG.fullmatch(tag):
        raise ValueError("Invalid published GitHub version tag")
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError("Missing verified GitHub release directory")
    sums = directory / "SHA256SUMS"
    if sums.is_symlink() or not sums.is_file():
        raise ValueError("Required signed-off release digest inventory missing")
    checks: dict[str, str] = {}
    for line in sums.read_text(encoding="ascii").splitlines():
        pieces = line.split(maxsplit=1)
        if len(pieces) != 2:
            raise ValueError("Malformed SHA256SUMS entry")
        digest, filename = pieces
        if (not re.fullmatch("[a-f0-9]{64}", digest) or
                not FILE.fullmatch(filename) or filename in checks):
            raise ValueError("Invalid or duplicated release checksum entry")
        checks[filename] = digest
    if not checks:
        raise ValueError("Empty release digest inventory")
    catalog = {"schema": 1, "platforms": {}}
    landing = {
        "schemaVersion": 1,
        "platforms": {os: {"version": None, "assets": []}
                      for os in ("windows", "macos", "linux", "android", "ios")},
    }
    version = tag.removeprefix("v")
    for artifact, (os, arch) in SUPPORTED.items():
        suffix = EXPECTED[artifact]
        # Release artifact folder identifiers are different from physical
        # filenames: GenyCaster-windows-x86_64 contains the versioned file
        # GenyCaster-v1.0.0-windows-x86_64.exe, not the folder prefix.
        expected_name = f"GenyCaster-{tag}-{artifact.removeprefix('GenyCaster-')}{suffix}"
        # A GenyConnect-style free macOS release has a verified ad-hoc code
        # signature, but NO Apple Developer ID or notarization. Keep that
        # limitation explicit in both the asset filename and the download UI.
        # Never silently choose between differently trusted DMGs.
        names = {expected_name}
        if os == "macos":
            names.add(expected_name.removesuffix(".dmg")
                      + "-adhoc-UNNOTARIZED.dmg")
        candidates = [p for p in directory.iterdir()
                      if p.is_file() and not p.is_symlink()
                      and p.name in names]
        if len(candidates) != 1:
            raise ValueError(f"Missing/ambiguous verified {artifact} installer")
        file = candidates[0]
        expected = checks.get(file.name)
        if not expected or not file.stat().st_size or sha256(file) != expected:
            raise ValueError(f"Missing or incorrect checksum for {file.name}")
        url = ("https://github.com/genyleap/genycaster/releases/download/" +
               quote(tag, safe="") + "/" + quote(file.name, safe=""))
        record = {"url": url, "version": version, "sha256": expected}
        if os == "macos":
            record["distribution"] = (
                "ad-hoc-unnotarized"
                if file.name.endswith("-adhoc-UNNOTARIZED.dmg")
                else "developer-id-notarized"
            )
        catalog["platforms"].setdefault(os, {})[arch] = record
        landing["platforms"][os]["version"] = version
        asset = {"label": file.name, "architecture": arch, "url": url}
        if os == "macos":
            asset["distribution"] = record["distribution"]
        landing["platforms"][os]["assets"].append(asset)
    return catalog, landing


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-dir", required=True, type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    native, landing = generate(args.release_dir, args.tag)
    destination = args.output_dir
    destination.mkdir(parents=True, exist_ok=True)
    native_path = destination / "native.json"
    landing_path = destination / "releases.json"
    for path, data in ((native_path, native), (landing_path, landing)):
        if path.is_symlink():
            raise ValueError(f"Refusing symlinked catalog destination: {path}")
        path.write_text(json.dumps(data, sort_keys=True, indent=2) + "\n",
                        encoding="utf-8")
    print("Generated verified native and legacy-landing catalogs from the "
          f"same published release: {args.tag}")


if __name__ == "__main__":
    main()
