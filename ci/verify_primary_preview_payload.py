#!/usr/bin/env python3
"""Validate a fully regenerated canonical WASM preview without tar recompression."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

if __package__:
    from .package_web import select_files
else:
    from package_web import select_files

ORIGIN = "https://genycaster.xyz"


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate(root: Path) -> dict:
    files = select_files(root)
    root = root.resolve(strict=True)
    manifest = json.loads((root / "genycaster-assets.json").read_text(encoding="utf-8"))
    names = manifest["files"]
    inventory = set(names)
    required = {"native-downloads.js", "native-downloads.css", "releases/native.json",
                "genycaster.html", "genycaster.js", "deployment_config.mjs"}
    if not required.issubset(inventory):
        raise ValueError("Missing primary-site native download/identity assets")
    if len(files) != len(inventory) + 2:
        raise ValueError("Primary WASM inventory mismatch")
    if manifest.get("appVersion") != "1.0.0-rc1":
        raise ValueError("Expected reviewed preview app version")

    checksums = manifest.get("integrity")
    engine = manifest.get("wasm")
    compressed = manifest.get("wasmGzip")
    if (not isinstance(checksums, dict)
            or not isinstance(engine, str)
            or not re.fullmatch(r"genycaster\.[a-f0-9]{20}\.wasm", engine)
            or compressed != engine + ".gz"):
        raise ValueError("Broken WASM manifest")
    if set(checksums) != inventory - {engine, "genycaster-assets.js"}:
        raise ValueError("Manifest is missing expected asset integrity hashes")
    for name, digest in checksums.items():
        if not isinstance(digest, str) or not re.fullmatch(r"[a-f0-9]{64}", digest):
            raise ValueError("Invalid asset checksum value")
        if sha(root / name) != digest:
            raise ValueError(f"Artifact integrity mismatch: {name}")
    engine_digest = sha(root / engine)
    if engine_digest[:20] != engine.split(".")[1]:
        raise ValueError("Immutable WASM hash mismatch")
    if sha(root / "genycaster.wasm") != engine_digest:
        raise ValueError("Stable WASM differs from content-addressed engine")
    if (manifest["wasmBytes"] != (root / engine).stat().st_size
            or manifest["wasmGzipBytes"] != (root / compressed).stat().st_size
            or manifest["wasmGzipSha256"] != sha(root / compressed)):
        raise ValueError("WASM sidecar/hash mismatch")

    config = (root / "deployment_config.mjs").read_text(encoding="utf-8")
    prefix = "export const deployment = Object.freeze("
    if not config.startswith("// GENERATED;") or prefix not in config or not config.rstrip().endswith(");"):
        raise ValueError("Malformed generated origin module")
    options = json.loads(config.split(prefix, 1)[1].rsplit(");", 1)[0])
    if any(options.get(k) != ORIGIN for k in ("publicOrigin", "siwfOrigin")):
        raise ValueError("Canonical origin or SIWF authority mismatch")
    catalog = json.loads((root / "releases/native.json").read_text(encoding="utf-8"))
    if catalog.get("schema") != 1 or not isinstance(catalog.get("platforms"), dict):
        raise ValueError("Malformed native releases catalog")
    html = (root / "genycaster.html").read_text(encoding="utf-8")
    if ("native-downloads.js?v=" not in html
            or "native-downloads.css?v=" not in html
            or "genycaster-loader.js?v=" not in html):
        raise ValueError("Browser HTML is missing versioned boot and download assets")
    if (root / engine).open("rb").read(4) != b"\x00asm":
        raise ValueError("Invalid WASM magic")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    manifest = validate(args.root)
    print(f"PRIMARY PREVIEW VERIFIED: {manifest['appVersion']} "
          f"{len(manifest['files'])} assets, origin {ORIGIN}")


if __name__ == "__main__":
    main()
