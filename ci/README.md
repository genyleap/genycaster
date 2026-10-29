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
| Core tests | Ubuntu 24.04, GCC 16 | C++26 + CTest + architecture guards |
| Linux x86_64 | Ubuntu 24.04, GCC 16 | Qt AppImage |
| macOS arm64 | macOS 15, Homebrew LLVM | Qt DMG |
| Windows x86_64 | Windows 2022, Clang-CL/MSVC ABI and vcpkg | Portable ZIP |
| Android arm64-v8a | Ubuntu 24.04, NDK 27.2.12479018 | APK |
| WebAssembly | Ubuntu 24.04, Emscripten 5.0.5 | Static-site tar.gz |

Qt 6.12.0 and CMake 4.4.4 are pinned in the workflow. GitHub Ubuntu
runners need GCC 16 from the Ubuntu toolchain PPA for GenyCaster's C++26
module/standard-library support; stock Clang 18 with the default libstdc++
failed on `std::expected` during the first hosted run. All Qt platforms
explicitly install the qtmultimedia addon; the base Qt archive alone is
incomplete. Windows uses `ci/install_qt_windows.py` to retrieve the official
pinned MSVC2022 Qt 6.12 archives with upstream checksums: the 6.12 public
repository has component directories but no `Updates.xml` understood by aqt.
Android compiles OpenSSL 3.5.9 for its target architecture, rather than linking macOS/Linux
binaries. Emscripten 5.0.5 matches the Qt 6.12 requirement. Linux, Windows
and Android use the QtKeychain 0.17 source fallback if no suitable installed
package exists. The official WebAssembly Qt 6.12 SDK does not provide the
qtmultimedia addon in its aqt package index, so the Web job builds the matching
Qt ShaderTools followed by the Qt Multimedia 6.12.0 technology-preview
module from official SHA-256-verified source archives; the standalone Web
module build is still an acceptance gate. Linux/Windows set GENYCASTER_FETCH_QTKEYCHAIN=ON
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
* macOS: the GitHub macOS 15 runner and its Homebrew OpenSSL libraries target
  macOS 15+; builds for macOS 14 require separately built deployment-target
  compatible dependencies. macdeployqt creates an ad-hoc development DMG; Developer ID signing
  and Apple notarization are NOT configured yet.
* Windows: The ZIP is not Authenticode-signed; production certificates and
  verified signing remain a separate release requirement.
* Linux: AppImage runtime checks must be performed on clean supported systems.
* Web: The static site is archived only. It is not auto-deployed. Browser
  startup, HTTPS/CORS and cache behavior need end-to-end verification.

## Automated package verification

Every platform job invokes `ci/verify_platform_package.py` **before** uploading
its artifact. The validator inspects Linux AppImage ELF/type-2 signatures,
macOS UDIF DMG trailers, WebAssembly bootstrap/loader/manifest files, Android
ARM64 Qt/SSL/QtKeychain libraries and DEX/manifest, and Windows EXE/Qt/SSL/
SQLite/curl/QtKeychain DLLs. The same checks are exercised by portable
`ci/test_pipeline.py` tests. Checksums remain separate and are recalculated
for tagged releases. None of these checks establishes a working runtime,
verified TLS handshake, a trusted signature, or completed user acceptance.

The Android job also checks `QT_FEATURE_ssl=1` in the installed Qt Network
SDK **for manual and tagged builds**, before building OpenSSL. Bundling TLS
shared libraries alone cannot enable SSL APIs disabled when Qt was compiled.
Windows installs the individual vcpkg Boost libraries used by the source,
checks native headers before C++26 module dependency scanning, and caches
vcpkg binary packages across compatible manually dispatched runs.

## Hosted validation status (2026-10-29)

The following **manual** GitHub-hosted builds produced successful packaging
artifacts and passed subsequent artifact inspection (not device runtime tests):

* Linux: [AppImage run 40858586032](https://github.com/genyleap/genycaster/actions/runs/40858586032)
* macOS: [DMG run 40848337565](https://github.com/genyleap/genycaster/actions/runs/40848337565)
* Android: [APK run 40849490289](https://github.com/genyleap/genycaster/actions/runs/40849490289)
* WebAssembly: [web archive run 40853856463](https://github.com/genyleap/genycaster/actions/runs/40853856463)

Windows was not yet confirmed at this checkpoint. The Core matrix compiled
with GCC 16 and passed 17/18 tests; `localization_catalog_guard` remained red
due to actual translation backlog, so no release tag was created. Security-
critical locale text requires native-language review before a public release.
Always consult current Actions runs rather than treating this historical
checkpoint as the live status.

The release job fails closed rather than silently skipping an unavailable
required platform. To stop tag-based CI, disable the workflow in GitHub
Actions. Ordinary commits and pull requests do not run it.
