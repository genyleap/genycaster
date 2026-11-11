# macOS Gatekeeper-ready distribution

CI builds on `macos-26` for Apple Silicon and `macos-26-intel` for x86_64.
A successful `macdeployqt` run, QML startup smoke test, and `hdiutil verify`
do **not** prove that Safari-downloaded software will pass Gatekeeper.

## Two intentionally different CI outputs

- **Experimental workflow_dispatch without Apple credentials:** The completed,
  pruned `Genycaster.app` is signed ad-hoc, then checked with
  `codesign --verify --deep --strict`. The DMG filename ends in
  `-adhoc-UNNOTARIZED.dmg`. This tests code-signature integrity, but macOS
  may reject the quarantined download. It is **not** a public release.
- **Developer ID + notarization:** CI signs the finished bundle inside-out
  with a Developer ID Application identity and Hardened Runtime, submits the
  exact DMG using `xcrun notarytool`, checks Apple returned `Accepted`,
  staples the ticket, and checks `spctl --assess` on the mounted payload.
  A version-tag release **fails** instead of falling back to ad-hoc.

GitHub repository **Settings → Secrets and variables → Actions** must contain
all five secrets before a Gatekeeper-ready macOS artifact can be produced:

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

The source of the re-signing model is the related GenyConnect workflow, but
GenyCaster uses inside-out signing and a mandatory notarization gate for
public releases rather than GenyConnect's ad-hoc-only approach.

References:
- https://developer.apple.com/developer-id/
- https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution
