# GenyCaster documentation

The project is organized around a small collection of maintained product and engineering references. Each document explains an enduring responsibility; past milestones and one-off verification reports are not part of this documentation set.

## Start here

| Guide | Audience | Purpose |
|---|---|---|
| [Product features](FEATURES.md) | Everyone | What the client supports, including i18n/l10n regional personalization, Mini Apps, Widgets, Native Apps and GENY |
| [Architecture](ARCHITECTURE.md) | Everyone / developers | Understand the shared core, local state, routes and signer boundaries |
| [Authentication and signing](08-security-auth-and-signing.md) | Developers / security reviewers | Independent identity seal, social signer, cloud-device authority, lifecycle and acceptance gates |
| [OpenProof & server services](OPENPROOF_AND_SERVICES.md) | Developers / operators | Identity `opp`, verified Farcaster connections, GenyCaster PostgreSQL and service trust boundaries |
| [Platform properties](../properties/README.md) | Developers / release maintainers | Platform-specific deployment metadata and icons without splitting the shared client |
| [Development roadmap](12-roadmap.md) | Everyone / contributors | Phase-by-phase status, implementation evidence and acceptance gates |
| [Performance](PERFORMANCE.md) | Developers | How launch, scrolling, media and resource use will be measured |
| [Build and CI](../ci/README.md) | Contributors | Toolchains, target profiles, verification and packaging |
| [Self-hosted nodes](SELF_HOSTED_NODES.md) | Operators | Configure and verify compatible independent protocol nodes |
| [Security](../SECURITY.md) | Everyone | Trust boundaries and private vulnerability reporting |
| [Contributing](../CONTRIBUTING.md) | Contributors | Development standards and pull request expectations |

## Core architecture contracts

- [Domain and capability model](00-architecture-overview.md), [invariants](01-non-negotiable-invariants.md) and [capability taxonomy](02-capability-taxonomy.md)
- [C++ module dependencies](03-dependency-and-module-rules.md), [provider routing](04-provider-boundary-and-routing.md) and [local-first state](05-local-first-state-and-sync.md)
- [Feature isolation](06-feature-compartmentalization.md), [domain normalization](07-domain-model-and-normalization.md) and [signer security](08-security-auth-and-signing.md)
- [Derived data ownership](09-derived-infrastructure.md), [build profiles](10-build-deployment-profiles.md), [testing contracts](11-testing-and-architecture-guards.md) and [Qt runtime](14-performance-and-ui-runtime.md)
- [Architecture Decision Records](adr/) and [source ownership](17-repository-layout.md)

## Technical runtime guides

- [C++ undefined-behavior and lifetime safety audit](15-cpp-undefined-behavior-audit.md) — maintained evidence register, sanitizer coverage and unresolved risks
- [Media upload lifecycle](44-media-upload-gateway.md)
- [Direct protocol reads](44-raw-protocol-read-adapter.md)
- [Browser/WebAssembly composition](webassembly.md), [startup/persistence](wasm-startup-persistence.md) and [static delivery](wasm-static-delivery.md)
- [Browser account recovery](farcaster-recovery-wasm.md)

All deployment addresses shown in guides are **example endpoints** (such as `https://app.example.com` or `https://node.example.com`). Actual hosts, credentials and environment-specific defaults belong in private operator configuration and the appropriate runtime composition, not copy-and-paste documentation commands.

For present-day behavior, prefer source code and tests over historical assumptions. Updates to features should update [Features](FEATURES.md); architectural changes belong in the relevant contract/ADR; incomplete work belongs in the [Roadmap](12-roadmap.md).
