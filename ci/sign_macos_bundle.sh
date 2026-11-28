#!/usr/bin/env bash
# Sign the *finished* macOS app after macdeployqt and bundle pruning.
# An ad-hoc signature seals experimental CI artifacts but does not make a
# Safari-downloaded DMG Gatekeeper-approved. New public releases require a
# persistent self-signed or Developer ID identity; neither ad-hoc nor
# self-signed mode implies Apple notarization or Gatekeeper approval.
set -euo pipefail

app="${1:?Pass Genycaster.app path}"
mode="${2:?Pass ad-hoc, self-signed or developer-id}"
[[ -d "$app/Contents" ]] || { echo "Missing app bundle: $app" >&2; exit 1; }
case "$mode" in ad-hoc|self-signed|developer-id) ;; *) echo "Unknown signing mode: $mode" >&2; exit 1 ;; esac

umask 077
scratch="$(mktemp -d)"
keychain=""
trusted_cert=""
remove_runner_trust() {
  sudo -n /usr/bin/security remove-trusted-cert -d "$trusted_cert" &&
    sudo -n /usr/bin/security delete-certificate -Z "$identity" /Library/Keychains/System.keychain
}
cleanup() {
  if [[ -n "$trusted_cert" ]]; then
    remove_runner_trust >/dev/null 2>&1 || true
  fi
  if [[ -n "$keychain" ]]; then
    security delete-keychain "$keychain" >/dev/null 2>&1 || true
  fi
  rm -rf -- "$scratch"
}
trap cleanup EXIT

