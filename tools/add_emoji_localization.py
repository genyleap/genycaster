#!/usr/bin/env python3
"""Adds the emoji-platform qsTrId entries to every Genycaster locale catalog.

Run after introducing new ID-based translation keys for the animated emoji
system. Idempotent and deterministic: each catalog is regenerated in the
canonical lupdate-like layout (single unnamed context, messages sorted by id).
English is the source of truth and every other locale must carry a real
translation (never a copy of the source).
"""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent.parent
TS_DIR = ROOT / "ui" / "i18n"

# id -> (English source, {locale: translation})
ENTRIES: dict[str, tuple[str, dict[str, str]]] = {
    "genycaster.emoji.animated": ("Animated", {
        "fa_IR": "متحرک", "de_DE": "Animiert", "ja_JP": "アニメーション",
        "ko_KR": "애니메이션", "zh_CN": "动态", "hi_IN": "एनिमेटेड",
        "ar_SA": "متحركة", "es_ES": "Animados", "fr_FR": "Animés",
        "pt_BR": "Animados", "ru_RU": "Анимированные",
    }),
    "genycaster.emoji.search_placeholder": ("Search emoji", {
        "fa_IR": "جستجوی ایموجی", "de_DE": "Emoji suchen", "ja_JP": "絵文字を検索",
        "ko_KR": "이모지 검색", "zh_CN": "搜索表情符号", "hi_IN": "इमोजी खोजें",
        "ar_SA": "ابحث عن رمز تعبيري", "es_ES": "Buscar emoji", "fr_FR": "Rechercher un emoji",
        "pt_BR": "Buscar emoji", "ru_RU": "Поиск эмодзи",
    }),
    "genycaster.emoji.category.recent": ("Recent", {
        "fa_IR": "اخیر", "de_DE": "Zuletzt", "ja_JP": "最近", "ko_KR": "최근",
        "zh_CN": "最近", "hi_IN": "हाल के", "ar_SA": "الأخيرة", "es_ES": "Recientes",
        "fr_FR": "Récents", "pt_BR": "Recentes", "ru_RU": "Недавние",
    }),
    "genycaster.emoji.category.smileys": ("Smileys", {
        "fa_IR": "شکلک‌ها", "de_DE": "Smileys", "ja_JP": "笑顔", "ko_KR": "스마일",
        "zh_CN": "表情", "hi_IN": "स्माइली", "ar_SA": "ابتسامات", "es_ES": "Emojis",
        "fr_FR": "Émoticônes", "pt_BR": "Emojis", "ru_RU": "Смайлики",
    }),
    "genycaster.emoji.category.people": ("People", {
        "fa_IR": "افراد", "de_DE": "Personen", "ja_JP": "人々", "ko_KR": "사람",
        "zh_CN": "人物", "hi_IN": "लोग", "ar_SA": "أشخاص", "es_ES": "Personas",
        "fr_FR": "Personnes", "pt_BR": "Pessoas", "ru_RU": "Люди",
    }),
    "genycaster.emoji.category.animals": ("Animals", {
        "fa_IR": "حیوانات", "de_DE": "Tiere", "ja_JP": "動物", "ko_KR": "동물",
        "zh_CN": "动物", "hi_IN": "जानवर", "ar_SA": "حيوانات", "es_ES": "Animales",
        "fr_FR": "Animaux", "pt_BR": "Animais", "ru_RU": "Животные",
    }),
    "genycaster.emoji.category.food": ("Food", {
        "fa_IR": "غذا", "de_DE": "Essen", "ja_JP": "食べ物", "ko_KR": "음식",
        "zh_CN": "食物", "hi_IN": "खाना", "ar_SA": "طعام", "es_ES": "Comida",
        "fr_FR": "Nourriture", "pt_BR": "Comida", "ru_RU": "Еда",
    }),
    "genycaster.emoji.category.activities": ("Activities", {
        "fa_IR": "فعالیت‌ها", "de_DE": "Aktivitäten", "ja_JP": "アクティビティ",
        "ko_KR": "활동", "zh_CN": "活动", "hi_IN": "गतिविधियाँ", "ar_SA": "أنشطة",
        "es_ES": "Actividades", "fr_FR": "Activités", "pt_BR": "Atividades",
        "ru_RU": "Действия",
    }),
    "genycaster.emoji.category.travel": ("Travel", {
        "fa_IR": "سفر", "de_DE": "Reisen", "ja_JP": "旅行", "ko_KR": "여행",
        "zh_CN": "旅行", "hi_IN": "यात्रा", "ar_SA": "سفر", "es_ES": "Viajes",
        "fr_FR": "Voyages", "pt_BR": "Viagem", "ru_RU": "Путешествия",
    }),
    "genycaster.emoji.category.objects": ("Objects", {
        "fa_IR": "اشیاء", "de_DE": "Objekte", "ja_JP": "物", "ko_KR": "사물",
        "zh_CN": "物品", "hi_IN": "वस्तुएँ", "ar_SA": "أشياء", "es_ES": "Objetos",
        "fr_FR": "Objets", "pt_BR": "Objetos", "ru_RU": "Предметы",
    }),
    "genycaster.emoji.category.symbols": ("Symbols", {
        "fa_IR": "نمادها", "de_DE": "Symbole", "ja_JP": "記号", "ko_KR": "기호",
        "zh_CN": "符号", "hi_IN": "प्रतीक", "ar_SA": "رموز", "es_ES": "Símbolos",
        "fr_FR": "Symboles", "pt_BR": "Símbolos", "ru_RU": "Символы",
    }),
    "genycaster.emoji.category.flags": ("Flags", {
        "fa_IR": "پرچم‌ها", "de_DE": "Flaggen", "ja_JP": "旗", "ko_KR": "깃발",
        "zh_CN": "旗帜", "hi_IN": "झंडे", "ar_SA": "أعلام", "es_ES": "Banderas",
        "fr_FR": "Drapeaux", "pt_BR": "Bandeiras", "ru_RU": "Флаги",
    }),
}

