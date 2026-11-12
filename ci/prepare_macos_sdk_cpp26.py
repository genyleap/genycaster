#!/usr/bin/env python3
"""Generate build-local, C++26-correct casts for two Apple SDK enum expressions.

The installed SDK is read-only. Clang VFS overlays redirect just these two
header paths to generated files; the app, Mini App browser and all ObjC++
translation units continue compiling as C++26. Unknown SDK layouts fail closed.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

COREGRAPHICS = (
    "System/Library/Frameworks/CoreGraphics.framework/Headers/CGImage.h"
)
COLORSYNC = (
    "System/Library/Frameworks/ApplicationServices.framework/"
    "Frameworks/QD.framework/Headers/ColorSyncDeprecated.h"
)
CG_EXPRESSION = "return (CGBitmapInfo)(alpha | component | byteOrder | pixelFormat);"
CG_REWRITTEN = (
    "return (CGBitmapInfo)("
    "static_cast<unsigned int>(alpha) | static_cast<unsigned int>(component)"
    " | static_cast<unsigned int>(byteOrder)"
    " | static_cast<unsigned int>(pixelFormat));"
)
# Apple's ColorSync enum initializer combines distinct anonymous C enums. An
# explicit integral conversion of the first enumerator is enough: the remaining
# C enum operands undergo ordinary integral promotions, preserving every value.
COLOR_SUM = re.compile(
    r"(?P<before>^[ \t]*[A-Za-z_][A-Za-z0-9_]*[ \t]*=[ \t]*)"
    r"(?P<space>cm[A-Za-z0-9_]+Space)(?P<plus>[ \t]*\+[ \t]*cm[A-Za-z0-9_]+)",
    re.MULTILINE,
)
def rewrite_cg(data: str) -> tuple[str, int]:
    hits = data.count(CG_EXPRESSION)
    if hits == 1:
        return data.replace(CG_EXPRESSION, CG_REWRITTEN), hits
    if hits == 0:
        # A newer Apple SDK may already have repaired the expression.
        return data, 0
    raise ValueError(f"Ambiguous CGImage SDK signature ({hits} matches)")

def rewrite_color(data: str) -> tuple[str, int]:
    updated, count = COLOR_SUM.subn(
        lambda m: f"{m['before']}static_cast<int>({m['space']}){m['plus']}",
        data,
    )
    # A future SDK may have already fixed this; emit the unchanged header.
    return updated, count

def prepare(sdk: pathlib.Path, destination: pathlib.Path) -> dict[str, int]:
    if not sdk.is_absolute() or not sdk.is_dir():
        raise ValueError(f"macOS SDK directory is unavailable: {sdk}")
    destination.mkdir(parents=True, exist_ok=True)
    details: list[tuple[pathlib.Path, pathlib.Path]] = []
    counts: dict[str, int] = {}
    for relative, transform, name in (
        (COREGRAPHICS, rewrite_cg, "CGImage.h"),
        (COLORSYNC, rewrite_color, "ColorSyncDeprecated.h"),
    ):
        original = sdk / relative
        if not original.is_file():
            raise ValueError(f"Required Apple SDK header is unavailable: {original}")
        content = original.read_text(encoding="utf-8")
        output, rewritten = transform(content)
        destination_file = destination / name
        destination_file.write_text(output, encoding="utf-8")
        details.append((original, destination_file))
        # Header search sometimes reports the SDK via a versioned symlink
        # instead of MacOSX.sdk; map both spellings to the same local fix.
        resolved_original = original.resolve()
        if resolved_original != original:
            details.append((resolved_original, destination_file))
        counts[name] = rewritten

    # Clang recognizes the JSON representation of its YAML VFS overlay.
    # Never alter framework trees, header search order, or public includes.
    roots = [
        {
            "type": "directory",
            "name": str(original.parent),
            "contents": [{
                "type": "file",
                "name": original.name,
                "external-contents": str(generated),
            }],
        }
        for original, generated in details
    ]
    output_path = destination / "macos_cpp26_sdk_overlay.json"
    output_path.write_text(
        json.dumps({
            "version": 0,
            "case-sensitive": "true",
            "roots": roots,
        }, indent=2) + "\n",
        encoding="utf-8",
    )
    return counts

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    try:
        result = prepare(args.sdk, args.output)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"macOS SDK C++26 compatibility overlay: {error}", file=sys.stderr)
        return 1
    print("macOS SDK C++26 compatibility overlay: "
          + ", ".join(f"{k}={v} explicit enum conversions" for k, v in result.items()))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
