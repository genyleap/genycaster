#!/usr/bin/env python3
"""Fail-closed release-tag/version gate for GenyCaster GitHub Actions."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ci.release_notes import load_release, render_markdown


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

def release_designation(root: Path) -> str:
    """Version in the canonical root release-notes filename."""
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    channel_match = re.search(
        r'\bset\s*\(\s*GENYCASTER_RELEASE_CHANNEL\s+"(stable|alpha|beta|rc)"', cmake)
    index_match = re.search(
        r'\bset\s*\(\s*GENYCASTER_RELEASE_CHANNEL_NUMBER\s+"([0-9]+)"', cmake)
    if not channel_match or not index_match:
        raise ValueError("Missing CMake release channel or channel number")
    channel, index = channel_match.group(1), index_match.group(1)
    if channel != "stable" and not re.fullmatch(r"[1-9]\d*", index):
        raise ValueError("Invalid prerelease channel number")
    designation = project_version(root) + (
        "" if channel == "stable" else "-" + channel + index)
    source = root / "release-notes" / f"v{designation}.json"
    generated = source.with_suffix(".md")
    if not source.is_file() or not generated.is_file():
        raise ValueError(f"Missing canonical JSON or generated release notes: {designation}")
    release = load_release(source)
    expected = render_markdown(release, designation)
    if generated.read_text(encoding="utf-8") != expected:
        raise ValueError("GitHub release notes are out of sync with localized JSON")
    return designation


def release_version(ref: str, expected: str) -> tuple[str, bool]:
    if not ref.startswith("refs/tags/"):
        return expected, False
    tag = ref.removeprefix("refs/tags/")
    if not re.fullmatch(r"v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-(?:alpha|beta|rc)[1-9]\d*)?", tag):
        raise ValueError(f"Invalid release tag {tag!r}; use vMAJOR.MINOR.PATCH[-alphaN|-betaN|-rcN]")
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
        version, tagged = release_version(args.ref, release_designation(Path.cwd()))
        manual_release = os.getenv("GENYCASTER_MANUAL_RELEASE", "").lower() == "true"
        refresh_existing = os.getenv("GENYCASTER_REFRESH_EXISTING_RELEASE", "").lower() == "true"
        if manual_release and refresh_existing:
            raise ValueError("Cannot create a new Release and refresh an existing Release in one run")
        if manual_release or refresh_existing:
            # Both paths require the same full release-grade checks, including
            # the permanent Android signing key. Refresh must NEVER move tags
            # or create a Release; its workflow job replaces only asset bytes.
            # Diagnostic/partial builds must not use the owner release signer.
            if (args.event != "workflow_dispatch"
                    or args.ref != "refs/heads/main"
                    or os.getenv("GENYCASTER_DISPATCH_PLATFORM") != "all"
                    or os.getenv("GENYCASTER_DISPATCH_ARCH") != "all"
                    or os.getenv("GENYCASTER_FULL_MATRIX_CONFIRMED", "").lower() != "true"):
                raise ValueError("Release-grade publication/asset refresh requires an approved full-matrix dispatch on main")
            tagged = True
        elif tagged and args.event != "push":
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
