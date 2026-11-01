#!/usr/bin/env python3
"""Validate one structured multilingual release and render its GitHub Markdown deterministically."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

LOCALES = ("en-US", "fa-IR", "ar-SA", "de-DE", "es-ES", "fr-FR",
           "hi-IN", "ja-JP", "ko-KR", "pt-BR", "ru-RU", "zh-CN")
TYPES = {"fix", "improvement", "feature", "security", "maintenance"}


def load_release(path: Path) -> dict:
    release = json.loads(path.read_text(encoding="utf-8"))
    if release.get("schemaVersion") != 1 or release.get("type") not in TYPES:
        raise ValueError("Unsupported release-notes schema or release type")
    changes = release.get("changes")
    if not isinstance(changes, list) or not 1 <= len(changes) <= 12:
        raise ValueError("Expected 1–12 keyed changes")
    ids = [change.get("id") for change in changes]
    if (any(not isinstance(item, str) or not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", item)
            for item in ids) or len(set(ids)) != len(ids)):
        raise ValueError("Change IDs must be unique and well-formed")
    if any(change.get("category") not in TYPES for change in changes):
        raise ValueError("Invalid change category")
    translations = release.get("translations", {})
    if not isinstance(translations, dict) or set(translations) != set(LOCALES):
        raise ValueError("All 12 existing app locales must have release translations")
    for tag, localized in translations.items():
        if not isinstance(localized, dict) or set(localized.get("items", {})) != set(ids):
            raise ValueError(f"Missing or extra changes for {tag}")
        for name in ("title", "summary"):
            value = localized.get(name)
            if not isinstance(value, str) or not value.strip() or len(value) > 220:
                raise ValueError(f"Invalid {name} for {tag}")
        for change_id, message in localized["items"].items():
            if (not isinstance(message, str) or not message.strip()
                    or len(message) > 400 or "\n" in message):
                raise ValueError(f"Invalid translation {tag}/{change_id}")
    return release


def render_markdown(release: dict, version: str) -> str:
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)\d+)?", version):
        raise ValueError("Invalid semantic version")
    lines = [f"# Genycaster v{version}", "",
             "<!-- Generated from the versioned JSON by ci/release_notes.py; do not edit. -->",
             f"<!-- genycaster-release-type:{release['type']} -->", ""]
    for tag in LOCALES:
        local = release["translations"][tag]
        lines += [f"<!-- genycaster-locale:{tag} -->", f"## {local['title']}", "",
                  local["summary"], ""]
        lines.extend(f"- {local['items'][change['id']]}" for change in release["changes"])
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--markdown", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.source.stem != f"v{args.version}" or args.markdown.stem != args.source.stem:
        parser.error("Source, generated Markdown, and CMake version must agree")
    rendered = render_markdown(load_release(args.source), args.version)
    if args.check:
        if not args.markdown.exists() or args.markdown.read_text(encoding="utf-8") != rendered:
            parser.error("Generated Markdown is out of sync with canonical JSON")
    else:
        args.markdown.write_text(rendered, encoding="utf-8")
    print(f"Validated 12-localized release {args.version}: {args.source}")


if __name__ == "__main__":
    main()
