#!/usr/bin/env python3
"""Deploy Genycaster's dedicated /download page without touching WASM or Release assets.

The canonical mode updates the four /download assets and exact Nginx routes.
When /etc is read-only, the fallback also patches the historic /about head
with an early, parser-blocking bootstrap so the legacy 302 never paints About.
The source-controlled Nginx template remains the permanent routing solution.
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
FILES = ("index.html", "download.css", "download.js", "about-forward.js")
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
    '    # Exactly one public URL, without a trailing slash or index.html.\n'
    '    location = /download/ { return 308 /download; }\n'
    '    location = /download/index.html { return 308 /download; }'
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
            "/download/download.css?v=3" not in html or
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
    if path.parent == SITE / "download":
        # The operator's umask is private (0700); Nginx still needs to
        # traverse this public directory to serve the static page.
        os.chmod(path.parent, 0o755)
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
        ("/download/download.js?v=3", "verifiedRecord("),
        ("/download/download.css?v=3", "corner-shape:superellipse(4)"),
    ):
        response = run(
            "curl", "--fail", "--silent", "--show-error", "--max-time", "14",
            "--resolve", "genycaster.xyz:443:127.0.0.1",
            "https://genycaster.xyz" + filename,
        )
        if marker not in response:
            raise RuntimeError("Canonical HTTPS route contains wrong asset: " + filename)

    for alias in ("/download/", "/download/index.html"):
        headers = run(
            "curl", "--head", "--silent", "--show-error", "--max-time", "14",
            "--resolve", "genycaster.xyz:443:127.0.0.1",
            "https://genycaster.xyz" + alias,
        )
        lines = headers.splitlines()
        status = lines[0] if lines else ""
        destination = next(
            (line.partition(":")[2].strip() for line in lines
             if line.lower().startswith("location:")), ""
        )
        if " 308 " not in status or not destination.endswith("/download"):
            raise RuntimeError("Noncanonical URL must redirect permanently: " + alias)


def deploy_legacy_route() -> None:
    """Keep /download functional even when the Nginx vhost is read-only.

    The existing server redirect sends /download to /about/#downloads.
    A parser-blocking, same-origin bootstrap suppresses the About paint,
    then mounts the verified download HTML in the same document without
    a second browser navigation. It does not touch Nginx, WASM or APIs.
    """
    historic = SITE / "about/index.html"
    if historic.is_symlink() or not historic.is_file():
        raise RuntimeError("Historic /about landing unavailable")
    before = historic.read_text(encoding="utf-8")
    script_tag = '<script src="/download/about-forward.js?v=3"></script>'
    legacy_tag = '<script src="/download/about-forward.js?v=1" defer></script>'
    prior_tag = '<script src="/download/about-forward.js?v=2"></script>'
    marker = '<meta charset="utf-8">'
    if before.count(marker) != 1 or before.count("</head>") != 1:
        raise RuntimeError("Historic landing HTML layout has changed")
    after = before.replace(legacy_tag, "").replace(prior_tag, "")
    if script_tag not in after:
        # This MUST block parsing in <head>, ahead of all visual resources.
        # Defer/async here reintroduces a visible flash of the About page.
        after = after.replace(marker, marker + chr(10) + "  " + script_tag, 1)
    after = (after.replace('href="#downloads"', 'href="/download"')
                  .replace('href="/download/index.html"', 'href="/download"'))

    backup = Path(tempfile.mkdtemp(prefix=".genycaster-download-backup.", dir=BASE))
    changed: list[tuple[Path, Path | None]] = []
    try:
        for name in FILES:
            target = SITE / "download" / name
            original = backup / name if target.is_file() else None
            if original is not None:
                shutil.copy2(target, original)
            write_atomic(target, (SOURCE / name).read_bytes())
            changed.append((target, original))
        if after != before:
            original = backup / "about-index.html"
            shutil.copy2(historic, original)
            write_atomic(historic, after.encode("utf-8"))
            changed.append((historic, original))
        for route, marker in (
            ("/download/index.html", 'id="download-primary"'),
            ("/download/download.js?v=3", "verifiedRecord("),
            ("/download/download.css?v=3", "corner-shape:superellipse(4)"),
            ("/download/about-forward.js?v=3", "mountDownload"),
            ("/about/", script_tag),
        ):
            response = run("curl", "--fail", "--silent", "--show-error",
                           "--max-time", "14", "--resolve",
                           "genycaster.xyz:443:127.0.0.1",
                           "https://genycaster.xyz" + route)
            if marker not in response:
                raise RuntimeError("Download fallback HTTPS verification failed: " + route)
        print("PUBLISHED download hub, no visible About paint or second page navigation")
        print("PUBLIC PAGE https://genycaster.xyz/download (browser address canonicalized)")
        print("BACKUP:", backup)
    except Exception:
        for target, original in reversed(changed):
            if original is not None and original.is_file():
                write_atomic(target, original.read_bytes())
            else:
                target.unlink(missing_ok=True)
        shutil.rmtree(backup)
        raise


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--legacy-route", action="store_true",
                        help="Deploy through the existing /about/#downloads redirect without changing Nginx")
    options = parser.parse_args()
    if options.apply and options.legacy_route:
        parser.error("Choose exactly one deployment mode")
    original = check()
    if options.legacy_route:
        deploy_legacy_route()
        return
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
