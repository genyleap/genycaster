#!/usr/bin/env python3
"""Verify each platform produced one nonempty package and aggregate SHA-256."""
from __future__ import annotations

import hashlib
from pathlib import Path
import shutil
import sys

EXPECTED = {
    "GenyCaster-linux-x86_64": ".AppImage",
    "GenyCaster-linux-arm64": ".AppImage",
    "GenyCaster-macos-arm64": ".dmg",
    "GenyCaster-macos-x86_64": ".dmg",
    "GenyCaster-windows-x86_64": ".exe",
    "GenyCaster-windows-arm64": ".exe",
    "GenyCaster-android-arm64-v8a": ".apk",
    "GenyCaster-android-armeabi-v7a": ".apk",
    "GenyCaster-web-wasm": ".tar.gz",
}

# Auxiliary deployable packages and updater transport metadata. The primary
# Windows release artifact is always the NSIS setup EXE, never a ZIP.
EXTRA_REQUIRED = {
    "GenyCaster-linux-x86_64": (".deb", ".AppImage.zsync"),
    "GenyCaster-linux-arm64": (".deb", ".AppImage.zsync"),
    "GenyCaster-macos-arm64": ("-macos-arm64.tar.gz",),
    "GenyCaster-macos-x86_64": ("-macos-x86_64.tar.gz",),
    "GenyCaster-windows-x86_64": ("-windows-x86_64.zip",),
    "GenyCaster-windows-arm64": ("-windows-arm64.zip",),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as data:
        for chunk in iter(lambda: data.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("Usage: release_artifacts.py <downloaded-artifacts> <release-dir>")
    root, destination = (Path(value) for value in sys.argv[1:])
    if destination.exists() and any(destination.iterdir()):
        raise ValueError(f"Release destination must start empty: {destination}")
    destination.mkdir(parents=True, exist_ok=True)
    checksums: list[str] = []
    for artifact, extension in EXPECTED.items():
        folder = root / artifact
        if not folder.is_dir():
            raise ValueError(f"Required platform artifact missing: {artifact}")
        packages = [p for p in folder.iterdir() if p.is_file() and
                    p.name.endswith(extension) and not p.is_symlink()]
        if len(packages) != 1:
            raise ValueError(f"Expected exactly one {extension} from {artifact}; got {packages}")
        package = packages[0]
        if not package.stat().st_size:
            raise ValueError(f"Empty package: {package}")
        target = destination / package.name
        if target.exists():
            raise ValueError(f"Package filename collision: {target.name}")
        shutil.copy2(package, target)
        checksums.append(f"{sha256(target)}  {target.name}")
        print(f"Verified {target.name}: {target.stat().st_size} bytes")

        # Publish architecture-matching secondary installers and update
        # transports alongside the primary installable release package.
        # Never accept an unexpected executable or silently omit an extra.
        for suffix in EXTRA_REQUIRED.get(artifact, ()):
            candidates = [
                entry for entry in folder.rglob("*")
                if entry.is_file() and not entry.is_symlink()
                and entry.name.endswith(suffix)
                and entry.name.startswith("GenyCaster-")
            ]
            if len(candidates) != 1:
                raise ValueError(
                    f"Expected one secondary {suffix} from {artifact}; got {candidates}"
                )
            extra = candidates[0]
            extra_target = destination / extra.name
            if extra_target.exists():
                raise ValueError(f"Package filename collision: {extra_target.name}")
            if not extra.stat().st_size:
                raise ValueError(f"Empty update/deployment asset: {extra}")
            shutil.copy2(extra, extra_target)
            checksums.append(f"{sha256(extra_target)}  {extra_target.name}")
            print(f"Verified {extra_target.name}: {extra_target.stat().st_size} bytes")

    (destination / "SHA256SUMS").write_text(
        "\n".join(checksums) + "\n", encoding="ascii"
    )


if __name__ == "__main__":
    main()
