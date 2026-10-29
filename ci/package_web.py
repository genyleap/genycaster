#!/usr/bin/env python3
"""Archive only deployable static Qt WebAssembly artifacts, never build internals."""
from __future__ import annotations

import argparse
from pathlib import Path
import tarfile

EXTENSIONS = {
    ".html", ".js", ".mjs", ".wasm", ".data", ".css", ".json",
    ".svg", ".png", ".jpg", ".jpeg", ".gif", ".webp", ".avif",
    ".ico", ".webmanifest", ".woff", ".woff2",
    ".ttf", ".otf", ".txt", ".map", ".license",
}
BLOCKED = {".qt", ".rcc", "CMakeFiles", "_deps", "CMakeFiles", "farcaster_client_autogen"}


def select_files(source: Path) -> list[Path]:
    required = ["farcaster_client.html", "farcaster_client.wasm"]
    for name in required:
        if not (source / name).is_file():
            raise ValueError(f"WebAssembly packaging missing {source / name}")
    result: list[Path] = []
    for path in source.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        relative = path.relative_to(source)
        if any(part in BLOCKED or part.startswith(".") for part in relative.parts):
            continue
        if path.suffix.lower() in EXTENSIONS or path.name.endswith(".LICENSE"):
            result.append(path)
    return sorted(result)


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
    print(f"Packaged {len(files)} web assets into {args.output}")


if __name__ == "__main__":
    main()
