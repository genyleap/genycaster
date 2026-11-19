# GenyCaster GitHub Actions: Controlled Multi-Platform Builds

The workflow is at `.github/workflows/ci.yml`. Its controlled
multi-platform build matrix deliberately has **no push-to-main,
pull-request, schedule, or release-edit triggers**.

## When workflows run

| Event | Hosted build | Publishes release |
|---|---|---|
| Ordinary commit pushed to main | No | No |
| Pull request | No | No |
| Actions > GenyCaster CI and Release > Run workflow | All requested targets in one run (default) | No |
| Push of vMAJOR.MINOR.PATCH-rcN version tag | All required platforms | Only if all jobs succeed |

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
manual work; version-tag release jobs are never cancelled.

The shared **gate** validates the version tag, main-branch ancestry and
full-matrix budget consent. Dedicated unit, integration, cryptographic,
UI and platform regression tests run on the GenyCaster server, **not** in
GitHub Actions. Hosted workflows are reserved for building and packaging.
Artifact integrity, dependency and release-signing checks remain mandatory
because they protect the actual deliverables.
GitHub Actions' billing quota still applies: this design does not bypass
GitHub Free compute limits. Manual runs upload artifacts for 14 days,
but cannot create GitHub Releases.

For tag runs, `ci/release_gate.py` checks that the tag matches the
`project(Genycaster VERSION ...)` numeric version plus the configured RC
suffix, that the canonical `release-notes/vVERSION.json` and generated Markdown exist, and that the tagged commit
is an ancestor of `main`. Missing/failed jobs prevent publishing. An existing release
is never deleted or overwritten.

## Canonical multilingual release notes

Each release has a **single authored JSON source**, for example
`release-notes/v1.0.0-rc1.json`. It contains a typed release, stable
change identifiers, and complete, independently translated titles, summaries
and change descriptions for the application's 12 supported locales.
The numeric version, channel and channel number are independent **CMake**
fields, never duplicated in the authored notes.

Generate the GitHub Release body from the JSON source:

```bash
python3 ci/release_notes.py --source release-notes/v1.0.0-rc1.json \
  --version 1.0.0-rc1 --markdown release-notes/v1.0.0-rc1.md
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
| WebAssembly | Ubuntu 24.04, Emscripten 5.0.5 | Verified site artifact for **app.genycaster.xyz only**, not a GitHub Release download |
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
GENYCASTER_SITE_URL=https://app.genycaster.xyz \
  bash ci/build_deploy_wasm_server.sh --web-root /var/www/app.genycaster.xyz/public
```

The process needs write access to the live static directory and its parent.
**DNS/TLS prerequisite:** `app.genycaster.xyz` must first resolve to the
VPS through a DNS record controlled by the domain administrator. Prepare the
dedicated static root `/var/www/app.genycaster.xyz/public`, issue its own
valid TLS certificate, and activate the reviewed vhost
[`ci/app_genycaster_nginx.conf`](app_genycaster_nginx.conf) after `nginx -t`.

The reviewed, rollback-safe provisioning command is:

```bash
# Read-only preflight; does not change Nginx or issue a certificate:
python3 ci/activate_app_site.py

# Requires direct authorized root access to /etc/nginx and the server's
# existing Certbot/Let's Encrypt account. An ACME email may be provided.
sudo python3 ci/activate_app_site.py --apply
curl --fail --silent --show-error \
  https://app.genycaster.xyz/.well-known/genycaster-ready
```

The script reuses an existing, valid app-host certificate; if missing, it
bootstraps an HTTP-only ACME challenge first. After enabling the two-server
HTTPS config, it verifies **the actual loopback Nginx origin** on
`127.0.0.1:443` with SNI `app.genycaster.xyz`, a verified certificate chain,
the exact readiness marker and an HTTP 200 HTML entrypoint. An Nginx/TLS/origin
failure rolls back its changes. It never modifies the separate landing vhost.

**External CDN routing is a distinct phase.** If the locally verified app
origin works but the Cloudflare-proxied URL returns 404/403, the script
reports `ORIGIN READY, PUBLIC CDN ROUTING NOT VERIFIED`, keeps the healthy
dedicated vhost enabled, and exits with code **2**. Do not rerun Certbot or
delete the working Nginx vhost. Instead verify the Cloudflare `app` DNS
record's origin IP (on this deployment `173.214.160.36`), any
hostname/origin override, Workers, Page/Redirect Rules, and cached redirects.
The HTTP fallback on this VPS is `status.genyleap.com`; a CDN route with
an incorrect Host or destination can serve this unrelated site. Re-running
the activator is safe and will check the existing origin without rewriting
its configuration. Only an exact public `genycaster-app-v1` marker results
in exit code **0**. An HTTP 200 from the status site is insufficient.

