#!/usr/bin/env python3
"""Post-link optional immutable WebAssembly URL and public asset manifest.

Run only after Emscripten emitted all assets and Qt's HTML was replaced.
Idempotent; never reads or copies user data or a .git directory.
"""
from __future__ import annotations
import argparse
import gzip
import hashlib
import json
import re
from pathlib import Path
import shutil
from release_notes import load_release

parser = argparse.ArgumentParser()
parser.add_argument("--dir", required=True, type=Path)
parser.add_argument("--app-version", default="", help="CMake PROJECT_VERSION; omit when packaging standalone fixtures")
parser.add_argument("--product-name", default="", help="CMake project product name")
parser.add_argument("--version-number", default="", help="Numeric CMake project version")
parser.add_argument("--release-channel", default="", help="CMake channel: stable, alpha, beta, rc")
parser.add_argument("--release-channel-number", default="", help="CMake prerelease index")
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
        "native-downloads.js", "native-downloads.css", "native-releases.json",
        "download/index.html", "download/download.css", "download/download.js",
        "farcaster_protocol.mjs", "browser_protocol_signer.mjs",
        "browser_device_session.mjs", "browser_account_session.mjs", "browser_media_upload.mjs", "farcaster_recovery.mjs", "farcaster_custody_recovery.mjs",
        "openproof_recovery.mjs", "recovery-phrase-dialog.mjs",
        "hypersnap_native_account.mjs", "hypersnap_native_pow_worker.mjs", "farcaster_new_account.mjs",
        "browser_account_create_dialog.mjs",
        "vendor/noble-secp256k1.mjs", "vendor/bip39-english.mjs",
        "vendor/noble-secp256k1.LICENSE", "vendor/scure-bip39.LICENSE",
        "vendor/hls-1.6.15.min.js", "vendor/hls-1.6.15.LICENSE",
    ):
        origin=(source/"genycaster-icon.svg"
                if rel=="genycaster-brand.svg" else
                (source/rel if (source/rel).is_file()
                 else source.parent.parent/"properties/WebAssembly/genycaster.webmanifest")
                if rel=="genycaster.webmanifest" else
                source.parent/"assets/icons/genyleap/icons/actions/repost.svg"
                if rel=="genycaster-update-icon.svg" else source/rel)
        dest=directory/("genycaster.html" if rel=="genycaster-shell.html" else
                        "releases/native.json" if rel=="native-releases.json" else rel)
        if not origin.is_file():
            parser.error(f"Missing public asset: {origin}")
        dest.parent.mkdir(parents=True, exist_ok=True)
        if not dest.exists() or dest.read_bytes()!=origin.read_bytes():
            shutil.copy2(origin,dest)
wasm = directory / "genycaster.wasm"
if not wasm.is_file():
    parser.error("genycaster.wasm is missing")

# Audit the actual linked binary, not just the source/CMake transform. A
# release once embedded the native QSB WebGL1 fragment despite the portable
# shader branch existing in source; every visible NEON effect then failed to
# compile on browsers without OES_standard_derivatives. Reject this artifact
# before hashing, caching, or publishing it. Search in bounded chunks.
forbidden_shader = b"#extension GL_OES_standard_derivatives : require"
with wasm.open("rb") as stream:
    overlap = b""
    while block := stream.read(1024 * 1024):
        if forbidden_shader in overlap + block:
            parser.error("WASM contains a derivatives-dependent WebGL1 shader; rebuild with portable QSB resources")
        overlap = block[-(len(forbidden_shader) - 1):]

def digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()

hash_full = digest(wasm)
wasm_short = hash_full[:20]
asset_name = f"genycaster.{wasm_short}.wasm"
target = directory / asset_name
if not target.exists() or target.stat().st_size != wasm.stat().st_size:
    shutil.copy2(wasm, target)
# Ship a deterministic, precompressed HTTP representation of the *same* URL.
# nginx gzip_static serves it with Content-Encoding: gzip and the original
# application/wasm MIME, so WebKit decodes it transparently before compilation.
# The gzip sidecar is a transport detail, not a second WASM resource URL.
compressed = directory / (asset_name + ".gz")
if not compressed.is_file():
    temporary = directory / (asset_name + ".gz.tmp")
    try:
        with target.open("rb") as input_file, temporary.open("wb") as output_file:
            with gzip.GzipFile(filename="", mode="wb", fileobj=output_file,
                               compresslevel=6, mtime=0) as zipper:
                shutil.copyfileobj(input_file, zipper, length=1024*1024)
        temporary.replace(compressed)
    finally:
        temporary.unlink(missing_ok=True)
