#!/usr/bin/env bash
# Replace v1.0.0 Android APK (and its SHA256SUMS line) in place. Never create
# a new release or rebuild other platforms. Runs with github-actions[bot].
set -euo pipefail
REPO=genyleap/genycaster
TAG=v1.0.0
APK=GenyCaster-v1.0.0-android-arm64-v8a.apk
TEST_APK=GenyCaster-v1.0.0-android-arm64-v8a-CI-TEST.apk
CI_RUN=44924051617
CI_SHA=a6b6c4578a82e32ecebf725d658edc5afe63d73a

[[ "$GITHUB_ACTIONS" == true && "$GITHUB_REPOSITORY" == "$REPO" &&
   "$GITHUB_REF" == refs/heads/main && -n "$GH_TOKEN" ]] ||
  { echo "::error::GitHub Actions on main only"; exit 1; }
gh api "repos/$REPO/actions/runs/$CI_RUN" \
  --jq '{id,conclusion,event,head_branch,head_sha}' > "$RUNNER_TEMP/source-run.json"
jq -e --arg sha "$CI_SHA" --argjson run "$CI_RUN" '
.id == $run and .conclusion == "success" and
.event == "workflow_dispatch" and .head_branch == "main" and .head_sha == $sha
' "$RUNNER_TEMP/source-run.json" >/dev/null
gh api "repos/$REPO/actions/runs/$CI_RUN/jobs?per_page=100" --jq \
  '[.jobs[] | select(.name | startswith("Android APK")) | .conclusion]' \
  > "$RUNNER_TEMP/android-jobs.json"
jq -e 'length == 1 and .[0] == "success"' "$RUNNER_TEMP/android-jobs.json" >/dev/null
gh api "repos/$REPO/releases/latest" > "$RUNNER_TEMP/previous-release.json"
jq -e --arg tag "$TAG" --arg apk "$APK" '
.tag_name == $tag and .prerelease == false and .draft == false and
.author.login == "github-actions[bot]" and
(.assets | length == 16) and
([.assets[] | select(.name == $apk)] | length == 1) and
([.assets[] | select(.name == "SHA256SUMS")] | length == 1)
' "$RUNNER_TEMP/previous-release.json" >/dev/null
jq '[.assets[]|{name,digest,size}] | sort_by(.name)' \
  "$RUNNER_TEMP/previous-release.json" > "$RUNNER_TEMP/previous-assets.json"

mkdir -p ci-apk previous dist
gh run download "$CI_RUN" -R "$REPO" \
  -n GenyCaster-android-arm64-v8a -D ci-apk
gh release download "$TAG" -R "$REPO" \
  -p "$APK" -p SHA256SUMS -D previous
test -s "ci-apk/$TEST_APK" && test -s ci-apk/SHA256SUMS
test -s "previous/$APK" && test -s previous/SHA256SUMS
(cd ci-apk && sha256sum --check --status SHA256SUMS)
(cd previous && grep -F "  $APK" SHA256SUMS | sha256sum --check --status)
old_sha="$(sha256sum "previous/$APK" | cut -d' ' -f1)"
jq -e --arg name "$APK" --arg sha "sha256:$old_sha" '
[.assets[] | select(.name == $name and .digest == $sha)] | length == 1
' "$RUNNER_TEMP/previous-release.json" >/dev/null

tools="$ANDROID_HOME/build-tools/35.0.0"
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
# existing release uses 1.0 in Android metadata while the GitHub tag
# and artifact name are v1.0.0.
assert old['versionName'] == new['versionName'] and old['versionName'] in ('1.0', '1.0.0'), (old,new)
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
rm -f "$RUNNER_TEMP/owner.p12"
python3 ci/prepare_android_release_replacement.py checksums \
  previous/SHA256SUMS "dist/$APK" dist/SHA256SUMS
(cd dist && grep -F "  $APK" SHA256SUMS | sha256sum --check --status)

# Release tag, title, pre-release state and all non-Android assets preserved.
test "$(gh api "repos/$REPO/releases/latest" --jq .tag_name)" = "$TAG"
gh release upload "$TAG" "dist/$APK" -R "$REPO" --clobber
gh release upload "$TAG" dist/SHA256SUMS -R "$REPO" --clobber
gh api "repos/$REPO/releases/latest" > "$RUNNER_TEMP/after-release.json"
jq -e --arg tag "$TAG" '
.tag_name == $tag and .prerelease == false and .draft == false and
.author.login == "github-actions[bot]" and (.assets | length == 16)
' "$RUNNER_TEMP/after-release.json" >/dev/null
jq '[.assets[]|{name,digest,size}] | sort_by(.name)' \
  "$RUNNER_TEMP/after-release.json" > "$RUNNER_TEMP/after-assets.json"
python3 - <<'PY'
import hashlib,json,os
from pathlib import Path
base=Path(os.environ['RUNNER_TEMP'])
before={x['name']:x for x in json.loads((base/'previous-assets.json').read_text())}
after={x['name']:x for x in json.loads((base/'after-assets.json').read_text())}
apk='GenyCaster-v1.0.0-android-arm64-v8a.apk'
assert before.keys()==after.keys() and len(before)==16, 'Release assets changed'
for name in before:
    if name not in (apk,'SHA256SUMS'):
        assert before[name]==after[name], 'Unrelated asset modified: '+name
def hash_file(path):
    h=hashlib.sha256()
    with open(path,'rb') as file:
        for block in iter(lambda: file.read(1048576),b''): h.update(block)
    return h.hexdigest()
assert after[apk]['digest']=='sha256:'+hash_file(Path('dist')/apk)
assert after[apk]['size']==(Path('dist')/apk).stat().st_size
assert after['SHA256SUMS']['digest']=='sha256:'+hash_file('dist/SHA256SUMS')
print('SUCCESS: Existing v1.0.0 Latest Release APK replaced with owner-signed build.')
print('SUCCESS: All 14 unrelated public release assets unchanged.')
PY
