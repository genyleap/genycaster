#!/usr/bin/env python3
"""Fail-closed validation for native runtime snapshot transport archives.

These artifacts are *not* executable patches or signed installers. Their
installAction remains 'unsupported' until a platform-specific, post-exit,
signature-verifying atomic updater is built and tested. This validator checks
the independent archive/manifest integrity contract before publication.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import posixpath
from pathlib import Path, PurePosixPath
import re
import stat
import tarfile
import zipfile

VERSION = re.compile(r"\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)[1-9]\d*)?")
HEX_SHA = re.compile(r"[0-9a-f]{64}")
ARCH = {"x86_64", "arm64"}
MAX_MEMBERS = 250_000
MAX_TOTAL_BYTES = 20 * 1024**3
MAX_MANIFEST_BYTES = 50 * 1024**2


def require(condition: bool, detail: str) -> None:
    if not condition:
        raise ValueError(detail)


def safe_path(raw: str) -> str:
    require(bool(raw) and "\\" not in raw and "\x00" not in raw,
            "Archive contains an empty or unsafe path")
    path = PurePosixPath(raw)
    require(not path.is_absolute() and ".." not in path.parts
            and "." not in path.parts and not raw.startswith("/")
            and "//" not in raw and not re.match(r"^[A-Za-z]:", raw),
            f"Unsafe archive path: {raw!r}")
    require(str(path) == raw.rstrip("/"), f"Noncanonical archive path: {raw!r}")
    return str(path)


def digest(stream) -> str:
    result = hashlib.sha256()
    while chunk := stream.read(1024 * 1024):
        result.update(chunk)
    return result.hexdigest()


def expected_files(manifest: dict) -> tuple[dict[str, dict], str]:
    require(manifest.get("schemaVersion") == 1
            and manifest.get("kind") == "runtime-snapshot"
            and manifest.get("product") == "GenyCaster"
            and manifest.get("installAction") == "unsupported",
            "Unknown/unapproved native runtime snapshot or install action")
    platform = manifest.get("platform")
    require(platform in {"windows", "macos"}, "Unsupported snapshot platform")
    require(manifest.get("architecture") in ARCH,
            "Unsupported snapshot architecture")
    version = manifest.get("version")
    require(isinstance(version, str) and VERSION.fullmatch(version) is not None,
            "Invalid snapshot version")
    root = "app" if platform == "windows" else "Genycaster.app"
    entries = manifest.get("files")
    require(isinstance(entries, list) and 0 < len(entries) <= MAX_MEMBERS,
            "Snapshot needs a bounded nonempty file manifest")
    expected: dict[str, dict] = {}
    folded: set[str] = set()
    total = 0
    for item in entries:
        require(isinstance(item, dict) and set(item) == {"path", "size", "sha256"},
                "Invalid file manifest entry")
        p = safe_path(item["path"])
        require(p.startswith(root + "/"), "Manifest file outside runtime root")
        require(p.casefold() not in folded, "Duplicate/case-colliding file path")
        folded.add(p.casefold())
        require(type(item["size"]) is int and 0 <= item["size"] <= MAX_TOTAL_BYTES,
                "Invalid file size")
        require(isinstance(item["sha256"], str)
                and HEX_SHA.fullmatch(item["sha256"]) is not None,
                "Invalid SHA-256 digest")
        total += item["size"]
        require(total <= MAX_TOTAL_BYTES, "Snapshot uncompressed size limit exceeded")
        expected[p] = item
    executable = (root + "/Genycaster.exe" if platform == "windows"
                  else root + "/Contents/MacOS/Genycaster")
    require(executable in expected, "Snapshot manifest missing main executable")
    return expected, platform


def validate_manifest(data: bytes) -> dict:
    require(len(data) <= MAX_MANIFEST_BYTES, "Manifest too large")
    try:
        manifest = json.loads(data)
    except (ValueError, UnicodeDecodeError) as exc:
        raise ValueError("Invalid snapshot JSON manifest") from exc
    require(isinstance(manifest, dict), "Manifest root must be an object")
    return manifest


def verify_snapshot(path: Path) -> dict:
    require(path.is_file(), "Snapshot archive is missing")
    if path.suffix == ".zip":
        archive_format = "windows"
        try:
            with zipfile.ZipFile(path) as archive:
                members = archive.infolist()
                require(len(members) <= MAX_MEMBERS + 1, "Archive file count exceeded")
                payload: dict[str, zipfile.ZipInfo] = {}
                folded: set[str] = set()
                for info in members:
                    name = safe_path(info.filename)
                    require(not info.is_dir(), "Windows snapshot must contain only files")
                    require(not info.flag_bits & 0x1, "Encrypted ZIP entries forbidden")
                    require(stat.S_IFMT(info.external_attr >> 16) != stat.S_IFLNK,
                            "Symlinks forbidden in Windows snapshot")
                    require(name.casefold() not in folded, "Duplicate ZIP path")
                    folded.add(name.casefold())
                    payload[name] = info
                require("selfupdate.json" in payload, "Missing snapshot manifest")
                require(payload["selfupdate.json"].file_size <= MAX_MANIFEST_BYTES,
                        "Manifest too large")
                with archive.open("selfupdate.json") as input_stream:
                    manifest = validate_manifest(input_stream.read(MAX_MANIFEST_BYTES + 1))
                files, platform = expected_files(manifest)
                require(platform == archive_format, "Platform/archive type mismatch")
                require(set(payload) == set(files) | {"selfupdate.json"},
                        "Snapshot contents differ from file manifest")
                for name, entry in files.items():
                    info = payload[name]
                    require(info.file_size == entry["size"], f"Size mismatch: {name}")
                    with archive.open(info) as stream:
                        require(digest(stream) == entry["sha256"],
                                f"SHA-256 mismatch: {name}")
                return manifest
        except (zipfile.BadZipFile, RuntimeError, OSError) as exc:
            raise ValueError(f"Corrupt Windows snapshot: {exc}") from exc

    require(path.name.endswith(".tar.gz"), "Expected a ZIP or tar.gz snapshot")
    archive_format = "macos"
    try:
        with tarfile.open(path, "r:gz") as archive:
            members = archive.getmembers()
            require(len(members) <= MAX_MEMBERS * 2 + 1,
                    "Archive member count exceeded")
            payload: dict[str, tarfile.TarInfo] = {}
            files_seen: set[str] = set()
            for info in members:
                name = safe_path(info.name)
                require(name not in payload, "Duplicate tar path")
                payload[name] = info
                if info.isfile():
                    files_seen.add(name)
                elif info.issym():
                    target = info.linkname
                    require(bool(target) and not target.startswith("/")
                            and "\\" not in target and "\x00" not in target,
                            "Unsafe absolute or empty macOS symlink")
                    resolved = posixpath.normpath(
                        posixpath.join(posixpath.dirname(name), target))
                    require(resolved == "Genycaster.app"
                            or resolved.startswith("Genycaster.app/"),
                            "Symlink escapes macOS app bundle")
                else:
                    require(info.isdir(), "Hardlinks and special archive entries forbidden")
            require("selfupdate.json" in files_seen, "Missing snapshot manifest")
            header = payload["selfupdate.json"]
            require(header.size <= MAX_MANIFEST_BYTES, "Manifest too large")
            with archive.extractfile(header) as manifest_stream:
                manifest = validate_manifest(manifest_stream.read(MAX_MANIFEST_BYTES + 1))
            files, platform = expected_files(manifest)
            require(platform == archive_format, "Platform/archive type mismatch")
            require(files_seen == set(files) | {"selfupdate.json"},
                    "Snapshot contents differ from file manifest")
            for name, entry in files.items():
                info = payload[name]
                require(info.size == entry["size"], f"Size mismatch: {name}")
                with archive.extractfile(info) as stream:
                    require(digest(stream) == entry["sha256"],
                            f"SHA-256 mismatch: {name}")
            return manifest
    except (tarfile.TarError, EOFError, OSError) as exc:
        raise ValueError(f"Corrupt macOS snapshot: {exc}") from exc


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    options = parser.parse_args()
    try:
        manifest = verify_snapshot(options.archive)
    except ValueError as error:
        parser.exit(1, f"Native runtime snapshot rejected: {error}\n")
    print(f"Verified {manifest['platform']} {manifest['architecture']} runtime snapshot: "
          f"{len(manifest['files'])} SHA-256-checked files; "
          "installer action unsupported (transport only)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
