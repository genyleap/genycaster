# GenyCaster GitHub Actions: Controlled Multi-Platform Builds

The workflow is at `.github/workflows/ci.yml`. Its controlled
multi-platform build matrix deliberately has **no push-to-main,
pull-request, schedule, or release-edit triggers**.

## When workflows run

| Event | Hosted build | Publishes release |
|---|---|---|
| Ordinary commit pushed to main | No | No |
| Pull request | No | No |
| Actions > GenyCaster CI and Release > Run workflow (normal validation) | All requested targets in one run (default) | No |
| Actions > Run workflow, main + all + approval + `publish_public_beta=true` | All required platforms | Yes, only after every job and package audit succeeds |
| Push of a personal version tag | Full gate rejects non-bot tagger | No |

**One invocation is the release-candidate validation unit**, as in
GenyConnect: one shared preflight gate fans out to Linux x86_64/ARM64,
macOS arm64/Intel, Windows x86_64/ARM64, Android arm64-v8a and
site-only WASM, uploading all artifacts into a single Actions run.
`all` is the default manual selection, and requires
**confirm_full_matrix=true** so the expensive runners are intentional.
The `linux`, `macos`, `windows`, `android`, and `web` selections are
**diagnostics only**, not release orchestration. iOS/NixOS likewise
run only when explicitly selected and are not part of the seven-target
release matrix. Never dispatch a separate CI run per release platform.
Release-tag pushes use that same full matrix automatically, then one
`publish` job waits for and checks every required artifact.
Repeat diagnostic runs of the same ref/selection cancel superseded
manual work; explicitly armed public Beta release jobs are isolated from
those cancellations. The Release tag is created by `github-actions[bot]`
AFTER all artifacts are verified, never by a personal Git identity.

The shared **gate** validates the version tag, main-branch ancestry and
full-matrix budget consent. Dedicated unit, integration, cryptographic,
UI and platform regression tests run on the GenyCaster server, **not** in
GitHub Actions. Hosted workflows are reserved for building and packaging.
Artifact integrity, dependency and release-signing checks remain mandatory
because they protect the actual deliverables.
GitHub Actions' billing quota still applies: this design does not bypass
GitHub Free compute limits. Ordinary manual diagnostic runs upload artifacts
for 14 days but cannot create GitHub Releases. Only a separately armed
`publish_public_beta` dispatch on main, with all architectures and budget
confirmation, may publish after the complete matrix succeeds.

For tag runs, `ci/release_gate.py` checks that the tag matches the
`project(Genycaster VERSION ...)` numeric version and configured semantic
channel (for `v1.0.0` the suffix is intentionally empty), that the canonical `release-notes/vVERSION.json` and generated Markdown exist, and that the tagged commit
is an ancestor of `main`. Missing/failed jobs prevent publishing. An existing release
is never deleted or overwritten.

## Canonical multilingual release notes

Each release has a **single authored JSON source**, for example
`release-notes/v1.0.0.json`. It contains a typed release, stable
change identifiers, and complete, independently translated titles, summaries
and change descriptions for the application's 12 supported locales.
The numeric version, channel and channel number are independent **CMake**
fields, never duplicated in the authored notes.

Generate the GitHub Release body from the JSON source:

```bash
python3 ci/release_notes.py --source release-notes/v1.0.0.json \
  --version 1.0.0 --markdown release-notes/v1.0.0.md
```

The release gate validates that every locale is complete and the generated
Markdown matches the JSON byte-for-byte. Native and WebAssembly bundles embed
the original JSON. Native updates parse generated locale sections from
GitHub Releases; WASM update metadata carries the structured JSON. Switching
the app's language reselects the release content immediately.

## Platform matrix

