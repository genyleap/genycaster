#!/usr/bin/env python3
"""Release gate: new first-party origins must never be scattered in runtime code.

The only reviewed exceptions are source-tree development fixtures and one-time
legacy migration identifiers (which must stay readable after an issuer move).
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
FIRST_PARTY_URL = re.compile(
    r'(?:https://)?(?:openproof|caster|storage-farcaster|media-farcaster|snapchain)'
    r'\.genyleap\.com\b|https://haatz\.quilibrium\.com\b'
)
SCOPES = ["ui/app", "ui/components", "ui/wasm", "src/application",
          "src/platform", "src/providers", "src/presentation", "src/transport",
          "services/account-sync", "services/media-gateway"]
REVIEWED_EXCEPTIONS = {
    "ui/wasm/deployment_config.mjs",     # source-tree Node test fallback only
}
errors = []
for directory in SCOPES:
    for path in (ROOT / directory).rglob("*"):
        if not path.is_file() or path.suffix not in (".cpp", ".cppm", ".hpp", ".qml", ".mjs", ".js"):
            continue
        rel = path.relative_to(ROOT).as_posix()
        if rel in REVIEWED_EXCEPTIONS or any(
            section in ("test", "tests", "build", "node_modules", "vendor")
            for section in path.relative_to(ROOT).parts
        ):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if FIRST_PARTY_URL.search(line):
                historical = {
                    "ui/components/profile/ProfileSurface.qml":
                        ('"https://openproof.genyleap.com:" + fid',),
                    "ui/app/miniapp/catalog.cpp":
                        ('parsed.host() == QStringLiteral("storage-farcaster.genyleap.com")',),
                    "src/presentation/media/classification.cpp":
                        ('host == "media-farcaster.genyleap.com"',),
                    "services/media-gateway/browser_image_proxy.mjs":
                        ("'media-farcaster.genyleap.com'",),
                }
                if any(fragment in line for fragment in historical.get(rel, ())):
                    # Existing published media/profile identifiers remain
                    # readable after migration; never use these for new URLs.
                    continue
                errors.append(f"{rel}:{number}: literal deployment URL")
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("Deployment boundary audit: PASS")