identity="-"
sign_args=(--force --sign -)
if [[ "$mode" != ad-hoc ]]; then
  if [[ "$mode" == self-signed ]]; then
    : "${MACOS_SELF_SIGNED_P12_BASE64:?Self-signed certificate required}"
    : "${MACOS_SELF_SIGNED_P12_PASSWORD:?Self-signed certificate password required}"
    : "${MACOS_SELF_SIGNED_CERT_SHA1:?Pinned certificate fingerprint required}"
    [[ "$MACOS_SELF_SIGNED_CERT_SHA1" =~ ^[[:xdigit:]]{40}$ ]] || exit 1
    p12="$MACOS_SELF_SIGNED_P12_BASE64"
    p12_password="$MACOS_SELF_SIGNED_P12_PASSWORD"
  else
    p12="${MACOS_DEVELOPER_ID_P12_BASE64:?Developer ID certificate required}"
    p12_password="${MACOS_DEVELOPER_ID_P12_PASSWORD:?Developer ID certificate password required}"
  fi
  keychain="$scratch/genycaster-signing.keychain-db"
  keychain_password="$(openssl rand -hex 24)"
  printf '%s' "$p12" | base64 -D > "$scratch/identity.p12"
  unset p12
  test -s "$scratch/identity.p12"
  if [[ "$mode" == self-signed ]]; then
    # Distinguish an incorrect secret from Apple's legacy PKCS12 importer.
    # OpenSSL 3 supports the modern AES container; rewrap only for import.
    openssl_bin="$(brew --prefix openssl@3)/bin/openssl"
    test -x "$openssl_bin"
    export GENYCASTER_P12_IMPORT_PASSWORD="$p12_password"
    if ! "$openssl_bin" pkcs12 -in "$scratch/identity.p12" -noout -passin env:GENYCASTER_P12_IMPORT_PASSWORD; then
      echo "PKCS12 password or container integrity validation failed before Apple import" >&2
      exit 1
    fi
    echo "PKCS12 password and container MAC verified by OpenSSL"
    # Never write an unencrypted key: the intermediate PEM is encrypted
    # using a unique per-run password and removed by the scratch trap.
    export GENYCASTER_PEM_PASSWORD="$(openssl rand -hex 32)"
    "$openssl_bin" pkcs12 -in "$scratch/identity.p12" -out "$scratch/identity.pem" \
      -passin env:GENYCASTER_P12_IMPORT_PASSWORD -passout env:GENYCASTER_PEM_PASSWORD
    "$openssl_bin" pkcs12 -export -in "$scratch/identity.pem" -out "$scratch/import.p12" \
      -passin env:GENYCASTER_PEM_PASSWORD -passout env:GENYCASTER_P12_IMPORT_PASSWORD \
      -keypbe PBE-SHA1-3DES -certpbe PBE-SHA1-3DES -macalg sha1
    mv "$scratch/import.p12" "$scratch/identity.p12"
    rm -f "$scratch/identity.pem"
    unset GENYCASTER_PEM_PASSWORD GENYCASTER_P12_IMPORT_PASSWORD
  fi
  security create-keychain -p "$keychain_password" "$keychain"
  security set-keychain-settings -lut 21600 "$keychain"
  security unlock-keychain -p "$keychain_password" "$keychain"
  security import "$scratch/identity.p12" -k "$keychain" \
    -P "$p12_password" -T /usr/bin/codesign
  unset p12_password
  security set-key-partition-list -S apple-tool:,apple: -s \
    -k "$keychain_password" "$keychain"
  if [[ "$mode" == self-signed ]]; then
    identity="$(printf '%s' "$MACOS_SELF_SIGNED_CERT_SHA1" | tr '[:lower:]' '[:upper:]')"
    # Export only the public certificate from the imported P12. Verify the
    # pinned identity before changing trust, and only trust it for code signing.
    security find-certificate -a -p "$keychain" > "$scratch/cert.pem"
    [[ "$(grep -c 'BEGIN CERTIFICATE' "$scratch/cert.pem")" == 1 ]]
    openssl x509 -in "$scratch/cert.pem" -outform DER -out "$scratch/cert.der"
    actual="$(shasum -a 1 "$scratch/cert.der" | awk '{print toupper($1)}')"
    [[ "$actual" == "$identity" ]] || { echo "Self-signed certificate pin mismatch" >&2; exit 1; }
    openssl verify -CAfile "$scratch/cert.pem" "$scratch/cert.pem"
    # This trust setting is for the ephemeral GitHub-hosted signing runner
    # only. Never install a root or change trust on an end-user machine.
    [[ "${GITHUB_ACTIONS:-}" == true && "${RUNNER_ENVIRONMENT:-}" == github-hosted ]] || {
      echo "Self-signed P12 import is restricted to GitHub-hosted runners" >&2; exit 1;
    }
    # User-domain trust changes prompt for GUI authorization on macOS 26.
    # Use the admin trust domain on the disposable hosted runner only.
    # Refuse a pre-existing system certificate so cleanup cannot remove it.
    if /usr/bin/security find-certificate -a -Z /Library/Keychains/System.keychain | grep -Fq "$identity"; then
      echo "Pinned certificate already present in system keychain; refusing to modify existing trust" >&2
      exit 1
    fi
    trusted_cert="$scratch/cert.der"
    perl -e 'alarm 30; exec @ARGV' sudo -n /usr/bin/security add-trusted-cert -d -r trustRoot \
      -p codeSign -k /Library/Keychains/System.keychain "$trusted_cert"
    security find-identity -v -p codesigning "$keychain" | grep -F "$identity"
    sign_args=(--force --sign "$identity" --timestamp=none --keychain "$keychain")
  else
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
fi

# First sign standalone Mach-O payloads. Qt frameworks and embedded bundles
# are signed as *bundles* below, not by individually changing their resources.
while IFS= read -r -d '' binary; do
  case "${binary#"$app/Contents/"}" in
    MacOS/Genycaster) continue ;;
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
elif [[ "$mode" == self-signed ]]; then
  requirement="identifier \"com.genyleap.genycaster\" and certificate leaf = H\"$identity\""
  codesign "${sign_args[@]}" --identifier com.genyleap.genycaster \
    -r "=designated => $requirement" "$app"
  # Remove the runner trust before verification, to model an ordinary Mac.
  remove_runner_trust
  trusted_cert=""
  bash ci/verify_macos_self_signed.sh "$app"
else
  codesign "${sign_args[@]}" "$app"
fi
codesign --verify --deep --strict --verbose=4 "$app"
codesign --display --verbose=2 "$app" 2>&1 | grep -E 'Authority=|Signature=|TeamIdentifier=' || true
echo "macOS bundle signed and verified ($mode); Gatekeeper approval requires notarization."
