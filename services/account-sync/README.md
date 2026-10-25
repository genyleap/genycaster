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
| `schema_migrations` | Administrative forward-only schema tracking. |

No speculative streak/leaderboard tables: these require a separate metric
definition, anti-abuse model, ranking time window and data-retention policy.

## Provisioning (operator gate)

The schema is source-controlled, but a database is **not** provisioned by
installing the WebAssembly client.

1. Authorized DBA creates a new PostgreSQL database using
   `migrations/000_create_database.sql`. Use a least-privileged Genycaster
   runtime role without access to OpenProof's database.
2. Apply `001_bookmarks.sql` and `002_cloud_state.sql` inside
   `genycaster_db` as the schema owner. Migrations are not automatically
   applied on every request.
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
Private endpoints under `/api/v1/cloud/{connections,bookmarks,notifications,tasks,drafts}`
support GET and PUT. `POST /api/v1/cloud/connections/refresh` fetches
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

Local-only `QSettings` manual LinkedIn URLs, SQLite bookmarks and drafts are
not cloud-synced until the user chooses sync and a production API is ready.
