# 17 — Repository Layout and Ownership Map

This document is the practical ownership map for engineers and coding agents.

## Root

```text
.
├── AGENTS.md
├── CMakeLists.txt
├── CMakePresets.json
├── PHASE_0_REVIEW.md
├── PHASE_1_REVIEW.md
├── README.md
├── cmake/
├── docs/
├── src/
├── tests/
├── tools/
└── ui/
```

## Core ownership

### `src/foundation/`

Low-level project primitives that do not know Farcaster product features or providers.

Allowed dependencies: standard library only unless an ADR explicitly expands this.

Current modules:

```text
farcaster.foundation.error
farcaster.foundation.result
farcaster.foundation.time
```

### `src/domain/`

Canonical Farcaster/client entities and value objects.

Allowed dependencies:

```text
foundation
```

Forbidden:

```text
Qt / QML
provider SDKs
HTTP
SQLite
JSON DTOs
managed backend concepts
```

### `src/capabilities/`

Provider-neutral capability metadata and, in later phases, capability contracts.

Allowed dependencies:

```text
foundation
domain
```

Provider implementations never live here.

### `src/application/`

Use cases, orchestration policy, feature/deployment intent and authoritative application behavior.

Allowed dependencies:

```text
foundation
domain
capabilities
application ports
local-state ports
```

Forbidden:

```text
provider modules
provider DTOs
QML
raw database calls
raw HTTP calls
```

## Phase 2 infrastructure ownership

### `src/execution/`

RAII execution mechanisms implementing Application scheduling ports. Phase 4
owns serialized and bounded-pool executor implementations; it contains no provider or Qt behavior.

### `src/routing/`

Capability registry, health snapshots, policy engine, route selection and the
provider-neutral routed read gateway. Concrete source objects are borrowed only
through executable composition bindings.

### `src/storage/`

SQLite/WAL database, migrations, canonical entity persistence, timeline
indexes, application-safe read continuation state, and social-list ordering.

### `src/transport/`

Provider-neutral transport ports and concrete transport mechanisms. The base
`farcaster.transport.http` port has no Qt dependency. Its libcurl and Qt Network
implementations are separate outer targets; the Qt implementation is isolated
under `src/transport/qt_network/`.

### `src/sync/`

Incremental synchronization, reconciliation, stale-state policy, background refresh and later durable outbox orchestration.

These packages may implement application/capability ports but may not redefine domain semantics.

## Providers

```text
src/providers/
├── hypersnap/
├── self_hosted/
└── compatibility/
```

Provider-specific details terminate here:

- URLs;
- authentication headers/tokens;
- DTOs;
- JSON schemas;
- cursor formats;
- rate-limit headers;
- provider error codes;
- SDK objects;
- backend-only semantics.

A provider adapter may depend inward on capability/domain contracts. The reverse dependency is forbidden.

## Optional product features

```text
src/features/
├── social/
├── feed/
├── channels/
├── miniapps/
├── chat/
├── search/
├── notifications/
└── wallet/
```

Every feature is optional from the core's perspective.

No empty feature targets exist. A feature directory/target is created only when
it owns behavior, a public contract, or independent tests. Current product
availability remains compile-time composition metadata in Application.

## Presentation and rendering

### `src/presentation/`

C++ presentation adapters and explicit Qt/MOC boundaries.

### `ui/`

QML application shell, mobile/desktop layouts, reusable visual components, theme and motion system.

QML never performs provider/network/database work directly.

### `src/render/`

Reserved performance-sensitive rendering paths:

```text
feed/
chat/
text/
media/
```

Do not introduce custom scene-graph rendering merely because it is possible. Benchmark first. The architecture keeps the boundary available so a hot path can later move from ordinary QML delegates to `QQuickItem`/QSG rendering without changing domain/application logic.

## Platform adapters

```text
src/platform/
├── ios/
├── android/
├── macos/
├── windows/
└── linux/
```

Own thin platform-specific integrations such as secure credential storage, passkeys, notifications, share sheets, lifecycle hooks and background execution.
HTTP clients and other transport implementations do not belong here merely
because their library integrates with an operating system.

## Tests

```text
tests/
├── architecture/
├── unit/
├── integration/
└── contract/
```

Test intent:

- `architecture`: dependency/provider leakage and build-profile guards;
- `unit`: deterministic module behavior;
- `integration`: storage/router/sync/provider composition;
- `contract`: provider adapter behavior against captured fixtures and provider contracts.

## Dependency summary

```text
                         ui / presentation
                                │
                                ▼
optional features ───────→ application
                                │
                                ▼
                           capabilities
                                │
                                ▼
                              domain
                                │
                                ▼
                            foundation

routing/storage/sync ────────────┘  (implement inward-facing ports)
providers ───────────────────────┘  (implement capabilities)
platform adapters ───────────────┘  (implement application ports)
```

No arrow is allowed from the core toward a concrete provider.