INDENT = "    "


def serialize_message(identifier: str, source: str, translation: str, depth: int) -> str:
    pad = INDENT * depth
    inner = INDENT * (depth + 1)
    return (
        f"{pad}<message id=\"{escape(identifier)}\">\n"
        f"{inner}<source>{escape(source)}</source>\n"
        f"{inner}<translation>{escape(translation)}</translation>\n"
        f"{pad}</message>"
    )


def read_catalog(path: Path) -> dict[str, tuple[str, str]]:
    root_el = ET.parse(path).getroot()
    messages: dict[str, tuple[str, str]] = {}
    for message in root_el.findall(".//message"):
        identifier = message.get("id", "").strip()
        if not identifier:
            continue
        source = message.findtext("source", default="")
        translation = message.findtext("translation", default="")
        messages[identifier] = (source, translation)
    return messages


def main() -> int:
    errors: list[str] = []
    for ts_path in sorted(TS_DIR.glob("qml_*.ts")):
        locale = ts_path.stem.removeprefix("qml_")
        catalog = read_catalog(ts_path)
        for identifier, (source, translations) in ENTRIES.items():
            translation = source if locale == "en_US" else translations.get(locale, "")
            if locale != "en_US" and not translation:
                errors.append(f"{ts_path.name}: no translation for {identifier}")
                continue
            catalog[identifier] = (source, translation)

        lines: list[str] = []
        lines.append("<?xml version='1.0' encoding='utf-8'?>")
        lines.append(
            f'<TS version="2.1" language="{locale}" sourcelanguage="en_US">'
        )
        lines.append(f"{INDENT}<context>")
        lines.append(f"{INDENT*2}<name />")
        for identifier in sorted(catalog):
            source, translation = catalog[identifier]
            lines.append(serialize_message(identifier, source, translation, 2))
        lines.append(f"{INDENT}</context>")
        lines.append("</TS>")
        lines.append("")
        ts_path.write_text("\n".join(lines), encoding="utf-8")
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
