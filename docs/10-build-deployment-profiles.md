# 10 — Build and Deployment Profiles

## Purpose

Provider and feature independence must be executable policy rather than documentation-only intent.

## Platform CMake ownership

Shared CMake checks remain at `cmake/` while target-specific toolchain,
transport dependency and bundle helpers live in each platform's single `cmake/platform/<target>/configure.cmake`.
Android Boost/OpenSSL/TLS logic belongs to Android alone; browser-specific
portable Boost logic belongs to WebAssembly; macOS bundle-case normalization
and the C++26 Apple SDK Objective-C++ compiler probe belong to macOS.
Changes specific to a target must stay inside that target's single
`configure.cmake`; the shared CMake composition retains only lifecycle
calls and common rules.

## Platform property ownership

Platform-specific packaging assets live under [`properties/`](../properties/README.md)
(Windows `.rc.in`, Android manifest and resources, iOS AppIcon, macOS ICNS,
Linux desktop/icon and WebAssembly PWA manifest). The shared Qt source and
runtime composition stay in `ui/` and `src/`; packaging metadata must not be
mistaken for separate platform implementations. CMake consumes these paths
from the repository root, and the icon generator verifies their deterministic
contents against the canonical source raster.

## Native executable identity

The CMake target remains `genycaster`, while built native application files are
named `Genycaster`: `Genycaster.app` (Apple platforms), `Genycaster.exe`
(Windows), and `Genycaster` (Linux/NixOS). The Linux `.desktop` launcher uses
`Exec=Genycaster`, with the original lowercase icon and desktop-file IDs.
This is a **case-sensitive file name** change, not a new application identity:
existing bundle/package identifiers, user data directories and translation IDs
remain unchanged. On macOS, CMake also sets `MACOSX_BUNDLE_BUNDLE_NAME=Genycaster`
(`CFBundleName` in `Info.plist`). An Apple-only post-build step repairs the
*physical* capitalization of a legacy `genycaster.app` directory left by
case-insensitive volumes after the executable was renamed. It uses a safe
two-stage rename, never deletes app contents, and does nothing on fresh builds.
The macOS release gate checks both `CFBundleName` and `CFBundleExecutable`.

The Android Qt native-library target and browser WebAssembly deployment
continue to use their existing lowercase resource names. In particular,
`genycaster.html`, `genycaster.js`, and `genycaster.wasm` are public web asset
paths and must not be renamed as part of native packaging changes.

## Installable desktop packages and update transports

Release jobs package each desktop target for its **native CPU architecture**
(`x86_64` and `arm64`), using the exact runtime that was compiled:

| Platform | Primary installation | Additional distribution |
| --- | --- | --- |
| macOS | `GenyCaster-v...-macos-<arch>.dmg` | `GenyCaster-selfupdate-v...-macos-<arch>.tar.gz` |
| Windows | `GenyCaster-v...-windows-<arch>.exe` (CPack/NSIS) | `GenyCaster-selfupdate-v...-windows-<arch>.zip` |
| Linux | `GenyCaster-v...-linux-<arch>.AppImage` | `GenyCaster-v...-linux-<arch>.deb`, AppImage `.zsync` |

The macOS DMG is assembled with `macdeployqt`; the Windows installer
includes the full `windeployqt`/QtKeychain/vcpkg payload and supplies an
uninstaller; Linux AppImage and DEB use the same `linuxdeploy` AppDir.
OS-provided system libraries are intentionally managed by the OS rather than
replacing critical system libraries in the package.

**Update transport vs. update installation:** GenyConnect's existing desktop
self-update files are copies of the native executable/DMG, not differential
binary patches. GenyCaster's `ci/package_selfupdate.py` outputs independent
runtime archives with a per-file SHA-256 manifest and a full-artifact
checksum. These snapshots are **not installable update patches**: their
manifest explicitly declares `installAction: unsupported`. The current
`UpdateController` only downloads official architecture-compatible
GitHub Release installer artifacts with SHA-256 verification and asks the
user to open the OS installer. It **does not** apply a Windows/macOS archive
in-place or replace running executables. A separate privileged/elevated
updater, signature and rollback checks are required before advertising
automatic patch installation. Linux also publishes `.zsync` metadata,
which enables block-level delta transfers when used by a compatible
zsync client; the built-in Qt updater does not yet consume it.

Unsigned CI test installers are not notarized/signed retail releases:
Windows SmartScreen and macOS Gatekeeper can require a trusted publisher
certificate and platform signing/notarization before general distribution.
Successful CI packaging alone does not establish retail release readiness.

