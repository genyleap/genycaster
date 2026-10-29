#!/usr/bin/env python3
"""Fail-closed release-tag/version gate for GenyCaster GitHub Actions."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import sys


def project_version(root: Path) -> str:
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(
        r"\bproject\s*\(\s*Genycaster\s+VERSION\s+(\d+\.\d+\.\d+)\b",
        cmake,
        flags=re.IGNORECASE,
    )
    if not match:
        raise ValueError("CMakeLists.txt project(Genycaster VERSION x.y.z) missing")
    return match.group(1)


def release_version(ref: str, expected: str) -> tuple[str, bool]:
    if not ref.startswith("refs/tags/"):
        return expected, False
    tag = ref.removeprefix("refs/tags/")
    if not re.fullmatch(r"v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)", tag):
        raise ValueError(f"Invalid release tag {tag!r}; use vMAJOR.MINOR.PATCH")
    version = tag.removeprefix("v")
    if version != expected:
        raise ValueError(
            f"Tag {tag} does not match CMake project version {expected}"
        )
    return version, True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ref", default=os.environ.get("GITHUB_REF", ""))
    parser.add_argument("--event", default=os.environ.get("GITHUB_EVENT_NAME", ""))
    parser.add_argument("--output", default=os.environ.get("GITHUB_OUTPUT"))
    args = parser.parse_args()

    try:
        version, tagged = release_version(args.ref, project_version(Path.cwd()))
        if tagged and args.event != "push":
            # A manually dispatched run on a tag must never publish a release.
            tagged = False
        if args.event == "push" and not tagged:
            raise ValueError("Push-triggered CI is restricted to validated version tags")
    except ValueError as error:
        print(f"::error::{error}", file=sys.stderr)
        return 1

    print(f"GenyCaster build version: {version}; release: {tagged}")
    if args.output:
        with Path(args.output).open("a", encoding="utf-8") as output:
            output.write(f"version={version}\n")
            output.write(f"release={'true' if tagged else 'false'}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
