#!/usr/bin/env bash
set -euo pipefail
app="${1:?Pass Genycaster.app path}"
pin="${MACOS_SELF_SIGNED_CERT_SHA1:?Missing pinned certificate fingerprint}"
[[ "$pin" =~ ^[[:xdigit:]]{40}$ ]] || exit 1
identifier=com.genyleap.genycaster
test "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$app/Contents/Info.plist")" = "$identifier"
requirement="identifier \"$identifier\" and certificate leaf = H\"$pin\""
codesign --verify --deep --strict --verbose=4 "$app"
codesign --verify --strict --verbose=4 -R "=$requirement" "$app"
# Require the explicit DR (not a synthesized cdhash or an anchor-trust rule).
actual="$(codesign -d -r- "$app")"
python3 - "$pin" "$actual" <<'PY'
import re
import sys
pin, actual = sys.argv[1:]
pattern = r'designated => identifier "?com\.genyleap\.genycaster"? and certificate leaf = H"([0-9a-fA-F]{40})"'
match = re.fullmatch(pattern, actual.strip())
if not match or match[1].lower() != pin.lower():
    raise SystemExit("Unexpected embedded designated requirement")
PY
printf '%s\n' "$actual"