## Current verification presets

### `core-debug`

Provider-free Debug core with tests and no Qt requirement.

### `strict-self-hosted`

Same provider-free core with `FARCASTER_SELF_HOSTED_ONLY=ON`. `configuredDeploymentProfile()` must report `StrictSelfHosted` and routing tests prove managed providers are ineligible under strict policy.

### `minimal-core`

All optional product composition flags are disabled. The flags are compiled into `farcaster.application.configuration.configuredFeatures()`; no empty feature libraries are created merely to represent them.

### `provider-auth`

Headless compatibility-auth profile. It keeps the read provider disabled, enables `FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS`, builds the SIWF adapter and its contract tests, and does not require Qt. This profile exists so authentication can be verified independently from the graphical composition and independently from live read-provider availability.

### `secure-signer`

Headless protocol-signer profile. It keeps read/auth providers and Qt disabled while enabling `FARCASTER_ENABLE_PROTOCOL_SIGNING`. On macOS this composes the Keychain/OpenSSL Ed25519 vault boundary; on platforms without an implemented secure vault the adapter compiles but returns `Unsupported` rather than persisting secrets insecurely.

### `ui-debug`

Qt Quick debug surface with tests. Requires Qt 6.12+ and must additionally pass strict QML lint and warning-fatal smoke verification. Use `cmake --preset ui-qt612-debug` and `cmake --build --preset ui-qt612-debug` on the Genycaster build server to select the version-pinned `/opt/genycaster/Qt/6.12.0/gcc_64` host toolchain and GCC 16. The `ui-debug` preset is retained for generic development, but it also requires Qt 6.12 or newer; do not reuse a Qt 6.11 CMake build directory because compiler module artifacts are toolchain-specific. The preset explicitly compiles the Hypersnap adapter because it is the only implemented live-read UI provider in the current checkpoint.

### Fresh IDE/default UI configuration

`FARCASTER_BUILD_UI=ON` now defaults `FARCASTER_ENABLE_HYPERSNAP_PROVIDER=ON`. Core-only presets still set both UI and provider support OFF explicitly. A runnable UI with provider support explicitly disabled fails at configure time instead of producing an executable whose first read can never route. Strict-self-hosted UI composition is also rejected until a self-hosted read adapter is implemented; the provider-free strict core preset remains valid.


### Android arm64-v8a OpenSSL (Qt 6.12)

Android NDK does not provide OpenSSL. Genycaster requires a target-architecture
OpenSSL 3.5.9 or newer when building its native Qt Android client; do not use
Homebrew/macOS OpenSSL, a host `Boost_DIR`, or an unversioned system Android
`libcrypto.so`. The Android job in `.github/workflows/ci.yml` downloads
the pinned OpenSSL 3.5.9 source and its SHA-256 checksum, cross-compiles
against NDK 27/API 28, packages `libcrypto_3.so` and `libssl_3.so`,
verifies AArch64 ELF headers and dependencies, and deploys them through
`QT_ANDROID_EXTRA_LIBS`. This is part of the authoritative CI build;
there is no separate project-maintained OpenSSL bootstrap script.
`GENYCASTER_ANDROID_OPENSSL_ROOT` remains an optional input for local
Android cross-builds that already have a correctly prepared target OpenSSL
prefix. The required package is assembled automatically by GitHub Actions.

### Android test configuration

Android client configurations default `FARCASTER_BUILD_TESTS=OFF` so APK
builds do not create unrelated desktop-style unit and contract executables.
Old Qt Creator caches retaining `FARCASTER_BUILD_TESTS=ON` are automatically
reset to `OFF` for Android APK builds. To opt into dedicated Android device
tests, set `GENYCASTER_ANDROID_ENABLE_TESTS=ON` and
`FARCASTER_BUILD_TESTS=ON` explicitly. Tests reuse `Boost::headers`, and the
Hypersnap curl-specific contract suite is omitted when the curl transport
is not compiled for that target. Linux CI test selection is unchanged.

### Android C++26 module threading consistency

Qt for Android adds `-pthread` to compilation of Qt-consuming translation
units. The Android Clang module scanner/PCM loader rejects a named-module
interface built without that mode when a Qt client imports it with `-pthread`
(`POSIX thread support was disabled in PCH file but is currently enabled`).
Genycaster applies `-pthread` to **all Android C++ targets**, including the
Qt-free module producers and Qt UI importers, before any subdirectories are
configured. This leaves the Linux/macOS/WASM compiler flags unchanged. When
switching to this revision, reconfigure the Android kit so CMake regenerates
module compile commands; the affected modules are then rebuilt automatically.

