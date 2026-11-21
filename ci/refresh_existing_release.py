#!/usr/bin/env python3
"""Fail-closed asset-only replacement of the existing v1.0.0 Release.

This command runs exclusively inside GitHub Actions, with github.token.
It never creates a tag/Release, changes its body or flips "latest".
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from ci.release_artifacts import EXPECTED
from ci.recover_v1_beta import verify_checksum_inventory

REPO = "genyleap/genycaster"
TAG = "v1.0.0"
RELEASE_NAME = "Genycaster 1.0 Beta"
REQUIRED_ARTIFACTS = frozenset({
    "GenyCaster-linux-x86_64",
    "GenyCaster-linux-arm64",
    "GenyCaster-macos-arm64",
    "GenyCaster-macos-x86_64",
    "GenyCaster-windows-x86_64",
    "GenyCaster-windows-arm64",
    "GenyCaster-android-arm64-v8a",
    "GenyCaster-web-wasm",
})
EXPECTED_ASSETS = frozenset({
    "GenyCaster-v1.0.0-linux-x86_64.AppImage",
    "GenyCaster-v1.0.0-linux-x86_64.AppImage.zsync",
    "GenyCaster-v1.0.0-linux-x86_64.deb",
    "GenyCaster-v1.0.0-linux-arm64.AppImage",
    "GenyCaster-v1.0.0-linux-arm64.AppImage.zsync",
    "GenyCaster-v1.0.0-linux-arm64.deb",
    "GenyCaster-v1.0.0-macos-arm64-adhoc-UNNOTARIZED.dmg",
    "GenyCaster-v1.0.0-macos-x86_64-adhoc-UNNOTARIZED.dmg",
    "GenyCaster-selfupdate-v1.0.0-macos-arm64.tar.gz",
    "GenyCaster-selfupdate-v1.0.0-macos-x86_64.tar.gz",
    "GenyCaster-v1.0.0-windows-x86_64.exe",
    "GenyCaster-v1.0.0-windows-arm64.exe",
    "GenyCaster-selfupdate-v1.0.0-windows-x86_64.zip",
    "GenyCaster-selfupdate-v1.0.0-windows-arm64.zip",
    "GenyCaster-v1.0.0-android-arm64-v8a.apk",
    "SHA256SUMS",
})


def ensure(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def execute(*args: str) -> str:
    return subprocess.check_output(args, text=True, timeout=90).strip()


def github(path: str) -> dict:
    return json.loads(execute("gh", "api", f"repos/{REPO}/{path}"))


def asset_names(release: dict) -> set[str]:
    names = [entry["name"] for entry in release["assets"]]
    ensure(len(names) == len(set(names)), "Duplicate Release asset names")
    return set(names)


def release_identity(release: dict, tag_sha: str) -> dict:
    ensure(release.get("tag_name") == TAG, "Wrong Release tag")
    ensure(release.get("name") == RELEASE_NAME, "Existing Release title changed")
    ensure(release.get("draft") is False, "Release is a draft")
    ensure(release.get("prerelease") is False, "Latest Release status must be retained")
    ensure(release.get("author", {}).get("login") == "github-actions[bot]",
           "Release must belong to GitHub Actions")
    ensure(isinstance(release.get("id"), int), "Missing existing Release ID")
    ensure(asset_names(release) == EXPECTED_ASSETS, "Existing Release asset inventory mismatch")
    ensure(re.fullmatch(r"[0-9a-f]{40}", tag_sha) is not None,
           "Unrecognized existing Git tag object")
    return {key: release.get(key) for key in
            ("id", "name", "tag_name", "draft", "prerelease", "body",
             "created_at", "published_at", "target_commitish")} | {
                 "tag_sha": tag_sha,
                 "asset_names": sorted(EXPECTED_ASSETS),
             }


def snapshot_now() -> dict:
    release = github(f"releases/tags/{TAG}")
    tag = github(f"git/ref/tags/{TAG}")
    ensure(tag["ref"] == f"refs/tags/{TAG}", "Existing tag has moved")
    identity = release_identity(release, tag["object"]["sha"])
    latest = github("releases/latest")
    ensure(latest["id"] == identity["id"], "v1.0.0 is not the current Latest Release")
    return identity


def require_actions_refresh() -> None:
    ensure(os.getenv("GITHUB_REPOSITORY") == REPO, "Unexpected repository")
    ensure(os.getenv("GITHUB_EVENT_NAME") == "workflow_dispatch", "Dispatch required")
    ensure(os.getenv("GITHUB_REF") == "refs/heads/main", "Only main may update assets")
    ensure(os.getenv("GENYCASTER_REFRESH_EXISTING_RELEASE") == "true",
           "Explicit approved asset-only refresh required")
    ensure(os.getenv("GENYCASTER_PUBLISH_PUBLIC_BETA") != "true",
           "Creating a new Release is forbidden during asset refresh")
    ensure(execute("git", "rev-parse", "HEAD") == os.getenv("GITHUB_SHA"),
           "The action must use the exact checked-out source commit")


def verify_artifacts() -> None:
    folders = Path("artifacts")
    ensure(folders.is_dir(), "No CI artifacts downloaded")
    actual = {p.name for p in folders.iterdir() if p.is_dir()}
    ensure(actual == REQUIRED_ARTIFACTS,
           f"Missing/extra CI build artifacts: {sorted(actual ^ REQUIRED_ARTIFACTS)}")
    ensure(len(EXPECTED) == 7, "Expected seven native platform outputs")
    for name in sorted(actual):
        verify_checksum_inventory(folders / name)
    from ci.verify_platform_package import validate_web
    browser_archive = folders / "GenyCaster-web-wasm" / "GenyCaster-v1.0.0-web-wasm.tar.gz"
    ensure(browser_archive.is_file(), "Missing browser WASM artifact")
    validate_web(browser_archive)
    print("PASS: all eight original CI artifacts and browser archive verified", flush=True)


def local_release_files() -> dict[str, int]:
    root = Path("release")
    ensure(root.is_dir(), "No staged Release installers")
    actual = {p.name: p.stat().st_size for p in root.iterdir()
              if p.is_file() and not p.is_symlink()}
    ensure(set(actual) == EXPECTED_ASSETS, "Would add/remove/rename Release assets")
    ensure(all(size > 100 for size in actual.values()), "Empty Release asset")
    ensure(not any("CI-TEST" in name or "unsigned" in name for name in actual),
           "Test-signed or unsigned assets must not replace the official Release")
    return actual


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("preflight", "verify-artifacts", "verify-assets", "postflight"))
    parser.add_argument("--snapshot", type=Path)
    args = parser.parse_args()
    if args.action == "verify-artifacts":
        verify_artifacts()
        return
    require_actions_refresh()
    ensure(args.snapshot is not None, "Release metadata snapshot is required")
    if args.action == "preflight":
        ensure(not args.snapshot.exists(), "Release snapshot must start empty")
        identity = snapshot_now()
        args.snapshot.write_text(json.dumps(identity, sort_keys=True), encoding="utf-8")
        print("Existing v1.0.0 Release ID, latest state and immutable tag verified",
              flush=True)
        return

    original = json.loads(args.snapshot.read_text(encoding="utf-8"))
    current = snapshot_now()
    ensure(current == original, "Existing Release identity, notes or tag changed")
    files = local_release_files()
    release = github(f"releases/tags/{TAG}")
    if args.action == "verify-assets":
        print(f"PASS: {len(files)} staged files have exactly the current Release filenames",
              flush=True)
        return
    reported = {item["name"]: item["size"] for item in release["assets"]}
    ensure(reported == files, "Updated Release assets do not match staged file sizes")
    print(f"PASS: updated {len(files)} files in SAME Release ID={original['id']}; "
          "tag, title, notes, latest state unchanged", flush=True)


if __name__ == "__main__":
    main()
