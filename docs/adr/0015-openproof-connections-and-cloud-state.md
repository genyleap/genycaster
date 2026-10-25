# ADR 0015 — OpenProof connection management and Genycaster cloud state

**Status:** Accepted boundary / implementation staged (2026-10-21).

## Responsibility
- OpenProof alone creates, verifies, lists and removes linked identities. Genycaster may cache limited display metadata in an app-owned `connections` table, but must not duplicate the connection authority, subjects, identity claims, credentials, refresh tokens or passwords. A manually entered URL has explicitly self-asserted provenance.
- Farcaster/Hypersnap is the canonical protocol authority for user-data, casts and signer-bound writes.
- Genycaster's optional PostgreSQL database `genycaster_db` stores application-only `connections`, `bookmarks`, `notifications`, `app_updates`, `tasks` and `drafts`. It references a server-verified OpenProof issuer + identity ID as an opaque principal, with no independent users or authentication table. Bookmarks and unpublished drafts are app-owned, not Farcaster protocol messages. No speculative streak/leaderboard tables are created until metrics and anti-abuse rules are defined.
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

Create the PostgreSQL database `genycaster_db` **only** using authorized database administration. Apply migrations with a restricted schema owner; use TLS-secured connection strings from secret runtime configuration, not the repository. Do not create a shadow OpenProof user table or read its internal tables directly.

## Implementation status (2026-10-21)

- Completed code: Qt OpenProof provider discovery, account-scoped in-app Connect/Disconnect,
  confirmed provider status, short-lived authorization, browser-safe one-time
  handoff and FID-verified identity ownership. OpenProof still owns upstream
  provider authentication, protocol signing and durable connection state.
- Prepared: separate `services/account-sync` HTTP service for public
  `GET /api/v1/connections/providers` and guarded cloud state routes. SQL migrations
  define `genycaster_db` and the six feature tables. Every private API request
  binds ownership from an authenticated OpenProof `/account/profile` response;
  the service contains no shadow identity table. Recovery-ticket delegation
  remains disabled until a server-side cookie-based adapter exists.
- **Not completed or deployed:** actual PostgreSQL provisioning (direct DBA
  operation denied by the remote tool policy), reverse-proxy/API activation,
  client-driven cloud bookmarks/drafts/notification sync, background task worker,
  owner-consented public profile link API and complete recovery-ticket delegation.
  The manual LinkedIn field is device-local until the cloud bridge is enabled. Component tests
  alone do not establish that a real X or GitHub account was connected.
- Verified: `cmake --preset ui-qt612-debug` succeeded,
  `node --test tests/wasm/*.test.mjs` 292/292,
  `node --test services/account-sync/test/api.test.mjs` 3/3,
  `python3 tools/check_architecture.py` passed, Qt controller unit target
  built and tests passed before the final issuer-property addition.
  The application QML generators and added C++ controller compiled in the
  native client target; full final link/startup is **not yet verified**.

## Recovery-setup canonical identity convergence (2026-10-21)

- **Responsibility:** After explicit Farcaster phrase recovery, use the verified in-memory custody signer for one fresh direct OpenProof SIWF challenge; resolve or self-provision only the identity associated with that canonical FID. The OpenProof database remains sole account authority.
- **Capability origin:** Compatibility OpenProof authentication; local Farcaster custody recovery; optional cloud account enrichment.
- **Contract:** Browser-only recovery can attempt `POST /v1/browser/farcaster/recovery/start` with canonical FID and verified custody address and finish with a signed one-time OpenProof challenge. Return only verified issuer/subject/FID to native Qt; never return mnemonic, custody key, private cookies or upstream tickets.
- **Canonical invariants:** Exact canonical FID+custody verified on Optimism before signing; fresh server-supplied message, never a client-invented proof; short-lived/replay-limited transaction; server uses its own cookie jar; OpenProof verifies signature and sole linked FID, and issues/retrieves exactly one identity. Do not auto-merge other accounts.
- **Failure/offline:** OpenProof unavailability does not corrupt a successful local Farcaster recovery or force another phrase entry. Present a clearly scoped local-only session on failure; retry account verification requires fresh authorized proof.
- **Dependencies:** Browser signer JS → fixed-origin Genycaster media-gateway auth adapter → OpenProof canonical challenge/identity. QML receives only verified identity metadata, never bearer/phrase.
- **Provider disablement:** No mandatory OpenProof call on cached read/guest or local device account restoration; only explicit new phrase setup tries convergence.
- **Verification:** Mock upstream OpenProof contracts for direct FID/custody matching, cookies and anti-replay, real secp256k1 personal-sign vector, recovery setup cancellation and optional outage, UI session issuer, architecture checks, WASM linkage. No claim of live auto-provision without an actual authorized user ceremony.

### Temporary account access and deployment gates

- On successful exact-FID proof, OpenProof session cookies remain in the
  media-gateway process; an opaque 256-bit `recovery.<ticket>` capability is
  returned once, stored only in Genycaster's native C++ session controller, and
  expires in at most **45 minutes**. HTTP account adapters exchange it for
  server-held cookies to list/mutate **that same OpenProof account**. Neither
  server cookies nor bearer credentials enter QML or persistent storage.
- Session cleanup occurs on expiry, and the client erases its ticket at
  sign-out/FID change. Gateway restarts intentionally invalidate outstanding
  tickets. Restored device-only FID state is *not* a fresh OpenProof proof, so
  its session is conservatively local until reauthorization.
- The OpenProof start/complete ceremony is one-shot and bound to canonical
  custody address and FID; browser-side signing is EIP-191 over the exact
  server-issued FIP-11 message. No phrase/seed/private key is forwarded.
- OpenProof outage **never** blocks local signer recovery: only a successfully
  verified issuer/subject enters the canonical account session; otherwise
  `farcaster:optimism-idregistry` remains an explicitly local issuer.
- Native clients without in-browser phrase recovery retain their existing
  OpenProof OAuth/device onboarding. This iteration does not add a native
  mnemonic signer.
- Production gates: compile/link of the *current* client, startup/QML tests,
  end-to-end signed proof against real OpenProof and explicit consent where
  required, and jointly verified gateway/client release. Test mocks do not
  prove that live OpenProof self-provision policy is enabled.
