# ADR 0019 — Independent authentication and device authorization authorities

**Status:** Accepted architecture; implementation deployed for the account API
and WebAssembly in compatibility mode. Cross-platform acceptance and strict-mode
rollout remain gated. See [Roadmap](../12-roadmap.md).

## Context

Restoring a viewer identity, signing a canonical social message and authorizing
private product data are different operations. Treating a remembered FID or a
registered social signer as a general application credential couples unrelated
privileges. Provider outages, local storage errors and stale asynchronous
responses must not silently change the active principal or grant new authority.

This decision extends [ADR 0009](0009-local-signer-port.md),
[ADR 0015](0015-openproof-connections-and-cloud-state.md) and
[ADR 0016](0016-protocol-scoped-account-registration.md). OpenProof remains the
independent verified identity and connection authority for Farcaster; HyperSnap
native identity remains separately registry-verified. This does not introduce an
OpenProof HyperSnap-native FID verifier.

## Decision

| Authority | Credential / evidence | Permitted result |
|---|---|---|
| Verified identity | Fresh OpenProof grant and linked Farcaster FID, or native custody and HyperSnap registry | Establish exact protocol-scoped viewer principal |
| Local continuity | Separate Ed25519 device key and versioned identity seal | Restore the local viewer identity for up to 30 days |
| Social signing | Network-registered Ed25519 key and applicable scopes, TTL and revocation state | Sign authorized canonical protocol messages |
| Private-cloud device | Independent Ed25519 key enrolled with fresh verified identity/custody authority | Restore or rotate app-owned server session |
| Custody / Mini App wallet | Explicit, operation- and origin-scoped consent and applicable wallet proof | Authorize only the approved wallet operation |

Use a decimal-string FID on browser/server boundaries and validate the exact
issuer, subject and protocol. Native HyperSnap FIDs begin at 2^32 and must not
be treated as Farcaster or OpenProof-linked identities based on their number.

The application-layer `AuthenticationCoordinator` owns restore/revalidation
policy, generation cancellation and bounded retry. Qt and browser bridges
dispatch lifecycle events; a foreground or connectivity event does not grant
new custody approval or automatically register a missing signer.

### Secure storage

- Apple: platform Keychain through Security and CoreFoundation.
- Linux, Windows, Android: QtKeychain secure backend or the platform boundary
  selected by the build; no plaintext fallback.
- WASM: origin-scoped, non-extractable WebCrypto keys in IndexedDB, with
  independent scopes for local continuity, social signing and cloud devices.
  Origin compromise can invoke those keys; this is not an OS keychain.
- Ordinary local persistence may contain public metadata, sealed identity
  snapshots and signed envelopes, but never mnemonic words, raw private keys
  or reusable OAuth credentials. Missing/malformed seals fail closed; transient
  secure-store failure permits bounded retry without granting authority.

### Cloud-device lifecycle

```mermaid
sequenceDiagram
    participant C as Client
    participant V as Secure vault
    participant A as Account API
    participant I as Verified identity authority
    participant D as App PostgreSQL

    C->>V: Create independent cloud-device key
    C->>A: Enroll challenge: protocol, FID, public key
    A-->>C: Single-use nonce (90 s)
    C->>V: Sign exact challenge
    C->>A: Device signature plus fresh identity proof
    A->>I: Verify OpenProof grant or native custody registry
    A->>D: Enroll public key and issue device session
    A-->>C: Secure HttpOnly cookie and verified principal
    C->>A: Restore challenge
    A-->>C: Single-use nonce
    C->>V: Sign challenge with enrolled key
    C->>A: Restore proof
    A->>D: Check active enrollment and rotate session
    A-->>C: Verified session (30 days)
    C->>V: Sign retirement receipt before local erase
    Note over C: Persist receipt; retry even after logout
    C->>A: Deliver revocation receipt
    A->>D: Tombstone enrolled key and delete its cookies
```

The service enforces a 90-day inactivity limit, single-use challenges, exact
principal matching and permanent key retirement. Unknown receipts do not create
rows. A known revoked key cannot reenroll. A failed or unavailable cloud session
does not itself revoke the independent social signer.

## Rollout

1. Back up the independent application database; apply migrations 017–018 with
   the DBA, not the restricted runtime role.
2. Deploy and verify the account API before clients; retain legacy compatibility
   for unmigrated principals. The old social-signer session path is denied for
   any principal with cloud-device history.
3. Roll out clients, verify real enrollment, restore, logout and account switching
   with both protocols. Retain old immutable web assets during the transition.
4. Only after compatible-client adoption and acceptance, set
   `GENYCASTER_ALLOW_LEGACY_SIGNER_SESSIONS=0` in the effective API composition
   and verify that the compatibility path is disabled. Fresh verified OpenProof
   grants remain valid identity evidence.

## Consequences and validation

The architecture prevents an intended grant for one authority from being treated
as a grant for another. It does not establish complete security on every target.
Validate principal-scoped cache/outbox behavior, asynchronous cancellation, signer
scope and registry handling, local and cloud key erasure, session rotation,
cross-origin rejection, and revocation after offline logout.

Source anchors: `src/application/authentication.cppm`,
`src/storage/session_store.cpp`, `src/platform/`,
`ui/app/identity/connections.cpp`, `ui/app/runtime/composition.cpp`,
`ui/wasm/browser_account_session.mjs`, `ui/wasm/browser_device_session.mjs`,
`ui/wasm/secure_signer_vault.mjs`, and
`services/account-sync/cloud_device_proof.mjs`.

Acceptance uses `ui.secure_vault_live` with a real unlocked OS keychain, the
application session/signer tests, browser security contract tests, and
`test/cloud_device_pg_integration.test.mjs` in a disposable database.
The dated qualification record is maintained in the [Roadmap](../12-roadmap.md);
see the [security contract](../08-security-auth-and-signing.md) for detailed
invariants and [account API](../../services/account-sync/README.md) for operations.