# Keep older hashed binaries until operators retire older deployments:
# tabs that loaded an older JS version can still request those exact URLs.
# Never auto-delete an active content-addressed release during packaging.
public = ["genycaster.html", "qtloader.js", "genycaster.js",
          "genycaster-loader.js", "genycaster-sw.js",
          "genycaster.webmanifest", "genycaster-icon.svg", "genycaster-brand.svg", "genycaster-wordmark.svg", "genycaster-wordmark-dark.svg", "genycaster-boot-icon.svg", "intro-base.svg", "intro-farcaster.svg", "intro-hypersnap.svg", "intro-openproof.svg", "genycaster-update-icon.svg",
          "apple-touch-icon.png", "apple-touch-icon-precomposed.png",
        "favicon.ico", "favicon-16x16.png", "favicon-32x32.png", "favicon-48x48.png",
        "genycaster-192.png", "genycaster-512.png",
          "recovery-phrase-dialog.mjs", "farcaster_recovery.mjs", "farcaster_custody_recovery.mjs",
          "hypersnap_native_account.mjs", "hypersnap_native_pow_worker.mjs", "farcaster_new_account.mjs",
          "browser_account_create_dialog.mjs",
          "openproof_recovery.mjs", "browser_protocol_signer.mjs", "browser_device_session.mjs", "browser_account_session.mjs",
          "browser_media_upload.mjs", "deployment_config.mjs", "farcaster_protocol.mjs", "secure_signer_vault.mjs",
          "native-downloads.js", "native-downloads.css", "releases/native.json",
          "download/index.html", "download/download.css", "download/download.js",
          "vendor/noble-secp256k1.mjs", "vendor/bip39-english.mjs",
          "miniapp-host-pre.js", "miniapp-host-bridge.mjs",
          "genycaster-walletconnect.mjs", "vendor/hls-1.6.15.min.js",
          "vendor/hls-1.6.15.LICENSE"]
# The first-party Mini App bridge imports WalletConnect dynamically. Do not
# publish a superficially valid PWA whose UI offers wallet pairing but whose
# script is absent: otherwise the user sees a generic module-load failure.
# Standalone asset fixtures without a Mini App host remain permitted.
if (directory / "miniapp-host-bridge.mjs").is_file():
    wallet_bundle = directory / "genycaster-walletconnect.mjs"
    if not wallet_bundle.is_file() or wallet_bundle.stat().st_size == 0:
        parser.error("Mini App host requires genycaster-walletconnect.mjs")
public = [name for name in public if (directory / name).is_file()]

# Cloudflare may cache public .js for hours despite origin revalidation.
# Version the four boot script requests in HTML with the deterministic
# release fingerprint, ensuring new deployments never execute stale JS.
# Strip our own previous stamp first so packaging remains idempotent and the
# release fingerprint has no circular dependency on the HTML it stamps.
html_path = directory / "genycaster.html"
scripts = ("qtloader.js", "genycaster.js",
           "genycaster-assets.js", "genycaster-loader.js",
           "miniapp-host-bridge.mjs", "native-downloads.js")
html = html_path.read_text(encoding="utf-8")
# The HTML source never owns a version. CMake passes its authoritative release
# product and version values to this packager independently.
product_placeholder = "@@GENYCASTER_PRODUCT_NAME@@"
if product_placeholder in html:
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9-]*", args.product_name):
        parser.error("Missing valid CMake product name")
    html = html.replace(product_placeholder, args.product_name)
release_placeholder = "@@GENYCASTER_RELEASE_VERSION@@"
if release_placeholder in html:
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)[1-9]\d*)?", args.app_version):
        parser.error("Versioned HTML requires the CMake semantic version")
    html = html.replace(release_placeholder, "v" + args.app_version)
if release_placeholder in html:
    parser.error("Unresolved version placeholder in generated HTML")