```bash
# Independent public checks after activating:
curl -fsS https://app.genycaster.xyz/.well-known/genycaster-ready
curl -sSI https://app.genycaster.xyz/genycaster.html
```

The procedure never deploys WASM assets; preview assets are staged in the
dedicated root separately.

The vhost proxies only the app's first-party cloud API to localhost and
preserves the separate `genycaster.xyz` landing-site vhost. The account
service must be deployed with the exact app Origin/Host acceptance patch;
the browser API uses host-only cookies and remains CSRF-protected. When DNS
or TLS is missing, **do not upload the WASM files to an alternative domain**
or weaken the release gate. The build artifact can still be verified offline.

**Dedicated HTTPS vhost readiness:** An HTTPS 200 response alone is not
sufficient: a wildcard/default server may serve the unrelated Genyleap
status page. The reviewed app vhost exposes a non-credential readiness
marker at `/.well-known/genycaster-ready` with the exact body
`genycaster-app-v1`. Verify it on the public origin before the first build
and deploy:

```bash
curl --fail --silent --show-error \
  https://app.genycaster.xyz/.well-known/genycaster-ready
# expected: genycaster-app-v1
```

`ci/build_deploy_wasm_server.sh --web-root ...` enforces this check before
Qt configuration, including for initial deployment to an empty site. A
successful marker proves that traffic reaches the reviewed vhost; it does not
replace TLS trust, account-service origin verification or release acceptance.

The deploy step stages the full archive outside the public root, installs
new immutable WASM first, atomically replaces mutable assets with backups,
and promotes `genycaster.html` last. Existing content-hashed WASM
is retained for open tabs. Failed file updates restore previously changed
mutable files; on success, backups remain in a sibling
`.genycaster-backup.*` directory to support manual rollback.

**Never use the example Nginx path in documentation as the real webroot
without inspecting the live server.** No server changes happen when
`--web-root` is omitted. Check `nginx -t` and the deployed
`/genycaster.html`, `/genycaster-assets.json` and WASM MIME/cache
headers over HTTPS. Static-file promotion needs no Nginx reload when its
configuration is unchanged.

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
the build evidence in that run. This manual validation **never publishes**
a Release, even if invoked from a tag. Only a validated version-tag push
is eligible to publish after signing, feature, runtime and artifact gates.

## How to publish a version-tag candidate

1. Set the numeric version in the top-level CMake `project()` declaration,
   and keep `GENYCASTER_RELEASE_CANDIDATE` in sync with the intended RC suffix.
   Author the canonical, fully localized JSON source, such as
   `release-notes/v1.0.0-rc1.json`, and generate its Markdown using
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
5. From the intended main commit, create and push an annotated matching tag:

   ```bash
   git checkout main
   git pull --ff-only
   git tag -a v1.0.0-rc1 -m "Genycaster v1.0.0-rc1"
   git push origin v1.0.0-rc1
   ```

6. Only that tag runs the complete matrix and, if all required jobs finish,
   publishes a **pre-release** with packages and aggregate SHA256SUMS.

## Release limitations and verification

* Android: Manually dispatched APKs are not release-signed. Tagged candidate
  builds require the GitHub keystore secrets. Device startup, TLS, signer
  vault, network access and login must be verified before wider distribution.
* macOS: macOS 26 runners build a macOS 15+ target using SHA-256-pinned
  OpenSSL 3.5.9 source compiled for the deployment target, rather than
  linking Homebrew bottles that may require macOS 26. `macdeployqt
  -qmldir=ui` bundles QML dependencies. QtSql/PostgreSQL/ODBC/Mimer
  drivers are excluded before deployment: GenyCaster uses native SQLite
  and must not depend on unrelated database clients. The QML and Cocoa plugins, Qt
  frameworks and Mach-O dependencies are audited in both the build `.app`
  and the `.app` actually mounted from its DMG; both must pass a startup
  smoke test with Qt SDK environment variables removed. Developer ID signing
  and Apple notarization are NOT configured yet.
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
DMG should be described as production-installable for every user.

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
