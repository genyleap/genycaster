# GenyCaster — Development Stages & Acceptance Gates

> **Current engineering roadmap · 2026-11-06 · Beta / v1.0.0-rc1 source**
>
> This is the **single forward-looking staged development plan**. It retains the actionable technical requirements previously scattered across root-level review, verification, design and delivery reports. It is **not** a substitute for the source code, current [status](../STATUS.md), architectural decision records, or fresh platform test results.

## Status vocabulary

- **Implemented in source**: a feature and its ownership boundary exist in the tree. This does not establish live platform acceptance.
- **Acceptance pending**: real-world tests, hardening or integration evidence remains to be collected.
- **Proposed**: no blanket claim of shipped functionality; requires a feature contract and acceptance gate.
- Future engineering notes belong **here or in the owning reference/ADR**, never in new root-level `PHASE_*_REVIEW.md` files.

| Stage | Goal | Current state | Required gate |
|---|---|---|---|
| 0 | Canonical core, C++ modules and protocol-neutral boundaries | Implemented in source | Architecture, dependency and toolchain guards |
| 1 | Local-first feeds, capability routing and signed writes | Implemented in source | Real-node, offline, signer/retry and fault-injection acceptance |
| 2 | Native UX, timeline/media rendering and navigation | Implemented in source | Scroll stability, interaction/accessibility, GPU/runtime checks |
| 3 | Identity, Mini Apps and scoped wallet permissions | Platform-dependent implementation | Cross-origin security, explicit consent/revocation, platform parity |
| 4 | Derived discovery, account services and media gateway | Implemented in source, operator-dependent | Authz, owner isolation, data durability and gateway recovery |
| 5 | Multi-platform, localization and measured performance | Ongoing | Qt 6.12 matrix and reproducible evidence |
| 6 | Public-source and release readiness | Ongoing | History/license audit, clean docs, full applicable CI/package gate |
| 7 | Optional protocol/product extensions | Proposed where not already implemented | ADR, threat model, capability contracts and tests |

## Stage 0 — Architectural foundation

**Retained requirements**

- Project-owned **C++26 named modules**, explicit ownership/RAII, and Qt **6.12** for the graphical UI. The release channel is read from the top-level `CMakeLists.txt`, never copied from old phase reports.
- Dependency direction is Presentation → Application → canonical Domain/Capability contracts. Provider DTOs, SDK types, credentials, Qt, HTTP and SQLite stay outside stable domain interfaces.
- Default routing policy prefers **HyperSnap**; users may select Snapchain or another compatible node where the requested operation is supported. There must be **no global hard-coded provider** in the domain.
- Provider-free, strict self-hosted and optional-feature compositions remain valid build policies where their adapters exist.
- An architectural change requires an ADR; a toolchain workaround must not silently weaken the module/dependency boundary.

**Acceptance:** `python3 ci/check_architecture.py`, applicable CMake presets and current module/toolchain compilation. References: [Architecture](ARCHITECTURE.md), [ADRs](adr/), [Build profiles](10-build-deployment-profiles.md), [Engineering authority](../AGENTS.md).

## Stage 1 — Read reliability and durable protocol writes

**Implemented in source; full target-specific acceptance pending.**

1. Read a canonical **local SQLite snapshot first**, then refresh over a background capability route. Render a cache hit without waiting for remote I/O.
2. Per-operation policy considers support, user-selected node, health, budget and valid fallback. Preserve provider-affine pagination and distinguish canonical protocol state from derived rankings.
3. Incremental reconciliation must preserve visible cast identity, content order and offline usability; a temporary provider error must not erase trusted cached state.
4. Account login, custody approval, local protocol signer and submission transport are **separate authorities**. Signed writes require explicit valid signer scopes, TTL and account identity.
5. Build canonical protocol messages and persist the **exact signed envelope** in the account-scoped durable outbox. A timeout, conflict or restart triggers confirmation/reconciliation **before** any resubmission.
6. Revoked, expired, replaced or mismatched signers cannot authorize old queued writes. Provider switching must not blindly replay a protocol write.

**Acceptance:** network loss, slow reads, rate limits, pagination, HyperSnap/Snapchain selection, crash recovery, ambiguous success, duplicate prevention, signer revocation/expiry and app-FID attestation on supported targets. No false success after transport failure.

References: [Capability routing](04-provider-boundary-and-routing.md), [Local state](05-local-first-state-and-sync.md), [Signer design](08-security-auth-and-signing.md), [Write hardening](39-phase5f-write-hardening.md), [Outbox ADR](adr/0010-durable-local-outbox.md).

