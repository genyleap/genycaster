#!/usr/bin/env python3
"""Register initial-setup texts with Qt's ID-based localization catalog."""
from pathlib import Path
import xml.etree.ElementTree as ET

base = Path(__file__).resolve().parents[1]
messages = {
    "choose_network": "Choose a connection",
    "connection_mode_title": "Choose how you connect",
    "connection_mode_help": "Choose which network Genycaster reads casts and profiles from.",
    "centralized_service_label": "Centralized",
    "decentralized_label": "Decentralized",
    "farcaster_not_configured": "Uses Farcaster via Neynar. A read API key is needed; Hypersnap remains available.",
    "farcaster_hosted_node": "Farcaster · Neynar API",
    "farcaster_configured": "https://api.neynar.com · API credential configured.",
    "farcaster_missing_key": "https://api.neynar.com · API key not configured. Set GENYCASTER_FARCASTER_READ_API_KEY.",
    "document_accepted": "Accepted",
    "confirm_document": "Scroll to the end, then confirm this document.",
    "legal_click_to_review": "Open both notices, review the summaries and confirm each one to continue.",
    "legal_summary_label": "Product notice summary",
    "hypersnap_explainer": "Uses Hypersnap for live reads. Sign-in and publishing permissions are verified separately.",
    "farcaster_explainer": "Uses the Farcaster/Neynar read API. Your sign-in and publishing keys are separate.",
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
    "account_methods_explainer": "Use your Farcaster identity or Genyleap account. Each sign-in is verified independently; neither changes your read network.",
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
    "connection_mode_help": "شبکه دریافت پست‌ها و پروفایل‌ها را برای جنی‌کستر انتخاب کنید.",
    "centralized_service_label": "متمرکز",
    "decentralized_label": "غیرمتمرکز",
    "farcaster_not_configured": "دریافت داده از فارکستر با نینار به کلید API نیاز دارد؛ هایپراسنپ در دسترس است.",
    "farcaster_hosted_node": "فارکستر · API نینار",
    "farcaster_configured": "https://api.neynar.com · کلید API پیکربندی شده است.",
    "farcaster_missing_key": "https://api.neynar.com · کلید API تنظیم نشده است: GENYCASTER_FARCASTER_READ_API_KEY",
    "document_accepted": "پذیرفته شد",
    "confirm_document": "متن را تا انتها مرور کنید و سپس تأیید کنید.",
    "legal_click_to_review": "هر دو سند را باز کنید، خلاصه‌ها را بخوانید و برای ادامه تأیید کنید.",
    "legal_summary_label": "خلاصه اطلاعیه محصول",
    "hypersnap_explainer": "داده‌های زنده از هایپراسنپ دریافت می‌شوند. ورود و مجوز انتشار جداگانه تأیید می‌شوند.",
    "farcaster_explainer": "خواندن داده با API فارکستر/نینار؛ ورود و مجوز انتشار از آن جدا هستند.",
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
    "account_methods_explainer": "با حساب فارکستر یا جنی‌لیپ وارد شوید. هر روش جداگانه تأیید می‌شود و شبکه دریافت داده را تغییر نمی‌دهد.",
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
