#!/usr/bin/env python3
"""Post-link optional immutable WebAssembly URL and public asset manifest.

Run only after Emscripten emitted all assets and Qt's HTML was replaced.
Idempotent; never reads or copies user data or a .git directory.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil

parser = argparse.ArgumentParser()
parser.add_argument("--dir", required=True, type=Path)
parser.add_argument("--source", type=Path, help="Refresh public source assets without relinking WASM")
args = parser.parse_args()
directory = args.dir.resolve()
if args.source:
    source=args.source.resolve()
    for rel in (
        "genycaster-shell.html", "genycaster-loader.js", "genycaster-sw.js",
        "genycaster.webmanifest", "genycaster-icon.svg", "secure_signer_vault.mjs",
        "farcaster_protocol.mjs", "browser_protocol_signer.mjs",
        "browser_device_session.mjs", "farcaster_recovery.mjs",
        "recovery-phrase-dialog.mjs",
        "vendor/noble-secp256k1.mjs", "vendor/bip39-english.mjs",
        "vendor/noble-secp256k1.LICENSE", "vendor/scure-bip39.LICENSE",
    ):
        origin=source/rel
        dest=directory/("farcaster_client.html" if rel=="genycaster-shell.html" else rel)
        if not origin.is_file():
            parser.error(f"Missing public asset: {origin}")
        dest.parent.mkdir(parents=True, exist_ok=True)
        if not dest.exists() or dest.read_bytes()!=origin.read_bytes():
            shutil.copy2(origin,dest)
wasm = directory / "farcaster_client.wasm"
if not wasm.is_file():
    parser.error("farcaster_client.wasm is missing")

def digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()

hash_full = digest(wasm)
wasm_short = hash_full[:20]
asset_name = f"farcaster_client.{wasm_short}.wasm"
target = directory / asset_name
if not target.exists() or target.stat().st_size != wasm.stat().st_size:
    shutil.copy2(wasm, target)
# Keep older hashed binaries until operators retire older deployments:
# tabs that loaded an older JS version can still request those exact URLs.
# Never auto-delete an active content-addressed release during packaging.
public = ["farcaster_client.html", "qtloader.js", "farcaster_client.js",
          "genycaster-loader.js", "genycaster-sw.js",
          "genycaster.webmanifest", "genycaster-icon.svg",
          "recovery-phrase-dialog.mjs", "farcaster_recovery.mjs",
          "browser_protocol_signer.mjs", "browser_device_session.mjs",
          "farcaster_protocol.mjs", "secure_signer_vault.mjs",
          "vendor/noble-secp256k1.mjs", "vendor/bip39-english.mjs",
          "miniapp-host-pre.js"]
public = [name for name in public if (directory / name).is_file()]
checksums = {name: digest(directory / name) for name in public}
fingerprint = hashlib.sha256(json.dumps(checksums, sort_keys=True).encode()).hexdigest()[:20]
release = {"version": fingerprint, "wasm": asset_name,
           "files": sorted(public + [asset_name, "genycaster-assets.js"]),
           "integrity": checksums}
manifest = directory / "genycaster-assets.json"
manifest.write_text(json.dumps(release, indent=2, sort_keys=True) + "\n", encoding="utf-8")
# JS config is intentionally small and contains no user data or secrets.
(directory / "genycaster-assets.js").write_text(
    "window.__genycasterAssets=" + json.dumps({"version": fingerprint, "wasm": asset_name}) + ";\n",
    encoding="utf-8")
print("Genycaster immutable WebAssembly:", asset_name)
