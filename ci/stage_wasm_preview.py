#!/usr/bin/env python3
"""Stage a verified prior CI WASM artifact for a temporary app-domain preview.

This deliberately does NOT assert that a prior successful CI artifact is from
the current HEAD. It only stages a manually selected, checksum-verified RC
archive in an EMPTY dedicated app root while its vhost is disabled.

Only the separate, operator-authorized ci/activate_app_site.py --apply may
enable HTTPS for this host. Never use this staging path for final releases.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import tarfile
import tempfile

if __package__:
    from .verify_platform_package import validate_web
else:
    from verify_platform_package import validate_web

APP_ROOT = Path("/var/www/app.genycaster.xyz/public")
APP_VHOST = Path("/etc/nginx/sites-enabled/app.genycaster.xyz")
VERSION = "1.0.0-rc1"


def verify_checksum(archive: Path, checksums: Path) -> str:
    """Fail closed unless the official bundle manifest names this exact file."""
    entries: list[tuple[str, str]] = []
    for line in checksums.read_text(encoding="utf-8").splitlines():
        fields = line.strip().split(maxsplit=1)
        if len(fields) != 2:
            raise ValueError("Invalid SHA256SUMS entry")
        digest, filename = fields
        if filename.startswith("*"):
            filename = filename[1:]
        if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise ValueError("Invalid SHA-256 checksum")
        entries.append((filename, digest))
    matching = [digest for name, digest in entries if name == archive.name]
    if len(matching) != 1:
        raise ValueError("Exactly one matching artifact SHA256SUMS entry is required")
    hasher = hashlib.sha256()
    with archive.open("rb") as data:
        for block in iter(lambda: data.read(1024 * 1024), b""):
            hasher.update(block)
    if hasher.hexdigest() != matching[0]:
        raise ValueError("WASM bundle SHA-256 differs from successful CI artifact")
    return matching[0]


def verify_archive(archive: Path, checksums: Path) -> tuple[str, list[str]]:
    if not archive.is_file() or archive.is_symlink():
        raise ValueError("Missing or symlinked preview archive")
    digest = verify_checksum(archive, checksums)
    validate_web(archive)
    with tarfile.open(archive, "r:gz") as bundle:
        manifest_member = bundle.extractfile("genycaster-assets.json")
        if manifest_member is None:
            raise ValueError("Missing WASM manifest")
        manifest = json.load(manifest_member)
        if manifest.get("appVersion") != VERSION:
            raise ValueError("Unexpected WASM preview version")
        names = [member.name for member in bundle]
        if "genycaster.html" not in names:
            raise ValueError("Missing browser entrypoint")
    return digest, names


def check_root(web_root: Path, *, offline: bool) -> None:
    if web_root != APP_ROOT:
        raise ValueError("Preview staging may only target app.genycaster.xyz")
    if not offline:
        raise ValueError("Only explicit --offline-stage is supported")
    if APP_VHOST.exists() or APP_VHOST.is_symlink():
        raise ValueError("Refusing offline stage: a dedicated Nginx vhost already exists")
    if not web_root.is_dir() or web_root.is_symlink():
        raise ValueError("Missing or symlinked app web root")
    if any(web_root.iterdir()):
        raise ValueError("Offline preview may only be staged in an empty app root")


def extract_to_stage(archive: Path, staging: Path) -> list[str]:
    paths = []
    with tarfile.open(archive, "r:gz") as bundle:
        for member in bundle:
            name = PurePosixPath(member.name)
            if (not member.isfile() or name.is_absolute()
                    or ".." in name.parts or not name.parts
                    or any(part.startswith(".") for part in name.parts)):
                raise ValueError("Unsafe WASM artifact member")
            destination = staging.joinpath(*name.parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            stream = bundle.extractfile(member)
            if stream is None:
                raise ValueError(f"Missing content for {member.name}")
            with stream, destination.open("wb") as out:
                shutil.copyfileobj(stream, out, length=1024 * 1024)
            destination.chmod(0o644)
            paths.append(member.name)
    return paths


def publish_once(stage: Path, root: Path, names: list[str]) -> None:
    """Promote files with the entrypoint LAST; undo on a partial failure."""
    promoted: list[Path] = []
    order = [name for name in names if name != "genycaster.html"]
    order.append("genycaster.html")
    try:
        for name in order:
            source = stage / name
            destination = root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists() or destination.is_symlink():
                raise RuntimeError(f"Destination changed during staging: {name}")
            os.replace(source, destination)
            promoted.append(destination)
    except Exception:
        for target in reversed(promoted):
            if target.is_file() and not target.is_symlink():
                target.unlink()
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--checksums", type=Path, required=True)
    parser.add_argument("--web-root", type=Path, required=True)
    parser.add_argument("--offline-stage", action="store_true",
                        help="Stage in empty app root while dedicated vhost is absent")
    args = parser.parse_args()
    try:
        root = args.web_root
        check_root(root, offline=args.offline_stage)
        archive = args.archive.resolve(strict=True)
        checksums = args.checksums.resolve(strict=True)
        digest, names = verify_archive(archive, checksums)
        with tempfile.TemporaryDirectory(
                prefix=".genycaster-wasm-preview-", dir=root.parent) as temp:
            staging = Path(temp)
            staged = extract_to_stage(archive, staging)
            if set(staged) != set(names) or len(staged) != len(names):
                raise ValueError("Extracted inventory mismatch")
            check_root(root, offline=True)
            publish_once(staging, root, names)
        print(f"STAGED PREVIEW: {VERSION}, {len(names)} verified files")
        print(f"CI artifact SHA-256: {digest}")
        print(f"Root: {root}")
        print("NOT LIVE until the dedicated Nginx app vhost is activated.")
        print("This is a PREVIEW from prior successful CI, not a current-HEAD release.")
        return 0
    except (OSError, ValueError, RuntimeError, tarfile.TarError) as error:
        print(f"WASM preview staging failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
