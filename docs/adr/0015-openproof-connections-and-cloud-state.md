# ADR 0015 — OpenProof connection management and Genycaster cloud state

**Status:** Accepted boundary / implementation staged (2026-10-21).

## Responsibility
- OpenProof alone creates, verifies, lists and removes linked identities. Genycaster must not duplicate the connection store, identity claims, credentials, refresh tokens or passwords.
- Farcaster/Hypersnap is the canonical protocol authority for user-data, casts and signer-bound writes.
- Genycaster's **optional** extension database named `genycaster` owns only client-specific cross-device state (initially synchronized bookmarks); it references the stable OpenProof `issuer + subject` identity as an opaque principal, with no `users` or `social_connections` table.
- The Qt profile presents an account-scoped picker for **the current authenticated profile only**; in-app Connect/Disconnect actions use an ephemeral account-scoped OpenProof bearer that has been checked against the active Farcaster FID. Provider credentials and OAuth tokens never enter QML, JS or persisted client state.

## Feature gate
- **Capability origin:** Extension (cloud sync), Compatibility (OpenProof linked account provider), Local (client cache).
- **Mandatory or optional:** Optional. Guest profiles, protocol reads, and offline local bookmarks remain available when OpenProof, the portal or extension API is down.
- **Public contract:** `GET /api/v1/... ` behind a replaceable configurable origin; stable schema, bearer-scoped OpenProof identity on the server (never a client-supplied subject), versioned bookmarks updates and incremental change cursor. The OpenProof issuer, provider-catalog URL and browser-safe connection gateway are configurable public endpoints rather than QML literals.
- **Canonical data/invariants:** OpenProof issuer+subject uniquely identifies the account; FID may change/relink. Bookmarks use canonical cast ID, at-most-one live row per owner+cast, monotonic change revision, idempotent mutation ID, tombstones for deletes. Provider catalog is display metadata only; **never infer linked status from its presence or the current FID**.
- **Dependencies:** QML < Qt textual presentation adapter < verified OpenProof account capability and CORS-safe gateway; future HTTPS sync adapter < provider-neutral application extension contract < local cache. Database schema lives under the optional account-sync service and must not leak provider DTOs into domain.
- **Failures:** no account session, expired authorization, different Farcaster identity, unavailable handoff, slow/disallowed catalog CORS, unknown provider and partial network responses. Never bypass identity validation or claim a successful link before OpenProof verifies it.
- **Local-state representation:** existing SQLite remains authoritative for cached personal data offline. Cloud reconciliation must use explicit revision and tombstones, not full replacements.
- **Offline behavior:** show cached local data; connection picker can show its verified built-in supported options but does not claim current connection status while offline. Linking requires online first-party authorization.
- **Provider policy / disablement:** the OpenProof manager and future sync API are independently optional; turning them off must not alter Farcaster reads.
- **Failover:** no fallback identity authority. A replacement issuer/catalog/gateway origin must be provided by configuration; do not silently assume a different issuer means the same account.
- **Boundary tests:** QML dialog restricted to own authenticated profile, HTTPS handoff URL validation and short-lived token ownership, catalog filtering, existing Qt QML import smoke, SQL migration syntax/uniqueness, architecture guard, and WebAssembly linking. Cross-browser connected-state and PostgreSQL deployment remain separate verification gates.

## Operational limits
OpenProof delegates `GET /account/connections`, `POST /account/connections/handoff` and `POST /account/connections/disconnect` with the first-party `account` OAuth scope. The in-app picker now uses the approved OpenProof device/verified-Farcaster grant and a fixed-destination browser CORS relay. A bearer is held only in native C++ until expiry/signout; the server retains no mirrored linked-account records. Redirect-provider consent is opened only through a user-clicked, one-time OpenProof handoff URL. A Farcaster-only login may authorize the same verified FID via OpenProof device approval; a mismatched linked FID is denied. Signed Web3 wallet linking remains an independent SIWF/SIWE capability not yet implemented in the picker.

Create the PostgreSQL database `genycaster` **only** using authorized database administration. Apply migrations with a restricted schema owner; use TLS-secured connection strings from secret runtime configuration, not the repository. Do not create a shadow OpenProof user table or read its internal tables directly.

## Implementation status (2026-10-21)

- Completed code: Qt OpenProof provider discovery, account-scoped in-app Connect/Disconnect,
  confirmed provider status, short-lived authorization, browser-safe one-time
  handoff and FID-verified identity ownership. OpenProof still owns upstream
  provider authentication, protocol signing and durable connection state.
- Prepared: separate `services/account-sync` HTTP service for public
  `GET /api/v1/connections/providers`, plus the DBA/create/migration SQL for
  a future PostgreSQL database called `genycaster`. The service is deliberately
  separate from the media gateway and contains no shadow identity table.
- **Not completed or deployed:** actual PostgreSQL provisioning (direct DBA
  operation denied by the remote tool policy), reverse-proxy/API activation,
  private authenticated cloud bookmarks API, multi-device bookmark sync, and
  verified production OAuth account-linking end to end. Component tests
  alone do not establish that a real X or GitHub account was connected.
- Verified: `cmake --preset ui-qt612-debug` succeeded,
  `node --test tests/wasm/*.test.mjs` 292/292,
  `node --test services/account-sync/test/api.test.mjs` 3/3,
  `python3 tools/check_architecture.py` passed, Qt controller unit target
  built and tests passed before the final issuer-property addition.
  The application QML generators and added C++ controller compiled in the
  native client target; full final link/startup is **not yet verified**.
