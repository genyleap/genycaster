# GenyCaster GitHub Actions: Controlled Multi-Platform Builds

The workflow is at `.github/workflows/ci.yml`. It adapts the
matrix/deployment/artifact structure of GenyConnect, but deliberately has
**no push-to-main, pull-request, schedule, or release-edit triggers**.

## When workflows run

| Event | Expensive build/test | Publishes release |
|---|---|---|
| Ordinary commit pushed to main | No | No |
| Pull request | No | No |
| Actions > GenyCaster cross-platform builds > Run workflow | Selected platform only | No |
| Push of vMAJOR.MINOR.PATCH version tag | All required platforms | Only if all jobs succeed |

The manual run offers `all`, `core`, `linux`, `macos`,
`windows`, `android`, and `web`. Manual runs upload artifacts
for 14 days, but cannot create GitHub Releases.

For tag runs, `ci/release_gate.py` checks that the tag matches
`project(Genycaster VERSION ...)`, and that the tagged commit is an ancestor
of `main`. Missing/failed jobs prevent publishing. An existing release
is never deleted or overwritten.

## Platform matrix

| Target | Runner | Result |
|---|---|---|
| Core tests | Ubuntu 24.04, Clang 18 | C++26 + CTest + architecture guards |
| Linux x86_64 | Ubuntu 24.04, Clang 18 | Qt AppImage |
| macOS arm64 | macOS 15, Homebrew LLVM | Qt DMG |
| Windows x86_64 | Windows 2022, MSVC and vcpkg | Portable ZIP |
| Android arm64-v8a | Ubuntu 24.04, NDK 27.2.12479018 | APK |
| WebAssembly | Ubuntu 24.04, Emscripten 5.0.5 | Static-site tar.gz |

Qt 6.12.0 and CMake 4.4.4 are pinned in the workflow. Android compiles
OpenSSL 3.5.9 for its target architecture, rather than linking macOS/Linux
binaries. Emscripten 5.0.5 matches the Qt 6.12 requirement. Linux, Windows
and Android use the QtKeychain 0.17 source fallback if no suitable installed
package exists. Linux/Windows set GENYCASTER_FETCH_QTKEYCHAIN=ON
explicitly, so ordinary developer machines without libsecret do not become
broken. Release configurations require a secure signer vault.

## How to build without releasing

Open GitHub > Actions > GenyCaster cross-platform builds > Run workflow.
Choose a platform. For a full candidate build choose `all`. This never
publishes a GitHub release even if you manually run it from a tag.

## How to publish a version-tag candidate

1. Set the desired version in the top-level CMake `project()` declaration.
2. Push that code to main; this does not trigger a CI build.
3. Prefer first running a manual `all` workflow.
4. Configure GitHub Actions release signing secrets:
   `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`,
   `ANDROID_KEY_ALIAS`, and `ANDROID_KEY_PASSWORD`.
   Never commit the keystore. Missing release signing secrets fail the
   Android release job instead of publishing an unsigned package.
5. From the intended main commit, create and push an annotated matching tag:

   ```bash
   git checkout main
   git pull --ff-only
   git tag -a v1.0.0 -m "GenyCaster v1.0.0"
   git push origin v1.0.0
   ```

6. Only that tag runs the complete matrix and, if all required jobs finish,
   publishes a **pre-release** with packages and aggregate SHA256SUMS.

## Release limitations and verification

* Android: Manually dispatched APKs are not release-signed. Tagged candidate
  builds require the GitHub keystore secrets. Device startup, TLS, signer
  vault, network access and login must be verified before wider distribution.
* macOS: macdeployqt creates an ad-hoc development DMG; Developer ID signing
  and Apple notarization are NOT configured yet.
* Windows: The ZIP is not Authenticode-signed; production certificates and
  verified signing remain a separate release requirement.
* Linux: AppImage runtime checks must be performed on clean supported systems.
* Web: The static site is archived only. It is not auto-deployed. Browser
  startup, HTTPS/CORS and cache behavior need end-to-end verification.

The first GitHub-hosted executions are not yet verified. A successful local
CMake configure is not proof that packaging, Android deployment or native
runtime behavior succeeds on every runner. The release job intentionally
fails closed rather than silently skipping an unavailable required platform.

To stop tag-based CI, disable the workflow in GitHub Actions. Ordinary
commits and pull requests do not run it.
