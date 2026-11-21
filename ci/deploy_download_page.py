#!/usr/bin/env python3
"""Deploy Genycaster's dedicated /download page without touching WASM or Release assets.

Only the three public /download files and the two explicit Nginx route clauses
are modified. A source-controlled Nginx template retains the same route.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

REPO = Path("/opt/genycaster/repository")
SITE = Path("/var/www/genycaster.xyz/public")
BASE = SITE.parent
SOURCE = REPO / "ui/wasm/download"
VHOST = Path("/etc/nginx/sites-available/genycaster.xyz")
ENABLED = Path("/etc/nginx/sites-enabled/genycaster.xyz")
TEMPLATE = REPO / "ci/primary_genycaster_nginx.conf"
FILES = ("index.html", "download.css", "download.js")
OLD_ROUTE = (
    "    location = /download { return 302 /about/#downloads; }\n"
    "    location = /download/ { return 302 /about/#downloads; }"
)
NEW_ROUTE = (
    '    # Dedicated download hub, independent of the WASM app and legacy /about.\n'
    '    location = /download {\n'
    '        add_header Cache-Control "no-store" always;\n'
    '        try_files /download/index.html =404;\n'
    '    }\n'
    '    location = /download/ {\n'
    '        add_header Cache-Control "no-store" always;\n'
    '        try_files /download/index.html =404;\n'
    '    }'
)


def run(*command: str) -> str:
    output = subprocess.run(command, check=True, capture_output=True, text=True, timeout=25)
    return output.stdout.strip()


def check() -> str:
    if SITE.is_symlink() or not SITE.is_dir():
        raise RuntimeError("Canonical website root must be a real directory")
    if VHOST.is_symlink() or not VHOST.is_file():
        raise RuntimeError("Canonical Nginx vhost is missing or redirected")
    if not ENABLED.is_symlink() or ENABLED.resolve() != VHOST:
        raise RuntimeError("Enabled canonical vhost must reference its owned file")
    original = VHOST.read_text(encoding="utf-8")
    if (original.count("server_name genycaster.xyz;") != 2
            or "location ^~ /api/v1/cloud/" not in original
            or "root /var/www/genycaster.xyz/public;" not in original
            or "location = /about/" not in original):
        raise RuntimeError("Unexpected Nginx vhost; refusing non-download changes")
    if original.count(OLD_ROUTE) != 1 and original.count(NEW_ROUTE) != 1:
        raise RuntimeError("Download route is not recognized")
    if NEW_ROUTE not in TEMPLATE.read_text(encoding="utf-8"):
        raise RuntimeError("Source Nginx template is not updated")
    for name in FILES:
        path = SOURCE / name
        if not path.is_file() or path.is_symlink() or path.stat().st_size < 100:
            raise RuntimeError("Missing standalone download resource: " + name)
        target = SITE / "download" / name
        if target.is_symlink():
            raise RuntimeError("Refusing symlinked download resource: " + name)
    html = (SOURCE / "index.html").read_text(encoding="utf-8")
    js = (SOURCE / "download.js").read_text(encoding="utf-8")
    if ('id="download-primary"' not in html or
            "/download/download.css?v=1" not in html or
            'fetch("/releases/native.json"' not in js or
            "verifiedRecord(" not in js):
        raise RuntimeError("Missing verified native installer UX")
    if not (SITE / "releases/native.json").is_file():
        raise RuntimeError("Canonical native release catalog missing")
    return original


def write_atomic(path: Path, payload: bytes) -> None:
    if path.is_symlink():
        raise RuntimeError("Refusing symlink: " + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".genycaster-download.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as file:
            file.write(payload)
            file.flush()
            os.fsync(file.fileno())
        os.chmod(temporary, 0o644)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def verify_https() -> None:
    for filename, marker in (
        ("/download", 'id="download-primary"'),
        ("/download/", 'id="download-primary"'),
        ("/download/download.js?v=1", "verifiedRecord("),
        ("/download/download.css?v=1", "corner-shape:superellipse(4)"),
    ):
        response = run(
            "curl", "--fail", "--silent", "--show-error", "--max-time", "14",
            "--resolve", "genycaster.xyz:443:127.0.0.1",
            "https://genycaster.xyz" + filename,
        )
        if marker not in response:
            raise RuntimeError("Canonical HTTPS route contains wrong asset: " + filename)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    options = parser.parse_args()
    original = check()
    if not options.apply:
        print("Download assets, manifest and Nginx route validated (dry run)")
        return
    backup = Path(tempfile.mkdtemp(prefix=".genycaster-download-backup.", dir=BASE))
    touched: list[tuple[Path, Path | None]] = []
    try:
        for name in FILES:
            target = SITE / "download" / name
            prior = backup / name if target.is_file() else None
            if prior:
                shutil.copy2(target, prior)
            write_atomic(target, (SOURCE / name).read_bytes())
            touched.append((target, prior))
        if OLD_ROUTE in original:
            prior = backup / "genycaster.xyz.conf"
            shutil.copy2(VHOST, prior)
            write_atomic(VHOST, original.replace(OLD_ROUTE, NEW_ROUTE).encode("utf-8"))
            touched.append((VHOST, prior))
        run("nginx", "-t")
        run("systemctl", "reload", "nginx")
        verify_https()
        print("PUBLISHED: https://genycaster.xyz/download")
        print("Changed only download page and canonical Nginx download route")
        print("BACKUP:", backup)
    except Exception:
        for target, prior in reversed(touched):
            if prior is not None and prior.is_file():
                write_atomic(target, prior.read_bytes())
            elif target.is_file():
                target.unlink()
        run("nginx", "-t")
        run("systemctl", "reload", "nginx")
        shutil.rmtree(backup)
        raise


if __name__ == "__main__":
    main()
