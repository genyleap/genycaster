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
    "GenyCaster-windows-x86_64": ".zip",
    "GenyCaster-windows-arm64": ".zip",
    "GenyCaster-android-arm64-v8a": ".apk",
    "GenyCaster-android-armeabi-v7a": ".apk",
    "GenyCaster-web-wasm": ".tar.gz",
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

    (destination / "SHA256SUMS").write_text(
        "\n".join(checksums) + "\n", encoding="ascii"
    )


if __name__ == "__main__":
    main()
