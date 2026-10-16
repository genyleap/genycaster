#!/usr/bin/env python3
"""Register initial-setup texts with Qt's ID-based localization catalog."""
from pathlib import Path
import xml.etree.ElementTree as ET

base = Path(__file__).resolve().parents[1]
messages = {
    "choose_network": "Connection network",
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
}
fa = {
    "choose_network": "شبکه اتصال",
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
}
for path in sorted((base / "ui/i18n").glob("qml_*.ts")):
    tree = ET.parse(path)
    root = tree.getroot()
    context = root.find("context")
    if context is None:
        context = ET.SubElement(root, "context")
        ET.SubElement(context, "name").text = ""
    existing = {m.attrib.get("id") for m in root.iter("message")}
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
)
anchor = '    (void)qtTrId("genycaster.shell.sign_in_with_farcaster");\n'
if "genycaster.setup.choose_network" not in original:
    if anchor not in original:
        raise SystemExit("Cannot locate catalog anchor")
    catalog.write_text(original.replace(anchor, anchor + lines, 1))
print("Setup localization IDs:", len(messages))
