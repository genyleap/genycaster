#!/usr/bin/env python3
"""Register initial-setup texts with Qt's ID-based localization catalog."""
from pathlib import Path
import xml.etree.ElementTree as ET

base = Path(__file__).resolve().parents[1]
messages = {
    "choose_network": "Choose a connection",
    "connection_mode_title": "Choose how you connect",
    "connection_mode_help": "Select a sign-in route; advanced node settings control where the app reads data.",
    "hypersnap_explainer": "Uses Hypersnap for live reads. Sign-in and publishing permissions are verified separately.",
    "farcaster_explainer": "Uses Farcaster sign-in. This build still reads from Hypersnap; no keys are transferred.",
    "profile_choice": "Your profile",
    "existing_profile": "Use existing",
    "new_profile": "Create new",
    "profile_creation_browser": "Register your new FID in a compatible app, then return here to connect it. Genycaster never asks for your recovery phrase.",
    "open_registration": "Find a registration app",
    "already_created": "I have a profile — connect",
    "network_settings_explainer": "Choose your preferred connection. Reads currently use Hypersnap and signed writes use Snapchain. Switching does not migrate your identity or keys.",
    "advanced_nodes": "Advanced: choose read node",
    "existing_profile_body": "Connect the Farcaster identity you already own.",
    "new_profile_body": "Register a new Farcaster identity in a compatible app.",
    "registration_handoff": "Continue in a compatible registration app, then come back to link your profile.",
    "authentication_explainer": "Sign-in uses a verified OpenProof session and a linked Farcaster FID. Publishing permissions are separate.",
    "workspace_label": "Secure account setup",
    "progress_label": "Setup steps",
    "explicit_terms_agreement": "I agree to the Terms of Use",
    "explicit_privacy_agreement": "I have read and acknowledge the Privacy Policy",
    "review_terms_first": "Open and review the Terms before enabling this agreement.",
    "review_privacy_first": "Open and review the Privacy Policy before acknowledging it.",
    "mark_document_reviewed": "I reviewed the document",
    "open_governing_document": "Open the official document and review the summary to continue.",
    "auth_rejected_recover": "Genyleap rejected this sign-in. Start a new request or try Farcaster sign-in.",
    "auth_provider_response_invalid": "The sign-in service could not complete the verification. Retry or use the other sign-in method.",
    "auth_conflict": "Another sign-in request conflicts with this one. Cancel and start again.",
    "auth_missing_link": "No linked Farcaster identity was found. Connect your existing profile after signing in.",
}
fa = {
    "choose_network": "مسیر اتصال",
    "connection_mode_title": "روش اتصال را انتخاب کنید",
    "connection_mode_help": "مسیر ورود را انتخاب کنید؛ تنظیمات پیشرفته، منبع دریافت داده‌ها را کنترل می‌کند.",
    "hypersnap_explainer": "داده‌های زنده از هایپراسنپ دریافت می‌شوند. ورود و مجوز انتشار جداگانه تأیید می‌شوند.",
    "farcaster_explainer": "ورود از مسیر فارکستر انجام می‌شود. در این نسخه داده‌ها همچنان از هایپراسنپ دریافت می‌شوند و هیچ کلیدی منتقل نمی‌شود.",
    "profile_choice": "پروفایل شما",
    "existing_profile": "پروفایل قبلی",
    "new_profile": "ساخت پروفایل",
    "profile_creation_browser": "شناسه جدید را در یک برنامه سازگار بسازید و سپس برای اتصال برگردید. جنی‌کستر هرگز عبارت بازیابی را درخواست نمی‌کند.",
    "open_registration": "برنامه ثبت‌نام را پیدا کن",
    "already_created": "پروفایل دارم؛ اتصال",
    "network_settings_explainer": "مسیر اتصال دلخواه ذخیره می‌شود. خواندن داده‌ها فعلاً با هایپراسنپ و انتشار امضاشده با اسنپ‌چین است. تغییر انتخاب، هویت یا کلیدها را جابه‌جا نمی‌کند.",
    "advanced_nodes": "پیشرفته: انتخاب نود دریافت داده",
    "existing_profile_body": "هویت فارکستر فعلی خودتان را متصل کنید.",
    "new_profile_body": "در یک برنامه سازگار، هویت فارکستر جدید بسازید.",
    "registration_handoff": "ساخت پروفایل در برنامه ثبت‌نام انجام می‌شود؛ سپس برای اتصال به جنی‌کستر برگردید.",
    "authentication_explainer": "ورود با نشست تأییدشده OpenProof و شناسه فارکستر پیوندخورده انجام می‌شود. مجوز انتشار جداگانه است.",
    "workspace_label": "راه‌اندازی امن حساب",
    "progress_label": "مراحل راه‌اندازی",
    "explicit_terms_agreement": "با شرایط استفاده موافقم",
    "explicit_privacy_agreement": "سیاست حریم خصوصی را مطالعه کردم و تأیید می‌کنم",
    "review_terms_first": "پیش از پذیرش، شرایط استفاده را باز کرده و بررسی کنید.",
    "review_privacy_first": "پیش از تأیید، سیاست حریم خصوصی را باز کرده و بررسی کنید.",
    "mark_document_reviewed": "سند را بررسی کردم",
    "open_governing_document": "سند رسمی را باز کنید و خلاصه آن را بررسی کنید.",
    "auth_rejected_recover": "جنی‌لیپ این ورود را نپذیرفت. درخواست جدید ایجاد کنید یا از ورود با فارکستر استفاده کنید.",
    "auth_provider_response_invalid": "سرویس ورود نتوانست تأیید را تکمیل کند. دوباره تلاش کنید یا مسیر دیگر را انتخاب کنید.",
    "auth_conflict": "درخواست ورود دیگری تداخل دارد. لغو کنید و دوباره شروع کنید.",
    "auth_missing_link": "هویت فارکستر پیوندخورده پیدا نشد. پس از ورود پروفایل موجود را متصل کنید.",
}
for path in sorted((base / "ui/i18n").glob("qml_*.ts")):
    tree = ET.parse(path)
    root = tree.getroot()
    context = root.find("context")
    if context is None:
        context = ET.SubElement(root, "context")
        ET.SubElement(context, "name").text = ""
    existing = {m.attrib.get("id") for m in root.iter("message")}
    # Keep the authoring catalog and already-generated TS source text synchronized.
    # Preserve human translations when they are not the previous English fallback.
    for message in root.iter("message"):
        message_id = message.attrib.get("id", "")
        if not message_id.startswith("genycaster.setup."):
            continue
        key = message_id.removeprefix("genycaster.setup.")
        if key not in messages:
            continue
        source_node = message.find("source")
        translation_node = message.find("translation")
        if source_node is None or translation_node is None:
            continue
        old_source = source_node.text or ""
        if old_source == messages[key]:
            continue
        source_node.text = messages[key]
        if path.name == "qml_fa_IR.ts":
            translation_node.text = fa.get(key, messages[key])
        elif translation_node.text == old_source or not translation_node.text:
            translation_node.text = messages[key]
    for key, source in messages.items():
        message_id = "genycaster.setup." + key
        if message_id in existing:
            continue
        message = ET.SubElement(context, "message", {"id": message_id})
        ET.SubElement(message, "source").text = source
        ET.SubElement(message, "translation").text = fa.get(key, source) if path.name == "qml_fa_IR.ts" else source
    ET.indent(tree, space="    ")
    tree.write(path, encoding="utf-8", xml_declaration=True)

catalog = base / "ui/i18n/catalog.cpp"
original = catalog.read_text()
lines = "".join(
    f'    //% "{source}"\n    (void)qtTrId("genycaster.setup.{key}");\n'
    for key, source in messages.items()
    if f'genycaster.setup.{key}"' not in original
)
anchor = '    (void)qtTrId("genycaster.shell.sign_in_with_farcaster");\n'
if lines:
    if anchor not in original:
        raise SystemExit("Cannot locate catalog anchor")
    catalog.write_text(original.replace(anchor, anchor + lines, 1))
print("Setup localization IDs:", len(messages))
