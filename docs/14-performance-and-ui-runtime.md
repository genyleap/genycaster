# 14 — Performance and UI Runtime Contract

## Goal

The product target is a native, highly responsive social client with Telegram-class interaction quality. C++ is not itself a performance guarantee; the architecture must keep expensive work off the UI path and use local-first state, deterministic layouts, incremental updates, and efficient rendering.

## UI architecture

Preferred long-term model:

```text
QML application shell
    │
    ├── navigation
    ├── sheets/dialogs
    ├── settings
    ├── profile/detail surfaces
    ├── ordinary forms/controls
    └── motion/orchestration

C++ presentation layer
    │
    ├── application-facing presentation models
    ├── immutable/incremental snapshots
    └── UI-safe commands

Hot timeline rendering
    ├── Feed
    └── Chat
```

For hot timelines, the architecture must permit migration to custom `QQuickItem` / Qt Quick Scene Graph rendering without changing domain/application/timeline engines.

## Do not prematurely custom-render everything

The first implementation may use optimized Qt Quick delegates where they meet performance targets.

Custom scene-graph rendering should be introduced when benchmarks demonstrate a meaningful bottleneck.

The key requirement in Phase 0 is **renderer replaceability**, not premature renderer complexity.

## Timeline separation

```text
FeedEngine / ChatTimelineEngine
          ↓
      Snapshot/Diff
          ↓
Presentation Model
          ↓
Renderer
```

The renderer must not own network, persistence, ranking, or synchronization logic.

## Thread/executor model

Conceptual responsibilities:

```text
UI/Main thread
  input, tiny state commits, layout scheduling

Qt Render/Scene Graph thread
  GPU rendering

Network executor
  HTTP/WebSocket/coroutine I/O

Database executor
  SQLite reads/writes/migrations

Sync/model executor
  reconciliation, timeline updates, normalization

Media worker pool
  image/video metadata/decode/preprocessing

Crypto worker
  signing/encryption/decryption where appropriate
```

Exact executor topology may evolve, but expensive work must not drift onto the UI thread.

## Hard UI-thread prohibitions

Never perform on the UI thread:

- network I/O;
- database I/O;
- JSON parsing of provider payloads;
- cryptographic operations;
- image/video decode;
- large persistence serialization;
- complete timeline rebuilds for incremental changes;
- large provider-to-domain normalization passes.

## Deterministic timeline layout

Feed/chat should avoid viewport instability caused by late media/reaction layout changes.

Long-term layout engine should support:

- cached text layout keyed by entity ID + width + font scale + theme-relevant parameters;
- known media aspect ratios before full decode where possible;
- stable variable-height estimation/calculation;
- incremental invalidation;
- exact viewport anchoring when older/newer content is inserted.

## Media pipeline

Conceptual pipeline:

```text
URL
 ↓
Cache lookup
 ↓
metadata/thumbnail
 ↓
worker decode
 ↓
GPU texture
```

Requirements:

- priority by viewport proximity;
- cancellation;
- request deduplication;
- memory-pressure handling;
- disk cache;
- reduced expensive work during high-velocity scroll where appropriate.

## Prefetch and navigation

Later phases may implement:

- adaptive prefetch based on direction/velocity/network/memory/battery;
- predictive navigation data preparation on pointer/touch-down with cancellation if navigation does not complete.

These are optimizations, not domain semantics.

## Engineering performance targets

Targets are budgets to test, not claims of achieved performance:

```text
touch → visible response       ≤ 1 frame where feasible
optimistic local insert        ≤ 16 ms target
cached conversation open       < 100 ms target
warm resume                    < 150 ms target
cached feed first paint        < 250 ms target
60 Hz flows                    near-zero dropped frames target
120 Hz                         first-class design target
UI-thread network/DB/JSON      0
```

All targets must be validated on a real device matrix before being treated as achieved.

## Device matrix direction

Performance testing should eventually cover:

- current iPhone flagship;
- older supported iPhone;
- current Pixel/Android flagship;
- mid-range Android;
- low-memory Android;
- Apple Silicon macOS;
- Windows x64;
- Linux desktop.
