# Phase 1 Review — Repository & Foundation Scaffold

> Historical phase record. Phase 2 compliance changes supersede C++23, namespace, build-profile naming, and empty feature-target details below; see `PHASE_2_REVIEW.md`.

## What changed

Phase 0 described the architecture. Phase 1 now makes that architecture executable through CMake targets, named C++ modules, optional feature targets, an explicit Qt boundary, and automated architecture checks.

## Buildable core graph

```text
farcaster_foundation
        │
        ▼
farcaster_domain
        │
        ▼
farcaster_capabilities
        │
        ▼
farcaster_application
```

Optional features point inward:

```text
feature::social ─────┐
feature::feed ───────┤
feature::channels ───┤
feature::miniapps ───┤──→ farcaster_application
feature::chat ───────┤
feature::search ─────┤
feature::notifications ─┤
feature::wallet ─────┘
```

The core never points back to a feature.

## Provider status

There are **no provider implementations in Phase 1**. This is intentional.

Provider ownership boundaries already exist under:

```text
src/providers/hypersnap/
src/providers/self_hosted/
src/providers/compatibility/
```

but none is linked to the core graph.

## Current canonical modules

### Foundation

- `farcaster.foundation.error`
- `farcaster.foundation.result`
- `farcaster.foundation.time`

### Domain

- `farcaster.domain.identity`
- `farcaster.domain.cast`
- `farcaster.domain.profile`
- `farcaster.domain.social`
- `farcaster.domain.channel`
- `farcaster.domain.feed`

### Capability metadata

- `farcaster.capability.model`
- `farcaster.capability.catalog`

### Application

- `farcaster.application.build_profile`

## Important design choices

### 1. No provider in the core build

A clean Phase 1 build requires no provider SDK, endpoint, credential, or provider model.

### 2. No mandatory Qt dependency

`FARCASTER_BUILD_UI=OFF` keeps the core independently buildable. The Qt/QML shell already has an explicit MOC boundary for the later UI phase.

### 3. Continuations are client-owned handles

`PageContinuation` is not a provider cursor. Provider-affine cursor state will be retained inside routing/storage infrastructure in later phases.

### 4. Capability IDs are operation-level

Examples:

```text
cast.read
profile.read
graph.followers.read
feed.following.read
feed.channel.read
miniapp.manifest.resolve
messaging.legacy.read
messaging.secure
```

No global provider selection exists.

### 5. Async runtime is deliberately deferred

Phase 1 does not prematurely bind capability contracts to Boost.Asio, Qt futures, or another executor model. Phase 2 will choose the execution contract together with sync/router concurrency requirements.

## Validation performed

The following configurations have been built and tested in the delivery environment:

```text
core-debug            PASS
strict-self-hosted    PASS
minimal-core          PASS
```

Each runs:

- C++23 named-module compilation;
- foundation unit test;
- architecture guard.

## What Phase 1 intentionally does not do

- no network access;
- no Hypersnap adapter;
- no compatibility backend adapter;
- no SQLite implementation;
- no router implementation;
- no sync implementation;
- no production feed UI;
- no auth/signing implementation.

Those belong to the following roadmap phases and should not be smuggled into the foundation.

## Recommended next review

Inspect these files first:

1. `CMakeLists.txt`
2. `CMakePresets.json`
3. `src/domain/*.cppm`
4. `src/capabilities/*.cppm`
5. `tools/check_architecture.py`
6. `docs/16-phase1-foundation-scaffold.md`

If this shape is accepted, the next implementation step is **Phase 2: routing + SQLite/WAL + local entity/timeline store skeleton**.
