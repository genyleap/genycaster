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