## Stage 2 — Visual system, native rendering and navigation

**Implemented in source; ongoing UX and frame-time acceptance.**

- The living visual authority is `ui/theme/Theme.qml` plus the current components. Branded surfaces use consistent **superellipse** geometry, semantic tokenized colors/type/spacing and restrained interaction accents; do not replace this with generic Qt default controls or a new unreviewed color system.
- **Chrome direction** follows UI locale; user content direction follows its first strong character. Isolate handles, URLs, FIDs and numeric LTR protocol identifiers correctly.
- Favor the Qt Quick scene graph and platform RHI. `QT_QUICK_BACKEND=software` in offscreen **tests** must not become a shipping GPU-disable switch. Inspect actual runtime `graphicsApi()` before claiming a hardware backend.
- Keep feed delegates and media work cheap: reuse row identity, lazy-load rich panels, avoid unbounded blur/layers, debounce geometry shifts, preserve image loading during rapid scroll and reserve the media frame before decoding completes.
- Opening a cast from Home/Explorer must preserve the source and the **same cast/viewport offset** on Back; do not rebuild the hidden model into an unrelated social mode. Share contextual cast actions across feed and detail.
- Setup and Mini App navigation need explicit loading states, stable Back behavior, soft hover/focus transitions, scrollable drawers, accessible icon labels and reduced-motion alternatives.

**Acceptance:** desktop/mobile breakpoints, light/dark themes, RTL and keyboard, 60/120-Hz targets, continuous media scroll, slow connections, back-to-exact-cast, Mini App loading and real QML runtime tests. Record frame-time percentile and layout stability evidence, not subjective speed claims.

References: [UI/scene-graph contract](14-performance-and-ui-runtime.md), [Current scroll work](scroll-stability-2026-11-06.md), [Benchmark plan](PERFORMANCE.md), `ui/components/` and `ui/theme/Theme.qml`.

## Stage 3 — Identity, Mini Apps and wallet isolation

**Source support exists but is target-dependent; security acceptance is pending.**

- The OpenProof-linked account session, Farcaster identity, custody-wallet signer authorization, locally stored Ed25519 protocol signer and Mini App EVM provider must remain independent privileges.
- Resolve a Mini App's manifest and validate `requiredCapabilities` / `requiredChains` **before** advertising what the host can do.
- Wallet grants must be bound to **origin, document/session, account, chain and operation**, not shared by every Mini App or automatically reapproved on restart. Pairing is not signing, and account access is not unconditional transaction/signature permission.
- Native desktop EIP-1193/EIP-6963 browser wallet relays use short-lived cryptographically random localhost sessions. Never claim to be the original HTTPS Mini App origin or expose keys via the bridge.
- Web/WASM WalletConnect and injected browser wallet support are **not evidence** of a native Android/iOS WalletConnect adapter; mobile requires authenticated app/universal links, a verified callback and separate acceptance.
- Denied requests must preserve rejection semantics. Principal change/disconnect/revocation must drop prior grants; provider events must never leak into a newly navigated document.
- Untrusted Mini Apps require cross-origin, DNS rebinding, private-network access, iframe isolation, redirect, manifest-authorship and hostile-content controls. Platform secure storage must fail closed, never fall back to plaintext.

**Acceptance:** two Mini Apps cannot share grants; reopening, logout, account switch, disconnect, same-address rediscovery, refused signature, chain switch, session expiration and browser restart cannot silently produce a signature. Verify independently on native and Web/WASM targets.

References: [Security policy](../SECURITY.md), [Authentication](08-security-auth-and-signing.md), [WebAssembly](webassembly.md), [Architecture](ARCHITECTURE.md) and the Mini App/wallet suites under `tests/wasm/`.

## Stage 4 — Account data, derived discovery and media lifecycle

**Implemented in source, with operator-side acceptance needed.**

