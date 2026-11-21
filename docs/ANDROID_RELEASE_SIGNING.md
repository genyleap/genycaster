# Android public release signing (free, persistent key)

GenyCaster follows the existing GenyConnect Android release model:
one **owner-controlled, persistent** Android APK signing key is reused for
every version. Android APK signing does **not** require a paid Google Play
Developer account when distributing the APK directly through GitHub Releases.

## Create or reuse the release keystore

**Reuse your existing GenyCaster release keystore if any APK has already
shipped with it.** Replacing that signing identity prevents ordinary Android
package-manager updates over existing installations.

If this is the very first public APK, create a long-lived PKCS#12 keystore
on a trusted offline/workstation Mac with a Java JDK:

```bash
keytool -genkeypair \
  -keystore "$HOME/genycaster-release.p12" \
  -storetype PKCS12 \
  -alias genycaster \
  -keyalg RSA -keysize 3072 -validity 10000
```

Store the keystore and its password securely, with a tested encrypted backup.
Never commit a keystore, certificate private key, or password to the repository.

Convert the keystore to a single-line Base64 string locally:

```bash
base64 < "$HOME/genycaster-release.p12" | tr -d '\n' | pbcopy
```

Paste **only into GitHub → Settings → Secrets and variables → Actions**:

| Secret name | Value |
| --- | --- |
| `ANDROID_KEYSTORE_BASE64` | Entire one-line Base64 keystore (Clipboard) |
| `ANDROID_KEYSTORE_PASSWORD` | Keystore password entered in `keytool` |
| `ANDROID_KEY_ALIAS` | `genycaster`, or the existing key's real alias |
| `ANDROID_KEY_PASSWORD` | Signing key password (normally the store password for PKCS#12) |

Never send these values to chat or store them in files tracked by Git.

## Workflow guarantees

- For **release tags**, all four secrets are required by the fast release gate.
  The workflow signs using the same durable release identity and verifies the
  actual APK with `apksigner`, plus 16-KiB alignment with `zipalign`.
- **Manual CI builds** use short-lived, explicitly suffixed
  `-CI-TEST.apk` signatures instead and **cannot** publish a tagged Release.
  Never distribute CI-TEST APKs as public upgrade/install packages.
- macOS is independent: its public DMGs may use **ad-hoc signing** with an
  `-adhoc-UNNOTARIZED.dmg` suffix. Apple's paid Developer ID/notarization
  account is not required for this free distribution mode.
- Publishing still requires the normal platform QA, feature, integrity,
  architecture and all-platform release gates. No CI or Release is triggered
  by merely editing this policy.
