# GenyCaster — Development Roadmap

**Status snapshot:** November 2026 · **Product:** Beta · **Source channel:** v1.0.0-rc1

This page tracks **implementation** independently from **qualification and release readiness**. Features that already work are not labeled “In progress” merely because cross-platform hardening or a broader release test matrix remains. A source implementation does not imply a released or fully tested capability on every platform.

## Status key

| Status | Definition |
|---|---|
| **Complete (source scope)** | The phase's stated baseline is implemented in source with supporting code/tests. This does **not** certify all platforms, live services, signer security or release artifacts. |
| **In progress** | Part of the phase's core implementation scope is incomplete or still gated, not merely awaiting final release tests. |
| **Planned** | Requirements and acceptance criteria exist, but the full feature or validation program has not been completed. |
| **Concept** | An exploration or placeholder only; not advertised as an available capability. |

## At a glance

| Phase | Scope | Source implementation | Source evidence | Remaining qualification / release work |
|---|---|---|---|---|
| **01** | Shared C++ core and architectural boundaries | **Complete** | Named modules, domain/capability ports, build and architecture checks | Preserve dependency and ownership rules |
| **02** | Local-first reads and multi-node routing | **Complete** | SQLite state, read adapters, HyperSnap-preferred routing, selectable node profiles | Offline, cursor and multi-node fault-injection acceptance |
| **03** | Identity, signer and social-write pipeline | **Complete** | OpenProof identity, session/signer boundaries, canonical writes and durable outbox | Authorization/revocation review, real-node retries and vault/device qualification |
| **04** | Social interface, media and localization | **Complete** | Cast feeds, threads, profiles, rich media, adaptive Qt Quick, i18n/l10n | Navigation/media stability, accessibility, device/browser and GPU frame-time acceptance |
| **05** | Mini Apps and wallet integration | **In progress** | Discovery, embedded host and runtime-specific wallet adapters | Consent isolation, expiry/revocation and cross-platform security parity |
| **06** | Widgets, GENY integration and account extras | **Complete** | Reorderable widgets, assets, GenyPulse, Geny Pro service policy and app-owned state | Live service verification, source accuracy and entitlement lifecycle tests |
| **07** | Native Apps catalog/runtime | **Concept** | Navigation entry and Coming Soon view | Design sandbox, permissions, install/update model |
| **08** | Independent nodes and derived discovery | **In progress** | Raw reads, bounded Following, node profiles and probes | Self-hosted read/write coverage and provenance at scale |
| **09** | Performance and platform qualification | **Planned** | Build targets and measurement methodology exist | Publish reproducible performance and device-level release matrix |
| **10** | Public-source and release readiness | **In progress** | Documentation, community policies, source CI and release tooling | License/SBOM, security review and reproducible target artifacts |
| **11** | Optional future protocol extensions | **Concept** | Architectural extension points | Capability proposal, ADR, threat model and tests |

### Phase 01 — Core architecture

**Complete · source foundation.** A C++26 application core defines protocol-neutral domain entities, capability contracts, commands, queries, routing and persistence ports. Qt Quick is the presentation layer; protocol adapters and storage implementations sit behind explicit boundaries.

**Evidence:** `src/`, `CMakePresets.json`, `ci/check_architecture.py` and [architecture decisions](adr/). **Keep passing:** provider-free core builds, dependency checks and module/toolchain verification.

### Phase 02 — Local-first data and protocol routes

**Complete — source implementation.** Cached canonical snapshots are available independently of a fresh network response. Read routes prefer HyperSnap by default while permitting Snapchain/compatible nodes for supported operations; local-derived views explicitly mark incomplete coverage.

**Remaining:** long-feed performance, source-affine pagination, offline/restart behavior, provider health, account isolation and route switching under failure. **Qualification gate:** changing eligible providers does not mix continuation tokens or corrupt a cached feed, and no partial result is claimed as globally complete.

### Phase 03 — Account identity and signed interaction

**Complete — baseline source implementation.** Authentication, custody approval, protocol signers and provider submission are separate authorities. Canonical messages and an account-scoped durable outbox support the write pipeline.

**Remaining:** platform vault acceptance, explicit signer scope/expiry/revocation, approval on sensitive operations, restart and ambiguous-outcome reconciliation. **Security/qualification gate:** no plaintext-key fallback, unauthorized signature or duplicate write on network retries is possible under the verified threat model.

### Phase 04 — Native social experience

