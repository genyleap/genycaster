# 10 — Build and Deployment Profiles

## Purpose

Provider and feature independence must be executable policy rather than documentation-only intent.

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
`libcrypto.so`. The standalone build script obtains the official OpenSSL 3.5.9
LTS release and its SHA-256 checksum, cross-compiles against the configured
NDK/API 28 with `SHLIB_VERSION_NUMBER=` (as required by the Qt 6 Android
OpenSSL instructions), copies the generated target headers and unversioned
shared libraries as `libcrypto_3.so` and `libssl_3.so`, and patches ELF
SONAME/dependencies. It validates both files as AArch64 ELF binaries, without
running `make install_sw` or installing an unused OpenSSL CLI.
Both shared libraries are packaged through `QT_ANDROID_EXTRA_LIBS`.

On a macOS host with Homebrew, run this from the Genycaster source root:

```sh
brew install patchelf
ANDROID_NDK_ROOT="$HOME/Library/Android/sdk/ndk/27.2.12479018" \
  bash tools/build_android_openssl.sh
```

Set the Qt Creator Android kit's CMake variable
`GENYCASTER_ANDROID_OPENSSL_ROOT` to the resulting `build/android-deps`
absolute path (for example, `/Users/compez/Developer/genycaster/build/android-deps`).
Clear the Qt Creator CMake cache before reconfiguring, especially when changing
ABI or OpenSSL prefix. This variable is distinct from
`OPENSSL_ROOT_DIR`; stale paths to host packages must not be reused.
The `ui-qt612-debug` CMake preset is intentionally Linux-only: it specifies
`/opt/gcc-16.2.0/bin/c++` on the build server and must not be used as a Qt
Creator kit on macOS. Select the Qt 6.12 Android arm64-v8a kit instead.

### Android test configuration

Android client configurations default `FARCASTER_BUILD_TESTS=OFF` so APK
builds do not create unrelated desktop-style unit and contract executables.
Old Qt Creator caches retaining `FARCASTER_BUILD_TESTS=ON` are automatically
reset to `OFF` for Android APK builds. To opt into dedicated Android device
tests, set `GENYCASTER_ANDROID_ENABLE_TESTS=ON` and
`FARCASTER_BUILD_TESTS=ON` explicitly. Tests reuse `Boost::headers`, and the
Hypersnap curl-specific contract suite is omitted when the curl transport
is not compiled for that target. Linux CI test selection is unchanged.

### Android Boost.JSON headers

Android compiles Boost.JSON directly from `boost/json/src.hpp` and only needs
portable Boost headers. `cmake/AndroidBoostHeaders.cmake` creates the
`Boost::headers` interface target before providers are configured, avoiding
Qt Creator's host-only `find_package(Boost)` package-manager interception.
If suitable Boost 1.75+ headers are available via Homebrew, only their
headers are used (never host compiled libraries); otherwise the official
SHA-256-pinned Boost 1.89.0 header distribution is fetched into the CMake
build cache. To override header discovery without linking native libraries,
set `GENYCASTER_ANDROID_BOOST_INCLUDE_DIR` to a directory containing
`boost/json/src.hpp`, `boost/json.hpp`, and `boost/version.hpp`.

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
