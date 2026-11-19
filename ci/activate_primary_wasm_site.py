#!/usr/bin/env python3
"""Switch canonical genycaster.xyz from landing to its verified WASM app.

Operator/root only. Requires a complete WASM preview in the existing primary
web root and the historical landing saved at /about/index.html. Never touch
the app.genycaster.xyz host or other unrelated virtual hosts.
"""
from __future__ import annotations

import argparse
import http.client
import json
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.request

from ci.activate_app_site import atomic_write

HOST = "genycaster.xyz"
BASE = Path("/var/www/genycaster.xyz")
ROOT = BASE / "public"
AVAILABLE = Path("/etc/nginx/sites-available/genycaster.xyz")
ENABLED = Path("/etc/nginx/sites-enabled/genycaster.xyz")
SOURCE = Path(__file__).with_name("primary_genycaster_nginx.conf")
CERT = Path("/etc/letsencrypt/live/genycaster.xyz/fullchain.pem")
KEY = Path("/etc/letsencrypt/live/genycaster.xyz/privkey.pem")
MARKER = "genycaster-main-v1"
PROBE = f"https://{HOST}/.well-known/genycaster-ready"
MANAGED = "# Managed by GenyCaster primary-domain migration.\n"
BACKUP = BASE / "primary-wasm-previous-nginx.conf"


def check_assets() -> None:
    if ROOT.is_symlink() or not ROOT.is_dir():
        raise RuntimeError("Missing dedicated primary web root")
    index = ROOT / "about/index.html"
    html = ROOT / "genycaster.html"
    manifest_path = ROOT / "genycaster-assets.json"
    config_path = ROOT / "deployment_config.mjs"
    catalog = ROOT / "releases/native.json"
    for file in (index, html, manifest_path, config_path, catalog):
        if file.is_symlink() or not file.is_file() or file.stat().st_size == 0:
            raise RuntimeError(f"Missing, empty or symlinked required asset: {file}")
    if "GenyCaster" not in index.read_text(encoding="utf-8"):
        raise RuntimeError("Landing-page preservation at /about is invalid")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    public_files = manifest.get("files")
    if not isinstance(public_files, list) or not public_files:
        raise RuntimeError("Missing release asset inventory")
    if any(not isinstance(name, str) or not (ROOT / name).is_file()
           for name in public_files):
        raise RuntimeError("Incomplete primary WebAssembly asset inventory")
    if "native-downloads.js" not in public_files or "native-downloads.css" not in public_files:
        raise RuntimeError("Native download dialog missing from WASM release inventory")
    from ci.stage_wasm_preview import verify_app_origin_module
    # The generic stage verifier targets app.genycaster.xyz; a primary
    # migration uses the separate canonical assertion below.
    content = config_path.read_text(encoding="utf-8")
    prefix = "export const deployment = Object.freeze("
    if not content.startswith("// GENERATED;") or prefix not in content or not content.rstrip().endswith(");"):
        raise RuntimeError("Noncanonical browser deployment config")
    values = json.loads(content.split(prefix, 1)[1].rsplit(");", 1)[0])
    if any(values.get(key) != "https://genycaster.xyz"
           for key in ("publicOrigin", "siwfOrigin")):
        raise RuntimeError("WASM build origins do not match canonical site")
    native = json.loads(catalog.read_text(encoding="utf-8"))
    if native.get("schema") != 1 or not isinstance(native.get("platforms"), dict):
        raise RuntimeError("Native release catalog is malformed")
    if "native-downloads.js" not in html.read_text(encoding="utf-8"):
        raise RuntimeError("WASM entrypoint does not load native downloads")


def verify_existing_site() -> str:
    if AVAILABLE.is_symlink() or not AVAILABLE.is_file():
        raise RuntimeError("Expected existing standalone canonical vhost file")
    if not ENABLED.is_symlink() or ENABLED.resolve() != AVAILABLE:
        raise RuntimeError("Canonical host vhost link is not owned by this installer")
    previous = AVAILABLE.read_text(encoding="utf-8")
    if (previous.count(f"server_name {HOST};") != 2
            or "location ^~ /api/v1/cloud/" not in previous
            or "ssl_certificate /etc/letsencrypt/live/genycaster.xyz/fullchain.pem;" not in previous):
        raise RuntimeError("Refusing to overwrite unknown or altered canonical Nginx site")
    return previous


