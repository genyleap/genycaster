# GenyCaster architecture

> **Read this after the [README](../README.md).** This document describes architectural boundaries and the implemented composition model. Some optional integrations and target-specific capabilities remain in beta; a diagram is not a claim that every adapter is available on every platform.

## 1. System map

```mermaid
flowchart TD
    subgraph Experience["Presentation boundary"]
        QML["Qt Quick / QML<br/>surfaces, input, layout"]
        P["C++ presentation adapters<br/>view models and signals"]
        QML <--> P
    end

    subgraph Core["Shared C++ application core"]
        A["Application<br/>use cases · queries · commands"]
        D["Domain<br/>canonical Farcaster entities"]
        C["Capabilities<br/>operation-level contracts"]
        R["Routing & policy<br/>eligible provider + health"]
        A --> D
        A --> C
        C --> R
    end

    subgraph State["Local durable state"]
        S[("SQLite<br/>cache · continuity · outbox")]
        E["Background executors<br/>sync · reconciliation"]
        S <--> E
    end

    subgraph Adapters["Infrastructure and protocol adapters"]
        H["HyperSnap<br/>default preference"]
        N["Snapchain / nodes<br/>explicit selection"]
        X["Derived / compatibility<br/>optional"]
        R --> H
        R --> N
        R --> X
    end

    P <--> A
    A <--> S
    E <--> R
    E --> A

    classDef core fill:#232323,color:#fff,stroke:#666;
    classDef storage fill:#eeeeee,color:#1a1a1a,stroke:#888;
    class A,D,C,R core;
    class S,E storage;
```

The primary dependency direction is **Presentation → Application → Domain/capability contracts**. Provider implementations, SQLite, transport libraries, browser APIs and Qt belong at outer boundaries. They do not become domain types.

Relevant source locations: `src/domain/`, `src/application/`, `src/capabilities/`, `src/providers/`, `src/presentation/`, `src/storage/`, and `ui/`. For the normative contracts, read [Architecture Overview](00-architecture-overview.md), [Dependency Rules](03-dependency-and-module-rules.md), and [ADR index](adr/).

## 2. The local-first read path

```mermaid
sequenceDiagram
    actor User
    participant UI as Qt Quick UI
    participant App as Application
    participant DB as Local state
    participant Router as Capability router
    participant Node as Selected protocol node

    User->>UI: Open feed or cast
    UI->>App: Query current view
    App->>DB: Read cached canonical snapshot
    DB-->>App: Stored state / empty state
    App-->>UI: Render immediately when cached
    App->>Router: Refresh eligible capability (async)
    Router->>Node: Network request
    Node-->>Router: Provider response
    Router-->>App: Normalized result
    App->>DB: Reconcile and persist
    DB-->>App: Committed state
    App-->>UI: Updated UI-facing snapshot
```

This is the *responsibility flow*, not a guarantee that every empty first-run screen has cached content. Offline mode retains what is available locally and does not fabricate remote state. Pagination and provider-specific cursors remain adapter-owned or are normalized into scoped application continuation state.

**Performance consequences:** native Qt UI input does not wait for a network round trip before rendering an existing local snapshot; expensive read and synchronization work is scheduled away from the GUI thread. See [Performance](PERFORMANCE.md).

## 3. A capability is not a provider

GenyCaster is **Farcaster-protocol-aware**, not locked to the protocol node selected today.

- **HyperSnap** is the preferred node/read policy in ordinary supported configurations.
- **Snapchain** and other compatible node routes are selectable where the operation and target build allow them.
- Selection is **per capability**: `cast.read`, `feed.following`, profile reads, signer registration and writes need not share one universal backend.
- Provider health, operation support, authentication requirements, error semantics and privacy/network policy determine eligibility.
- Compatibility features and derived product data are modeled as extensions; they must not redefine canonical Farcaster messages.
- Read failover is permitted only when semantics match. **Write failover is not a blind retry** because ambiguous completion can cause duplicates.

```mermaid
flowchart LR
    REQUEST["Capability + operation<br/>e.g. cast.read"] --> ELIGIBLE{"Supported and<br/>policy-eligible?"}
    ELIGIBLE --> ROUTE["Priority + health<br/>+ explicit node choice"]
    ROUTE --> H["HyperSnap"]
    ROUTE --> S["Snapchain / compatible"]
    ROUTE --> F["Fallback where valid"]
    H --> NORMALIZE["Canonical result"]
    S --> NORMALIZE
    F --> NORMALIZE
    NORMALIZE --> LOCAL[("Local state")]
```

The distinction between protocol operations and derived services is documented in [Capability Taxonomy](02-capability-taxonomy.md), [Routing](04-provider-boundary-and-routing.md), and [Provider Affinity ADR](adr/0007-provider-affine-continuations.md). A self-hosted deployment can compose a different set of eligible adapters; support is not assumed for features whose adapters are missing.

