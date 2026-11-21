#!/usr/bin/env python3
"""Repack the Android-only CI test artifact for owner-key signing, safely.

The CI APK is deliberately signed using an ephemeral key. Repackage its zip
members (excluding only APK v1 signing records) to strip both v1 and APK v2/v3
signatures; then run zipalign and apksigner with the existing owner's release
key. Never publish the CI test certificate.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import re
import shutil
import zipfile

SIGNATURE = re.compile(r"^META-INF/(?:MANIFEST\.MF|[^/]+\.(?:SF|RSA|DSA|EC))$", re.I)


def check(ok: bool, message: str) -> None:
    if not ok:
        raise RuntimeError(message)


def content_fingerprints(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    with zipfile.ZipFile(path, "r") as archive:
        for info in archive.infolist():
            name = info.filename
            check(name not in result, f"Duplicate ZIP member {name}")
            parsed = PurePosixPath(name)
            check(not parsed.is_absolute() and ".." not in parsed.parts,
                  f"Unsafe ZIP member {name}")
            if info.is_dir() or SIGNATURE.fullmatch(name):
                continue
            check(info.file_size <= 550 * 1024 * 1024,
                  f"Unexpected APK entry size {name}")
            sha = hashlib.sha256()
            with archive.open(info, "r") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    sha.update(block)
            result[name] = sha.hexdigest()
    return result


def unpack(source: Path, output: Path) -> None:
    check(source.is_file() and not output.exists(),
          "Missing source APK or unsigned output already exists")
    check(source.stat().st_size > 1024 * 1024,
          "Source APK is unexpectedly small")
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(
            output, "w", allowZip64=True) as unsigned:
        names: set[str] = set()
        for info in original.infolist():
            check(info.filename not in names,
                  f"Duplicate source APK ZIP entry {info.filename}")
            names.add(info.filename)
            parsed = PurePosixPath(info.filename)
            check(not parsed.is_absolute() and ".." not in parsed.parts,
                  f"Unsafe ZIP member {info.filename}")
            if SIGNATURE.fullmatch(info.filename):
                continue
            check(info.file_size <= 550 * 1024 * 1024,
                  f"Unexpected APK file size: {info.filename}")
            if info.is_dir():
                unsigned.writestr(info, b"")
            else:
                with original.open(info, "r") as reader, unsigned.open(info, "w") as writer:
                    shutil.copyfileobj(reader, writer, 1024 * 1024)
    original_fingerprints = content_fingerprints(source)
    new_fingerprints = content_fingerprints(output)
    check(original_fingerprints == new_fingerprints,
          "APK source content changed while removing ephemeral test signature")
    check("AndroidManifest.xml" in new_fingerprints,
          "Missing Android package manifest")
    check(any(name.startswith("lib/arm64-v8a/") and name.endswith(".so")
              for name in new_fingerprints),
          "Missing Android arm64 native libraries")
    check(not any(name.startswith("lib/x86") for name in new_fingerprints),
          "Unexpected non-arm64 library packaged")
    print(f"Content identity VERIFIED: {len(new_fingerprints)} APK members, arm64-v8a only")


def replace_checksum(old: Path, apk: Path, output: Path) -> None:
    check(old.is_file() and apk.is_file() and not output.exists(),
          "Missing checksums or new APK")
    original = old.read_text("utf-8")
    lines = original.splitlines(keepends=True)
    check(len(lines) == 15, "Unexpected size of existing v1.0.0 checksums")
    check(all(re.fullmatch(r"[a-f0-9]{64}  [A-Za-z0-9._-]+\n?", line)
              for line in lines), "Existing SHA256SUMS format invalid")
    matches = [i for i, line in enumerate(lines)
               if line.strip().endswith("  " + apk.name)]
    check(len(matches) == 1,
          "Existing SHA256SUMS must include exactly one original Android APK")
    digest = hashlib.sha256()
    with apk.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    new_line = digest.hexdigest() + "  " + apk.name + "\n"
    lines[matches[0]] = new_line
    output.write_text("".join(lines), encoding="utf-8")
    unchanged = [
        (a, b) for i, (a, b) in enumerate(zip(original.splitlines(), output.read_text().splitlines()))
        if i != matches[0]
    ]
    check(all(a == b for a, b in unchanged) and len(unchanged) == 14,
          "Checksums for a non-Android release asset were changed")
    print("Checksums VERIFIED: exactly one Android APK SHA-256 was updated")


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    unpack_command = sub.add_parser("unpack")
    unpack_command.add_argument("ci_test_apk", type=Path)
    unpack_command.add_argument("unsigned_apk", type=Path)
    check_command = sub.add_parser("verify")
    check_command.add_argument("ci_test_apk", type=Path)
    check_command.add_argument("signed_apk", type=Path)
    checksum_command = sub.add_parser("checksums")
    checksum_command.add_argument("original_checksums", type=Path)
    checksum_command.add_argument("signed_apk", type=Path)
    checksum_command.add_argument("new_checksums", type=Path)
    args = parser.parse_args()
    if args.command == "unpack":
        unpack(args.ci_test_apk, args.unsigned_apk)
    elif args.command == "verify":
        check(content_fingerprints(args.ci_test_apk) ==
              content_fingerprints(args.signed_apk),
              "Signed APK differs from successful Android CI payload")
        print("PASS: owner-key APK has byte-identical non-signature app payload to CI")
    else:
        replace_checksum(args.original_checksums, args.signed_apk,
                         args.new_checksums)


if __name__ == "__main__":
    main()
