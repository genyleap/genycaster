#!/usr/bin/env python3
"""Fail when Genycaster UI localization coverage or ID-based translation invariants regress."""

from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ID_PATTERN = re.compile(r"(?:qsTrId|qtTrId)\(\s*\"([^\"]+)\"")
SOURCE_TRANSLATION_PATTERN = re.compile(r"\b(?:qsTr|qsTranslate)\s*\(")
VISIBLE_LITERAL_PATTERN = re.compile(
    r'^\s*(?:text|title|placeholderText|Accessible\.(?:name|description))\s*:\s*"([^"$]*[A-Za-z][^"]*)"',
    re.MULTILINE,
)
PLACEHOLDER_PATTERN = re.compile(r"%(?:L)?[1-9n]")

INTENTIONALLY_IDENTICAL_IDS = {
    "genycaster.app.genycaster",
    "genycaster.brand.geny_ticker",
    "genycaster.header.genycaster",
    "genycaster.side_nav.genycaster",
    "genycaster.detail.fid_value",
    "genycaster.locale.asset_quote",
    "genycaster.locale.compact_billion",
    "genycaster.locale.compact_million",
    "genycaster.locale.compact_thousand",
    "genycaster.locale.shahanshahi_long",
    "genycaster.locale.shahanshahi_medium",
    "genycaster.locale.shahanshahi_short",
    "genycaster.composer.markdown",
    "genycaster.composer.url",
    "genycaster.player.pause",
    "genycaster.player.volume",
    "genycaster.composer.fid",
    "genycaster.settings.contract_address_placeholder",
    "genycaster.settings.custom_color_placeholder",
    "genycaster.collectible.nft_badge",
    # Proper protocol/service names and placeholder-based brand labels
    # are intentionally identical across languages.
    "genycaster.settings.farcaster_label",
    "genycaster.settings.hypersnap_label",
    "genycaster.setup.network_farcaster_category",
    "genycaster.setup.network_hypersnap_category",
    "genycaster.setup.neynar_hub_farcaster",
    "genycaster.setup.pinata_hub_farcaster",
    "genycaster.setup.standard_crypto_hub_farcaster",
}

INTENTIONALLY_IDENTICAL_BY_LOCALE = {
    "de_DE": {
        "Video", "IRT — Toman", "Cast", "Recast", "System", "Ember", "Geny",
        "Ocean", "Violet", "Live", "Offline", "Editor %1", "Link", "Smileys", "Updates",
    },
    "es_ES": {
        "Recast · %1 recasts", "Shahanshahi (Imperial)", "Popular", "Cast", "Recast",
        "Ember", "Geny", "Editor %1", "Visual",
    },
    "fr_FR": {
        "Explorer", "Notifications", "Image", "Recast · %1 recasts", "IRT — Toman",
        "Messages", "Cast", "Recast", "Mode", "Geny", "Violet", "Mention", "Direct",
    },
    "pt_BR": {
        "Recast · %1 recasts", "Shahanshahi (Imperial)", "IRT — Toman", "%1 toman",
        "Popular", "Cast", "Recast", "Geny", "Offline", "Editor %1", "Visual", "Link",
    },
    "ru_RU": {"Cast", "Geny"},
    "ar_SA": {"Cast", "Ember", "Geny", "Ocean", "Violet"},
    "hi_IN": {"Cast", "Recast", "Ember", "Geny", "Ocean", "Violet"},
    "ja_JP": {"Ember", "Geny", "Ocean", "Violet"},
    "ko_KR": {"Ember", "Geny", "Ocean", "Violet"},
    "zh_CN": {"Cast", "Geny"},
}


def intentionally_identical(identifier: str, source: str, locale: str) -> bool:
    if identifier in INTENTIONALLY_IDENTICAL_IDS:
        return True
    if identifier.startswith("genycaster.settings.theme_"):
        return True
    return source in INTENTIONALLY_IDENTICAL_BY_LOCALE.get(locale, set())


def read_ts(path: Path) -> dict[str, tuple[str, str]]:
    root = ET.parse(path).getroot()
    messages: dict[str, tuple[str, str]] = {}
    for message in root.findall(".//message"):
        identifier = message.get("id", "").strip()
        if not identifier:
            continue
        source = message.findtext("source", default="")
        translation = message.findtext("translation", default="")
        messages[identifier] = (source, translation)
    return messages


def named_contexts(path: Path) -> list[str]:
    root = ET.parse(path).getroot()
    return [
        name
        for context in root.findall("context")
        if (name := context.findtext("name", default="").strip())
    ]


def placeholders(value: str) -> list[str]:
    return sorted(PLACEHOLDER_PATTERN.findall(value))


