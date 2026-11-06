# GenyCaster documentation

This is the navigation page for the current public technical documentation. Start with [the project README](../README.md) for product capabilities and the architecture diagram.

## Essential references

| Guide | Read it for |
|---|---|
| [Architecture](ARCHITECTURE.md) | Canonical system boundaries, routing, trust and signed-write lifecycle |
| [Performance](PERFORMANCE.md) | Repeatable benchmarking methodology and comparison evidence policy |
| [Current status](../STATUS.md) | Source-tree capabilities, target acceptance and beta limitations |
| [Build and CI](../ci/README.md) | Pinned multi-platform build configuration, verification and delivery |
| [WebAssembly](webassembly.md) | Browser composition, runtime differences and deployment |
| [Security](../SECURITY.md) | Private vulnerability reporting and security boundaries |
| [Contributing](../CONTRIBUTING.md) | Engineering rules, setup, tests and pull requests |

## Protocol and design contracts

| Topic | Reference |
|---|---|
| Core architecture | [Architecture Overview](00-architecture-overview.md), [Non-negotiable Invariants](01-non-negotiable-invariants.md) |
| Capability vocabulary | [Taxonomy](02-capability-taxonomy.md), [Provider Routing](04-provider-boundary-and-routing.md) |
| State and sync | [Local-first State](05-local-first-state-and-sync.md), [Read Pipeline](22-phase4-local-first-read-pipeline.md) |
| Authorization and trust | [Identity, Authentication and Signing](08-security-auth-and-signing.md) |
| Engineering direction | [Dependencies](03-dependency-and-module-rules.md), [UI Runtime](14-performance-and-ui-runtime.md) |
| Build variants | [Build Profiles](10-build-deployment-profiles.md) |
| Architectural decisions | [ADRs](adr/) |

## Historical implementation documents

The numbered phase notes, review and verification reports, and date-stamped fixes document **how the implementation evolved**. They are preserved for engineering traceability but are not always valid for today's Qt version, release channel, deployment routes or platform support.

For current facts, use the top-level `CMakeLists.txt`, current code, [STATUS.md](../STATUS.md) and validated release artifacts. Never use an old phase report as proof that a current target build or security audit passed.

## Documentation maintenance

- Keep the [README](../README.md) concise, product-focused and honest about maturity.
- Record architecture and trust-boundary changes in [ARCHITECTURE.md](ARCHITECTURE.md) and the relevant ADR.
- Only publish numerical performance comparisons when the [benchmark protocol](PERFORMANCE.md) and raw evidence are available.
- Update user-visible feature claims when an option becomes available, changes its approval model or is dropped on a platform.
- Avoid duplicating release-version metadata; version identity is defined in CMake and release notes are canonical in `release-notes/`.

Corrections and improvements are welcome through [Contributing](../CONTRIBUTING.md).
