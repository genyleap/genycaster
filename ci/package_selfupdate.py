#!/usr/bin/env python3
"""Publish installer-independent native runtime snapshots, with per-file hashes.

This is the *transport format* for a future supervised updater, not a patch
engine. In particular, it neither modifies the installed application nor
provides unsigned in-process code replacement. Standard installers remain the
only supported update action in UpdateController until an atomic updater is
implemented and verified for each OS.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import posixpath
from pathlib import Path, PurePosixPath
import re
import tarfile
import zipfile


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def create_snapshot(platform: str, arch: str, version: str,
                    source: Path, output: Path) -> Path:
    if platform not in {"windows", "macos"}:
        raise ValueError("Only Windows and macOS support runtime snapshots")
    if arch not in {"x86_64", "arm64"}:
        raise ValueError("Unsupported runtime snapshot architecture")
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)[1-9]\d*)?", version):
        raise ValueError("Invalid release version")
    if not source.is_dir():
        raise ValueError("Runtime stage directory does not exist")
    if not (source / "Genycaster.exe" if platform == "windows"
            else source / "Contents/MacOS/Genycaster").is_file():
        raise ValueError("Runtime snapshot missing main executable")

    root = "Genycaster.app" if platform == "macos" else "app"
    files = []
    symlinks = []
    for file in sorted(source.rglob("*")):
        relative = PurePosixPath(root, file.relative_to(source).as_posix())
        if file.is_symlink():
            if platform == "windows":
                raise ValueError("Windows runtime snapshots cannot contain symlinks")
            # A macOS .app normally contains Qt framework symlinks. Their
            # targets are part of the immutable manifest and may never escape
            # the app root, even when the link uses legal '..' components.
            target = file.readlink().as_posix()
            resolved = posixpath.normpath(
                posixpath.join(posixpath.dirname(str(relative)), target))
            if (not target or target.startswith("/") or "\\" in target
                    or not (resolved == root or resolved.startswith(root + "/"))):
                raise ValueError("Unsafe macOS runtime symlink target")
            symlinks.append({"path": str(relative), "target": target})
            continue
        if file.is_file():
            files.append({
                "path": str(relative),
                "size": file.stat().st_size,
                "sha256": sha256(file),
            })
    manifest = {
        "schemaVersion": 1, "kind": "runtime-snapshot",
        "product": "GenyCaster", "version": version,
        "platform": platform, "architecture": arch,
        "installAction": "unsupported", "files": files,
        "symlinks": symlinks,
    }
    output.mkdir(parents=True, exist_ok=True)
    suffix = ".tar.gz" if platform == "macos" else ".zip"
    result = output / f"GenyCaster-selfupdate-v{version}-{platform}-{arch}{suffix}"
    metadata = (json.dumps(manifest, separators=(",", ":"), ensure_ascii=True)
                + "\n").encode("utf-8")
    if platform == "windows":
        with zipfile.ZipFile(result, "w", zipfile.ZIP_DEFLATED, compresslevel=8) as zip:
            zip.writestr("selfupdate.json", metadata)
            for file in files:
                rel = str(PurePosixPath(file["path"]).relative_to(root))
                zip.write(source / rel, file["path"])
    else:
        with tarfile.open(result, "w:gz") as archive:
            info = tarfile.TarInfo("selfupdate.json")
            info.size = len(metadata)
            info.mode = 0o644
            archive.addfile(info, io.BytesIO(metadata))
            archive.add(source, arcname=root, recursive=True)
    result.with_suffix(result.suffix + ".sha256").write_text(
        f"{sha256(result)}  {result.name}\n", encoding="ascii")
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", required=True, choices=("windows", "macos"))
    parser.add_argument("--arch", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(create_snapshot(args.platform, args.arch, args.version,
                          args.source, args.output))


if __name__ == "__main__":
    main()
