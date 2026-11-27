#!/usr/bin/env bash
# Replace v1.0.512 Android APK (and its SHA256SUMS line) in place. Never create
# a new release or rebuild other platforms. Runs with github-actions[bot].
set -euo pipefail
REPO=genyleap/genycaster
TAG=v1.0.512
APK=GenyCaster-v1.0.512-android-arm64-v8a.apk
TEST_APK=GenyCaster-v1.0.512-android-arm64-v8a-CI-TEST.apk

[[ "$GITHUB_ACTIONS" == true && "$GITHUB_REPOSITORY" == "$REPO" &&
   "$GITHUB_REF" == refs/heads/main && "$GITHUB_EVENT_NAME" == workflow_dispatch && -n "$GH_TOKEN" ]] ||
  { echo "::error::GitHub Actions on main only"; exit 1; }
gh api "repos/$REPO/releases/tags/$TAG" > "$RUNNER_TEMP/previous-release.json"
jq -e --arg tag "$TAG" --arg apk "$APK" '
.id == 459920728 and .tag_name == $tag and .prerelease == false and .draft == false and
.author.login == "github-actions[bot]" and
(.assets | length == 16) and
([.assets[] | select(.name == $apk)] | length == 1) and
([.assets[] | select(.name == "SHA256SUMS")] | length == 1)
' "$RUNNER_TEMP/previous-release.json" >/dev/null
jq '[.assets[]|{name,digest,size}] | sort_by(.name)' \
  "$RUNNER_TEMP/previous-release.json" > "$RUNNER_TEMP/previous-assets.json"

mkdir -p ci-apk previous dist
gh release download "$TAG" -R "$REPO" \
  -p "$APK" -p SHA256SUMS -D previous
test -s "ci-apk/$TEST_APK" && test -s ci-apk/SHA256SUMS
test -s "previous/$APK" && test -s previous/SHA256SUMS
(cd ci-apk && sha256sum --check --status SHA256SUMS)
(cd previous && grep -F "  $APK" SHA256SUMS | sha256sum --check --status)
old_sha="$(sha256sum "previous/$APK" | cut -d' ' -f1)"
jq -e --arg name "$APK" --arg sha "sha256:$old_sha" '
[.assets[] | select(.name == $name and (.digest == null or .digest == $sha))] | length == 1
' "$RUNNER_TEMP/previous-release.json" >/dev/null

tools="$ANDROID_HOME/build-tools/35.0.0"
if [[ ! -x "$tools/apksigner" || ! -x "$tools/zipalign" || ! -x "$tools/aapt" ]]; then
  timeout 360s "$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" --install "build-tools;35.0.0"
fi
for executable in aapt apksigner zipalign; do test -x "$tools/$executable"; done
"$tools/apksigner" verify --verbose "previous/$APK"
"$tools/apksigner" verify --verbose "ci-apk/$TEST_APK"
"$tools/zipalign" -c -P 16 4 "previous/$APK"
"$tools/zipalign" -c -P 16 4 "ci-apk/$TEST_APK"
"$tools/aapt" dump badging "previous/$APK" | grep '^package:' \
  > "$RUNNER_TEMP/original-package.txt"
"$tools/aapt" dump badging "ci-apk/$TEST_APK" | grep '^package:' \
  > "$RUNNER_TEMP/test-package.txt"
python3 - <<'PY'
import os,re
from pathlib import Path
root=Path(os.environ['RUNNER_TEMP'])
def metadata(name):
    text = (root / name).read_text().strip()
    # Anchor field names: otherwise compileSdkVersionCodename='16'
    # overrides package name when converted to dict.
    assert text.startswith('package: '), 'Invalid Android package metadata'
    fields = re.findall(r"(?<![A-Za-z0-9_])(name|versionCode|versionName)='([^']+)'", text)
    result = dict(fields)
    assert len(fields) == 3 and len(result) == 3, 'Ambiguous package fields'
    return result
old=metadata('original-package.txt')
new=metadata('test-package.txt')
assert old['name'] == new['name'] == 'com.genyleap.genycaster', (old,new)
# Preserve exactly the installed application's manifest version. The
# existing release identity must remain unchanged.
assert old['versionName'] == new['versionName'] and old['versionName'] == '1.0.512', (old,new)
assert int(new['versionCode']) >= int(old['versionCode']), (old,new)
print('VALIDATED: same Android package, release version and compatible versionCode')
PY

# The test APK must NEVER be uploaded. Repackage unsigned app payload,
# 16-KiB-align it, then sign with protected owner key used for the old APK.
umask 077
for item in "$ANDROID_KEYSTORE_BASE64" "$ANDROID_KEYSTORE_PASSWORD" \
            "$ANDROID_KEY_ALIAS" "$ANDROID_KEY_PASSWORD"; do
  test -n "$item" || { echo "::error::Missing protected APK signing secret"; exit 1; }
done
printf '%s' "$ANDROID_KEYSTORE_BASE64" | base64 -d > "$RUNNER_TEMP/owner.p12"
test -s "$RUNNER_TEMP/owner.p12"
keytool -list -storetype PKCS12 -keystore "$RUNNER_TEMP/owner.p12" \
  -alias "$ANDROID_KEY_ALIAS" -storepass:env ANDROID_KEYSTORE_PASSWORD >/dev/null
