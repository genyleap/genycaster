# Contributing to GenyCaster

Thank you for helping build a native, local-first and provider-independent Farcaster client. This is a beta project with security-sensitive identity, signing and browser surfaces; small, verified changes are preferred.

Read the [README](README.md), [Architecture](docs/ARCHITECTURE.md), and [Security policy](SECURITY.md) before opening a pull request. For C++ and Qt work, also read [AGENTS.md](AGENTS.md), the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines), and the pinned engineering reference documented in AGENTS.md.

## Before you start

1. Search existing issues and pull requests to avoid duplicate work. For major protocol/API changes, discuss the design and its trust boundaries before implementation.
2. Reproduce bugs on a known commit and platform; keep a minimal test case that fails before the fix.
3. Keep an individual pull request focused on one behavior, architectural boundary or documentation topic.
4. Protect public APIs and data migrations. State compatibility and rollback implications if changing persistence or protocol contracts.
5. **Do not include real user data, keys, credentials, wallet recovery phrases or production access material.**

## Toolchains and configuration

- CMake **3.30+**, Ninja and a Modern C++ toolchain. The codebase uses **C++23 and newer features**, while the current named-module reference build explicitly requires a **C++26-capable compiler**.
- Qt **6.12** and matching host tools for the graphical UI.
- Target-specific dependencies (Boost, OpenSSL, QtKeychain and others) as required by the selected CMake profile.
- Qt WebAssembly requires a **matching** Qt 6.12 + Emscripten kit; Android and iOS use their respective SDK/NDK environments.
- GitHub Actions uses pinned environments; a developer's local compiler may differ. Do not assume that configuring a generic UI preset installs the required Qt kit.

For the headless core:

```sh
cmake --preset core-debug
cmake --build --preset core-debug
ctest --preset core-debug --output-on-failure
```

For a compatible local Qt installation:

```sh
cmake --preset ui-debug
cmake --build --preset ui-debug
ctest --preset ui-debug --output-on-failure
```

The `ui-qt612-debug` preset targets the documented Linux reference environment; it may refer to paths that are **not** present on your machine. See [Build profiles](docs/10-build-deployment-profiles.md), [CI](ci/README.md), and [WebAssembly](docs/webassembly.md) for the correct prerequisites and platform gates.

Run applicable source and regression checks for the part you changed:

```sh
python3 ci/check_architecture.py
python3 ci/verify_ci.py
node --test tests/wasm/*.test.mjs
```

The full WASM regression collection may require a compatible Node runtime and may contain target-specific tests. Do not claim all tests passed when only a subset ran. For Qt changes, run QML lint, relevant native UI tests, and ideally a clean target build; note missing toolchain coverage in the pull request.

## Engineering invariants

- **Provider-neutral core.** Canonical domain/application types do not carry HyperSnap, Snapchain or managed-backend DTOs. Select the route at an operation/capability boundary.
- **Local-first state.** Queries render local snapshots; remote responses reconcile into state. Do not introduce synchronous HTTP, DB or media decoding on the UI thread.
- **Signer isolation.** Application session, wallet custody, local protocol signer and gateway are different authorities. Add signer-related tests for scope, origin, replay and revocation.
- **Durable writes.** Preserve idempotency, exact signed envelopes and reconcile-before-retry logic. Never blindly repeat a write through another provider.
- **Compartmentalization.** Optional wallet, messaging, Mini App, discovery or product-extension features do not become required by the core.
- **Native and web parity without fake equivalence.** Each target keeps its actual security/runtime limits. A browser cannot be treated as a native Keychain host.

Use RAII and explicit ownership in C++; prefer `std::expected` for recoverable errors, named modules for project-owned interfaces and the repository's established Qt/QML conventions. Write comments to describe the actual runtime/capability boundary: a WebAssembly/browser behavior should not be labeled iOS or macOS merely because it was discovered there. Preserve OS names when they identify a real platform-specific API or constraint. Separate independently documented declarations with exactly one blank line; never put a blank line between an API's Doxygen block and the declaration itself. Keep enum lists and access labels compact. Each `.cppm` and `.hpp` starts with a Doxygen file header identifying its repository-relative `@file`, specific `@brief` and `@details`, credited `@author`, original Git introduction date (`@since`), copyright and the applicable license. Generated third-party assets must preserve upstream attribution and license; update their generator as well. The architecture guard validates both file extensions. Add or update an ADR when changing an architectural invariant. Do not bypass source guards or weaken tests simply to make CI green.

## Pull request checklist

- [ ] The PR has a clear problem statement, scope and before/after behavior.
- [ ] No unrelated refactor or generated artifact is included.
- [ ] Tests cover the bug or new behavior (with exact commands/results).
- [ ] The affected target(s), toolchain version(s) and limitations are stated.
- [ ] No plaintext secret, private user data or privileged endpoint is exposed.
- [ ] New UI text has translation IDs and a suitable accessibility label.
- [ ] Security, authorization, storage and provider changes explain their trust boundaries.
- [ ] New dependencies/assets have reviewed licenses and corresponding notices.
- [ ] README/Architecture/Performance documentation is updated if the public contract changed.
- [ ] No speed, security or cross-platform claim is made without appropriate evidence.

## Benchmark contributions

Performance optimizations are encouraged, but do not submit unsourced "X% faster" claims. Follow [docs/PERFORMANCE.md](docs/PERFORMANCE.md), submit raw measurements, tested commits, hardware/runtime configuration and comparable workloads. A useful performance PR includes both a regression test and a profiler trace or repeatable measurement.

## Security reports

**Never file public security exploit details.** Follow [SECURITY.md](SECURITY.md) for private disclosure. The same applies to session handling, browser wallet permissions and Mini App origin isolation.

## Review and release

Maintainers review correctness, architecture, user experience, performance, licensing and security. A merge to `main` is not a public release. Cross-platform builds and the release process are separately controlled through [.github/workflows/ci.yml](.github/workflows/ci.yml) and [ci/README.md](ci/README.md).

The project uses [GPL-3.0-only](LICENSE) for its original source; contributions must be compatible with that license. Do not submit code or assets you lack the right to distribute. GenyCaster and Genyleap trademarks are not licensed automatically with the source.
