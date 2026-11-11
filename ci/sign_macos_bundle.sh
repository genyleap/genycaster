#!/usr/bin/env bash
# Sign the *finished* macOS app after macdeployqt and bundle pruning.
# An ad-hoc signature seals experimental CI artifacts but does not make a
# Safari-downloaded DMG Gatekeeper-approved. Public releases use Developer ID
# Application and Apple notarization, never an ad-hoc fallback.
set -euo pipefail

app="${1:?Pass Genycaster.app path}"
mode="${2:?Pass ad-hoc or developer-id}"
[[ -d "$app/Contents" ]] || { echo "Missing app bundle: $app" >&2; exit 1; }
case "$mode" in ad-hoc|developer-id) ;; *) echo "Unknown signing mode: $mode" >&2; exit 1 ;; esac

umask 077
scratch="$(mktemp -d)"
keychain=""
cleanup() {
  if [[ -n "$keychain" ]]; then
    security delete-keychain "$keychain" >/dev/null 2>&1 || true
  fi
  rm -rf -- "$scratch"
}
trap cleanup EXIT

identity="-"
sign_args=(--force --sign -)
if [[ "$mode" == developer-id ]]; then
  : "${MACOS_DEVELOPER_ID_P12_BASE64:?Developer ID certificate required}"
  : "${MACOS_DEVELOPER_ID_P12_PASSWORD:?Developer ID certificate password required}"
  keychain="$scratch/genycaster-signing.keychain-db"
  keychain_password="$(openssl rand -hex 24)"
  printf '%s' "$MACOS_DEVELOPER_ID_P12_BASE64" | base64 -D > "$scratch/identity.p12"
  test -s "$scratch/identity.p12"
  security create-keychain -p "$keychain_password" "$keychain"
  security set-keychain-settings -lut 21600 "$keychain"
  security unlock-keychain -p "$keychain_password" "$keychain"
  security import "$scratch/identity.p12" -k "$keychain" \
    -P "$MACOS_DEVELOPER_ID_P12_PASSWORD" -T /usr/bin/codesign
  security set-key-partition-list -S apple-tool:,apple: -s \
    -k "$keychain_password" "$keychain"
  # Restrict the operation to the temporary keychain. There must be exactly
  # one suitable Developer ID Application signing identity in this bundle.
  matches="$(security find-identity -v -p codesigning "$keychain" |
    awk '/Developer ID Application:/ {print $2}')"
  [[ "$(printf '%s\n' "$matches" | grep -cE '^[A-Fa-f0-9]{40}$')" == 1 ]] || {
    echo "Expected exactly one Developer ID Application identity in P12" >&2
    exit 1
  }
  identity="$matches"
  sign_args=(--force --sign "$identity" --options runtime --timestamp --keychain "$keychain")
fi

# First sign standalone Mach-O payloads. Qt frameworks and embedded bundles
# are signed as *bundles* below, not by individually changing their resources.
while IFS= read -r -d '' binary; do
  case "$binary" in
    "$app/Contents/MacOS/Genycaster") continue ;;
    *.framework/*|*.app/*|*.bundle/*|*.appex/*|*.xpc/*|*.plugin/*) continue ;;
  esac
  if file -b "$binary" | grep -q 'Mach-O'; then
    codesign "${sign_args[@]}" "$binary"
  fi
done < <(find "$app/Contents" -type f -print0)

# find -depth visits children before parents. The app's outer signature is
# generated last, so subsequent Qt deployment cannot invalidate its seal.
while IFS= read -r -d '' bundle; do
  codesign "${sign_args[@]}" "$bundle"
done < <(find "$app/Contents" -depth -type d \
  \( -name '*.framework' -o -name '*.app' -o -name '*.bundle' \
     -o -name '*.appex' -o -name '*.xpc' -o -name '*.plugin' \) -print0)

if [[ "$mode" == developer-id ]]; then
  codesign "${sign_args[@]}" \
    --entitlements properties/macOS/Genycaster.entitlements.plist "$app"
else
  codesign "${sign_args[@]}" "$app"
fi
codesign --verify --deep --strict --verbose=4 "$app"
codesign --display --verbose=2 "$app" 2>&1 | grep -E 'Authority=|Signature=|TeamIdentifier=' || true
echo "macOS bundle signed and verified ($mode); Gatekeeper approval requires notarization."