**Complete — source feature set.** Qt Quick implements Home, Explorer, public Channels directory, joined-channel discovery, scoped channel reading, casts, conversations, profiles, rich media, notifications, composer, search, appearance and localization. Public channel browsing is separate from the signed composer picker; publishing to a channel still requires provider-verified membership. Channel join/leave mutations are not advertised without a supported authenticated protocol write.

**Remaining:** reliable navigation back to the exact cast and scroll offset, steady media rendering, touch/keyboard accessibility, RTL support and real GPU frame-time acceptance. **Qualification gate:** representative device/browser tests demonstrate stable UX and measured frame performance.

### Phase 05 — Mini Apps and wallet permissions

**In progress.** Mini App discovery, hosted sessions and runtime-specific wallet integrations exist in source.

**Remaining:** separate account disclosure from signing, origin/session/account/chain isolation, consent expiry, navigation/disconnect revocation, hostile content handling and native versus WASM parity tests. **Done when:** one Mini App cannot inherit another's approval and reconnecting does not silently authorize signing.

### Phase 06 — Context widgets and GENY product services

**Complete — source feature set.** Users can reorder built-in context widgets: assets, GenyPulse activity, calendar, market and suggestions. GENY balances on Base can be displayed for verified Farcaster wallets. The account service evaluates GENY Pro eligibility server-side from verified wallet balances, not from a local UI indicator.

**Remaining:** data-source resilience, cross-device product-state correctness, server authorization and entitlement lifecycle tests. **Operational qualification gate:** unavailable data is labeled correctly and no unverified balance, badge or client-supplied claim upgrades permissions. GENY functionality is optional and does not change the Farcaster domain.

### Phase 07 — Native Apps

**Concept.** The application displays a Native Apps category with a Coming Soon placeholder. It does **not** currently provide a verified native app marketplace, package format or sandboxed host.

**Before implementation:** specify packaging, code-signing, permission model, platform boundaries, lifecycle, discovery, distribution and updates. The status cannot change to In progress until an approved capability/ADR exists.

### Phase 08 — Independent nodes and discovery

**In progress.** Node profiles, raw protocol reads, capability probes and bounded local/remote derived feeds are implemented in source.

**Remaining:** independent read/write operation tests, complete network coverage limits, self-hosted setup, provider provenance and safe switching. **Done when:** every advertised self-hosted capability is demonstrated without a hidden dependency on a managed index, and writing is tested against a compatible validator.

### Phase 09 — Performance and platform verification

**Planned acceptance program.** The repository has macOS, Windows, Linux, Android, iOS and WebAssembly build targets, but their existence is not certification of each distributable.

**Measure:** cold launch, cached warm launch, time to first cast/media, 100-cast frame time p50/p95/p99, memory, return-to-cast accuracy, Mini App startup and WASM download/compile time. **Done when:** real-device results, identical workloads, exact release identifiers and evidence are published following [Performance](PERFORMANCE.md).

### Phase 10 — Public-source and release engineering

**In progress.** Public introduction, security and contribution policies, architecture, docs index and source-level tests are present.

**Remaining:** delete non-core/one-off reports, replace real deployment URLs in *documentation examples* with reserved example domains, verify dependencies and notices, scan reachable Git history for secrets, and pass release/package acceptance. **Done when:** a clean repository, complete licensing review and reproducible target artifacts are demonstrable.

### Phase 11 — Optional future capabilities

**Concept.** New protocol features, messaging or real-time experiences require a separate capability contract, privacy/security model and ADR before public release promises. Cryptographic functionality must use reviewed protocols rather than custom schemes.

## Moving a phase forward

1. **Concept → Planned:** scope, ownership, user value and testable acceptance criteria are approved.
2. **Planned → In progress:** owning code and regression tests exist; incomplete paths are visibly gated.
3. **In progress → Complete (source scope):** the described baseline implementation and its applicable source tests exist. Open security, operational or platform-qualification work remains explicitly listed rather than silently implying release readiness.
4. **Complete → Released:** separate platform-specific packaging, signing, deployment, security and operational acceptance checks succeed. This is a release decision, not an automatic outcome of merging code.

No percentages or completion dates are implied without a traceable measurement or delivery plan. Status changes require evidence, not only a UI screenshot.

See [Features](FEATURES.md), [Architecture](ARCHITECTURE.md), [Security](../SECURITY.md) and [Build/CI](../ci/README.md).
