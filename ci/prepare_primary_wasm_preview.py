#!/usr/bin/env python3
"""Prepare and install a separately labeled WASM preview at genycaster.xyz.

Requires a successful, signed-off prior CI WASM tarball and its SHA256SUMS,
whose compiled public/SIWF origins both match https://genycaster.xyz.
Only adds new WASM files to the existing landing root. It preserves index.html
and snapshots it at /about/index.html. Does not touch Nginx or publish releases.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

if __package__:
    from .stage_wasm_preview import verify_checksum, extract_to_stage
    from .verify_platform_package import validate_web
    from .verify_primary_preview_payload import validate as validate_prepared
else:
    from stage_wasm_preview import verify_checksum, extract_to_stage
    from verify_platform_package import validate_web
    from verify_primary_preview_payload import validate as validate_prepared

ROOT = Path("/var/www/genycaster.xyz/public")
SITE = ROOT.parent
SOURCE = Path(__file__).resolve().parent.parent / "ui/wasm"
ORIGIN = "https://genycaster.xyz"


def check_previous_bundle(archive: Path, checksums: Path) -> dict:
    verify_checksum(archive, checksums)
    validate_web(archive)
    with tarfile.open(archive, "r:gz") as bundle:
        manifest = json.load(bundle.extractfile("genycaster-assets.json"))
        module = bundle.extractfile("deployment_config.mjs")
        if module is None:
            raise ValueError("Cannot move a bundle missing the generated deployment module")
        text = module.read(8192).decode("utf-8")
        prefix = "export const deployment = Object.freeze("
        if not text.startswith("// GENERATED;") or prefix not in text or not text.rstrip().endswith(");"):
            raise ValueError("Invalid generated deployment authority")
        settings = json.loads(text.split(prefix, 1)[1].rsplit(");", 1)[0])
        if settings.get("publicOrigin") != ORIGIN or settings.get("siwfOrigin") != ORIGIN:
            raise ValueError("Preview compiled for a different site or relying party")
        if manifest.get("appVersion") != "1.0.0-rc1":
            raise ValueError("Only the reviewed v1.0.0-rc1 preview may be installed")
        if manifest.get("productName") not in ("Genycaster", "GenyCaster"):
            raise ValueError("Unknown GenyCaster product")
        if manifest.get("releaseChannel") != "rc" or manifest.get("releaseChannelNumber") != 1:
            raise ValueError("Expected a preview release candidate")
    return manifest


def prepare(archive: Path, checksums: Path, stage: Path) -> tuple[Path, str]:
    if stage.is_symlink() or stage.exists():
        raise ValueError("Preview staging path already exists; refusing overwrite")
    info = check_previous_bundle(archive, checksums)
    with tempfile.TemporaryDirectory(prefix=".primary-wasm-prep-", dir=SITE) as temp:
        temp_dir = Path(temp)
        payload = temp_dir / "payload"
        payload.mkdir()
        extract_to_stage(archive, payload)
        for name, destination in (
            ("genycaster-shell.html", "genycaster.html"),
            ("native-downloads.js", "native-downloads.js"),
            ("native-downloads.css", "native-downloads.css"),
            ("native-releases.json", "releases/native.json"),
        ):
            origin = SOURCE / name
            if origin.is_symlink() or not origin.is_file():
                raise RuntimeError(f"Missing trusted UI source asset {name}")
            target = payload / destination
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(origin, target)
        # Recalculate the HTML boot fingerprint and the *entire* integrity map.
        # The compiled engine remains from the reviewed older CI artifact.
        package_script = Path(__file__).with_name("package_wasm_assets.py")
        subprocess.run([
            sys.executable, str(package_script),
            "--dir", str(payload),
            "--app-version", info["appVersion"],
            "--product-name", info["productName"],
            "--version-number", info["versionNumber"],
            "--release-channel", info["releaseChannel"],
            "--release-channel-number", str(info["releaseChannelNumber"]),
        ], check=True)
        verified = validate_prepared(payload)
        digest = verified["version"]
        shutil.move(str(payload), str(stage))
        return stage, digest


def promote(stage: Path) -> int:
    """Only add new files, never overwrite landing or existing app resources."""
    if not ROOT.is_dir() or ROOT.is_symlink():
        raise RuntimeError("Canonical web root is missing or symlinked")
    landing = ROOT / "index.html"
    if landing.is_symlink() or not landing.is_file():
        raise RuntimeError("Cannot preserve missing canonical landing")
    about = ROOT / "about/index.html"
    if about.exists() or about.is_symlink():
        if not about.is_file() or about.read_bytes() != landing.read_bytes():
            raise RuntimeError("Preexisting /about landing differs; operator review required")
    else:
        about.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(landing, about)

    inventory = validate_prepared(stage)
    assets = inventory["files"] + ["genycaster.wasm", "genycaster-assets.json"]
    promoted = []
    try:
        for name in assets:
            origin = stage / name
            target = ROOT / name
            if target.exists() or target.is_symlink():
                if target.is_symlink() or not target.is_file() or (
                    hashlib.sha256(target.read_bytes()).digest() !=
                    hashlib.sha256(origin.read_bytes()).digest()
                ):
                    raise RuntimeError(f"Existing canonical asset differs: {name}")
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(".new-primary-" + target.name)
            if temporary.exists():
                raise RuntimeError(f"Temporary promotion target exists: {temporary}")
            shutil.copy2(origin, temporary)
            temporary.chmod(0o644)
            os.replace(temporary, target)
            promoted.append(target)
        return len(assets)
    except Exception:
        for target in reversed(promoted):
            target.unlink(missing_ok=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--checksums", type=Path, required=True)
    parser.add_argument("--stage", type=Path, default=SITE / ".wasm-primary-stage")
    parser.add_argument("--promote", action="store_true", help="Also place verified assets in the current web root without replacing index.html")
    args = parser.parse_args()
    try:
        if args.stage != SITE / ".wasm-primary-stage":
            raise ValueError("Staging must remain outside the public primary root")
        if not args.stage.exists():
            staged, digest = prepare(
                args.archive.resolve(strict=True),
                args.checksums.resolve(strict=True),
                args.stage)
            print(f"STAGED: canonical preview, release fingerprint {digest}")
        else:
            staged = args.stage
            print("Using existing primary preview stage")
        if args.promote:
            count = promote(staged)
            print(f"PROMOTED: {count} verified WASM files; landing preserved at /about/index.html")
            print("Nginx still needs the separately reviewed primary-domain activation.")
        return 0
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError,
            tarfile.TarError) as error:
        print(f"Canonical preview preparation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
