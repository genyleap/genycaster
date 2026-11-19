#!/usr/bin/env python3
"""Safely deploy browser-only GenyCaster chrome without replacing the WASM.

The already-verified canonical origin and compiled Qt engine remain unchanged.
Stage using hardlinks to *read-only* immutable assets, copy every mutable file
before regenerating release fingerprints, fully validate the staged manifest,
then atomically promote the six HTML/JS/CSS/manifest files with rollback.
Never touch the old marketing site at /about, API, wallet data, or Nginx.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

if __package__:
    from .verify_primary_preview_payload import validate
else:
    from verify_primary_preview_payload import validate

ROOT = Path("/var/www/genycaster.xyz/public")
PARENT = ROOT.parent
SOURCE = Path(__file__).resolve().parents[1] / "ui/wasm"
CHANGED = (
    "native-downloads.css", "native-downloads.js", "genycaster-loader.js",
    "genycaster-assets.js", "genycaster-assets.json", "genycaster.html",
)
SOURCE_FILES = {
    "native-downloads.css": "native-downloads.css",
    "native-downloads.js": "native-downloads.js",
    "genycaster-loader.js": "genycaster-loader.js",
    "genycaster.html": "genycaster-shell.html",
}
METADATA = ("appVersion", "productName", "versionNumber",
            "releaseChannel", "releaseChannelNumber")


def check_scope() -> None:
    if ROOT.is_symlink() or not ROOT.is_dir():
        raise RuntimeError("Canonical GenyCaster web root must exist, never a symlink")
    if PARENT.is_symlink() or not PARENT.is_dir():
        raise RuntimeError("Canonical site parent is unavailable")
    for name in CHANGED:
        source = ROOT / name
        if source.is_symlink() or not source.is_file():
            raise RuntimeError(f"Missing or symlinked live asset: {name}")
    for name in SOURCE_FILES.values():
        source = SOURCE / name
        if source.is_symlink() or not source.is_file():
            raise RuntimeError(f"Missing or symlinked checked-in chrome asset: {name}")
    if not (ROOT / "about/index.html").is_file():
        raise RuntimeError("Preserved historical landing missing")


def prepare(directory: Path, existing: dict) -> dict:
    """Only mutate private staging copies; hardlinked assets stay read-only."""
    keep = list(existing["files"]) + ["genycaster.wasm", "genycaster-assets.json"]
    for name in keep:
        source = ROOT / name
        destination = directory / name
        if source.is_symlink() or not source.is_file():
            raise RuntimeError(f"Untrusted source inventory entry: {name}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if name in CHANGED:
            shutil.copy2(source, destination)
        else:
            os.link(source, destination)
    for name, origin in SOURCE_FILES.items():
        shutil.copy2(SOURCE / origin, directory / name)
    args = [sys.executable, str(Path(__file__).with_name("package_wasm_assets.py")),
            "--dir", str(directory),
            "--app-version", existing["appVersion"],
            "--product-name", existing["productName"],
            "--version-number", existing["versionNumber"],
            "--release-channel", existing["releaseChannel"],
            "--release-channel-number", str(existing["releaseChannelNumber"])]
    subprocess.run(args, check=True, timeout=45)
    fresh = validate(directory)
    if fresh["wasm"] != existing["wasm"]:
        raise RuntimeError("Browser-only update attempted to replace compiled WASM")
    for key in METADATA:
        if fresh[key] != existing[key]:
            raise RuntimeError(f"Release metadata changed unexpectedly: {key}")
    html = (directory / "genycaster.html").read_text(encoding="utf-8")
    if ('id="native-github-star"' not in html
            or 'role="listbox"' not in html
            or "<select" in html):
        raise RuntimeError("New custom download controls or GitHub CTA missing")
    return fresh


def promote(directory: Path, fresh: dict, existing: dict) -> Path:
    """Roll back to all former mutable files if promotion or audit fails."""
    backup = PARENT / (".browser-chrome-backup-" + existing["version"])
    if backup.exists() or backup.is_symlink():
        raise RuntimeError("Refusing to overwrite an existing site rollback backup")
    backup.mkdir(mode=0o700)
    promoted: list[str] = []
    try:
        for name in CHANGED:
            if (ROOT / name).is_symlink():
                raise RuntimeError(f"Live path changed unexpectedly: {name}")
            shutil.copy2(ROOT / name, backup / name)
        # A browser can fetch CSS/JS before the new HTML is visible. The
        # final HTML move is the publication boundary.
        for name in CHANGED:
            published = ROOT / name
            temporary = ROOT / (".next-chrome-" + name)
            if temporary.exists() or temporary.is_symlink():
                raise RuntimeError(f"Temporary target already exists: {temporary}")
            try:
                shutil.copy2(directory / name, temporary)
                temporary.chmod(0o644)
                os.replace(temporary, published)
            finally:
                temporary.unlink(missing_ok=True)
            promoted.append(name)
        observed = validate(ROOT)
        if observed["version"] != fresh["version"]:
            raise RuntimeError("Promoted release manifest fingerprint mismatch")
    except Exception:
        for name in reversed(promoted):
            previous = backup / name
            if previous.is_file():
                os.replace(previous, ROOT / name)
        try:
            validate(ROOT)
        except Exception as error:
            print(f"CRITICAL: manual rollback inspection required: {error}",
                  file=sys.stderr)
        raise
    return backup


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true",
                        help="Promote only after validating the regenerated browser chrome")
    args = parser.parse_args()
    try:
        check_scope()
        current = validate(ROOT)
        with tempfile.TemporaryDirectory(prefix=".browser-chrome-stage-", dir=PARENT) as temp:
            stage = Path(temp)
            fresh = prepare(stage, current)
            print(f"STAGED: {current['version']} -> {fresh['version']}")
            print(f"Verified unchanged engine: {fresh['wasm']}")
            if not args.apply:
                print("DRY RUN: no public files changed")
                return 0
            backup = promote(stage, fresh, current)
        print("PUBLISHED: browser-only Superellipse / theme / GitHub-star overlay")
        print(f"Rollback backup: {backup}")
        print("Qt/QML Changes require a separate latest-main WASM binary.")
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as error:
        print(f"Browser chrome deployment FAILED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
