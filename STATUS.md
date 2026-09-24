# Genycaster — Current Engineering Status

_Last updated: 2026-09-24. This file is the canonical snapshot of what is in the tree now. Phase review files are historical evidence, not the source of truth for current scope._

## Product state

Genycaster is an **advanced alpha / pre-beta engineering build**. The provider-neutral/local-first core, durable write pipeline, signer boundary, routing, storage, and protocol abstractions are substantially implemented. The Qt presentation is functional and feature-rich, but presentation decomposition, platform acceptance, and security hardening still gate a production release.

Current source includes more than the Phase 5F/5G baseline described by older continuation notes: public-data adapters, rich media/upload lifecycle, Mini App hosting, data-storage controls, localization across 12 locales, visual-system iterations, feed/search previews, and current social/write flows.

## Architectural invariants

- C++26 named modules are the reference language path; module namespaces mirror module names.
- Domain/Application do not depend on concrete provider identities.
- UI/QML does not own network or database I/O.
- Local state is authoritative for rendering; network work reconciles into local state.
- Protocol writes use canonical signing, durable exact-envelope outbox storage, idempotency/reconciliation, and account-scoped optimistic state.
- Session, custody wallet, local protocol signer, and App-FID attestation are distinct authorities.
- Optional product/provider features remain compile-time composition choices.

See `AGENTS.md`, `DESIGN.md`, and `docs/` for the detailed contracts.

## Build and toolchain

- Project version: `0.4.0`.
- CMake baseline: `3.30+`.
- Shipping/reference desktop toolchain: **Clang 22 + Qt 6.11.x**.
- GCC 14 can configure core profiles but is not a release compiler for this snapshot because its C++ module serialization can hit a known internal compiler error.
- Core presets: `core-debug`, `core-release`, `strict-self-hosted`, `minimal-core`, `provider-hypersnap`, `provider-auth`, `secure-signer`.
- UI preset: `ui-debug`.
- Diagnostic presets: `core-asan`, `core-ubsan`, `core-tsan`.

## Storage and runtime boundaries

- SQLite schema migrations are implemented through **migration 23**.
- Snapchain and media gateway boundaries are separate from client credentials/provider semantics.
- `ui/mobile/runtime_composition.cppm` owns outer runtime composition/lifecycle; `main.cpp` remains the Qt composition/startup root.
- Translation catalogs are ID-based and synchronized across **12 locales**; `tools/check_localization_catalog.py` scans the full UI C++/QML translation surface.

## Verification gates

A change is not release-ready until the relevant gates are green:

1. Source contract guards in `tools/check_*.py`.
2. Gateway contract tests (`services/snapchain-gateway`, `services/media-gateway`).
3. Clean configure/build/test on the reference Clang/Qt toolchain.
4. Warning-fatal QML lint/smoke tests for GUI changes.
5. Platform-specific secure-store, signer, write-recovery and live-network acceptance for every shipping OS.
6. Sanitizer lanes for touched concurrent/resource-owning C++ code where supported.

GitHub Actions runs deterministic source guards and gateway contract tests. The authoritative C++/Qt shipping build remains a pinned Clang 22 + Qt 6.11.x environment until equivalent hosted runners are established.

### 2026-09-24 Linux verification evidence

- Architecture/source contract guards: **PASS**.
- Localization catalog guard: **PASS** with 817 referenced IDs, 817 catalog IDs and 12 locales.
- Snapchain gateway contract tests: **4/4 PASS**.
- Media gateway contract tests: **16/16 PASS**.
- Clean Linux configure with Clang 23.1.2, CMake 4.4.3 and Qt 6.11.2: **PASS**.
- The UI build compiled through the presentation layer, including `PresentationBridge`, then the Clang 23 frontend crashed internally while generating code for `ui/mobile/emoji_runtime.cpp`. This is recorded as a toolchain crash, not as a source-build pass or a semantic source failure.
- Qt6Keychain >= 0.17 is not installed on that Linux verifier, so the secure signer vault correctly remains fail-closed there.

## Known release blockers / consolidation work

- `PresentationBridge` is still a large presentation facade and should continue moving feature state toward scoped models/signals rather than one broad invalidation surface.
- Large QML stateful components, especially `GenyVideoPlayer`, should continue moving non-visual state-machine behavior into C++ controllers.
- Mini App hosting still requires full untrusted-content hardening (including private-network/DNS-rebinding controls, complete request isolation, and manifest-authorship verification) before arbitrary untrusted apps are treated as production-safe.
- macOS, Windows, and Linux require independent current release acceptance; one platform run is not portability evidence.
- Historical phase/review documents need not be deleted, but new work should update this file rather than creating another competing “current state” document.

## Change discipline

Do not redesign the provider-neutral core to solve presentation debt. Prefer small, behavior-preserving decomposition at composition/presentation boundaries, keep ownership explicit (RAII/`std::unique_ptr`), use `std::expected`/project `Result` for recoverable failures, and follow the C++ Core Guidelines plus the stricter project rules in `AGENTS.md`.