def main() -> int:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd().resolve()
    qml_files = sorted((root / "ui").rglob("*.qml"))
    # Translation IDs are part of the complete UI surface, not only the main
    # composition root. Scan every UI C++/module/header source so optional
    # Mini App and controller strings cannot be misclassified as catalog orphans.
    ui_cpp_files = sorted(
        path
        for suffix in ("*.cpp", "*.cppm", "*.hpp")
        for path in (root / "ui").rglob(suffix)
        if path != root / "ui" / "i18n" / "catalog.cpp"
    )
    presentation_cpp_files = [
        root / "src" / "presentation" / "qt" / "presentation_bridge.cpp",
        root / "src" / "presentation" / "qt" / "localization_controller.cpp",
        root / "src" / "presentation" / "qt" / "rich_composer_controller.cpp",
        root / "src" / "presentation" / "qt" / "media_upload_controller.cpp",
    ]
    source_files = [
        *qml_files,
        *ui_cpp_files,
        *[path for path in presentation_cpp_files if path.exists()],
    ]

    errors: list[str] = []
    referenced_ids: set[str] = set()
    for path in source_files:
        text = path.read_text(encoding="utf-8")
        referenced_ids.update(ID_PATTERN.findall(text))
        if SOURCE_TRANSLATION_PATTERN.search(text):
            errors.append(f"{path.relative_to(root)}: source-text translation call found; use qsTrId/qtTrId")
        if path.suffix == ".qml":
            for match in VISIBLE_LITERAL_PATTERN.finditer(text):
                errors.append(
                    f"{path.relative_to(root)}: visible hard-coded Latin text: {match.group(1)!r}"
                )

    # Profile category/role labels are a finite server-validated vocabulary.
    # The QML constructs their translation keys from the canonical option,
    # rather than embedding literal IDs in individual qsTrId calls. Resolve
    # this specific bounded vocabulary when checking coverage: an unknown
    # option still fails if it has no translation in the English catalog.
    profile_path = root / "ui" / "components" / "ProfileSurface.qml"
    if profile_path.is_file():
        profile_text = profile_path.read_text(encoding="utf-8")
        if 'qsTrId("genycaster.profile.option." + key)' in profile_text:
            referenced_ids.discard("genycaster.profile.option.")
            options_region = profile_text.split("readonly property var profileCategoryOptions:", 1)
            if len(options_region) == 2:
                options_region = options_region[1].split("function profileOptionLabel(", 1)[0]
                for canonical in re.findall(r'"([^"\\n]+)"', options_region):
                    key = re.sub(r"[^a-z0-9]+", "_", canonical.lower()).strip("_")
                    if key:
                        referenced_ids.add("genycaster.profile.option." + key)
            else:
                errors.append("ProfileSurface: canonical category/role list not found")

    locale_paths = sorted((root / "ui" / "i18n").glob("qml_*.ts"))
    catalogs = {path.stem.removeprefix("qml_"): read_ts(path) for path in locale_paths}
    required_locales = {
        "en_US", "fa_IR", "de_DE", "ja_JP", "ko_KR", "zh_CN",
        "hi_IN", "ar_SA", "es_ES", "fr_FR", "pt_BR", "ru_RU",
    }
    missing_locales = sorted(required_locales - set(catalogs))
    if missing_locales:
        errors.append("missing required locale catalogs: " + ", ".join(missing_locales))

    for locale, catalog in catalogs.items():
        path = root / "ui" / "i18n" / f"qml_{locale}.ts"
        contexts = named_contexts(path)
        if contexts:
            errors.append(
                f"qml_{locale}.ts: ID-based catalogs require empty contexts; found "
                + ", ".join(contexts)
            )

    english = catalogs.get("en_US", {})
    for identifier in sorted(referenced_ids):
        if identifier not in english:
            errors.append(f"qml_en_US.ts: missing referenced id {identifier}")
            continue
        en_source, en_translation = english[identifier]
        if not en_translation.strip():
            errors.append(f"qml_en_US.ts: empty translation for {identifier}")
        if placeholders(en_source) != placeholders(en_translation):
            errors.append(f"qml_en_US.ts: placeholder mismatch for {identifier}")

        for locale, catalog in catalogs.items():
            if locale == "en_US":
                continue
            if identifier not in catalog:
                errors.append(f"qml_{locale}.ts: missing referenced id {identifier}")
                continue
            source, translation = catalog[identifier]
            if source != en_source:
                errors.append(f"catalog source mismatch for {identifier} in {locale}")
            if not translation.strip():
                errors.append(f"qml_{locale}.ts: empty translation for {identifier}")
            if placeholders(en_source) != placeholders(translation):
                errors.append(f"qml_{locale}.ts: placeholder mismatch for {identifier}")
            if (
                translation.strip() == en_source.strip()
                and not intentionally_identical(identifier, en_source.strip(), locale)
            ):
                errors.append(
                    f"qml_{locale}.ts: untranslated source text for {identifier}: {en_source!r}"
                )

    orphan_ids = sorted(set(english) - referenced_ids)
    if len(orphan_ids) > 4:
        errors.append(
            "qml_en_US.ts: too many unreferenced ids: " + ", ".join(orphan_ids[:12])
        )

    english_ids = set(english)
    for locale, catalog in catalogs.items():
        if set(catalog) != english_ids:
            missing = sorted(english_ids - set(catalog))
            extra = sorted(set(catalog) - english_ids)
            if missing:
                errors.append(f"ids missing in {locale}: " + ", ".join(missing[:12]))
            if extra:
                errors.append(f"extra ids in {locale}: " + ", ".join(extra[:12]))

    if errors:
        print("Localization catalog guard: FAILED", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        f"Localization catalog guard: PASS ({len(referenced_ids)} referenced IDs, "
        f"{len(english)} catalog IDs, {len(catalogs)} locales)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
