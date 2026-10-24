#!/usr/bin/env python3
"""Publish static docs into a separate release tree; never touch caster.genyleap.com."""
from __future__ import annotations
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

REPO = Path("/opt/genycaster/repository")
SOURCE = REPO / "docs-site"
TARGET = Path("/var/www/docs.genycaster.xyz")
PAGES = ("quickstart", "architecture", "providers", "client-switching",
         "build-your-own", "operations", "status")
FILES = ("index.html", "llms.txt", "robots.txt", "sitemap.xml")
ASSETS = ("site.css", "site.js", "genycaster-icon.svg", "genycaster-wordmark.svg")

def main() -> int:
    subprocess.run(["node", "--test", str(SOURCE / "tests/site.test.mjs")],
                   check=True, cwd=REPO)
    rev = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
    # Fail closed if publishing dirty public source files.
    pending = subprocess.check_output(
        ["git", "status", "--porcelain", "--", "docs-site"], cwd=REPO, text=True)
    if pending.strip():
        raise SystemExit("Refusing to publish uncommitted docs-site files")
    releases = TARGET / "releases"
    releases.mkdir(parents=True, exist_ok=True)
    # The deployment account uses a restrictive umask; public docs need
    # directory traversal by the Nginx worker, even for pre-existing releases.
    TARGET.chmod(0o755)
    releases.chmod(0o755)
    version = rev[:12]
    destination = releases / version
    if destination.exists():
        print("Release already staged:", destination)
    else:
        temporary = Path(tempfile.mkdtemp(prefix=".docs-", dir=releases))
        try:
            for name in FILES:
                shutil.copy2(SOURCE / name, temporary / name)
            (temporary / "assets").mkdir()
            for name in ASSETS:
                shutil.copy2(SOURCE / "assets" / name, temporary / "assets" / name)
            for name in PAGES:
                (temporary / name).mkdir()
                shutil.copy2(SOURCE / name / "index.html", temporary / name / "index.html")
            if any(p.suffix.lower() in {".wasm", ".pem", ".key", ".zip"} for p in temporary.rglob("*")):
                raise RuntimeError("Forbidden asset found in documentation release")
            temporary.rename(destination)
        finally:
            if temporary.exists():
                shutil.rmtree(temporary)
    # tempfile.mkdtemp defaults to 0700. Nginx runs as www-data and needs
    # read/traverse access to the immutable release directory.
    destination.chmod(0o755)
    for static_dir in ("assets", *PAGES):
        (destination / static_dir).chmod(0o755)
    for document in destination.rglob("*"):
        if document.is_file():
            document.chmod(0o644)
    current = TARGET / "current"
    if current.exists() and not current.is_symlink():
        raise RuntimeError("Existing docs root is not a release symlink")
    next_link = TARGET / ".next-current"
    if next_link.is_symlink():
        next_link.unlink()
    next_link.symlink_to(destination)
    os.replace(next_link, current)
    print("STAGED DOCS:", current.resolve())
    print("Static docs only; enabling DNS, HTTPS and Nginx is a separate prerequisite.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
