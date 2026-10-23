# Provider-neutral read and write routes

## Contract

A Farcaster client needs *capability routes*, not one global hostname. A
Snapchain read node can serve `GET /v1/...` but need not accept
`POST /v1/submitMessage`. A validator or another verified write-capable
provider is the write route. An indexed provider such as Hypersnap exposes a
different read data model from the raw Snapchain HTTP API. Do not substitute
one HTTP origin for another unless it supports the required contract.

A user-selected network profile must eventually bind, independently:

- Raw protocol reads (Snapchain RPC, including signer state)
- Indexed/social reads (feeds, profile hydration, discovery, search)
- Signed message submit (validator or a compatible writer)
- Signer-key submission (same write-capable route as signed casts)
- Media upload (distinct protocol, may require additional service)
- Capabilities, credentials, health, sync state, and explicit fallback policy

**Security requirements**: The client signs locally; never send a recovery
phrase, custody secret, or unencrypted signer private key to any provider.
Credentials are scoped to their configured destination. Route switching must
not move in-flight writes to a new destination. Writes with ambiguous outcomes
must reconcile canonical message hash/state before any retry; never broadcast
unconfirmed writes across destinations.

## Implemented in the October 24 routing increment

1. `--snapchain-base-url` / `GENYCASTER_SNAPCHAIN_BASE_URL` now also
   accept a saved `network/snapchainReadBaseUrl`, retaining the compiled
   deployment default as fallback. They configure authoritative signer reads.
2. `--snapchain-write-base-url` /
   `GENYCASTER_SNAPCHAIN_WRITE_BASE_URL` / user preference
   `network/snapchainWriteBaseUrl` configure the canonical signed-message
   destination (Cast, Reaction, Link). If not supplied the previous single
   Snapchain origin is retained for backwards compatibility. The WriteSubmitter
   now reconciles canonical Cast/Reaction/Link state via the selected Snapchain
   read endpoint, even if a separate validator receives signed submissions.
3. The signer registry now reads `/v1/signersByFid` from the read route,
   but submits `KEY_ADD` and `KEY_REMOVE` to the selected write route.
4. Optional write-only credentials are configured with
   `--snapchain-write-auth-header` and `--snapchain-write-auth-value`,
   or their `GENYCASTER_SNAPCHAIN_WRITE_AUTH_HEADER` and
   `GENYCASTER_SNAPCHAIN_WRITE_AUTH_VALUE` equivalents. The client's read
   credentials must NOT be copied to a distinct validator route. The UI
   intentionally does not persist credentials. Synchronous POST submissions
   and HTTP requests carrying custom headers do not auto-follow redirects;
   asynchronous requests with custom headers also use manual redirects.
5. The Settings screen has a validator write-endpoint field, validated as
   HTTP(S), and communicates that a restart is needed to activate changes.
   All 12 supported localization catalogues include the new wording.

These options configure routing **at runtime construction** (on restart).
They do not yet implement instantaneous live switching across operations.
The read-side Hypersnap node selector is still restricted to compatible
indexed Hypersnap origins; a raw Snapchain URL must not be entered there.

## Work remaining before declaring the multi-provider feature complete

- Add native raw Snapchain read adapter and self-hosted provider registration
  to the `RoutedReadGateway`. `StrictSelfHosted` currently has no complete
  read route and must remain fail-closed rather than silently using managed
  backends.
- Map raw protocol DTOs into domain models and derive indexed feed/search/
  channel projections independently from a third-party API.
- Model provider profiles with explicit read/write capabilities, validation,
  credentials, health, persistence, and change propagation in the general
  routing registry. Replace the interim write URL override with capability
  routing and per-operation diagnostics.
- Implement client/signing provenance filtering and independent on-chain
  custody-wallet import / account setup, with secure handling of seed phrases.
- Ship read-node provisioning, sync monitoring, API compatibility tests,
  no-API-key E2E read/write verification, and operator quickstart.

## Manual smoke verification

A. Without overrides, launch normally, browse and publish to confirm the
existing Genyleap compatibility route stays operational.

B. Configure a self-hosted Snapchain *read* URL and an independent write-capable
validator URL. Inspect signer state: only the read node receives signer GETs;
observe KEY_ADD/KEY_REMOVE/Cast/Reaction/Link POSTs on the validator. Verify
that read-only endpoint rejects submissions when incorrectly configured.

C. Configure distinct credentials. Inspect outbound HTTP: a read token may
appear only on read requests and a write token only on write requests.
Invalid URLs and incomplete write credential pairs must fail closed.

D. Restart after UI changes; verify the selected write endpoint is applied
without changing the Hypersnap indexed read provider.