## 4. Signed writes and trust boundaries

```mermaid
flowchart TD
    INTENT["Authenticated UI intent"] --> AUTH{"Session + signer<br/>authority ready?"}
    AUTH -->|No| DENY["Fail closed / request approval"]
    AUTH -->|Yes| CANON["Canonical message construction"]
    CANON --> SIGN["Scoped signer boundary"]
    SIGN --> OUTBOX[("Durable exact-envelope outbox")]
    OUTBOX --> SUBMIT["Selected write transport"]
    SUBMIT --> RECON["Confirm / reconcile ambiguous result"]
    RECON --> OUTBOX
    RECON --> VIEW["Canonical local projection"]
```

**Separate trust authorities:** four distinct identities/privileges must not be conflated:

| Boundary | Responsibility | Must not imply |
|---|---|---|
| Account authentication | Session identity and a linked Farcaster FID | Permission to sign arbitrary protocol messages |
| Custody wallet | Approval of scoped signer registration where required | A reusable unscoped Mini App wallet grant |
| Protocol signer | Canonical signed Farcaster messages with authorized scopes | Authority for unrelated networks or dApps |
| Provider transport | Submit, read back and reconcile messages | Control of the private signer key |

Native signer material belongs in an OS-backed secure vault. Private key bytes must not enter QML or ordinary SQLite state. The WebAssembly target has a **different browser capability model**: compiled-out native vault capabilities cannot be inferred from the desktop implementation. Mini Apps require origin-scoped permission checks and browser-specific validation. Never substitute local storage or a persistent "approved" UI flag for cryptographic authorization.

Related references: [Security and Signing](08-security-auth-and-signing.md), [Outbox ADR](adr/0010-durable-local-outbox.md), [Write Idempotency ADR](adr/0011-write-idempotency-strategy.md), [SECURITY.md](../SECURITY.md).

## 5. Rendering and threading

```text
Qt GUI thread             input · layout · small presentation updates
Qt Scene Graph / RHI      visual scene rendering, GPU backend where available
Background executors      network · cache I/O · normalization · reconciliation
OS secure storage         signer material; isolated behind a native adapter
```

Qt Quick's graphics API is selected for the platform and driver (Metal, Direct3D, OpenGL, and WebGL-backed browser contexts as applicable). `QT_QUICK_BACKEND=software` appears in specific headless test environment settings in `ui/CMakeLists.txt`; this is not a production-wide renderer assignment. Do not claim hardware acceleration solely because the build has WebGL support: inspect runtime `QQuickWindow::rendererInterface()->graphicsApi()` and collect frame-time measurements.

**UI invariants:** QML does not perform database I/O, cryptographic signing, provider HTTP/JSON parsing or large model rebuilds on the presentation hot path. Expensive media admission and cached timeline reconciliation are asynchronous. [UI performance contract](14-performance-and-ui-runtime.md).

## 6. Optional deployment-side services

The `services/` tree contains independent account/product and gateway boundaries: account synchronization for GenyCaster-specific metadata, media handling, authentication integrations, and a Snapchain gateway. These services are **not** the canonical Farcaster protocol itself and must not be represented as required for ordinary provider-independent domain logic.

A source-code build is not equivalent to an authorized production deployment. Credentials, App-FID attestation keys, database access and service-side controls remain operator-managed. Review individual service READMEs and [SECURITY.md](../SECURITY.md) before exposing endpoints.

## 7. Repository guide

| Area | Start with |
|---|---|
| Domain, types, protocol | `src/domain/`, `src/capabilities/` |
| Commands, read models and sync | `src/application/` |
| Network routing and adapters | `src/providers/`, `src/transport/` |
| Persistence and migrations | `src/storage/` |
| Qt UI / native entry points | `ui/components/`, `ui/mobile/` |
| WebAssembly runtime | `ui/wasm/`, [WebAssembly](webassembly.md) |
| Build configurations | `CMakePresets.json`, [Build profiles](10-build-deployment-profiles.md) |
| Tests and verification | `tests/`, [CI](../ci/README.md) |

## 8. Non-negotiable principles

1. A provider can be replaced without redesigning the domain/application/UI contract.
2. Canonical protocol state and derived product state are separate.
3. A valid cached view renders without requiring a fresh network response.
4. Sign-in, wallet authorization and canonical signer provisioning are different operations.
5. Outgoing signed writes are scoped, durable and reconciled after ambiguous results.
6. Optional features do not become mandatory dependencies of core social capabilities.
7. A platform may **fail closed** on unsupported security capabilities rather than weaken their contract.
8. Claims of performance or security require measured evidence, platform-specific tests and explicit scope.

The detailed normative documents in `docs/` and `AGENTS.md` supersede simplified overview diagrams wherever implementation conditions or platform constraints differ.
