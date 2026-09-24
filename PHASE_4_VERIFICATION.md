# Phase 4 Verification

## Verification environment

```text
OS/architecture: macOS Darwin 25.5.0, Apple Silicon arm64
CMake:          4.4.2
Generator:      Ninja 1.13.2
Compiler:       Homebrew Clang 22.1.8
Qt:             6.11.1 macOS arm64
Python:         3.14.7
SQLite:         3.51.0
libcurl:        8.21.0
C++ mode:       project C++26 baseline
```

The compiler was selected explicitly for every configure:

```bash
export CC=/opt/homebrew/opt/llvm/bin/clang
export CXX=/opt/homebrew/opt/llvm/bin/clang++
```

Qt was located for `ui-debug` through the machine-local configure argument:

```text
-DQt6_DIR=/Users/compez/Qt/SDK/6.11.1/macos-arm64/lib/cmake/Qt6
```

No machine-specific Qt path was added to source-controlled CMake.

## Mandatory pre-implementation baseline

All five existing presets were configured from clean build directories before
Phase 4 work. `core-debug`, `provider-hypersnap`, `strict-self-hosted`, and
`minimal-core` passed. `ui-debug` built and passed strict QML lint after the
Apple headless-platform correction described in `PHASE_4_REVIEW.md`.

The initial Apple compiler default was `/usr/bin/c++` (AppleClang 21), which
could not provide the required C++ module dependency scanner. Reconfiguring with
the mandated Homebrew Clang 22 toolchain resolved the configuration failure.

Boost 1.92 then exposed the provider codec compatibility correction from
`boost::json::error_code` to `std::error_code`. The existing malformed-JSON
contract test passed after the change.

## Final clean build matrix

Every listed build directory was absent before its final configure. Commands:

```bash
cmake --preset core-debug
cmake --build --preset core-debug
ctest --preset core-debug --output-on-failure

cmake --preset provider-hypersnap
cmake --build --preset provider-hypersnap
ctest --preset provider-hypersnap --output-on-failure

cmake --preset strict-self-hosted
cmake --build --preset strict-self-hosted
ctest --preset strict-self-hosted --output-on-failure

cmake --preset minimal-core
cmake --build --preset minimal-core
ctest --preset minimal-core --output-on-failure

cmake --preset ui-debug -DQt6_DIR=/Users/compez/Qt/SDK/6.11.1/macos-arm64/lib/cmake/Qt6
cmake --build --preset ui-debug
cmake --build build/ui-debug --target farcaster_client_qmllint_strict
ctest --preset ui-debug --output-on-failure
```

Final result:

```text
core-debug          5/5 PASS
provider-hypersnap  6/6 PASS
strict-self-hosted  5/5 PASS
minimal-core        5/5 PASS
ui-debug            7/7 PASS
strict QML lint     PASS (0 project warnings)
```

The profile tests include the architecture guard and Apple SDK include guard.
No compile command reintroduced an explicit `MacOSX.sdk/usr/include` path.

## Deterministic read-pipeline coverage

`application.local_first_read` uses deterministic provider-neutral fakes and
real temporary SQLite databases. It verifies:

- the cached feed observer fires before the gateway is entered;
- provider refresh replaces local canonical profile/cast/timeline data;
- a second application-safe page appends and deduplicates entries;
- refresh failure retains cached entries and sets stale/offline/error state;
- closing every store and reopening the database still returns feed, profile,
  and cast content while the gateway is unavailable;
- identity, profile, cast, followers, and following paths persist and reread
  canonical values;
- the real serialized executor delivers the ready observer from a thread other
  than the caller.

The Hypersnap contract suite continues to verify provider/capability pagination
affinity and that raw cursors cannot be used as public continuation tokens.

## Qt and visual verification

The warning-fatal headless test loads the complete QML module with deterministic
presentation content and verifies the root window, app shell, and read list.
`farcaster_client_qmllint_strict` runs with `--max-warnings 0`.

Deterministic PNGs were captured from the real Qt executable at:

```text
390 × 844   compact
760 × 900   standard
1280 × 900  wide
```

Rendered review checked viewport containment, text wrapping, the 720 px reading
measure, wide navigation ownership, explicit cached/refreshing status, and list
geometry. The first compact capture found a control-width overflow; compact tab
and channel widths were corrected and the capture was repeated successfully.

The external Qt macOS offscreen plugin emits a non-project generic `Sans Serif`
alias startup diagnostic during PNG capture. The warning-fatal smoke uses the
warning-clean `minimal` plugin and remains green.

## Design-system verification

```bash
python3 .../frontend-design-premium/scripts/audit_project.py . --mode strict
npx --yes -p @google/design.md designmd lint DESIGN.md
```

Result:

```text
premium UI strict audit: 0 findings
DESIGN.md lint:           0 errors, 13 orphan-token warnings, 1 token summary
```

The lint warnings arise because official DESIGN.md component frontmatter uses
empty component maps while the exact token-to-QML mapping is documented in
prose and implemented in `ui/theme/Theme.qml`. Every warned semantic token is
consumed by that runtime singleton; QML lint and rendered inspection are the
runtime drift evidence.

## Verified / not verified

VERIFIED:

- Apple Silicon Homebrew Clang 22 named-module builds;
- Qt 6.11.1 build, MOC, QML registration/resources/cache generation;
- strict QML lint and warning-fatal smoke;
- all required provider-free/provider/deployment profiles;
- local-first behavior, offline restart, worker delivery, pagination opacity;
- compact/standard/wide deterministic rendering;
- architecture and Apple SDK include-path guards.

NOT VERIFIED:

- live public-network availability or latency of any configured provider;
- production signing/notarization, mobile packaging, or physical-device input;
- translated locale review, screen-reader session, or a future dark theme;
- realtime reconciliation, writes, durable outbox, or Phase 5 behavior.

Normal test success does not depend on external network access.
