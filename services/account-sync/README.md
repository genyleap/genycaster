# Genycaster Cloud State — PostgreSQL (application-owned)

This is Genycaster's optional app-data service, **not an OpenProof database**.
OpenProof continues to authenticate and link accounts. Farcaster/Hypersnap
continues to own protocol messages and signer-bound writes. The application
keeps local SQLite/IDBFS data available offline, whether or not cloud sync is
enabled. Use the isolated database **genycaster_db**, not `openproof`.

## Tables and purposes

| Table | Owner and function |
| --- | --- |
| `connections` | Display-only sanitized snapshot from authenticated OpenProof `/account/connections`, plus **separate**, self-asserted manual profile URL. Never a provider session, verified claim authority or token. |
| `bookmarks` | App-private cast hashes with tombstones and `bookmark_changes` for incremental/replay-safe reconciliation (prepared migration 001). Not a Farcaster protocol message. |
| `notifications` | Bounded account-specific notification snapshots, read state and revisions for cross-device reconciliation. Never an authority for protocol state. |
| `app_updates` | Release-admin-published title, description and structured changelog; user API is strictly GET-only. |
| `tasks` | Account-owned definitions for future scheduling, durable idempotency keys and states. **No scheduler, wallet key or unattended signer has been deployed.** |
| `drafts` | Private cast/article/thread source content, revision-based optimistic conflict control, explicit cloud upload. Not published to Farcaster. |
| `profile_extras` | Owner-selected professional category, role, and up to five labelled HTTPS links. Private by default, one row per server-verified account, optimistic revision checks; no OpenProof user data is copied. |
| `schema_migrations` | Administrative forward-only schema tracking. |

No speculative streak/leaderboard tables: these require a separate metric
definition, anti-abuse model, ranking time window and data-retention policy.

## Provisioning (operator gate)

The schema is source-controlled, but a database is **not** provisioned by
installing the WebAssembly client.

1. Authorized DBA creates a new PostgreSQL database using
   `migrations/000_create_database.sql`. Use a least-privileged Genycaster
   runtime role without access to OpenProof's database.
2. Apply `001_bookmarks.sql`, `002_cloud_state.sql` and
   `003_profile_extras.sql` **in order** inside `genycaster_db`
   as the schema owner. Migrations are not automatically applied on requests.
   After the changes the `schema_migrations` table must contain 1, 2 and 3.
3. Configure a **secret** `GENYCASTER_DATABASE_URL` on the Genycaster
   account API runtime. Install Node dependencies with `npm ci`.
   Set `GENYCASTER_OPENPROOF_ISSUER` to the trusted HTTPS OpenProof issuer.
4. Expose `/api/v1/cloud/` only through a same-origin HTTPS proxy with
   bounded body sizes. Verify owner scoping, CORS/no CORS for browser clients,
   auth expiration, and backup/restore before making the UI Sync button live.
5. Explicitly enable per-account sync in the client only after end-to-end tests.
   A successful local Save never implies the cloud accepted the write.

Without `GENYCASTER_DATABASE_URL`, private cloud endpoints return
`503 cloud_not_configured`; offline UI and protocol operations remain usable.

## API contract (staged)

`GET /api/v1/connections/providers` is public provider discovery.
`GET /api/v1/cloud/app_updates` is public published release notes.
Private endpoints under `/api/v1/cloud/{connections,bookmarks,notifications,tasks,drafts,profile_extras}`
support GET and PUT. `profile_extras` GET returns `{item:null}` for an
uninitialized account, and PUT requires `revision:0` on creation or the
last server revision on edits; stale writes return 409.
Bookmarks support `GET /api/v1/cloud/bookmarks?since=<sequence>` to
retrieve the next 200 append-only receipts with `next_cursor`; PUT accepts
an optional UUID `mutation_id` for idempotent retries. `POST /api/v1/cloud/connections/refresh` fetches
authenticated OpenProof provider metadata and copies only a limited set of
display fields. A client may never submit an OpenProof subject or claim
that a manually entered LinkedIn URL is verified.

The sync API verifies the caller on every request via the upstream
`GET /account/profile`, binding all SQL parameters to the returned
`identity_id` and the configured OpenProof issuer. No client-supplied FID,
subject, email, username or JWT payload is used as an identity authority.
Bearer recovery tickets use a server-held cookie and **are not yet supported
by the dedicated cloud service**; a verified gateway adapter is required.
Do not work around this by trusting client identifiers.

PUT for drafts, tasks and notifications carries an expected `revision`
(0 for initial create), and concurrent updates return HTTP 409. There is
no automatic scheduled publishing: a future worker needs explicit user
authorization, safe signer custody, lease/claims and replay protection.

For historical and public integration, `connections` is NOT a parallel
OpenProof identity directory: it is a revocable account-owned profile
presentation cache. The account owner explicitly chooses whether a manual
URL is visible on a future public profile endpoint. **No public per-user
link-reading endpoint has been enabled.**

**Feature status:** Genycaster's own PostgreSQL schema and guarded REST
implementation exist. The native Qt profile editor contains an explicit,
opt-in upload/restore path for `profile_extras` and keeps local data when
unavailable. The API must pass an authenticated PostgreSQL production check
before its actions become enabled; HTTP 200 HTML from an SPA fallback is
deliberately rejected. No access bearer is exposed to QML/JavaScript.

**Not yet live:** The current Remote Operations execution identity lacks
PostgreSQL DBA privileges (peer authentication rejects `root`, and
`sudo -u postgres` is unavailable under its policy), so it cannot create
`genycaster_db` itself. The production Nginx /api/v1/cloud/ location is
also not enabled. Do not advertise cloud persistence while those operator
gates remain unmet. Local bookmarks/drafts/notification mutations are
not yet wired into the two-way client reconciliation adapter. This work
is intentionally distinguished from a complete multi-device Sync feature.

To activate:
- Have an authorized DBA provision `genycaster_db` and a least-privileged
  service login, apply migrations 001–003 and grant only the required table
  and bookmark sequence access. Never use the OpenProof database role.
- Set `GENYCASTER_DATABASE_URL` outside Git in the Genycaster API
  process environment and use the dedicated `server.mjs` launcher
  (which validates the schema at startup). A deployment wrapper that always
  calls `createAccountApi({catalogUrl})` without passing `store`
  disables every private endpoint, even if the DB exists.
- Route `/api/v1/cloud/` from `caster.genyleap.com` to the local
  Genycaster account API, use HTTPS, request body size limits and no
  cross-origin credential forwarding; reload Nginx only after `nginx -t`.
- Exercise two different OpenProof identities, logout/expiry, a second
  device, concurrent edits (409), bookmark retry/del/delete, backup/restore
  before enabling default synchronization.
