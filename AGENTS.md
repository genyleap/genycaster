# AGENTS.md — Mandatory Context for AI Coding Agents

This file is the canonical entry point for any AI coding agent working on this repository. Do not modify production code before reading it.

## External engineering authority

This repository uses the following revision of `thecompez/ai-for-modern-cpp` as a normative engineering authority:

```text
repository: https://github.com/thecompez/ai-for-modern-cpp
revision:   326cd850c2cb5e39264b7d2ccf2c29fba6627380
```

Required upstream guides for ordinary implementation work:

- `AGENTS.md`
- `docs/agent/ARCHITECTURE.md`
- `docs/agent/MODULES.md`
- `docs/agent/NAMING.md`
- `docs/agent/SYNTAX_AND_STYLE.md`
- `docs/agent/API_DESIGN.md`
- `docs/agent/ERRORS_AND_RESOURCES.md`
- `docs/agent/CMAKE_AND_TOOLCHAINS.md`
- `docs/agent/TESTING_AND_VERIFICATION.md`
- `docs/agent/QT_QUICK_UI.md` for graphical work
- `docs/agent/PROJECT_CMAKE_BASELINE.md` for Qt/CMake baseline changes

If those rules conflict with this repository's product architecture, report the conflict before editing. Do not silently invent a third policy.

## Mandatory local reading order

1. README.md — product, support scope, feature and architecture overview.
2. docs/ARCHITECTURE.md — live system boundaries, read/write pipeline and trust.
3. docs/01-non-negotiable-invariants.md — hard dependency constraints.
4. docs/03-dependency-and-module-rules.md — code module ownership.
5. docs/04-provider-boundary-and-routing.md — per-capability provider policies.
6. docs/08-security-auth-and-signing.md — session, custody and signer isolation.
7. docs/12-roadmap.md — current engineering stages and acceptance gates.
8. docs/14-performance-and-ui-runtime.md — rendering, layout and responsiveness.
9. docs/README.md — documentation index.
10. Read the relevant docs/adr/ entries and owning tests for the specific task.

Historical phase, design-prototype and handoff reports were deliberately
removed. Do not recreate them. Integrate lasting new constraints into the
canonical roadmap, architecture documents or affected ADR and tests.

## Project intent

Build a high-performance native Farcaster client in Modern C++ and Qt Quick/QML with:

- Telegram-class local-first interaction behavior;
- shared C++ domain/application/routing/sync/storage core;
- mobile-first product design and first-class adaptive desktop support;
- provider-independent Farcaster capabilities;
- Hypersnap preferred by routing policy for suitable protocol-native operations;
- compatibility providers isolated to backend-only/legacy capabilities;
- self-owned search, ranking, discovery, and indexing infrastructure;
- optional secure messaging and realtime extensions;
- explicit feature compartmentalization.

## Modern C++ contract

- The reference path is **C++26**.
- New project-owned production boundaries use named C++ modules.
- Module identities are dotted lowercase.
- Export namespaces mirror module identities exactly.
- `.cppm` contains exported declarations, concepts, lightweight templates, and small compile-time values only.
- Every `.cppm` and `.hpp` begins with a Doxygen `/*!` file header **before** `module;`, `export module`, any existing comments or `#pragma once`. Include `@file` (unique repository-relative path), specific `@brief` and `@details`, credited `@author`, and `@since` (original Git introduction date, `DD Mon YYYY`), `@copyright`, and `@license`. Project-owned code references the root GPL-3.0-only license; generated third-party data retains its own author attribution and license. Keep generators consistent with their generated headers.
- `.cpp` implementation units contain non-trivial algorithms, mutation, I/O, persistence, network, platform behavior, and private implementation.
- `.hpp` is allowed only at documented Qt MOC, C/ABI, third-party, or similar textual compatibility boundaries.
- `.h` is forbidden unless an external ecosystem requires it.
- Never use `import std;`.
- Standard headers in module units belong in the global module fragment between `module;` and the named module declaration.
- CMake module interfaces belong in `FILE_SET CXX_MODULES` and targets enable module scanning.
- Exported APIs require English Doxygen describing behavior, ownership/lifetime and recoverable failures when relevant. Describe cross-platform behavior by its actual scope (for example, Qt Quick canvas, WebAssembly, browser, native platform, touch input or virtual keyboard), not by the OS where a bug was first noticed. Name iOS, macOS, Android or a browser engine only for a genuinely platform-specific API, conditional implementation, or verified engine limitation. Use exactly one blank line before each independently documented function or member when it follows a completed declaration or definition; keep the Doxygen block immediately adjacent to the declaration it describes. Do not insert blank lines between compact enum entries or immediately after access labels (public/private/protected/signals). The architecture guard enforces this spacing in project C++ sources.
- Recoverable failures use `std::expected` through the project's result vocabulary.
- Resource ownership is deterministic RAII.
- Types/enums use PascalCase; functions/locals use lowerCamelCase; private members use `m_` + lowerCamelCase.

## Dependency direction

Stable concepts point inward; volatile mechanisms adapt to them.

```text
composition root
    ├── presentation → application
    ├── routing      → capability/provider-neutral metadata
    ├── storage      → local/sync ports
    └── providers    → capabilities/domain

application → stable contracts/domain
capabilities → domain/foundation
domain → foundation
```

Concrete provider, SQLite, Qt, OS, HTTP, JSON and SDK semantics never move inward.

## Absolute prohibitions

An agent must not introduce:

- managed-backend or `Hypersnap*` DTOs outside provider adapters;
- managed backend URLs outside provider adapters/configuration;
- a global `provider = X` application architecture;
- a monolithic API client mixing protocol, messaging, wallet, search, Mini Apps, referrals and settings;
- provider cursors in public domain/application pagination contracts;
- provider auth tokens or provider error enums in domain/application models;
- QML network, websocket or database I/O;
- Qt dependencies in core/domain modules;
- mandatory DMs/Channels/Mini Apps/Wallet/Search dependencies in the core;
- vendor-style protocol writes such as `hypersnap.createCast()` in application code;
- empty ceremonial feature libraries/directories before they own real behavior;
- `utils`, `helpers`, `common`, or `misc` dumping grounds;
- classic project header architecture as a workaround for module/toolchain issues.

## Feature implementation gate

Before implementing a feature, record:

```text
Responsibility:
Capability origin: Protocol / Derived / Compatibility / Extension / Local
Mandatory or optional:
Public contract:
Canonical data/invariants:
Dependencies:
Expected failures:
Local-state representation:
Offline behavior:
Provider policy and disablement:
Failover semantics:
Tests proving the boundary:
```

Do not create a feature target merely to reserve a name. Create the target when the feature owns real behavior, a public contract, or independent tests.

## Required agent loop

For every implementation task:

```text
inspect current tree/diff
locate owning subsystem
read implementation + tests
state assumptions
make smallest coherent change
configure
build
run tests
inspect/fix failures
review final diff
report exact verification evidence
```

No success claim is valid without actual configure/build/test evidence.

## Definition of architectural success

The architecture remains healthy only if:

- provider-free core builds;
- strict self-hosted profile builds;
- disabling optional product composition does not redesign core modules;
- domain/application/QML never link or import concrete providers;
- cached local state remains usable without network/provider availability;
- provider-affine cursors remain infrastructure-private;
- a new provider can be added without changing canonical domain models or application use cases.
