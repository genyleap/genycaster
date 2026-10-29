#!/usr/bin/env python3
"""Reject explicit Apple SDK usr/include injection into project compile commands.

CMAKE_OSX_SYSROOT is the supported way to expose Apple SDK headers. An explicit
MacOSX.sdk/usr/include include search path can bypass a non-Apple Clang resource
header and break Darwin typedef discovery (for example __uint32_t and size_t).
"""

from __future__ import annotations

import json
import pathlib
import re
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_macos_sdk_includes.py <compile_commands.json>", file=sys.stderr)
        return 2

    database_path = pathlib.Path(sys.argv[1])
    if not database_path.is_file():
        print(f"compile database not found: {database_path}", file=sys.stderr)
        return 2

    commands = json.loads(database_path.read_text(encoding="utf-8"))
    violations: list[str] = []
    explicit_sdk_include = re.compile(
        r"(?:-I|-isystem)\s*(?:\"?)([^\"\s]*MacOSX\.sdk/usr/include)(?:\"?)"
    )

    for entry in commands:
        source = str(entry.get("file", ""))
        command = str(entry.get("command", ""))
        match = explicit_sdk_include.search(command)
        if match is not None:
            violations.append(f"{source}: explicit SDK include {match.group(1)}")

    if violations:
        print(
            "Apple toolchain contract violation: project commands must rely on "
            "CMAKE_OSX_SYSROOT rather than an explicit MacOSX.sdk/usr/include path.",
            file=sys.stderr,
        )
        for violation in violations:
            print(f"  {violation}", file=sys.stderr)
        return 1

    print("macOS SDK include guard: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
