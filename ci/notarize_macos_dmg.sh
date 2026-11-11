#!/usr/bin/env bash
# Notarize the exact final disk image and staple the ticket after Apple accepts it.
# Fails closed: success of hdiutil verify alone is never Gatekeeper approval.
set -euo pipefail
dmg="${1:?Pass final DMG}"
[[ -s "$dmg" ]] || { echo "Missing macOS disk image: $dmg" >&2; exit 1; }
: "${MACOS_NOTARY_KEY_P8_BASE64:?Apple notary API key required}"
: "${MACOS_NOTARY_KEY_ID:?Apple notary key ID required}"
: "${MACOS_NOTARY_ISSUER_ID:?Apple notary issuer ID required}"

umask 077
tmp="$(mktemp -d)"
trap 'rm -rf -- "$tmp"' EXIT
key="$tmp/AuthKey_${MACOS_NOTARY_KEY_ID}.p8"
printf '%s' "$MACOS_NOTARY_KEY_P8_BASE64" | base64 -D > "$key"
test -s "$key"
response="$(xcrun notarytool submit "$dmg" \
  --key "$key" --key-id "$MACOS_NOTARY_KEY_ID" \
  --issuer "$MACOS_NOTARY_ISSUER_ID" \
  --wait --timeout 45m --output-format json)"
printf '%s' "$response" | python3 -c '
import json, sys
result = json.load(sys.stdin)
print("Apple notarization:", result.get("status"), result.get("id", ""))
if result.get("status") != "Accepted":
    raise SystemExit("Notarization rejected or incomplete; refusing publication")
'
xcrun stapler staple "$dmg"
xcrun stapler validate "$dmg"
hdiutil verify "$dmg"
echo "Apple notarized and stapled disk image: $dmg"
