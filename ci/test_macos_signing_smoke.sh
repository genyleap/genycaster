#!/usr/bin/env bash
# No Qt, SDK download, package installation, compilation or published artifact.
set -euo pipefail
test "$(bash ci/select_macos_signing_mode.sh)" = self-signed
scratch="$(mktemp -d)"
mount="$scratch/mounted"
cleanup() {
  hdiutil detach "$mount" -quiet >/dev/null 2>&1 || true
  rm -rf "$scratch"
}
trap cleanup EXIT
make_fixture() {
  local app="$1" binary="$2" version="$3"
  mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" "$app/Contents/Helpers"
  cp "$binary" "$app/Contents/MacOS/Genycaster"
  cp /usr/bin/true "$app/Contents/Helpers/helper"
  printf '%s\n' "$version" > "$app/Contents/Resources/version.txt"
  cat > "$app/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>com.genyleap.genycaster</string>
<key>CFBundleExecutable</key><string>Genycaster</string>
<key>CFBundleName</key><string>Genycaster signing fixture</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>$version</string>
</dict></plist>
PLIST
}
a="$scratch/a/Genycaster.app"
b="$scratch/b/Genycaster.app"
make_fixture "$a" /usr/bin/true 1
make_fixture "$b" /usr/bin/false 2
bash ci/sign_macos_bundle.sh "$a" self-signed
bash ci/sign_macos_bundle.sh "$b" self-signed
dr_a="$(codesign -d -r- "$a" 2>/dev/null)"
dr_b="$(codesign -d -r- "$b" 2>/dev/null)"
test -n "$dr_a"
test "$dr_a" = "$dr_b"
hash_a="$(codesign -dv --verbose=4 "$a" 2>&1 | sed -n 's/^CDHash=//p')"
hash_b="$(codesign -dv --verbose=4 "$b" 2>&1 | sed -n 's/^CDHash=//p')"
test -n "$hash_a" && test -n "$hash_b" && test "$hash_a" != "$hash_b"
echo "PASS: different binary hashes, identical certificate-backed designated requirements"
# Cross-evaluate the first build's actual DR against the second build.
codesign --verify --strict -R "=${dr_a#designated => }" "$b"
# Negative control: an ad-hoc app with the same identifier must not match.
bad="$scratch/negative/Genycaster.app"
make_fixture "$bad" /usr/bin/true 3
bash ci/sign_macos_bundle.sh "$bad" ad-hoc
if codesign --verify --strict -R "=${dr_a#designated => }" "$bad"; then
  echo "Unexpectedly accepted ad-hoc signer" >&2
  exit 1
fi
echo "PASS: ad-hoc replacement rejected by pinned requirement"
hdiutil create -quiet -ov -volname GenycasterSigningFixture -srcfolder "$b" -format UDZO "$scratch/fixture.dmg"
hdiutil verify "$scratch/fixture.dmg"
mkdir "$mount"
hdiutil attach -readonly -nobrowse -mountpoint "$mount" "$scratch/fixture.dmg"
bash ci/verify_macos_self_signed.sh "$mount/Genycaster.app"
hdiutil detach "$mount"
echo "PASS: mounted DMG signature and persistent identity"
