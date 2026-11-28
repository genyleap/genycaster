# macOS distribution: free ad-hoc vs. Apple Developer ID

CI builds on `macos-26` for Apple Silicon and `macos-26-intel` for x86_64.
A successful `macdeployqt` run, QML startup smoke test, and `hdiutil verify`
do **not** prove that Safari-downloaded software will pass Gatekeeper.

## Two explicitly labeled distribution modes

- **Free ad-hoc mode (GenyConnect model):** The completed, pruned
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

Both variants require the same architecture validation, Qt deployment audits,
DMG integrity verification and security gate. There is no paid Apple account
requirement for the public ad-hoc mode.

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

## Local Qt Creator / Ninja builds and repeated Keychain prompts

Apple's file-based login Keychain recognizes a trusted application by its
code-signing requirement, not merely its bundle identifier or executable
path. An ad-hoc requirement is tied to the particular build's code hash.
A new ad-hoc build may therefore request access again even after Always
Allow. Repeated requests from an unchanged binary need a separate ACL/
keychain-lock diagnosis.

Genycaster intentionally keeps three authorities independent: the local
identity seal, social signer, and the exact protocol/account cloud-device
key. Do not merge these entries, delete existing seeds, set universal
access, disable Keychain prompts, or migrate to plaintext to avoid dialogs.

On the development Mac, list the valid signing identities:

```bash
security find-identity -v -p codesigning
```

The local CMake configuration selects and pins the sole valid Apple
Development identity by default (outside CI). If none or more than one is
available, set the following CMake configuration variable in Qt Creator,
using the chosen certificate fingerprint:

```text
GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY:STRING=<certificate SHA-1 fingerprint>
```

The existing post-build step signs and verifies the application with this
identity. Use the same identity across rebuilds, and avoid signing over it
with an ad-hoc identity when running a local package. If no Apple
Development identity is available, an explicitly selected, locally
trusted code-signing certificate is an option for development only.
It is not Developer ID distribution or notarization.

After reconfiguration and rebuilding, inspect the exact app you run:

```bash
APP="/path/to/Genycaster.app"
codesign --verify --strict "$APP"
codesign -d -r- "$APP" 2>&1
codesign -dv --verbose=4 "$APP" 2>&1
```

The designated requirement must remain stable after another build and
must not be merely a cdhash. Existing Keychain entries may request
authorization once after changing the signing identity. Grant access
only to the verified app; then quit and reopen twice, rebuild once with
the same identity and reopen again. Verify that profile, social writes,
Pro and streak state survive without additional access dialogs. If
prompts recur on the identical signed binary, inspect the dialog's
item name and the item's Access Control in Keychain Access, and confirm
the login keychain isn't locked or configured to relock on sleep. Do not
delete any production credentials as a troubleshooting step.

CI retains its explicit ad-hoc or Developer ID signing modes; no stable
cross-update trust is promised for ad-hoc public packages.

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
disclose ad-hoc signing to users. Disabling Gatekeeper or stripping quarantine
is not an acceptable distribution workaround.

References:
- https://developer.apple.com/developer-id/
- https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution
