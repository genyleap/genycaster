# GenyCaster GitHub Actions: Controlled Multi-Platform Builds

The workflow is at `.github/workflows/ci.yml`. It adapts the
matrix/deployment/artifact structure of GenyConnect, but deliberately has
**no push-to-main, pull-request, schedule, or release-edit triggers**.

## When workflows run

| Event | Hosted build | Publishes release |
|---|---|---|
| Ordinary commit pushed to main | No | No |
| Pull request | No | No |
| Actions > GenyCaster cross-platform builds > Run workflow | Selected platform only | No |
| Push of vMAJOR.MINOR.PATCH version tag | All required platforms | Only if all jobs succeed |

The manual run offers `web` (default), `linux`, `macos`,
`windows`, `android`, `ios`, `nixos`, and `all`. Selecting `all`
requires checking **confirm_full_matrix** to acknowledge its macOS/Windows
runner cost. Each family can be built separately without running every
architecture. Repeat manual runs of the same family/ref cancel the older
in-progress run; version-tag releases are never cancelled.

The shared **gate** validates the version tag, main-branch ancestry and
full-matrix budget consent. Dedicated unit, integration, cryptographic,
UI and platform regression tests run on the GenyCaster server, **not** in
GitHub Actions. Hosted workflows are reserved for building and packaging.
Artifact integrity, dependency and release-signing checks remain mandatory
because they protect the actual deliverables.
GitHub Actions' billing quota still applies: this design does not bypass
GitHub Free compute limits. Manual runs upload artifacts for 14 days,
but cannot create GitHub Releases.

For tag runs, `ci/release_gate.py` checks that the tag matches
`project(Genycaster VERSION ...)`, and that the tagged commit is an ancestor
of `main`. Missing/failed jobs prevent publishing. An existing release
is never deleted or overwritten.

## Platform matrix

| Target | Runner | Result |
|---|---|---|
| Linux x86_64 / arm64 | Ubuntu 24.04 x64 / ARM hosted, GCC 16 | Architecture-specific AppImage |
| macOS arm64 / x86_64 | macOS 15 ARM / Intel, LLVM | Architecture-specific DMG |
| Windows x86_64 / arm64 | Windows 2022 x64 / Windows 11 ARM, Clang-CL/MSVC ABI | Portable ZIP |
| Android arm64-v8a / armeabi-v7a | Ubuntu 24.04, NDK 27.2.12479018 | ABI-specific APK |
| WebAssembly | Ubuntu 24.04, Emscripten 5.0.5 | Static-site tar.gz |
| iOS arm64 | macOS 15 + iOS SDK | CI-only unsigned .app archive (not a distributable IPA) |
| NixOS x86_64 / arm64 | Native Nix GCC 16 + Qt on Linux x64 / ARM | Nix package build and closure manifest |

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

## Building and updating the WebAssembly site on the VPS

Unlike the GitHub-hosted workflow, `ci/build_deploy_wasm_server.sh` compiles
the **current server checkout** using the VPS's **installed** Qt 6.12.0
single-thread WebAssembly SDK, matching Qt host tools and Emscripten 5.0.5.
It **does not require GitHub Actions minutes**. It is not a remote-deployment
API and cannot install missing SDKs by itself. If WebAssembly Qt ShaderTools
or Multimedia is missing, it uses the existing pinned SHA-256-verified
`ci/build_qt_wasm_multimedia.sh` build.

First verify that the VPS checkout is clean and current on `main`:

```bash
cd /opt/genycaster/repository
git fetch origin main
git switch main
git pull --ff-only origin main
git status --short
```

Activate the VPS's installed Emscripten 5.0.5 environment and use **actual**
Qt paths (do not guess from the example below):

```bash
source /path/to/emsdk/emsdk_env.sh
export QT_ROOT_DIR=/path/to/Qt/6.12.0/wasm_singlethread
export QT_HOST_PATH=/path/to/Qt/6.12.0/gcc_64
bash ci/build_deploy_wasm_server.sh
```

This creates and verifies `dist/GenyCaster-v<version>-web-wasm.tar.gz`,
including the hashed WASM, loader, manifest and Service Worker, **without
changing the website**. Only after the archive passes verification, publish
to the **actual Nginx static root** (check the live vhost configuration first):

```bash
GENYCASTER_SITE_URL=https://genycaster.xyz \
  bash ci/build_deploy_wasm_server.sh --web-root /ACTUAL/NGINX/STATIC/ROOT
```

The process needs write access to the live static directory and its parent.
The deploy step stages the full archive outside the public root, installs
new immutable WASM first, atomically replaces mutable assets with backups,
and promotes `farcaster_client.html` last. Existing content-hashed WASM
is retained for open tabs. Failed file updates restore previously changed
mutable files; on success, backups remain in a sibling
`.genycaster-backup.*` directory to support manual rollback.

**Never use the example Nginx path in documentation as the real webroot
without inspecting the live server.** No server changes happen when
`--web-root` is omitted. Check `nginx -t` and the deployed
`/farcaster_client.html`, `/genycaster-assets.json` and WASM MIME/cache
headers over HTTPS. Static-file promotion needs no Nginx reload when its
configuration is unchanged.

## Billing discipline

GitHub Free has a finite monthly Actions compute allowance. The organization
may exhaust its allowance even though the billable subtotal is $0. Keep
manual `web`/single-family runs as the normal build workflow;
the `all` checkbox is a deliberate expensive release-candidate check.
A paid budget increase is **not** required for VPS-local WASM builds.

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
SQLite/curl/QtKeychain DLLs. The packaging checks are also covered by server-run
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
