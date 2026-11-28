#!/usr/bin/env bash
# Output only the selected mode; never echo secret values.
set -euo pipefail
developer=0
self_signed=0
for name in MACOS_DEVELOPER_ID_P12_BASE64 MACOS_DEVELOPER_ID_P12_PASSWORD \
    MACOS_NOTARY_KEY_P8_BASE64 MACOS_NOTARY_KEY_ID MACOS_NOTARY_ISSUER_ID; do
  if [[ -n "${!name:-}" ]]; then developer=$((developer + 1)); fi
done
for name in MACOS_SELF_SIGNED_P12_BASE64 MACOS_SELF_SIGNED_P12_PASSWORD \
    MACOS_SELF_SIGNED_CERT_SHA1; do
  if [[ -n "${!name:-}" ]]; then self_signed=$((self_signed + 1)); fi
done
if [[ "$developer" != 0 && "$developer" != 5 ]]; then
  echo "Incomplete macOS Developer ID/notarization configuration" >&2; exit 1
fi
if [[ "$self_signed" != 0 && "$self_signed" != 3 ]]; then
  echo "Incomplete macOS self-signed configuration" >&2; exit 1
fi
if [[ "$self_signed" == 3 && ! "$MACOS_SELF_SIGNED_CERT_SHA1" =~ ^[[:xdigit:]]{40}$ ]]; then
  echo "Invalid pinned macOS certificate fingerprint" >&2; exit 1
fi
if [[ "$developer" == 5 ]]; then
  echo developer-id
elif [[ "$self_signed" == 3 ]]; then
  echo self-signed
elif [[ "${REQUIRE_MACOS_RELEASE_SIGNING:-false}" == true ]]; then
  echo "Public macOS release requires a persistent self-signed or Developer ID identity" >&2; exit 1
else
  echo ad-hoc
fi
