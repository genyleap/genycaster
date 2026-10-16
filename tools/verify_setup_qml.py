#!/usr/bin/env python3
"""Parse the onboarding surface with the Qt toolchain used by the native client."""
from pathlib import Path
import subprocess
import sys

base = Path(__file__).resolve().parents[1]
formatter = Path("/opt/Qt/current/gcc_64/bin/qmlformat")
if not formatter.is_file():
    sys.exit("SKIP: Qt 6 qmlformat not available on this build host")
files = [
    "ui/components/SetupActionCard.qml",
    "ui/components/SetupLanguageSelector.qml",
    "ui/components/InitialSetup.qml",
    "ui/components/GenyDrawer.qml",
    "ui/components/NotificationSurface.qml",
    "ui/components/LocaleSettingsPopup.qml",
    "ui/components/AppShell.qml",
    "ui/components/FeedSurface.qml",
    "ui/components/SystemStatusCard.qml",
]
failed = False
for file in files:
    result = subprocess.run(
        [str(formatter), "--ignore-settings", str(base / file)],
        capture_output=True,
        text=True,
        check=False,
        timeout=25,
    )
    if result.returncode or "Error" in result.stderr:
        failed = True
        print("FAIL:", file, result.stderr[:2500])
    else:
        print("PASS:", file)
sys.exit(1 if failed else 0)
