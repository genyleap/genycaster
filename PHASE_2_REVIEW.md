# Phase 2 Review — Routing, Local State, Sync Foundation

## Status

**CORE COMPLETE AND VERIFIED. Qt Quick surface structurally updated but NOT VERIFIED because Qt is unavailable in the delivery environment.**

Phase 2 turns the provider-neutral architecture into executable infrastructure without adding any production provider adapter. Exact clean-build evidence is recorded in `PHASE_2_VERIFICATION.md`.

## Implemented module graph

```text
foundation
   ↓
domain
   ↓
capabilities      provider.model
      \              /
       \            /
        routing

foundation + domain
       ↓
 local entity/timeline ports
       ↓
 SQLite storage adapters

foundation
   ↓
sync model/checkpoint/coordinator
       ↓
 SQLite checkpoint adapter
```

Application build composition is separate from provider identity:

```text
farcaster.application.configuration
```

## Routing

Implemented:

- `farcaster.routing.registry`
- `farcaster.routing.health`
- `farcaster.routing.policy`
- `farcaster.routing.router`

Routing is operation-level and evaluates:

- exact capability support;
- access mode;
- capability quality;
- deployment privacy policy;
- self-hostability;
- per-capability health;
- deterministic provider priority/latency scoring.

No health probe or network I/O occurs on the request path.

## Local-first storage

SQLite storage uses WAL and owns its C API boundary privately.

Current schema covers:

```text
schema_migrations
profiles
casts
cast_embeds
timeline_entries
continuations
sync_checkpoints
```

Provider cursors are stored only in `farcaster.storage.continuation`. The application sees only `farcaster.capability.pagination.ContinuationToken`.

## Sync

Implemented:

- semantic `SyncKey`;
- provider-neutral `SyncCheckpoint`;
- checkpoint persistence port;
- serialized sync coordinator state machine;
- stale generation rejection;
- structured failure state.

Network fetching, reconciliation, background executors and durable write outbox remain later-phase work.

## Modern C++ compliance refactor

Phase 2 also corrected Phase 1 scaffold deviations from the pinned `ai-for-modern-cpp` contract:

- C++26 reference path;
- CMake 3.30+;
- module namespaces now exactly mirror dotted module identities;
- Doxygen expanded on exported public contracts;
- no `import std;`;
- no fallback to classic project headers;
- `.hpp` retained only for SQLite C API and Qt MOC textual boundaries;
- empty ceremonial feature targets removed;
- build feature composition is generated into a real application configuration implementation;
- dependency links reduced to the modules actually owned/imported.

## Architecture guard

`tools/check_architecture.py` now rejects:

- vendor/backend semantics outside provider adapters;
- `import std;`;
- `.h` project headers;
- unauthorized `.hpp` files;
- includes inside named-module purview;
- module/namespace identity mismatches;
- Qt dependencies in non-presentation core;
- forbidden inward dependency directions;
- QML network/websocket/database I/O;
- C++23 regression in CMake;
- experimental standard-library module configuration;
- vague `utils/helpers/common/misc` ownership buckets.

## Build profiles

The required Phase 2 core profiles are:

```text
core-debug
strict-self-hosted
minimal-core
```

`minimal-core` disables all optional product composition flags. These flags are not empty feature libraries; they are compiled into `configuredFeatures()` and become actual build configuration state.

## Qt Quick scaffold

The Qt shell was updated to the pinned project baseline:

- Qt 6.6 minimum contract;
- C++26;
- project-wide QML output directory;
- guarded QTP0004 policy;
- explicit Basic Controls style selected before QML load;
- deterministic QML resource aliases;
- strict `qmllint` target;
- warning-fatal offscreen smoke test;
- explicit readiness check rather than fixed-delay success;
- Qt MOC boundary remains a documented `.hpp` exception.

A Qt-enabled clean build is required before GUI status can be promoted from NOT VERIFIED to PASS.

## Phase 3 gate

Phase 3 may introduce the first real provider only behind capability contracts. Hypersnap DTOs, URLs, auth headers, cursors and errors must remain entirely under its adapter package.