- GenyCaster-owned profile metadata, statuses, bookmarks/collections, feedback and categories are distinct from Farcaster protocol messages. Synchronize eligible product data across devices through authorized services **without manual cloud-sync UI**.
- Discovery, topics, channels, indexing and recommendations are **derived capabilities** with provenance, bounded pagination, fallback and access controls. They must never rewrite canonical protocol truth.
- The service folders (`services/account-sync/`, `services/quick-auth/`, `services/snapchain-gateway/`, `services/media-gateway/`) are separate deployable trust boundaries: server-side authorization, privacy policy, rate limits and rollback are mandatory. **CORS alone is not security**.
- For the existing QStorage adapter, the bounded multipart gateway uses SigV4 **Authorization-header signing** for writes; presigned PUT is not an accepted substitute without new interoperability evidence.
- Keep backend credentials and provider-specific object IDs out of client-facing domain/UI types. Upload tickets, stable URLs, signed owner actions and managed cleanup tokens must remain scoped, validated and durable.
- On confirmed Cast deletion, cleanup must not delete media referenced by other local Casts; retry offline cleanup without weakening the owner/signer proof.

**Acceptance:** authenticated two-device reconciliation, race/conflict handling, owner isolation, upload/abort/complete/delete, bounded memory, service restart, graceful provider outage and recovery/rollback.

References: [Derived infrastructure](09-derived-infrastructure.md), [Media gateway architecture](44-media-upload-gateway.md), [Account service](../services/account-sync/README.md), [Media service](../services/media-gateway/README.md).

## Stage 5 — Cross-platform, localization and comparative performance

**Ongoing.**

- Confirm actual Qt **6.12** C++/QML builds, signer vaults, native network/TLS, media decode and packaging on every targeted OS/architecture separately. An iOS configuration is **not** proof of a signed App Store release.
- Use Qt scene graph/RHI diagnostics (Metal/D3D/OpenGL/WebGL as applicable); distinguish real GPU performance from headless software test rendering.
- Validate twelve translation catalogs, screen-reader labels, Persian/RTL mixed text, time zones/calendars, responsiveness and reduced motion.
- Measure **cold/warm startup**, cached feed availability, return-from-conversation anchor, image first paint, 100-cast scroll, p50/p95/p99 frame time, memory and WASM download/startup under controlled conditions.
- No published "faster than X" or "uses Y% less memory" without matching versions/hardware/network/cache/feature workloads plus inspectable raw runs.

**Acceptance:** target/device matrix, test commands, measured distributions and linked evidence as specified by [Performance](PERFORMANCE.md), [Localization](42-phase5g-localization.md) and [CI](../ci/README.md).

## Stage 6 — Public repository and release engineering

**Ongoing.**

- Keep root limited to canonical build, README, license, status, contribution and security entry points. One-off phase/review/delivery Markdown files should **not return** under root or as a renamed archive.
- Keep this one roadmap, living architecture/ADR docs and source tests current; correct stale links and CMake/Qt version text.
- Audit Git **history, branches, tags**, release assets, package dependencies and transitive licenses; deleting an old Markdown file from `main` does **not** erase history.
- Security-sensitive public API endpoints require independently enforced authentication and authorization; never distribute service secrets, signer/custody private keys or production DB credentials.
- Review Qt/third-party redistribution, fonts, media icons, npm/vendor dependencies, trademarks and an SBOM before claiming full public-release compliance.
- Run architecture/CI source checks, affected regression tests, Qt runtime tests, package verification and controlled multi-platform CI. Pushing to `main` is not a release.

**Acceptance:** clean tracked root, no references to removed reports, successful affected tests, documented vulnerability process, complete licensing review and verifiable release artifacts.

References: [Contributing](../CONTRIBUTING.md), [Security](../SECURITY.md), [Third-party notices](../THIRD_PARTY_NOTICES.md) and [CI](../ci/README.md).

## Stage 7 — Optional extensions (proposed)

Future work may include independently owned ranking/recommendation infrastructure, open messaging extensions, realtime content surfaces and new Farcaster capabilities. Existing partial features are not declared complete merely because a QML page or backend stub exists. E2EE must use a reviewed, mature protocol; never invent cryptography.

Each proposal must define: capability class, domain ownership, dependencies, threat model, offline/retry behavior, authority, platform support, feature gating, test strategy and an ADR if architecture changes.

## Definition of done

Mark a stage release-accepted only after: **(1)** implementation matches contract, **(2)** relevant tests and platform builds are green, **(3)** signer/permission boundaries are live-verified, **(4)** restart/offline/ambiguous errors are safe, **(5)** accessibility/localization are checked, **(6)** performance claims have reproducible evidence, and **(7)** deployment/rollback and licensing gates are complete.

**Near-term priorities:** Mini App origin-scoped wallet consent and signer safety; stable feed/media/navigation under scroll; mobile/WASM acceptance; service security; measurable performance; and public-source licensing/CI readiness. These priorities are **not** claims of completion.