| Target | Runner | Result |
|---|---|---|
| Linux x86_64 / arm64 | Ubuntu 24.04 x64 / ARM hosted, GCC 16 | Architecture-specific AppImage, DEB and `.zsync` metadata |
| macOS arm64 / x86_64 | macOS 26 ARM / Intel, LLVM | Architecture-specific DMG and verified self-update transport archive |
| Windows x86_64 / arm64 | Windows 2022 x64 / Windows 11 ARM, Clang-CL/MSVC ABI | NSIS `.exe` installer and verified self-update transport archive |
| Android arm64-v8a only | Ubuntu 24.04, NDK 27.2.12479018 | Signed release APK; other Android ABIs excluded |
| WebAssembly | Ubuntu 24.04, Emscripten 5.0.5 | Verified site artifact for **genycaster.xyz only**, not a GitHub Release download |
| iOS arm64 | macOS 15 + iOS SDK | CI-only unsigned .app archive (not a distributable IPA) |
| NixOS x86_64 / arm64 | Native Nix GCC 16 + Qt on Linux x64 / ARM | Nix package build and closure manifest |

Qt 6.12.0 and CMake 4.4.4 are pinned in the workflow. GitHub Ubuntu
runners need GCC 16 from the Ubuntu toolchain PPA for GenyCaster's C++26
module/standard-library support; stock Clang 18 with the default libstdc++
failed on `std::expected` during the first hosted run. All Qt platforms
explicitly install the qtmultimedia addon; the base Qt archive alone is
incomplete. Windows uses `ci/install_qt_windows.py` to retrieve the official
pinned MSVC2022 Qt 6.12 archives with upstream checksums and
`ci/install_qt_webengine_windows.py` to retrieve the separately versioned,
checksum-verified official WebEngine 6.140 extension for x64 and ARM64: the 6.12 public
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

## Building and updating the canonical WebAssembly site on the VPS

**Canonical URL:** `https://genycaster.xyz/` serves the app. The preserved
landing lives at `/about/`, downloads at `/download`, and the existing
host-only account API continues under `/api/v1/cloud/`.
`https://app.genycaster.xyz` is retired. Its source activation/staging
scripts were removed; the live Nginx vhost and DNS record must also be
retired independently by the VPS/DNS operator. Never deploy a current WASM
bundle to that origin.

For the reviewed, rollback-safe migration from the former landing page,
follow [PRIMARY_WASM_MIGRATION.md](PRIMARY_WASM_MIGRATION.md). The verified
origin-consistent preview files have been staged and promoted to the primary
web root while leaving `index.html` untouched. The final live Nginx switch
requires explicit authorized root access:

```bash
cd /opt/genycaster/repository
git pull --ff-only
python3 ci/verify_primary_preview_payload.py /var/www/genycaster.xyz/public
sudo python3 ci/activate_primary_wasm_site.py --apply
curl -fsS https://genycaster.xyz/.well-known/genycaster-ready
# expected: genycaster-main-v1
```

The local certificate and Nginx origin checks cannot be weakened or skipped.
An origin failure triggers Nginx rollback; a Cloudflare-only failure leaves
the healthy origin active and is reported separately.

After the canonical vhost serves the app, a current-`main` WASM build can
be performed directly on the VPS with its installed Qt 6.12 single-thread
WASM SDK, Qt host tools and Emscripten 5.0.5. This avoids GitHub Actions
usage:

```bash
cd /opt/genycaster/repository
source /opt/genycaster/emsdk/emsdk_env.sh
export QT_ROOT_DIR=/opt/genycaster/Qt/6.12.0/wasm_singlethread
export QT_HOST_PATH=/opt/genycaster/Qt/6.12.0/gcc_64
export GENYCASTER_SITE_URL=https://genycaster.xyz
bash ci/build_deploy_wasm_server.sh \
  --web-root /var/www/genycaster.xyz/public
```

The deployment script requires the exact canonical marker before touching
the root, forces `GENYCASTER_PUBLIC_ORIGIN` and `GENYCASTER_SIWF_ORIGIN`
to `https://genycaster.xyz`, verifies the complete package, stages assets,
preserves immutable WASM versions and promotes HTML last.

**Native download invitation:** The WASM shell now contains an adaptive,
accessible desktop dialog/mobile bottom sheet. The default
`/releases/native.json` catalog is intentionally empty; it cannot advertise
an unsigned, unpublished or absent installer. After an official tagged GitHub
Release passes integrity/signing gates, use
`ci/generate_native_release_catalog.py` to generate the app and preserved
landing catalogs from the same verified native release assets. Never paste
a speculative GitHub download URL into a manifest.

## Billing discipline

