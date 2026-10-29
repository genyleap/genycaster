# 62 — Account portability and runtime inventory

## Product contract

Genycaster-owned bookmark records and profile categories are account data, not
browser state. The authoritative store is the dedicated Genycaster PostgreSQL
database. Every Genycaster client should use the same authenticated HTTP API,
with the same FID-derived account ownership and the same revision/deletion
semantics. Local Qt settings are a cache/outbox; users never initiate a sync
operation manually.

Do not implement the semantics for bookmarking, classification, conflict
resolution, or database authorization in a browser-only JavaScript bridge.

## Implementation boundaries (October 26, 2026)

| Responsibility | Current implementation | Required platform |
| --- | --- | --- |
| Bookmarked Cast UI | `ui/components/BookmarksSurface.qml` and `EngagementBar.qml` | All Qt targets |
| Bookmark local model | `src/presentation/qt/presentation_bridge.cpp` | All Qt targets |
| Profile editor and account pending state | `ui/components/ProfileSurface.qml` | All Qt targets |
| Authenticated account HTTP and durable bookmark queue | `ui/mobile/openproof_connections_controller.cpp` | All Qt targets |
| Public account API URL | `ui/config/public_endpoints.json` | All Qt targets |
| Account API, auth, validation and PostgreSQL persistence | `services/account-sync/*.mjs` (Node.js) | Server only |
| Browser signer, IndexedDB persistence and Web Crypto | `ui/wasm/*` | Web target only |
| Build/verification programs | `ci/*.py` and `tests/*` | Developer/CI only |

Both portable C++ account methods and QML components are registered in
`ui/CMakeLists.txt` **before** the `if(EMSCRIPTEN)` browser packaging block.
`EM_ASM` in the controller is used only in browser-specific persistence and
handoff branches; the underlying account HTTP transport uses Qt Network.

The shared QML editor currently includes ECMAScript expressions for form and
pending-state orchestration; consequently the project cannot be described as
**JavaScript-free**. The server is also intentionally implemented in Node.js
at this checkpoint. The HTTP/JSON/PostgreSQL protocol, however, is independent
of the client's target architecture and browser implementation. Removing
Node.js entirely is a **separate backend migration**, not a build option or a
reason to delete server source files.

## Tracked source inventory

The original inventory was a point-in-time snapshot. The obsolete
`docs-site/` and `tools/` directories have been removed from the repository.
Supported CI checks, WASM asset packaging and generators now live under
`ci/`; Qt/C++ sources remain under `src/` and `ui/`. Node.js application
services and browser companion modules remain intact. Python is not required
for the already-built native Qt client or the deployed Node account service,
but is required for selected source builds, CI tests and asset generation.
Additional ad-hoc deployment scripts under `/opt/genycaster/caster-deployment`
are **outside** this tracked repository and must be reviewed separately before
archival or removal.

Do not delete tracked tests simply because they use .mjs. They do not create
a native application runtime dependency. Likewise, do not delete the actual
Node.js account modules: they serve production API requests.

## Release and verification gates

1. Validate account auth/ownership, bookmark idempotency, delete receipts,
   category role validation, cross-device revisions and local-offline fallback
   against the real account API contract.
2. Keep browser-specific signer/cookie/IndexedDB code inside WebAssembly
   branches; never make account-bookmark and profile save semantics rely on it.
3. Run a clean Qt native build with an end-to-end authenticated account save,
   reopen and second-device read before claiming desktop/mobile parity.
4. Run the WebAssembly bundle build, static asset verification and origin/CDN
   smoke checks independently. A passing WASM build **does not** prove native
   runtime compatibility.
5. Never report a local optimistic save as a committed database write.
   Pending and failure state must remain distinguishable.
6. Profile extras are currently **private to the authenticated owner**.
   Making category/role visible to other profile viewers requires a distinct,
   deliberate read-only public projection, with privacy and authorization
   review; do not expose the private account endpoint.

`tests/wasm` currently houses some target-independent contract tests. Its
directory name is historical; a later organization change can move those
tests to `tests/contracts` without removing tests or altering the runtime.
