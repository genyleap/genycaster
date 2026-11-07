#!/usr/bin/env python3
"""Archive only public assets declared by this exact WASM release manifest."""
from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import re
import tarfile

EXTENSIONS = {
    ".html", ".js", ".mjs", ".wasm", ".data", ".css", ".json",
    ".svg", ".png", ".jpg", ".jpeg", ".gif", ".webp", ".avif",
    ".ico", ".webmanifest", ".woff", ".woff2",
    ".ttf", ".otf", ".txt", ".map", ".license",
}
BLOCKED = {".qt", ".rcc", "CMakeFiles", "_deps", "genycaster_autogen"}
HASHED_WASM = re.compile(r"genycaster\.[0-9a-f]{20}\.wasm")


def select_files(source: Path) -> list[Path]:
    source = source.resolve(strict=True)
    manifest_path = source / "genycaster-assets.json"
    if manifest_path.is_symlink() or not manifest_path.is_file():
        raise ValueError("Missing canonical WASM release manifest")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (ValueError, OSError) as error:
        raise ValueError("Invalid canonical WASM release manifest") from error
    entries = manifest.get("files")
    engine = manifest.get("wasm")
    compressed = manifest.get("wasmGzip")
    if (not isinstance(entries, list) or not entries
            or any(not isinstance(name, str) for name in entries)
            or len(entries) != len(set(entries))
            or not isinstance(engine, str) or not HASHED_WASM.fullmatch(engine)):
        raise ValueError("Malformed WASM release file inventory")
    required = {
        "genycaster.html", "genycaster.js", "genycaster-loader.js",
        "genycaster-sw.js", engine,
    }
    if isinstance(compressed, str) and compressed:
        if compressed != engine + ".gz":
            raise ValueError("WASM compressed file does not match the engine")
        required.add(compressed)
    if not required.issubset(set(entries)):
        raise ValueError("WASM release manifest omits required public assets")

    # The stable binary is used by Qt's loader and remains part of the
    # verified release, even though the manifest points to the immutable
    # content-addressed equivalent.
    selected: set[Path] = set()
    for name in [*entries, "genycaster.wasm", "genycaster-assets.json"]:
        if (name.startswith("farcaster_client") or
                (name.endswith((".wasm", ".wasm.gz"))
                 and name not in {"genycaster.wasm", engine, compressed})):
            raise ValueError(f"Unexpected obsolete WASM engine asset: {name!r}")
        rel = PurePosixPath(name)
        if (not name or "\\" in name or rel.is_absolute()
                or any(part in BLOCKED or part.startswith(".") or part == ".."
                       for part in rel.parts)
                or (rel.suffix.lower() not in EXTENSIONS
                    and not name.endswith((".wasm.gz", ".LICENSE")))):
            raise ValueError(f"Unsafe or unsupported WASM release path: {name!r}")
        path = source.joinpath(*rel.parts)
        if (not path.is_file() or path.is_symlink()
                or any(parent.is_symlink() for parent in path.parents if parent != source)
                or not path.resolve().is_relative_to(source)):
            raise ValueError(f"Missing or unsafe WASM release asset: {name!r}")
        selected.add(path)

    return sorted(selected)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    files = select_files(source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(args.output, "w:gz") as archive:
        for path in files:
            archive.add(path, arcname=path.relative_to(source), recursive=False)
    if args.output.stat().st_size == 0:
        raise ValueError("Empty WebAssembly archive")
    print(f"Packaged {len(files)} manifest-pinned web assets into {args.output}")


if __name__ == "__main__":
    main()