GitHub Free has a finite monthly Actions compute allowance. The organization
may exhaust its allowance even though the billable subtotal is $0. Use a single `all` invocation when validating a release candidate;
the confirmation flag is a deliberate expensive release-candidate check.
Single-family invocations exist only to diagnose failures, never to
replace the unified release run.
A paid budget increase is **not** required for VPS-local WASM builds.

## How to build without releasing

Open GitHub > Actions > **GenyCaster CI and Release** > Run workflow.
Keep the default `all`, check `confirm_full_matrix`, and launch **once**.
Alternatively, via GitHub CLI:

```bash
gh workflow run ci.yml --repo genyleap/genycaster --ref main \
  -f platform=all -f target_arch=all -f confirm_full_matrix=true
```

One run fans out to the complete release-target matrix and aggregates
the build evidence in that run. This ordinary manual validation **never
publishes** a Release. A separate, explicitly armed `publish_public_beta`
dispatch performs the same full matrix with permanent Android signing,
then creates the tag and the public GitHub Release via `GITHUB_TOKEN` as
`github-actions[bot]`, only after every required build succeeds.

## How to publish a version-tag candidate

1. Set the numeric version in the top-level CMake `project()` declaration,
   and keep the release channel and tag suffix consistent. The first public
   Beta uses the numeric `v1.0.0` tag and GitHub's Beta/prerelease label,
   NOT a `-rc` or `-betaN` tag. Author the fully localized JSON source
   `release-notes/v1.0.0.json`, and generate its Markdown using
   `ci/release_notes.py`. The JSON is embedded into native and WebAssembly
   resources; the release gate verifies the generated Markdown matches the
   JSON before that Markdown is published as the GitHub Release description.
2. Push that code to main; this does not trigger a CI build.
3. Prefer first running a manual `all` workflow.
4. Configure GitHub Actions release signing secrets:
   `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`,
   `ANDROID_KEY_ALIAS`, and `ANDROID_KEY_PASSWORD`.
   Never commit the keystore. Missing release signing secrets fail the
   Android release job instead of publishing an unsigned package.
5. In GitHub Actions, select **GenyCaster CI and Release** and run it
   on `main`. Choose `platform=all`, `target_arch=all`, check
   `confirm_full_matrix`, and check `publish_public_beta` **only after a
   passing, architecture-complete validation**. Do **not** create or push
   `v1.0.0` from a personal Git identity.
6. The workflow builds and audits everything, then creates the annotated
   `v1.0.0` tag as `github-actions[bot]` using `GITHUB_TOKEN`, and
   publishes **Genycaster 1.0 Beta** as a non-draft public **prerelease**
   with native packages and aggregate SHA256SUMS. WASM deployment to
   `genycaster.xyz` remains a separate artifact-verified, rollback-safe
   operation, never automatic on a failed CI.

## Release limitations and verification

* Android: Diagnostic APKs are signed with disposable CI TEST keys only.
  The separately armed `publish_public_beta` run requires the persistent
  GitHub signing secrets. All Qt FFmpeg and first-party ELF PT_LOAD segments
  must satisfy 16 KiB alignment; vendor binaries are rebuilt from SHA-pinned
  FFmpeg 9.0.1 sources rather than bypassing the release validator. Device startup, TLS, signer
  vault, network access and login must be verified before wider distribution.
* macOS: macOS 26 runners build a macOS 15+ target using SHA-256-pinned
  OpenSSL 3.5.9 source compiled for the deployment target, rather than
  linking Homebrew bottles that may require macOS 26. `macdeployqt
  -qmldir=ui` bundles QML dependencies. QtSql/PostgreSQL/ODBC/Mimer
  drivers are excluded before deployment: GenyCaster uses native SQLite
  and must not depend on unrelated database clients. The QML and Cocoa plugins, Qt
  frameworks and Mach-O dependencies are audited in both the build `.app`
  and the `.app` actually mounted from its DMG; both must pass a startup
  smoke test with Qt SDK environment variables removed. Free public macOS
  releases are ad-hoc signed and explicitly marked UNNOTARIZED. Developer ID
  signing and Apple notarization are optional, not configured yet. Gatekeeper
  may block browser-downloaded ad-hoc DMGs; never claim Apple approval.
