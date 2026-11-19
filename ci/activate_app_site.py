#!/usr/bin/env python3
"""Safely activate the dedicated app.genycaster.xyz Nginx/TLS vhost.

This is an explicit operator-only action. It does not build, publish or copy
WASM assets, alter the separate genycaster.xyz site, or change account data.
Without --apply it only validates the tracked vhost and prints prerequisites.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

HOST = "app.genycaster.xyz"
ROOT = Path("/var/www/app.genycaster.xyz/public")
AVAILABLE = Path("/etc/nginx/sites-available/app.genycaster.xyz")
ENABLED = Path("/etc/nginx/sites-enabled/app.genycaster.xyz")
CERT = Path("/etc/letsencrypt/live/app.genycaster.xyz/fullchain.pem")
KEY = Path("/etc/letsencrypt/live/app.genycaster.xyz/privkey.pem")
SOURCE = Path(__file__).resolve().with_name("app_genycaster_nginx.conf")
MANAGED = "# Managed by GenyCaster ci/activate_app_site.py; do not hand-edit.\n"
MARKER = "genycaster-app-v1"
PROBE = f"https://{HOST}/.well-known/genycaster-ready"


def validate_config(source: str) -> None:
    """Reject a source template that could capture a different domain."""
    if source.count(f"server_name {HOST};") != 2:
        raise ValueError("Expected dedicated HTTP and HTTPS application vhosts")
    if "root /var/www/app.genycaster.xyz/public;" not in source:
        raise ValueError("Dedicated web root missing")
    if f"ssl_certificate {CERT};" not in source:
        raise ValueError("Dedicated TLS certificate missing from template")
    if f"ssl_certificate_key {KEY};" not in source:
        raise ValueError("Dedicated TLS key missing from template")
    if 'return 200 "genycaster-app-v1";' not in source:
        raise ValueError("Dedicated HTTPS readiness marker missing")
    if "server_name genycaster.xyz" in source:
        raise ValueError("Landing site must remain separate")


def http_certificate_bootstrap() -> str:
    """Webroot ACME challenge on port 80 without referencing missing certs."""
    return MANAGED + f"""server {{
    listen 80;
    listen [::]:80;
    server_name {HOST};
    root {ROOT};
    location ^~ /.well-known/acme-challenge/ {{
        default_type text/plain;
        try_files $uri =404;
    }}
    location / {{ return 301 https://{HOST}$request_uri; }}
}}
"""


def run_checked(*arguments: str) -> None:
    subprocess.run(arguments, check=True)


def atomic_write(path: Path, text: str) -> None:
    """Write only the dedicated available-vhost path on the same filesystem."""
    descriptor, tmp = tempfile.mkstemp(prefix=".genycaster-nginx-", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as out:
            out.write(text)
            out.flush()
            os.fsync(out.fileno())
        os.chmod(tmp, 0o644)
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def verify_ownership() -> None:
    """Never overwrite a preexisting site not managed by this procedure."""
    if AVAILABLE.is_symlink():
        raise RuntimeError("Refusing symlinked sites-available target")
    if AVAILABLE.exists():
        if not AVAILABLE.read_text(encoding="utf-8").startswith(MANAGED):
            raise RuntimeError(f"Refusing to modify unmanaged vhost: {AVAILABLE}")
    if ENABLED.exists() or ENABLED.is_symlink():
        if not ENABLED.is_symlink() or ENABLED.resolve() != AVAILABLE:
            raise RuntimeError(f"Refusing to replace existing enabled vhost: {ENABLED}")
    if ROOT.is_symlink() or not ROOT.is_dir():
        raise RuntimeError(f"Missing or symlinked dedicated web root: {ROOT}")
    for part in (AVAILABLE.parent, ENABLED.parent):
        if not part.is_dir():
            raise RuntimeError(f"Nginx configuration directory missing: {part}")


def verify_certificate() -> None:
    if not CERT.is_file() or not KEY.is_file():
        raise RuntimeError("Dedicated app certificate or private key is missing")
    run_checked("openssl", "x509", "-in", str(CERT), "-noout",
                "-checkhost", HOST)
    run_checked("openssl", "x509", "-in", str(CERT), "-noout",
                "-checkend", "604800")


def check_public_marker() -> None:
    # Cloudflare's Browser Integrity Check rejects Python-urllib's default
    # User-Agent with error 1010, even when the HTTPS vhost is correctly
    # configured. Use the ordinary curl User-Agent accepted by this host,
    # while preserving strict TLS verification, exact-origin/no-redirect
    # checks and the exact response-body marker requirement.
    request = urllib.request.Request(
        PROBE,
        headers={"Cache-Control": "no-cache",
                 "User-Agent": "curl/8.5.0",
                 "Accept": "*/*"},
    )
    with urllib.request.urlopen(request, timeout=15) as response:
        if response.geturl() != PROBE:
            raise RuntimeError("Readiness probe unexpectedly redirected")
        if response.status != 200:
            raise RuntimeError(f"Readiness probe returned HTTP {response.status}")
        marker = response.read(128).decode("utf-8")
    if marker != MARKER:
        raise RuntimeError("HTTPS is serving another virtual host, not GenyCaster")


def activate(email: str | None) -> None:
    if os.geteuid() != 0:
        raise RuntimeError("Activation requires operator root permissions")
    for tool in ("nginx", "systemctl", "certbot", "openssl"):
        if not shutil.which(tool):
            raise RuntimeError(f"Required system tool unavailable: {tool}")
    verify_ownership()
    previous = AVAILABLE.read_text(encoding="utf-8") if AVAILABLE.exists() else None
    created_link = not (ENABLED.exists() or ENABLED.is_symlink())
    source = SOURCE.read_text(encoding="utf-8")
    validate_config(source)

    def reload_nginx() -> None:
        run_checked("nginx", "-t")
        run_checked("systemctl", "reload", "nginx")

    try:
        # Serve the HTTP-01 challenge from the dedicated root while keeping
        # the existing landing/status vhosts untouched.
        atomic_write(AVAILABLE, http_certificate_bootstrap())
        if created_link:
            ENABLED.symlink_to(AVAILABLE)
        reload_nginx()

        command = ["certbot", "certonly", "--webroot", "-w", str(ROOT),
                   "-d", HOST, "--non-interactive", "--agree-tos",
                   "--keep-until-expiring"]
        if email:
            command.extend(("--email", email))
        run_checked(*command)
        verify_certificate()

        # Only a valid, host-matched certificate enables the HTTPS vhost.
        atomic_write(AVAILABLE, MANAGED + source)
        reload_nginx()
        check_public_marker()
    except Exception:
        # The HTTP bootstrap is not a successful site installation. On
        # any failure, restore the previous vhost and reload its old config.
        try:
            if previous is None:
                if created_link and ENABLED.is_symlink():
                    ENABLED.unlink()
                if AVAILABLE.exists():
                    AVAILABLE.unlink()
            else:
                atomic_write(AVAILABLE, previous)
            reload_nginx()
        except Exception as rollback_error:
            print(f"CRITICAL: Nginx rollback needs operator attention: "
                  f"{rollback_error}", file=sys.stderr)
        raise

    print(f"READY: {PROBE} = {MARKER}")
    print(f"Dedicated static web root: {ROOT}")
    print("No WASM assets were published by this activation.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true",
                        help="Install dedicated Nginx vhost and request HTTPS certificate")
    parser.add_argument("--email", help="ACME account email (if not already registered)")
    args = parser.parse_args()
    try:
        validate_config(SOURCE.read_text(encoding="utf-8"))
        if not args.apply:
            print(f"DRY RUN: reviewed Nginx template valid for {HOST}")
            print(f"Public root: {ROOT}")
            print(f"Certificate: {CERT}")
            print("Requires operator-managed /etc, root access, and ACME DNS/HTTP reachability.")
            print("Apply with --apply after verifying the domain's DNS record.")
            return 0
        activate(args.email)
        return 0
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError,
            urllib.error.URLError) as error:
        print(f"App-site activation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