### Android Boost.JSON headers

Android compiles Boost.JSON directly from `boost/json/src.hpp` and only needs
portable Boost headers. `cmake/platform/Android/configure.cmake` creates the
`Boost::headers` interface target before providers are configured, avoiding
Qt Creator's host-only `find_package(Boost)` package-manager interception.
If suitable Boost 1.75+ headers are available via Homebrew, only their
headers are used (never host compiled libraries); otherwise the official
SHA-256-pinned Boost 1.89.0 header distribution is fetched into the CMake
build cache. To override header discovery without linking native libraries,
set `GENYCASTER_ANDROID_BOOST_INCLUDE_DIR` to a directory containing
`boost/json/src.hpp`, `boost/json.hpp`, and `boost/version.hpp`.

The precompiled Qt for Android SDK must itself have the `ssl` feature
compiled in. Packaged OpenSSL shared objects only satisfy *runtime* TLS
libraries; they cannot enable QSslSocket or QNetworkReply SSL APIs in a Qt
Network binary that was built without this feature. Inspect the Android kit's
`include/QtNetwork/qtnetwork-config.h` for `QT_FEATURE_ssl` (1 means enabled;
-1 means disabled). **CMake now refuses Android UI builds when this feature
is absent**, including local Qt Creator Release/Debug kits. For a macOS Qt kit:

```sh
grep QT_FEATURE_ssl "$HOME/Qt/6.12.0/android-arm64-v8a/include/QtNetwork/qtnetwork-config.h"
```

If it reports `-1`, the SDK's QtNetwork binary was built without TLS; no
change to the application's `OPENSSL_ROOT_DIR`, `QT_ANDROID_EXTRA_LIBS`,
`qtnetwork-config.h`, or CMake gate can repair that binary.

For reproducible release candidates, **GitHub Actions is the authoritative
Android toolchain**. Its Android job installs Qt 6.12.0, verifies the SDK's
compiled-in TLS feature, builds Android AArch64 OpenSSL, packages the APK and
checks mandatory runtime libraries. Development-machine Qt recovery scripts
are intentionally not part of the project. If an independently installed Qt
SDK reports `-1`, use a compatible Qt for Android build with SSL enabled,
following [Qt's source-build guide](https://doc.qt.io/qt-6.12/android-building.html);
do not replace libraries or edit feature headers. Merely bundling
`libssl_3.so` or `libcrypto_3.so` cannot enable SSL in a disabled SDK.
Qt Network adapters also check that the TLS backend is usable at
runtime, and certificate errors are never ignored. Node diagnostics report
HTTP status and Qt network error in Android Logcat without logging secrets;
a cold-emulator HTTPS probe has an 8-second timeout to avoid false offline
results. The default HyperSnap `/v1/info` route must still return a valid
`version` and `shardInfos` payload before a node is marked healthy.

Android icon SVGs use an alpha-preserving QtSvg rasterizer and native tint,
instead of relying on Qt Quick MultiEffect's GPU colorization path that can
produce solid-square artifacts on Android renderers. The desktop/Web path
remains unchanged. A standalone Qt 6.12 test in `tests/icon_raster` checks
shape transparency, tint and rejection of invalid SVG resource paths.

`Qt6Keychain >= 0.17` is a separate Android Keystore-backed signer-vault
requirement. If the effective backend remains `unavailable`, signed-write
flows must remain disabled. Release validation should enable
`FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON` and verify packaged libraries,
the ABI, and actual device startup/signing. A successful CMake configure is
not evidence that an APK has been built, signed, or tested.

## Future provider-policy profiles

Once real adapters exist, deployment policy may express:

```text
Standard:
  preferred protocol provider allowed
  self-hosted providers allowed
  compatibility providers optional

Private:
  privacy-acceptable protocol providers only
  compatibility providers disabled unless explicitly allowed

Strict Self-Hosted:
  public managed providers disabled
  compatibility providers disabled
  self-hosted/local services only
```

## Required future provider independence tests

When the relevant adapters exist, CI must prove removal/disablement without core redesign:

```text
NO_MANAGED_BACKEND
NO_HYPERSNAP
SELF_HOSTED_ONLY
```

Until those adapters exist, the stronger Phase 2 statement is that the core has **zero production provider implementation dependency at all**.

## Missing capability behavior

If no eligible provider exists for an optional capability:

- unrelated functionality continues;
- local cached state remains available;
- UI shows unavailable/disabled state where applicable;
- no hidden fallback to a managed backend is permitted.
