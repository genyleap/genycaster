# macOS distribution: persistent self-signed identity or Apple Developer ID

CI builds on `macos-26` for Apple Silicon and `macos-26-intel` for x86_64.
A successful `macdeployqt` run, QML startup smoke test, and `hdiutil verify`
do **not** prove that Safari-downloaded software will pass Gatekeeper.

## Three explicitly labeled signing modes

- **Persistent self-signed (free, public unnotarized preview):** The same
  owner-controlled certificate signs both architectures and every update.
  The app's explicit designated requirement is its stable bundle identifier
  and the pinned leaf certificate hash. CI verifies this requirement on the
  final bundle and the mounted DMG. Files are named
  `GenyCaster-vVERSION-macos-ARCH-selfsigned-UNNOTARIZED.dmg`.
  This establishes continuity, not Apple-vetted publisher trust. Apple
  advises against shipping self-signed apps; Gatekeeper can still block them.
  No root certificate installation is required or recommended for users.
- **Free ad-hoc mode (diagnostic builds; historical releases only):** The completed, pruned
  `Genycaster.app` is signed inside-out with `codesign --sign -`, and both
  the original and DMG-mounted bundles pass `codesign --verify --deep --strict`
  and the Qt startup smoke tests. The public release DMG is explicitly named
  `GenyCaster-vVERSION-macos-ARCH-adhoc-UNNOTARIZED.dmg`. This validates
  bundle integrity but **does not** provide Apple Developer ID trust or
  notarization. macOS Gatekeeper may block the quarantined download. Do not
  describe it as Apple-approved.
- **Developer ID + notarization (optional future upgrade):** Only when
  **all five** Apple credentials are present, CI signs with a Developer ID
  Application identity and Hardened Runtime, submits the exact DMG to Apple,
  checks `Accepted`, staples the ticket, and checks `spctl --assess` on
  the mounted app. Partial Apple credentials remain a hard error.

All modes require the same architecture validation, Qt deployment audits,
DMG integrity verification and security gate. New public releases fail
before toolchain installation if neither persistent signing identity is
configured; incomplete credentials also fail closed. No paid Apple account
is required for the self-signed mode.

## Provision the free persistent release identity once

On the maintainer's trusted Mac, run from the checkout:

```bash
bash ci/create_macos_self_signed_identity.sh "$HOME/Genycaster-release-identity"
```

Choose strong passphrases when prompted. The script refuses to overwrite
an existing directory or put the signing identity in the repository. It
creates a 3072-bit RSA self-signed code-signing certificate, valid for ten
years, and a password-protected P12. It never installs trust or uploads
anything. Alternatively, use Keychain Access Certificate Assistant with
Self Signed Root / Code Signing and export the identity as encrypted P12.
Keep an offline encrypted backup of the original P12 and store the
password separately. Losing or replacing this certificate breaks the
designated requirement; compromise cannot be revoked by Apple.

In GitHub repository Settings → Secrets and variables → Actions:

| Type | Name | Value |
| --- | --- | --- |
| Secret | `MACOS_SELF_SIGNED_P12_BASE64` | Base64 of encrypted P12 |
| Secret | `MACOS_SELF_SIGNED_P12_PASSWORD` | P12 export password |
| Variable | `MACOS_SELF_SIGNED_CERT_SHA1` | Public fingerprint in certificate-sha1.txt |

With authenticated GitHub CLI, avoid printing secrets or including passwords
in arguments or shell history:

```bash
IDENTITY_DIR="$HOME/Genycaster-release-identity"
base64 < "$IDENTITY_DIR/Genycaster-release.p12" | tr -d '\n' |
  gh secret set MACOS_SELF_SIGNED_P12_BASE64 -R genyleap/genycaster
gh secret set MACOS_SELF_SIGNED_P12_PASSWORD -R genyleap/genycaster
gh variable set MACOS_SELF_SIGNED_CERT_SHA1 -R genyleap/genycaster \
  --body "$(cat "$IDENTITY_DIR/certificate-sha1.txt")"
```

The password command prompts interactively. Do not send the private key,
P12, or password to chat. CI validates the encrypted P12 with OpenSSL 3, then rewraps it temporarily
with macOS-compatible PKCS12 encryption (3DES/SHA-1 MAC) without changing
the certificate or exposing an unencrypted key. The original encrypted
backup remains unchanged. CI uses a temporary signing keychain on a
GitHub-hosted runner, adds it to the runner's user keychain search list,
signs with the exact pinned certificate, restores the original search list
and deletes the temporary keychain before verification. No user or admin
certificate trust settings are modified. The self-signed certificate is
intentionally not Apple-trusted; signature verification checks the explicit
certificate-backed designated requirement, not Gatekeeper approval.
It never changes users' Keychain ACLs or Gatekeeper settings. All three
parameters must exist together. Developer ID takes precedence only if all
its credentials exist.

For a fast CI signing-only diagnostic without compiling Qt or any app code,
run the manual `macos-signing-smoke.yml` workflow on main, selecting
`arm64` or `x86_64`. It signs two different existing macOS executables
packaged as temporary app fixtures, checks that their designated requirements
are identical, rejects an ad-hoc replacement, and verifies a mounted DMG.
No release or downloadable artifact is created. This is a signing test,
not a test of Genycaster's application functionality or Keychain prompts.

### Acceptance before a public release

First exercise a disposable self-signed certificate on a real Mac (not the
release private key). Use the same certificate to sign two different builds
with the following requirement. Sign nested code before the outer app for
deployed bundles; the command below is for local Qt Creator bundles only.