for script in scripts:
    html = re.sub(
        r'(?<=src=")' + re.escape(script) + r'\?v=[a-f0-9]{20}(?=")',
        script, html)
html = re.sub(r'(?<=href=")native-downloads\.css\?v=[a-f0-9]{20}(?=")',
              "native-downloads.css", html)
html_path.write_text(html, encoding="utf-8")
checksums = {name: digest(directory / name) for name in public}
# The WebKit runtime explicitly fetches the gzip sidecar. Treat it as a
# required release asset, not an optional local build by-product.
checksums[compressed.name] = digest(compressed)
release_notes = None
if args.release_notes:
    note_path = args.release_notes
    if note_path.suffix.lower() != ".json":
        parser.error("Structured localized release notes must be canonical JSON")
    if not args.app_version or note_path.stem != "v" + args.app_version:
        parser.error("Release-note JSON filename must match the CMake release version")
    try:
        release_notes = load_release(note_path)
    except (ValueError, KeyError, TypeError) as error:
        parser.error(f"Invalid localized release notes: {error}")
if args.app_version:
    suffix = "" if args.release_channel == "stable" else (
        f"-{args.release_channel}{args.release_channel_number}")
    if args.release_channel not in {"stable", "alpha", "beta", "rc"} or (
        args.release_channel != "stable" and not re.fullmatch(
            r"[1-9]\d*", args.release_channel_number)):
        parser.error("Invalid CMake release channel")
    if args.app_version != args.version_number + suffix:
        parser.error("Numeric version/channel fields do not match CMake app version")
# Include the semantic version in the release fingerprint as well as files:
# a version-only update must be visible to the PWA cache update protocol.
fingerprint = hashlib.sha256(json.dumps({
    "files": checksums, "wasmSha256": hash_full,
    "appVersion": args.app_version, "productName": args.product_name,
    "versionNumber": args.version_number, "releaseChannel": args.release_channel,
    "releaseChannelNumber": args.release_channel_number, "releaseNotes": release_notes,
}, sort_keys=True).encode()).hexdigest()[:20]
for script in scripts:
    html = html.replace(f'src="{script}"',
                        f'src="{script}?v={fingerprint}"')
html = html.replace('href="native-downloads.css"',
                    f'href="native-downloads.css?v={fingerprint}"')
html_path.write_text(html, encoding="utf-8")
# The release identity hashes the canonical HTML; integrity verifies the
# actual stamped HTML delivered to browsers.
checksums["genycaster.html"] = digest(html_path)
release = {"version": fingerprint, "wasm": asset_name,
           "wasmBytes": wasm.stat().st_size,
           "wasmGzip": compressed.name,
           "wasmGzipBytes": compressed.stat().st_size,
           "wasmGzipSha256": digest(compressed),
           "buildId": fingerprint[:8],
           "files": sorted(public + [asset_name, compressed.name, "genycaster-assets.js"]),
           "integrity": checksums}
if args.app_version:
    release["appVersion"] = args.app_version
if args.app_version:
    release["productName"] = args.product_name
    release["versionNumber"] = args.version_number
    release["releaseChannel"] = args.release_channel
    release["releaseChannelNumber"] = int(args.release_channel_number or 0)
if release_notes:
    release["releaseNotes"] = release_notes
manifest = directory / "genycaster-assets.json"
manifest.write_text(json.dumps(release, indent=2, sort_keys=True) + "\n", encoding="utf-8")
# JS config is intentionally small and contains no user data or secrets.
(directory / "genycaster-assets.js").write_text(
    "window.__genycasterAssets=" + json.dumps({
        "version": fingerprint, "wasm": asset_name,
        "wasmBytes": wasm.stat().st_size,
        "wasmGzip": compressed.name,
        "wasmGzipBytes": compressed.stat().st_size,
        "buildId": fingerprint[:8],
        **({"appVersion": args.app_version} if args.app_version else {}),
        **({"productName": args.product_name, "versionNumber": args.version_number,
            "releaseChannel": args.release_channel,
            "releaseChannelNumber": int(args.release_channel_number or 0)} if args.app_version else {}),
    }) + ";\n",
    encoding="utf-8")
print("Genycaster immutable WebAssembly:", asset_name)
