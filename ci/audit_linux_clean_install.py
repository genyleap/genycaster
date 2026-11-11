#!/usr/bin/env python3
"""Audit every shipped dynamic ELF in an installed Linux release.

Run in a minimal target distro after apt installs the .deb; inspecting only the
main executable overlooks Qt multimedia, WebEngine, QML, and TLS plugins.
"""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


def audit(root: Path) -> int:
    if not (root / "bin/Genycaster").is_file():
        raise ValueError(f"Installed GenyCaster executable missing: {root}")
    environment = dict(os.environ)
    environment["LD_LIBRARY_PATH"] = str(root / "lib")
    failures: dict[str, list[str]] = {}
    checked = 0

    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as file:
            if file.read(4) != b"\x7fELF":
                continue
        checked += 1
        result = subprocess.run(
            ["ldd", str(path)],
            capture_output=True,
            text=True,
            env=environment,
            timeout=30,
            check=False,
        )
        output = result.stdout + "\n" + result.stderr
        # Static binaries do not have dynamic link dependencies.
        if "not a dynamic executable" in output or "statically linked" in output:
            continue
        messages = [
            line.strip() for line in output.splitlines()
            if "not found" in line or "version `" in line
        ]
        if messages or result.returncode != 0:
            failures[str(path.relative_to(root))] = (
                messages or [f"ldd exit {result.returncode}: {output.strip()[:300]}"]
            )

    if checked == 0:
        raise ValueError(f"No ELF files inspected: {root}")
    if failures:
        lines = [
            f"Unresolved dependencies across {len(failures)} installed ELF files:"
        ]
        for path, reasons in sorted(failures.items()):
            lines.append(f"  {path}")
            lines.extend(f"    {reason}" for reason in reasons)
        raise ValueError("\n".join(lines))
    print(f"Clean Ubuntu ELF dependency audit: PASS ({checked} shipped ELF files)")
    return checked


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("Usage: audit_linux_clean_install.py /opt/genycaster/usr")
        audit(Path(sys.argv[1]))
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        sys.exit(str(error))