python3 ci/verify_platform_package.py android "ci-apk/$TEST_APK"
python3 ci/audit_android_native_linkage.py "ci-apk/$TEST_APK"
python3 ci/prepare_android_release_replacement.py unpack \
  "ci-apk/$TEST_APK" "$RUNNER_TEMP/unsigned.apk"
"$tools/zipalign" -f -P 16 4 \
  "$RUNNER_TEMP/unsigned.apk" "$RUNNER_TEMP/aligned.apk"
"$tools/apksigner" sign \
  --ks "$RUNNER_TEMP/owner.p12" --ks-type PKCS12 \
  --ks-key-alias "$ANDROID_KEY_ALIAS" \
  --ks-pass env:ANDROID_KEYSTORE_PASSWORD \
  --key-pass env:ANDROID_KEY_PASSWORD \
  --out "dist/$APK" "$RUNNER_TEMP/aligned.apk"
"$tools/apksigner" verify --verbose --print-certs \
  "previous/$APK" > "$RUNNER_TEMP/old-cert.txt"
"$tools/apksigner" verify --verbose --print-certs \
  "dist/$APK" > "$RUNNER_TEMP/new-cert.txt"
old_cert="$(sed -n 's/^Signer #1 certificate SHA-256 digest: //p' "$RUNNER_TEMP/old-cert.txt")"
new_cert="$(sed -n 's/^Signer #1 certificate SHA-256 digest: //p' "$RUNNER_TEMP/new-cert.txt")"
[[ "$old_cert" =~ ^[a-f0-9]{64}$ && "$old_cert" == "$new_cert" ]] ||
  { echo "::error::Official signing certificate mismatch"; exit 1; }
"$tools/zipalign" -c -P 16 4 "dist/$APK"
python3 ci/prepare_android_release_replacement.py verify "ci-apk/$TEST_APK" "dist/$APK"
python3 ci/audit_android_native_linkage.py "dist/$APK"
rm -f "$RUNNER_TEMP/owner.p12"
python3 ci/prepare_android_release_replacement.py checksums \
  previous/SHA256SUMS "dist/$APK" dist/SHA256SUMS
(cd dist && grep -F "  $APK" SHA256SUMS | sha256sum --check --status)

# Release identity, tag and all unrelated assets must remain unchanged.
gh api "repos/$REPO/releases/tags/$TAG" > "$RUNNER_TEMP/current-release.json"
python3 - <<'PY'
import json,os
from pathlib import Path
b=Path(os.environ["RUNNER_TEMP"])
a=json.loads((b/"previous-release.json").read_text())
c=json.loads((b/"current-release.json").read_text())
assert a["id"] == c["id"] == 459920728
def assets(r):
    return sorted((x["id"], x["name"], x["size"], x.get("digest")) for x in r["assets"])
assert assets(a)==assets(c), "Release changed during signing"
(b/"previous-body.md").write_text(a["body"])
PY
rollback() {
  rc=$?
  trap - ERR
  echo "::error::Android replacement failed; restoring original APK, checksums and notes"
  gh release upload "$TAG" "previous/$APK" previous/SHA256SUMS -R "$REPO" --clobber || true
  gh release edit "$TAG" -R "$REPO" --notes-file "$RUNNER_TEMP/previous-body.md" || true
  exit "$rc"
}
trap rollback ERR
gh release upload "$TAG" "dist/$APK" -R "$REPO" --clobber
gh release upload "$TAG" dist/SHA256SUMS -R "$REPO" --clobber
gh api "repos/$REPO/releases/tags/$TAG" > "$RUNNER_TEMP/after-release.json"
mkdir -p verified
gh release download "$TAG" -R "$REPO" -p "$APK" -p SHA256SUMS -D verified
cmp "dist/$APK" "verified/$APK"
cmp dist/SHA256SUMS verified/SHA256SUMS
python3 - <<'PY'
import json,os
from pathlib import Path
b=Path(os.environ["RUNNER_TEMP"])
a=json.loads((b/"previous-release.json").read_text())
c=json.loads((b/"after-release.json").read_text())
assert a["id"] == c["id"] == 459920728
for key in ("tag_name","target_commitish","name","draft","prerelease"):
    assert a[key]==c[key], key
before={x["name"]:x for x in a["assets"]}
after={x["name"]:x for x in c["assets"]}
apk="GenyCaster-v1.0.512-android-arm64-v8a.apk"
assert before.keys()==after.keys() and len(before)==16
for name in before:
    if name not in (apk,"SHA256SUMS"):
        for field in ("id","name","size","digest"):
            assert before[name].get(field)==after[name].get(field), (name,field)
body=a["body"]
marker="<!-- genycaster-android-linkage-repair-512 -->"
body=body.split(marker)[0].rstrip()
body+="\n\n"+marker+"\n\nAndroid APK replacement: [CI "+os.environ["GITHUB_RUN_ID"]+"](https://github.com/genyleap/genycaster/actions/runs/"+os.environ["GITHUB_RUN_ID"]+"), commit `"+os.environ["GITHUB_SHA"]+"`. Qt 6.12.1 / Android NDK r30; verified native symbol closure and original signing certificate. All other release assets unchanged. Device-level startup validation remains pending.\n"
(b/"updated-body.md").write_text(body)
PY
gh release edit "$TAG" -R "$REPO" --notes-file "$RUNNER_TEMP/updated-body.md"
trap - ERR
echo "SUCCESS: Existing v1.0.512 APK replaced with owner-signed verified build; 14 unrelated assets unchanged."
