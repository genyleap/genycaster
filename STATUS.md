# GenyCaster — engineering status

_Last reviewed: 2026-11-06. This is a source-tree status overview, **not** a certification that every release artifact and platform passes live acceptance._

## Public-facing release identity

| Attribute | Current source configuration |
|---|---|
| Product | GenyCaster |
| Release version | **v1.0.0-rc1** (from top-level `CMakeLists.txt`) |
| Product stage | **Beta**, under active development |
| Primary implementation | **C++26** named modules and Qt Quick |
| Reference UI generation | **Qt 6.12** |
| CMake baseline | **3.30+** |
| Licensing | **GPL-3.0-only** for original source; third-party assets retain their own terms |

The RC suffix labels the packaging channel; Beta describes user-facing readiness. Neither label asserts a third-party security audit, full cross-platform certification or equivalent feature support on every runtime.

## What is present in source

- Provider-neutral Farcaster domain and capability-driven routing with a HyperSnap-preferred policy and configurable Snapchain/compatible routes where supported.
- Cache-first social reads and incremental reconciliation, SQLite persistence and durable write/outbox mechanisms.
- Qt Quick desktop/mobile interfaces, WebAssembly runtime support and C++ presentation integration.
- Social timelines, discovery, profiles, channels, reactions, casts, replies, quotes and media/preview components.
- Account/session and protocol-signer boundaries with OS-specific native secure-vault integrations and distinct browser flows.
- Mini App surfaces, optional product-specific account metadata, bookmarks, profile extensions and media gateway capabilities.
- Twelve translation catalogs and RTL-aware UI/content handling.

These are **source capabilities**, not an unconditional runtime feature matrix. Some are optional by build or provider; some require operator-side services and user-approved signer/wallet permissions.

## What remains gated

- Independent clean build and runtime acceptance on every target architecture and supported operating-system version.
- Browser Mini App and wallet origin isolation, signer consent/revocation and protocol-specific security tests.
- Provider failover / ambiguous-write reconciliation under network fault injection.
- Memory, frame-time, startup and battery benchmarks with raw reproducible evidence; no competitive performance claim is established.
- License/notices review for all redistributed third-party assets and dependencies.
- Current localization, QML, media and architecture regression test suites; individual successful commands are not proof of complete CI green.

The source alone cannot establish these release gates. See [Performance](docs/PERFORMANCE.md), [Security](SECURITY.md), [CI](ci/README.md) and the [Build profiles](docs/10-build-deployment-profiles.md).

## Architecture

- **Local-first:** the UI consumes cached canonical state before background read refresh.
- **Provider-neutral:** capability choice is per operation, not one global provider.
- **Separate trust:** session identity, custody authorization, Ed25519 protocol signing and submission transport are distinct.
- **Optional features:** gateway/extension surfaces do not redefine the Farcaster domain.
- **Platform honest:** native secure storage and browser wallet APIs have distinct guarantees and constraints.

See [Architecture at a glance](README.md#architecture-at-a-glance), [Architecture detail](docs/ARCHITECTURE.md), [ADRs](docs/adr/) and [AGENTS.md](AGENTS.md).

## Verification

Use `ci/verify_ci.py` and `ci/check_architecture.py` for the documented source checks, plus the applicable native build, Qt tests and regression suites. Some target-specific or historical tests may require tools unavailable on another developer machine. Every status report should record exact commands, commits and target-specific passes/failures rather than silently generalize.

GitHub Actions is **manually driven** for ordinary development and runs a controlled release matrix for validated tags; a push to `main` is not an app release. Full instructions: [CI README](ci/README.md).

## Development continuity

The old one-off phase/review, handoff and prototype files are no longer part
of the current tree. Their actionable requirements were consolidated into
[Development Stages & Acceptance Gates](docs/12-roadmap.md). Numbered
architectural reference documents and ADRs remain, but historical verification
statements are **not** current release/build proof. Use live source, CMake,
the active architecture documents and platform-specific test evidence.
