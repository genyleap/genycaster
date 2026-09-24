# Phase 4 Review — Local-First Read Vertical Slice

## Status

Phase 4 is implemented end to end. The Qt client reads canonical SQLite state
first, refreshes through operation-level routing on a worker, persists normalized
results, and incrementally commits presentation state without exposing provider
objects or cursors above infrastructure.

## Public architecture added

### Application

- `farcaster.application.executor` — minimal scheduling port for blocking work.
- `farcaster.application.read.model` — provider-neutral selectors, canonical
  states, application-safe pagination, and lifecycle metadata.
- `farcaster.application.read.gateway` — blocking routed-read boundary used only
  on an I/O executor.
- `farcaster.application.read.service` — cache-first orchestration for identity,
  profile, cast, feed, and social reads.

The existing blocking capability contracts were not redesigned.

### Local state and storage

- `farcaster.local.read` owns the stable complete-read persistence port.
- `farcaster.storage.read` implements it over SQLite WAL plus the existing
  canonical entity/timeline stores.
- Schema migration 5 adds durable application-safe read continuation state and
  ordered social-list entries.
- Feed/social append paths deduplicate canonical IDs/edges; refresh failures do
  not erase materialized content.
- Username lookup is case-insensitive at the canonical profile store boundary.

### Execution and routing

- `farcaster.execution.serial` owns one RAII `std::jthread`, a guarded FIFO, and
  shutdown/drain behavior.
- `farcaster.routing.read.gateway` resolves every operation independently and
  dispatches only through provider-neutral capability interfaces.
- Runtime source bindings contain borrowed capability pointers associated with
  stable provider IDs. Application never imports routing or provider modules.

ADR 0008 records why Phase 4 uses one serialized executor and why cancellation
suppresses observer delivery rather than pretending to interrupt an arbitrary
blocking third-party call.

## Implemented user flows

The same local-first lifecycle is implemented for:

1. username resolution and identity opening;
2. Following Feed;
3. Trending Feed;
4. Channel Feed;
5. Cast Detail;
6. Profile;
7. Followers;
8. Following;
9. explicit Load more pagination;
10. cached restart/offline reads;
11. refresh and canonical reconciliation after availability returns.

Every use case publishes local state before invoking the routed gateway. A
failed refresh publishes stale/offline/error metadata while retaining cached
content. An empty local snapshot remains distinguishable from usable stale data.

## Qt presentation

`PresentationBridge` remains an explicit Qt/MOC textual boundary. It owns only
QML-facing view state and emits user intents. The executable composition root:

- creates the database, curl transport, continuation store, provider adapters,
  router, gateway, executor, and service on a bootstrap worker;
- posts all SQLite/provider work to the serialized executor;
- converts owned application snapshots into small Qt values;
- uses queued `QMetaObject` delivery for GUI-thread commits.

QML contains no network, database, provider, or cursor behavior.

The adaptive shell includes identity input, Following/Trending selection,
channel navigation, a bounded feed, cast/profile detail, followers/following,
status/retry/offline/stale/loading states, and pagination. `DESIGN.md`,
`UX-CONTRACT.md`, and `ui/theme/Theme.qml` define a daylight field-console visual
system shared across compact, standard, and wide layouts.

## Boundary preservation

- Hypersnap remains opt-in and is named only inside its adapter or the explicit
  executable composition root.
- The provider-free core, minimal product composition, and strict self-hosted
  deployment do not link the adapter.
- Raw cursors remain in provider-private continuation records; Presentation and
  Application see only opaque application tokens.
- Qt remains absent from Foundation, Domain, Capabilities, Local State,
  Application, Routing, Execution, Sync, Storage, and the provider-neutral
  Transport port. Phase 4.5B adds one explicit Qt-linked outer implementation
  under `src/transport/qt_network/`.
- The architecture guard explicitly recognizes `ui/mobile/main.cpp` as the
  executable composition root; it does not broaden provider-name allowances for
  Application, Presentation, or QML.

## Portability fixes made before Phase 4 implementation

The mandatory clean baseline exposed two current-toolchain issues:

1. Boost 1.92 no longer provides `boost::json::error_code`; the provider codec
   now uses standard `std::error_code`, covered by the malformed-JSON contract.
2. Qt 6.11's macOS `offscreen` plugin emits a generic-font alias warning before
   project QML loads. Warning-fatal smoke uses Qt's `minimal` platform plugin on
   Apple; rendered screenshot capture still uses `offscreen` without treating
   the external startup diagnostic as project output.

Neither correction changes public architecture or suppresses compiler/QML
diagnostics.

## Deliberate Phase 4 limits

- The worker topology is serialized rather than a speculative task graph.
- Blocking provider calls cannot be forcibly interrupted; request cancellation
  suppresses later observer delivery.
- Realtime subscriptions, durable retry/backoff scheduling, writes/outbox,
  reactions, signing, wallet, Mini Apps, chat, and derived ranking remain out of
  scope.
- Feed delegates use ordinary Qt Quick items. A custom QSG renderer remains a
  profiling-led future option, not a Phase 4 assumption.

## Acceptance

Detailed configure/build/test and visual evidence is recorded in
`PHASE_4_VERIFICATION.md`. All required profiles pass on the actual Apple
Silicon/Homebrew Clang 22/Qt 6.11.1 environment.