def check_template() -> str:
    source = SOURCE.read_text(encoding="utf-8")
    if (source.count(f"server_name {HOST};") != 2
            or 'return 200 "genycaster-main-v1";' not in source
            or "location = /about/" not in source
            or "location ^~ /api/v1/cloud/" not in source
            or "root /var/www/genycaster.xyz/public;" not in source
            or "server_name app.genycaster.xyz" in source):
        raise RuntimeError("Unsafe or incomplete primary Nginx template")
    return MANAGED + source


def read_origin(path: str) -> bytes:
    ctx = ssl.create_default_context()
    with socket.create_connection(("127.0.0.1", 443), timeout=7) as raw:
        with ctx.wrap_socket(raw, server_hostname=HOST) as session:
            session.settimeout(7)
            session.sendall((f"GET {path} HTTP/1.1\r\n"
                             f"Host: {HOST}\r\nConnection: close\r\n\r\n").encode("ascii"))
            response = http.client.HTTPResponse(session)
            response.begin()
            if response.status != 200:
                raise RuntimeError(f"Primary origin {path}: HTTP {response.status}")
            return response.read(512)


def check_origin() -> None:
    last_error: Exception | None = None
    for attempt in range(20):
        try:
            if read_origin("/.well-known/genycaster-ready") != MARKER.encode():
                raise RuntimeError("Primary Nginx readiness marker mismatch")
            if b"<html" not in read_origin("/").lower():
                raise RuntimeError("Primary Nginx entrypoint is not WASM HTML")
            if b"<html" not in read_origin("/about/").lower():
                raise RuntimeError("Historical landing page not served at /about")
            return
        except (OSError, RuntimeError, http.client.HTTPException) as error:
            last_error = error
            if attempt < 19:
                time.sleep(0.5)
    raise RuntimeError(f"Canonical local TLS origin did not activate: {last_error}")


def check_public() -> bool:
    req = urllib.request.Request(PROBE, headers={
        "User-Agent": "curl/8.5.0", "Accept": "*/*", "Cache-Control": "no-cache"})
    try:
        with urllib.request.urlopen(req, timeout=15) as result:
            valid = (result.status == 200 and result.geturl() == PROBE
                     and result.read(64) == MARKER.encode())
    except (OSError, urllib.error.URLError):
        valid = False
    return valid


def activate() -> bool:
    if not hasattr(__import__("os"), "geteuid") or __import__("os").geteuid() != 0:
        raise RuntimeError("Explicit root permission required to update Nginx")
    check_assets()
    new_config = check_template()
    previous = verify_existing_site()
    if not CERT.is_file() or not KEY.is_file():
        raise RuntimeError("Canonical site's public TLS certificate is missing")
    for binary in ("nginx", "systemctl", "openssl"):
        if not shutil.which(binary):
            raise RuntimeError(f"Required tool is unavailable: {binary}")
    subprocess.run(["openssl", "x509", "-in", str(CERT),
                    "-noout", "-checkhost", HOST], check=True)
    subprocess.run(["openssl", "x509", "-in", str(CERT),
                    "-noout", "-checkend", "604800"], check=True)

    if previous != new_config:
        if BACKUP.exists() and BACKUP.read_text(encoding="utf-8") != previous:
            raise RuntimeError("An unexpected previous-config backup exists")
        if not BACKUP.exists():
            atomic_write(BACKUP, previous)
            BACKUP.chmod(0o600)
        try:
            atomic_write(AVAILABLE, new_config)
            subprocess.run(["nginx", "-t"], check=True)
            subprocess.run(["systemctl", "reload", "nginx"], check=True)
            check_origin()
        except Exception:
            atomic_write(AVAILABLE, previous)
            subprocess.run(["nginx", "-t"], check=True)
            subprocess.run(["systemctl", "reload", "nginx"], check=True)
            raise
    else:
        check_origin()

    print("LOCAL READY: https://genycaster.xyz/ (WASM); /about/ (landing)")
    if not check_public():
        print("LOCAL READY, PUBLIC CDN ROUTE UNVERIFIED; preserving validated vhost",
              file=sys.stderr)
        return False
    print(f"PUBLIC READY: {PROBE} = {MARKER}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    try:
        check_template()
        if not args.apply:
            print("DRY RUN: reviewed primary Nginx template valid")
            print("Activation requires verified WASM assets, preserved landing, root access.")
            return 0
        return 0 if activate() else 2
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError,
            json.JSONDecodeError) as error:
        print(f"Primary WASM activation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
