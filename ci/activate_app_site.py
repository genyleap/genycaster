#!/usr/bin/env python3
"""Safely activate the dedicated app.genycaster.xyz Nginx/TLS vhost.

This is an explicit operator-only action. It does not build, publish or copy
WASM assets, alter the separate genycaster.xyz site, or change account data.
Without --apply it only validates the tracked vhost and prints prerequisites.
"""
from __future__ import annotations

import argparse
import hashlib
import http.client
import os
import socket
import ssl
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
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



def _probe_origin_once() -> None:
    """Verify the dedicated loopback origin using the public CA trust store.

    Connecting to loopback avoids DNS and the CDN, but SNI/Host still select
    the application vhost. No test content or unverified certificate is ever
    accepted as successful.
    """
    for path, required in (
        ("/.well-known/genycaster-ready", MARKER.encode("ascii")),
        ("/genycaster.html", None),
    ):
        context = ssl.create_default_context()
        with socket.create_connection(("127.0.0.1", 443), timeout=7) as raw:
            with context.wrap_socket(raw, server_hostname=HOST) as connection:
                connection.settimeout(7)
                request = (f"GET {path} HTTP/1.1\r\n"
                           f"Host: {HOST}\r\n"
                           "Accept: */*\r\n"
                           "Connection: close\r\n\r\n")
                connection.sendall(request.encode("ascii"))
                response = http.client.HTTPResponse(connection)
                response.begin()
                status = response.status
                payload = response.read(256)
                response.close()
        if status != 200:
            raise RuntimeError(f"Local HTTPS origin {path} returned HTTP {status}")
        if required is not None and payload != required:
            raise RuntimeError(f"Local HTTPS origin {path} returned the wrong marker")
        if required is None and b"<html" not in payload.lower() and b"<!doctype html" not in payload.lower():
            raise RuntimeError("Local HTTPS origin is not serving GenyCaster HTML")


def _origin_leaf_diagnostic() -> str:
    """Inspect, but NEVER trust, a peer certificate after TLS validation fails.

    OpenSSL may route a handshake to an old default vhost while workers
    replace their configuration after a graceful reload. We inspect the
    untrusted leaf only to classify the failure; this path never allows the
    application to be marked READY.
    """
    try:
        with socket.create_connection(("127.0.0.1", 443), timeout=4) as raw:
            with ssl._create_unverified_context().wrap_socket(
                    raw, server_hostname=HOST) as connection:
                leaf = connection.getpeercert(binary_form=True)
        expected_pem = CERT.read_text(encoding="ascii").split(
            "-----END CERTIFICATE-----", 1)[0] + "-----END CERTIFICATE-----"
        expected = ssl.PEM_cert_to_DER_cert(expected_pem)
        digest = hashlib.sha256(leaf).hexdigest()
        expected_digest = hashlib.sha256(expected).hexdigest()
        if digest == expected_digest:
            return ("Nginx is presenting the configured app Let’s Encrypt "
                    "leaf, but its chain is not trusted by this TLS client; "
                    f"leaf sha256={digest}")
        return ("Nginx is presenting a DIFFERENT leaf from the configured "
                "app certificate (likely the fallback status vhost or a "
                "stale worker after reload); "
                f"received sha256={digest}, expected sha256={expected_digest}")
    except (OSError, ValueError, ssl.SSLError) as error:
        return f"Unable to inspect loopback TLS certificate: {error}"


def check_origin_service(*, attempts: int = 20, retry_delay: float = 0.5) -> None:
    """Verify active Nginx after a reload, tolerating only bounded churn.

    systemctl reload returns after signalling the Nginx master, not after
    every prior worker has drained. A loopback handshake within milliseconds
    of that signal may select a stale fallback/default vhost. Retry full
    CA-validated SNI+Host+body checks before deciding the origin is broken.
    """
    if attempts < 1 or retry_delay < 0:
        raise ValueError("Invalid origin readiness retry budget")
    last_error: Exception | None = None
    for attempt in range(attempts):
        try:
            _probe_origin_once()
            return
        except (OSError, RuntimeError, ValueError, http.client.HTTPException) as error:
            last_error = error
            if attempt + 1 < attempts:
                time.sleep(retry_delay)
    diagnostic = _origin_leaf_diagnostic()
    raise RuntimeError(
        f"Local HTTPS origin failed after {attempts} checked attempts: "
        f"{last_error}. Certificate diagnostic: {diagnostic}"
    ) from last_error


def report_public_delivery() -> bool:
    """Do not remove a verified working vhost because a CDN is misrouted."""
    try:
        check_public_marker()
    except (OSError, RuntimeError, ValueError, urllib.error.URLError) as error:
        print(
            "ORIGIN READY, PUBLIC CDN ROUTING NOT VERIFIED: "
            f"{error}. Nginx and the validated app certificate remain active. "
            "Check that Cloudflare's app.genycaster.xyz record, redirect rules, "
            "origin overrides and cache route to this VPS. "
            "The app's public URL is NOT yet confirmed.",
            file=sys.stderr,
        )
        return False
    print(f"READY: {PROBE} = {MARKER}")
    return True


def activate(email: str | None) -> bool:
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

    desired = MANAGED + source
    if previous == desired and not created_link:
        # Re-running after a public-CDN routing error must not disturb the
        # already-validated Nginx origin or repeat the Certbot ceremony.
        verify_certificate()
        check_origin_service()
        return report_public_delivery()

    try:
        # Existing valid app certificates can be reused. A temporary
        # HTTP-only ACME bootstrap is needed only for initial issuance.
        certificate_exists = CERT.is_file() and KEY.is_file()
        if certificate_exists:
            verify_certificate()
        else:
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

        # Install only the reviewed dedicated HTTPS config and verify the
        # actual local Nginx endpoint before any public CDN request.
        atomic_write(AVAILABLE, desired)
        if created_link:
            ENABLED.symlink_to(AVAILABLE)
        reload_nginx()
        check_origin_service()
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

    print(f"Local origin READY: https://{HOST}/genycaster.html")
    print(f"Dedicated static web root: {ROOT}")
    print("No WASM assets were published by this activation.")
    # External CDN/cache/DNS mistakes are separate from local configuration
    # failures. Keep the working app vhost installed for repair and report a
    # nonzero result until real public reachability is confirmed.
    return report_public_delivery()


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
        return 0 if activate(args.email) else 2
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError,
            urllib.error.URLError) as error:
        print(f"App-site activation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
