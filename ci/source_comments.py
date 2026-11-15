"""Strip only comments from C++ and QML/JavaScript for architecture checks.

The scanner keeps string and raw-string contents untouched. In particular,
https:// endpoints and provider issuers in executable string literals remain
visible to the provider-boundary audit.
"""

from __future__ import annotations

import re

_RAW = re.compile(r'R"([^\\()\s]{0,16})\(')


def without_comments(source: str) -> str:
    """Mask C++/JS comments with spaces, retaining newlines and source offsets."""
    masked = list(source)
    length = len(source)
    index = 0
    while index < length:
        if source.startswith("//", index):
            end = source.find("\n", index + 2)
            if end < 0:
                end = length
            for cursor in range(index, end):
                masked[cursor] = " "
            index = end
            continue

        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            end = length if end < 0 else end + 2
            for cursor in range(index, end):
                if source[cursor] != "\n":
                    masked[cursor] = " "
            index = end
            continue

        # C++ raw literals can contain comment-looking delimiters and URLs.
        if source.startswith('R"', index):
            raw = _RAW.match(source, index)
            if raw:
                terminator = ")" + raw.group(1) + '"'
                end = source.find(terminator, raw.end())
                if end >= 0:
                    index = end + len(terminator)
                    continue

        # C++ digit separators (e.g. 9\'007\'199) are not char literals.
        # Treating one as an opening quote could suppress later security checks.
        if (source[index] == "'" and index > 0 and index + 1 < length
                and source[index - 1].isdigit() and source[index + 1].isdigit()):
            index += 1
            continue

        if source[index] in {'"', "'", "`"}:
            quote = source[index]
            index += 1
            while index < length:
                if source[index] == "\\":
                    index += 2
                    continue
                if source[index] == quote:
                    index += 1
                    break
                index += 1
            continue
        index += 1
    return "".join(masked)