* Windows: The staged application must bundle QtQuick.Controls, the Basic
  style and the Multimedia backend. Windows CI runs the executable's QML
  startup smoke test without Qt SDK/vcpkg PATH entries before NSIS packaging.
  The signed Microsoft VC redistributable is supplied for clean installations.
  The NSIS installer itself is not Authenticode-signed; production certificates
  and verified signing remain a separate release requirement.
* Linux: Qt/QML/Multimedia components and ELF dependencies in linuxdeploy's
  AppDir are audited. GCC 16 requires GLIBCXX symbols unavailable in the
  stock Ubuntu 24.04 C++ library; linuxdeploy blacklists libstdc++ by
  default. The matching compiler-provided `libstdc++.so.6` is therefore
  bundled into `AppDir/usr/lib` and selected by the packaged `AppRun`
  launcher. The audit verifies ELF architecture, GLIBCXX symbol coverage,
  glibc 2.39 compatibility and actual dynamic-loader selection. Both the
  Debian installer **and the AppImage** must start in a clean Ubuntu 24.04
  Docker container without Qt installed. Debian system graphics/X11 libraries
  remain managed by the OS; do not bundle glibc or require GCC 16 from users.
  AppImage interoperability with other distributions and GPU drivers still
  requires further platform-specific testing.
* Web: The static site is archived only. It is not auto-deployed. Browser
  startup, HTTPS/CORS and cache behavior need end-to-end verification.

## Automated package verification

Every platform job invokes `ci/verify_platform_package.py` **before** uploading
its artifact. The validator inspects Linux AppImage ELF/type-2 signatures,
macOS UDIF DMG trailers, WebAssembly bootstrap/loader/manifest files, Android
ARM64 Qt/SSL/QtKeychain libraries and DEX/manifest, and Windows EXE/Qt/SSL/
SQLite/curl/QtKeychain DLLs. The packaging checks are also covered by server-run
`ci/test_pipeline.py` tests. Checksums remain separate and are recalculated
for tagged releases. The new desktop startup smoke tests go further and
exercise the packaged QML root on CI, but they do not replace actual-device
testing across supported OS versions, GPU drivers, secure-vault backends and
wallet/identity flows. Neither an unsigned iOS bundle nor an unnotarized macOS
DMG should be described as Apple-approved or automatically installable for
every user. Android public APKs require one persistent owner-controlled
keystore in four GitHub Actions Secrets; temporary CI-TEST keys are never
used for tagged releases. This does not require a paid Google Play account.

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


## Local/server release acceptance

Use the fail-closed server gate before packaging or requesting hosted
cross-platform builds:

```bash
python3 ci/server_acceptance.py
```

This checks architecture and module headers, the complete 12-locale catalog,
network/resource ceilings, data/media policies, release metadata, source-layout
invariants, and adversarial update-archive integrity tests. It invokes no
GitHub Actions jobs and creates no release.

After the Linux executable actually compiles and links, validate the *real*
Qt shell and local fake-provider runtime as well:

```bash
python3 ci/server_acceptance.py --native-executable build/ui-native-validation/bin/Genycaster
```

The native stage uses disposable settings and database files and Qt's
offscreen software backend. It checks initial QML construction, local
provider hydration, sign-in-to-Home behavior, and clean guest restart without
contacting a public API. Paths and compiler-runtime library discovery must
match the actual server; do not treat this as a portable binary substitute.

Use `--strict-design` when evaluating the remaining interface style/debt
gate. **Do not label a release approved if that required gate fails.**
Cross-platform artifacts, OS signing/notarization, installation, and platform
device behavior must still be checked on their respective runners.

Windows and macOS `selfupdate` archives contain complete runtime transport
snapshots and SHA-256 file/symlink manifests. Their archive content and path
safety are now checked by `ci/verify_selfupdate_snapshot.py` before CI
uploads them. They are explicitly marked `installAction=unsupported`:
**a signed post-exit atomic updater and authenticated incremental transport
are not implemented for these platforms yet**. Linux's zsync AppImage
metadata permits HTTP Range/block reuse but does not bypass system trust
or signature checks. No platform is authorized to overwrite a running,
unverified executable.
