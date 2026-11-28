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

If exactly one valid Apple Development certificate is installed, local CMake
configuration selects and pins its fingerprint automatically (outside CI).
If multiple certificates exist, set `GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY`
explicitly and use the same identity and bundle identifier for successive
local builds. Disable automatic selection with
`GENYCASTER_MACOS_AUTO_LOCAL_SIGNING=OFF` if desired. If the login
keychain itself is locked or its password is out of sync, that is a separate
OS authorization condition; the application must not bypass it.

Acceptance on a real Mac: launch, authorize the existing entries if needed,
quit explicitly, reopen twice without rebuilding, then rebuild using the
same stable identity and reopen. Check account restore, cloud authorization
and a social signature, and verify that Deny does not silently reauthorize.
The fake-Keychain regression fixture validates access counts and Ed25519
behavior but cannot validate Apple's actual ACL or SecurityAgent dialogs.