```bash
APP="/path/to/Genycaster.app"
PIN="<SHA-1 of the test certificate>"
codesign --force --sign "$PIN" --timestamp=none \
  --identifier com.genyleap.genycaster \
  -r "=designated => identifier \"com.genyleap.genycaster\" and certificate leaf = H\"$PIN\"" "$APP"
MACOS_SELF_SIGNED_CERT_SHA1="$PIN" bash ci/verify_macos_self_signed.sh "$APP"
```

The signing Mac must have the test identity visible in the user keychain
search list. Do not install certificate trust on the clean recipient Mac. Install build A, authorize
existing Keychain items if needed, quit/reopen twice, replace with build B,
then quit/reopen again. Exercise account restore, cloud proof and social
signing with an isolated test account. Verify an unrelated signer fails the
pinned requirement. A fresh macOS user must also test Gatekeeper separately.
A single targeted macOS CI diagnostic using the provisioned release identity
must succeed on each architecture before publishing. The real macOS trust
behavior is not covered by Linux static tests.

## Optional Apple Developer ID configuration

GitHub repository **Settings → Secrets and variables → Actions** needs the
following five secrets **only for the optional Gatekeeper-ready mode**:

| Secret | Source |
| --- | --- |
| `MACOS_DEVELOPER_ID_P12_BASE64` | Base64-encoded, password-protected Developer ID **Application** certificate exported as P12 |
| `MACOS_DEVELOPER_ID_P12_PASSWORD` | Password for that P12 |
| `MACOS_NOTARY_KEY_P8_BASE64` | Base64-encoded Apple App Store Connect notary API-key P8 |
| `MACOS_NOTARY_KEY_ID` | Key ID for the notarization API key |
| `MACOS_NOTARY_ISSUER_ID` | API issuer ID associated with the key |

Generate the base64 contents locally from the source certificate/key and paste
them **directly into GitHub Actions Secrets**, never into a repository, chat,
issue, log, or application binary. The CI imports the certificate into a
temporary keychain and removes it after signing. Partial credentials are an
error, not a silent fall back.

## End-user verification on a fresh download

On the user's Mac, inspect the copied application:

```bash
codesign --verify --deep --strict --verbose=4 /Applications/Genycaster.app
spctl --assess --type execute --verbose=4 /Applications/Genycaster.app
xattr -p com.apple.quarantine /Applications/Genycaster.app 2>/dev/null || true
```

Quarantine metadata is normally **added by Safari/Finder after downloading**;
clearing the builder's local xattrs cannot prevent that. Do not remove the
user's quarantine metadata, disable Gatekeeper, or pretend ad-hoc signing
provides Developer ID trust.

The free signing model follows the verified
[GenyConnect v1.4.950 workflow](https://github.com/genyleap/GenyConnect/blob/v1.4.950/.github/workflows/ci.yml).
GenyCaster adds inside-out signing, mounted-DMG audit and explicit
`UNNOTARIZED` filenames, while preserving Developer ID notarization as a
future optional upgrade. A release note and the native download catalog must
disclose ad-hoc or self-signed signing to users. Disabling Gatekeeper or stripping quarantine
is not an acceptable distribution workaround.

References:
- https://developer.apple.com/developer-id/
- https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution

## Keychain trust and repeated prompts during development

The device session seal, registered social signer, and protocol-scoped cloud
device credentials remain separate Keychain items. A signer handle derives
its public identity during its authorized read; public-key inspection no
longer reads the secret again. Apple device/cloud proof ceremonies use an
opaque single-signature handle, with a 20-second validity limit, consumed
on use or revoked by vault erasure. Cloud continuations are owned by the
specific challenge, released on cancellation, and recheck the active
principal and retirement marker before producing the signature. Ordinary
social publishing re-reads the OS vault and checks the public identity.
No plaintext storage or broad Keychain ACL is introduced.

An ad-hoc signature does not provide a stable certificate-backed designated
requirement across rebuilds. Keychain's Always Allow decision is controlled
by macOS, and repeated prompts after a rebuild can still occur. Never add
all applications to an item's Access Control list, unlock the login keychain
programmatically, or delete existing credentials to silence the prompt.

For local Qt Creator builds, inspect the existing app signature and available
local signing identities (no password or private key should be shared):

```sh
codesign -d -r- --verbose=2 /path/to/Genycaster.app
security find-identity -v -p codesigning
```

Local CMake configuration selects and pins a valid Apple Development
certificate fingerprint automatically (outside CI). If multiple valid
certificates share the same identity name, it selects the lexicographically
first fingerprint, independent of Keychain enumeration order. Existing
cached or explicitly configured identities always take precedence. If
different development identity names exist, configuration requires an
explicit `GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY` fingerprint rather than
silently choosing a developer or falling back to ad-hoc. Keep the same
identity and bundle identifier for successive builds; if a pinned certificate
expires, explicitly select a replacement. Disable automatic selection with
`GENYCASTER_MACOS_AUTO_LOCAL_SIGNING=OFF` if desired. If the login
keychain itself is locked or its password is out of sync, that is a separate
OS authorization condition; the application must not bypass it.

Acceptance on a real Mac: launch, authorize the existing entries if needed,
quit explicitly, reopen twice without rebuilding, then rebuild using the
same stable identity and reopen. Check account restore, cloud authorization
and a social signature, and verify that Deny does not silently reauthorize.
The fake-Keychain regression fixture validates access counts and Ed25519
behavior but cannot validate Apple's actual ACL or SecurityAgent dialogs.
