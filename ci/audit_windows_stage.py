#!/usr/bin/env python3
"""Audit a deployable Windows runtime tree before CPack builds the installer.

windeployqt may copy host-side Qt SDK directories alongside runtime plugins.
Those development headers, import libraries and command-line tools are not
runtime dependencies. Qt's actual plugin directories and qml/ are preserved.
The architecture audit reads PE COFF headers without third-party packages.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import struct

DEVELOPMENT_DIRECTORIES = ("bin", "include", "lib", "mkspecs", "qmltooling")
RUNTIME_DLLS = ("Qt6Core.dll", "Qt6Network.dll", "qt6keychain.dll")
PE_MACHINES = {"arm64": 0xAA64, "x86_64": 0x8664}


def pe_machine(path: Path) -> int:
    with path.open("rb") as stream:
        header = stream.read(64)
        if len(header) < 64 or header[:2] != b"MZ":
            raise ValueError(f"Invalid PE DOS header: {path}")
        offset = struct.unpack_from("<I", header, 0x3C)[0]
        if not 64 <= offset <= 16 * 1024 * 1024:
            raise ValueError(f"Invalid PE header offset: {path}")
        stream.seek(offset)
        coff = stream.read(6)
        if len(coff) != 6 or coff[:4] != b"PE\x00\x00":
            raise ValueError(f"Invalid PE signature: {path}")
        return struct.unpack_from("<H", coff, 4)[0]


def audit(stage: Path, arch: str, *, clean: bool = True) -> int:
    if arch not in PE_MACHINES:
        raise ValueError(f"Unsupported Windows target: {arch}")
    if not stage.is_dir():
        raise ValueError(f"Missing Windows runtime stage: {stage}")
    if clean:
        for name in DEVELOPMENT_DIRECTORIES:
            directory = stage / name
            if directory.is_symlink():
                raise ValueError(f"Unexpected SDK directory symlink: {directory}")
            if directory.is_dir():
                shutil.rmtree(directory)

    for name in ("Genycaster.exe", *RUNTIME_DLLS, "platforms/qwindows.dll"):
        if not (stage / name).is_file():
            raise ValueError(f"Runtime dependency missing: {name}")
    if not (stage / "qml").is_dir():
        raise ValueError("Qt QML modules missing from Windows runtime")
    for name in DEVELOPMENT_DIRECTORIES:
        if (stage / name).exists():
            raise ValueError(f"SDK-only directory packaged in installer: {name}")

    binaries = sorted(path for path in stage.rglob("*")
                      if path.is_file() and path.suffix.lower() == ".dll")
    binaries.insert(0, stage / "Genycaster.exe")
    for path in binaries:
        machine = pe_machine(path)
        if machine != PE_MACHINES[arch]:
            raise ValueError(
                f"Wrong Windows PE architecture in {path.relative_to(stage)}: "
                f"0x{machine:04x}, expected 0x{PE_MACHINES[arch]:04x}"
            )
    print(f"Verified {len(binaries)} Windows {arch} PE runtime binaries")
    return len(binaries)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", type=Path)
    parser.add_argument("arch", choices=tuple(PE_MACHINES))
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    audit(args.stage, args.arch, clean=not args.verify_only)


if __name__ == "__main__":
    main()
