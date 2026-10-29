# 11 — Testing and Architecture Guards

## Architecture tests are first-class tests

Structural regressions must fail CI before they become product debt.

## Current executable guard

`ci/check_architecture.py` rejects:

- provider/backend names or managed endpoints outside provider adapters;
- project-owned `.h` files;
- unauthorized `.hpp` files outside documented textual compatibility boundaries;
- `import std;` and experimental standard-library module CMake settings;
- textual includes inside named-module purview;
- exported namespace/module identity mismatches;
- Qt leakage into non-presentation core;
- forbidden dependency direction by module import;
- QML network/websocket/database I/O;
- root CMake below 3.30 or loss of the C++26 reference path;
- non-PascalCase `enum class` enumerators;
- vague `utils`, `helpers`, `common`, or `misc` ownership directories.

## Required core profile verification

At Phase 2 the release gate includes clean configure/build/test for:

```text
core-debug
strict-self-hosted
minimal-core
```

`minimal-core` compiles optional product composition flags into `configuredFeatures()` rather than creating empty placeholder feature targets.

## Router tests

Required behavior includes:

- preferred provider selection;
- unhealthy provider exclusion;
- deterministic fallback;
- strict self-hosted exclusion of managed providers;
- access mode and quality filtering;
- per-capability health rather than global provider health.

## Pagination/local storage tests

Provider-affine cursor storage must prove that raw cursors are not embedded in public continuation tokens.

SQLite integration tests cover canonical profile/cast round-trip, embed replacement, timeline order, continuation bind/resolve/erase and semantic checkpoint persistence.

Phase 4 local-first tests additionally prove:

- cached feed publication occurs before the routed refresh is entered;
- refresh persists canonical profiles/casts and replaces or appends timeline state;
- refresh failure retains cache and exposes stale/source-degradation/error metadata without inventing device offline state;
- pagination appends and deduplicates through application-safe tokens;
- closing and reopening the real SQLite store preserves feed/profile/cast data;
- serialized and bounded-pool executors deliver work off the caller/UI thread;
- identity, profile, cast, followers, and following orchestration share the same provider-neutral boundary.

## Sync tests

The coordinator must reject duplicate begin operations and stale generation completion/failure.

## Qt verification

A core-only build never validates the GUI. A Qt-enabled final gate must include:

- clean Qt configure/build with generated MOC/QML registration/resource/cache sources;
- `farcaster_client_qmllint_strict` with zero project warnings;
- warning-fatal offscreen `ui.smoke`;
- actual runtime/QML output path inspection;
- later visual acceptance screenshots for real product screens.

If Qt is unavailable, report UI as **NOT VERIFIED**. Current Phase 2 evidence is recorded in `PHASE_2_VERIFICATION.md`.

## Later provider contract tests

Each provider adapter must normalize captured/provider responses into canonical domain semantics without exposing its DTOs upstream.
