#!/usr/bin/env python3
"""One-time, fail-closed v1.0.0 Beta recovery from 7 verified CI artifacts.

The original full CI attempt succeeded on seven platforms and failed ONLY
on macOS Intel's invalid Qt scenegraph smoke-test environment. Reuse its
retained artifacts. Only the Intel diagnostic job is rebuilt, with no
application-source changes. GitHub Actions exclusively owns the final tag
and Release (github-actions[bot]); no personal tag and no new full matrix.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
REPO = "genyleap/genycaster"
TAG = "v1.0.0"
VERSION = "1.0.0"
BASE_RUN = 44897476297
INTEL_RUN = 44900638292
BASE_SHA = "f5016b6dcddc42dc464146941d2b980dc1c646d7"
INTEL_SHA = "9557e12ca1d95cd1f30d8ba9933ae98db0e9e1eb"
PRESERVED = (
    "GenyCaster-linux-arm64",
    "GenyCaster-linux-x86_64",
    "GenyCaster-macos-arm64",
    "GenyCaster-windows-arm64",
    "GenyCaster-windows-x86_64",
    "GenyCaster-android-arm64-v8a",
    "GenyCaster-web-wasm",
)
INTEL = "GenyCaster-macos-x86_64"
PERMITTED_RECOVERY_CHANGES = {
    "ci/audit_macos_bundle.py",
    "ci/test_macos_smoke_backend.py",
    "ci/recover_v1_beta.py",
    "ci/test_recover_v1_beta.py",
    ".github/workflows/recover-v1-beta.yml",
}


def command(*args: str, timeout: int = 120) -> str:
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True,
                            timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"Command {args[:5]!r} failed: {result.stderr[-1200:]}")
    return result.stdout


def gh(*args: str) -> dict:
    return json.loads(command("gh", "api", *args, timeout=45))


def ensure(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def unchanged_application_source() -> None:
    diff = set(command("git", "diff", "--name-only", BASE_SHA, "HEAD").splitlines())
    ensure(diff.issubset(PERMITTED_RECOVERY_CHANGES),
           f"Recovery commit changes application files: {sorted(diff - PERMITTED_RECOVERY_CHANGES)}")
    intel_diff = set(command("git", "diff", "--name-only", BASE_SHA, INTEL_SHA).splitlines())
    ensure(intel_diff == {"ci/audit_macos_bundle.py", "ci/test_macos_smoke_backend.py"},
           f"Intel Build changed unexpected source files: {sorted(intel_diff)}")
    ensure(command("git", "status", "--porcelain").strip() == "",
           "Recovery requires clean tracked/untracked working tree")


def run_details(run_id: int) -> dict:
    result = gh(f"repos/{REPO}/actions/runs/{run_id}")
    ensure(result.get("id") == run_id and result.get("name") == "GenyCaster CI and Release",
           f"Unexpected workflow for run {run_id}")
    ensure(result.get("event") == "workflow_dispatch"
           and result.get("head_branch") == "main"
           and result.get("status") == "completed",
           f"Run {run_id} is incomplete or unrelated")
    return result


def jobs(run_id: int) -> list[dict]:
    result = gh(f"repos/{REPO}/actions/runs/{run_id}/jobs?per_page=100")
    return result.get("jobs", [])


def artifacts(run_id: int) -> dict[str, dict]:
    records = gh(f"repos/{REPO}/actions/runs/{run_id}/artifacts?per_page=100")
    return {entry["name"]: entry for entry in records.get("artifacts", [])}


def preflight() -> None:
    ensure(os.environ.get("GITHUB_EVENT_NAME") == "workflow_dispatch"
           and os.environ.get("GITHUB_REF") == "refs/heads/main"
           and os.environ.get("GENYCASTER_APPROVED_RECOVERY") == "true",
           "Recovery must be explicitly approved and dispatched on main")
    ensure(command("git", "rev-parse", BASE_SHA).strip() == BASE_SHA
           and command("git", "rev-parse", INTEL_SHA).strip() == INTEL_SHA,
           "Missing pinned application and Intel source commits")
    unchanged_application_source()
    ensure(command("git", "ls-remote", "--tags", "origin", "refs/tags/v1.0.0").strip() == "",
           "Version tag already exists; refuse overwrite")
    if subprocess.run(("gh", "release", "view", TAG, "--repo", REPO),
                      cwd=ROOT, capture_output=True).returncode == 0:
        raise ValueError("Release already published; refuse overwrite")
    original = run_details(BASE_RUN)
    repaired = run_details(INTEL_RUN)
    ensure(original.get("head_sha") == BASE_SHA
           and original.get("conclusion") == "failure",
           "Original full-matrix source/result mismatch")
    ensure(repaired.get("head_sha") == INTEL_SHA
           and repaired.get("conclusion") == "success",
           "Repaired Intel run must complete successfully")
    original_jobs, repaired_jobs = jobs(BASE_RUN), jobs(INTEL_RUN)
    ensure(sum(j["name"].startswith("macOS DMG (x86_64") and j["conclusion"] == "failure"
               for j in original_jobs) == 1, "Original failure was not only macOS Intel")
    required_job_fragments = {
        "Linux AppImage and DEB (arm64", "Linux AppImage and DEB (x86_64",
        "macOS DMG (arm64", "Windows NSIS installer (arm64",
        "Windows NSIS installer (x86_64", "Android APK (arm64-v8a",
        "WebAssembly (single-threaded)",
    }
    for prefix in required_job_fragments:
        ensure(sum(j["name"].startswith(prefix) and j["conclusion"] == "success"
                   for j in original_jobs) == 1, f"Original required job not successful: {prefix}")
    ensure(sum(j["name"].startswith("macOS DMG (x86_64")
               and j["conclusion"] == "success" for j in repaired_jobs) == 1,
           "Intel diagnostic did not pass")
    ensure(all(j["conclusion"] in ("success", "skipped")
               for j in repaired_jobs),
           "Intel diagnostic also had unexpected failed or cancelled jobs")
    stored, new = artifacts(BASE_RUN), artifacts(INTEL_RUN)
    ensure(set(stored) == set(PRESERVED),
           f"Original retained artifacts mismatch: {sorted(set(stored).symmetric_difference(PRESERVED))}")
    ensure(INTEL in new and len(new) == 1, "Unexpected Intel artifact inventory")
    for name, record in [(name, stored[name]) for name in PRESERVED] + [(INTEL, new[INTEL])]:
        ensure(record.get("expired") is False and record.get("size_in_bytes", 0) > 4096,
               f"Missing, expired or empty retained artifact: {name}")
    print("RECOVERY PREFLIGHT PASS: 7 preserved artifacts + 1 validated Intel; "
          "same application source, original signatures, no release/tag", flush=True)


def verify_checksum_inventory(folder: Path) -> None:
    inventory = folder / "SHA256SUMS"
    ensure(inventory.is_file() and not inventory.is_symlink(),
           f"Artifact missing SHA256SUMS: {folder}")
    count = 0
    for line in inventory.read_text(encoding="ascii").splitlines():
        checksum, sep, filename = line.partition("  ")
        ensure(sep and re.fullmatch(r"[a-fA-F0-9]{64}", checksum) is not None,
               f"Malformed SHA256SUMS in {folder.name}")
        rel = PurePosixPath(filename)
        ensure(not rel.is_absolute() and ".." not in rel.parts and
               len(rel.parts) == 1, f"Unsafe SHA256SUMS filename: {filename}")
        file = folder / filename
        ensure(file.is_file() and not file.is_symlink(),
               f"Missing checksum-listed file: {filename}")
        ensure(file.name.startswith(f"GenyCaster-{TAG}-"),
               f"Artifact belongs to another release: {filename}")
        with file.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        ensure(digest == checksum.lower(), f"Invalid source artifact checksum: {filename}")
        count += 1
    ensure(count >= 1, f"Empty SHA256SUMS: {folder.name}")
    print(f"VERIFIED {folder.name}: {count} signed/packaged checksums", flush=True)


def download(destination: Path) -> None:
    ensure(not destination.exists() or not any(destination.iterdir()),
           "Recovery input directory must start empty")
    destination.mkdir(parents=True, exist_ok=True)
    for name in PRESERVED + (INTEL,):
        source = BASE_RUN if name != INTEL else INTEL_RUN
        target = destination / name
        command("gh", "run", "download", str(source),
                "--repo", REPO, "--name", name, "--dir", str(target),
                timeout=1200)
        verify_checksum_inventory(target)
    ensure(len(list(destination.iterdir())) == 8, "Incomplete artifact staging")
    print("RECOVERY DOWNLOAD PASS: all 8 platform artifacts independently checked")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--preflight", action="store_true")
    parser.add_argument("--download", type=Path)
    args = parser.parse_args()
    preflight()
    if args.download is not None:
        download(args.download)
    elif not args.preflight:
        parser.error("Specify --preflight or --download")


if __name__ == "__main__":
    main()
