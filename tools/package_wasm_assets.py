#!/usr/bin/env python3
"""Post-link optional immutable WebAssembly URL and public asset manifest.

Run only after Emscripten emitted all assets and Qt's HTML was replaced.
Idempotent; never reads or copies user data or a .git directory.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil

parser = argparse.ArgumentParser()
parser.add_argument("--dir", required=True, type=Path)
parser.add_argument("--app-version", default="", help="CMake PROJECT_VERSION; omit when packaging standalone fixtures")
parser.add_argument("--release-label", default="", help="Public release designation, e.g. Beta 1.0")
parser.add_argument("--source", type=Path, help="Refresh public source assets without relinking WASM")
parser.add_argument("--release-notes", type=Path, help="Curated release metadata shipped with this build")
args = parser.parse_args()
directory = args.dir.resolve()
if args.source:
    source=args.source.resolve()
    for rel in (
        "genycaster-brand.svg", "genycaster-wordmark.svg", "genycaster-wordmark-dark.svg", "genycaster-boot-icon.svg", "intro-base.svg", "intro-farcaster.svg", "intro-hypersnap.svg", "intro-openproof.svg", "genycaster-update-icon.svg", "genycaster-shell.html", "genycaster-loader.js", "genycaster-sw.js",
        "apple-touch-icon.png", "apple-touch-icon-precomposed.png",
        "favicon.ico", "favicon-16x16.png", "favicon-32x32.png", "favicon-48x48.png",
        "genycaster-192.png", "genycaster-512.png",
        "genycaster.webmanifest", "genycaster-icon.svg", "secure_signer_vault.mjs",
        "farcaster_protocol.mjs", "browser_protocol_signer.mjs",
        "browser_device_session.mjs", "browser_media_upload.mjs", "farcaster_recovery.mjs",
        "openproof_recovery.mjs", "recovery-phrase-dialog.mjs",
        "vendor/noble-secp256k1.mjs", "vendor/bip39-english.mjs",
        "vendor/noble-secp256k1.LICENSE", "vendor/scure-bip39.LICENSE",
        "vendor/hls-1.6.15.min.js", "vendor/hls-1.6.15.LICENSE",
    ):
        origin=(source/"genycaster-icon.svg"
                if rel=="genycaster-brand.svg" else
                source.parent/"assets/icons/genyleap/icons/actions/repost.svg"
                if rel=="genycaster-update-icon.svg" else source/rel)
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
          "genycaster.webmanifest", "genycaster-icon.svg", "genycaster-brand.svg", "genycaster-wordmark.svg", "genycaster-wordmark-dark.svg", "genycaster-boot-icon.svg", "intro-base.svg", "intro-farcaster.svg", "intro-hypersnap.svg", "intro-openproof.svg", "genycaster-update-icon.svg",
          "apple-touch-icon.png", "apple-touch-icon-precomposed.png",
        "favicon.ico", "favicon-16x16.png", "favicon-32x32.png", "favicon-48x48.png",
        "genycaster-192.png", "genycaster-512.png",
          "recovery-phrase-dialog.mjs", "farcaster_recovery.mjs",
          "openproof_recovery.mjs", "browser_protocol_signer.mjs", "browser_device_session.mjs",
          "browser_media_upload.mjs", "farcaster_protocol.mjs", "secure_signer_vault.mjs",
          "vendor/noble-secp256k1.mjs", "vendor/bip39-english.mjs",
          "miniapp-host-pre.js", "vendor/hls-1.6.15.min.js",
          "vendor/hls-1.6.15.LICENSE"]
public = [name for name in public if (directory / name).is_file()]

# Cloudflare may cache public .js for hours despite origin revalidation.
# Version the four boot script requests in HTML with the deterministic
# release fingerprint, ensuring new deployments never execute stale JS.
# Strip our own previous stamp first so packaging remains idempotent and the
# release fingerprint has no circular dependency on the HTML it stamps.
html_path = directory / "farcaster_client.html"
scripts = ("qtloader.js", "farcaster_client.js",
           "genycaster-assets.js", "genycaster-loader.js")
html = html_path.read_text(encoding="utf-8")
for script in scripts:
    html = re.sub(
        r'(?<=src=")' + re.escape(script) + r'\?v=[a-f0-9]{20}(?=")',
        script, html)
html_path.write_text(html, encoding="utf-8")
checksums = {name: digest(directory / name) for name in public}
release_notes = None
if args.release_notes:
    notes = json.loads(args.release_notes.read_text(encoding="utf-8"))
    if (not isinstance(notes, dict)
        or notes.get("type") not in {"fix", "improvement", "feature", "security", "maintenance"}
        or not isinstance(notes.get("title"), str) or not notes["title"].strip()
        or not isinstance(notes.get("changes"), list) or not 1 <= len(notes["changes"]) <= 12
        or any(not isinstance(item, str) or not item.strip() or len(item) > 300 for item in notes["changes"])):
        parser.error("Release notes must have a type, title and 1..12 bounded changes")
    release_notes = {"type": notes["type"], "title": notes["title"][:120],
                     "changes": [item.strip() for item in notes["changes"]]}
# Include the semantic version in the release fingerprint as well as files:
# a version-only update must be visible to the PWA cache update protocol.
fingerprint = hashlib.sha256(json.dumps({
    "files": checksums, "wasmSha256": hash_full,
    "appVersion": args.app_version, "releaseLabel": args.release_label,
    "releaseNotes": release_notes,
}, sort_keys=True).encode()).hexdigest()[:20]
for script in scripts:
    html = html.replace(f'src="{script}"',
                        f'src="{script}?v={fingerprint}"')
html_path.write_text(html, encoding="utf-8")
# The release identity hashes the canonical HTML; integrity verifies the
# actual stamped HTML delivered to browsers.
checksums["farcaster_client.html"] = digest(html_path)
release = {"version": fingerprint, "wasm": asset_name,
           "wasmBytes": wasm.stat().st_size,
           "buildId": fingerprint[:8],
           "files": sorted(public + [asset_name, "genycaster-assets.js"]),
           "integrity": checksums}
if args.app_version:
    release["appVersion"] = args.app_version
if args.release_label:
    release["releaseLabel"] = args.release_label
if release_notes:
    release["releaseNotes"] = release_notes
manifest = directory / "genycaster-assets.json"
manifest.write_text(json.dumps(release, indent=2, sort_keys=True) + "\n", encoding="utf-8")
# JS config is intentionally small and contains no user data or secrets.
(directory / "genycaster-assets.js").write_text(
    "window.__genycasterAssets=" + json.dumps({
        "version": fingerprint, "wasm": asset_name,
        "wasmBytes": wasm.stat().st_size, "buildId": fingerprint[:8],
        **({"appVersion": args.app_version} if args.app_version else {}),
        **({"releaseLabel": args.release_label} if args.release_label else {}),
    }) + ";\n",
    encoding="utf-8")
print("Genycaster immutable WebAssembly:", asset_name)
