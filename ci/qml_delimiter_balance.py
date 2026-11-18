"""Early QML/JS delimiter scan before the Qt SDK is installed.

Only balances executable parentheses, ignoring strings, comments and JS regex
literals. Native Qt linting remains authoritative on actual build runners.
"""
from __future__ import annotations


def parentheses_balanced(source: str) -> bool:
    depth, index = 0, 0
    previous = ""
    state = "code"
    regex_class = False
    while index < len(source):
        char = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if state == "line_comment":
            if char == "\n":
                state = "code"
            index += 1
            continue
        if state == "block_comment":
            if char == "*" and following == "/":
                state = "code"
                index += 2
            else:
                index += 1
            continue
        if state in ("'", '"', chr(96)):
            if char == "\\":
                index += 2
            elif char == state:
                state = "code"
                index += 1
            else:
                index += 1
            continue
        if state == "regex":
            if char == "\\":
                index += 2
                continue
            if char == "[":
                regex_class = True
            elif char == "]" and regex_class:
                regex_class = False
            elif char == "/" and not regex_class:
                state = "code"
            index += 1
            continue
        if char == "/" and following == "/":
            state = "line_comment"
            index += 2
            continue
        if char == "/" and following == "*":
            state = "block_comment"
            index += 2
            continue
        if char in ("'", '"', chr(96)):
            state = char
            index += 1
            continue
        if char == "/" and (previous in "([=:,!&|?{}"
                            or source[max(0, index - 14):index].rstrip().endswith("return")):
            state = "regex"
            regex_class = False
            index += 1
            continue
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth < 0:
                return False
        if not char.isspace():
            previous = char
        index += 1
    return depth == 0 and state not in ("'", '"', chr(96), "regex", "block_comment")
