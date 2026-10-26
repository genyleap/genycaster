# Genycaster application-owned account state

Genycaster stores the user features **not provided by Farcaster** in its own
PostgreSQL database `genycaster_db`. The account user interface must **not**
show any "Cloud Sync", "Enable Sync", "Upload" or "Restore" controls.
Bookmarking, editing profile links, and changing self-authored connection URLs
are ordinary product actions. They are stored locally first and then
replicated automatically when a verified Genycaster account is online.

OpenProof remains the identity and connection authority. A user-entered
LinkedIn URL is *self-asserted*; it does not become OpenProof-verified.
Farcaster/Snapchain remain the protocol source for casts, signers and likes.

## Schema

Apply migrations `001_bookmarks.sql`, `002_cloud_state.sql`, then
`003_profile_extras.sql` inside the separate `genycaster_db`.

| Table | Genycaster-owned purpose |
| --- | --- |
| `connections` | Bounded OpenProof-linked social metadata and owner-authored LinkedIn URL |
| `profile_extras` | Selected profession/category and up to five labelled HTTPS URLs |
| `bookmarks` | Current per-account cast hash reference and deletion state |
| `bookmark_changes` | Monotonic, repeat-safe mutation history and incremental cursor |
| `drafts` | Private application drafts; no protocol publishing |
| `notifications` | Provider-event snapshots (future adapter) and the app-owned, monotonic read watermark |
| `tasks` | Future scheduled task definitions; no unattended signer or scheduler |
| `app_updates` | Public version information, read-only to users |
| `schema_migrations` | Administrative schema tracking |

The API runtime must connect with a restricted `genycaster_api` role; no
connection to or grants on OpenProof's PostgreSQL database are required.

## Production infrastructure

As of October 26, 2026, the operator has provisioned the independent
`genycaster_db`, applied migrations 1–3, configured `GENYCASTER_DATABASE_URL`
outside Git, and enabled HTTPS `/api/v1/cloud/` through Nginx. Live probes
confirmed `200 {"items":[]}` for `app_updates` and `401
{"error":"authentication_required"}` for unauthenticated private reads.

If the DB is unavailable, the client must keep the local edit/outbox rather
than implying that a remote write succeeded. The API responds with JSON,
never its browser app shell. Browser clients only contact the same-origin
`caster.genyleap.com` API; they do not see PostgreSQL credentials.

## Authentication and ownership

All private endpoints authenticate **every request**. The API ignores
client-submitted FID, identity ID, username and issuer when choosing an owner.

- Standard OpenProof bearer: verify `GET /account/profile` and exactly one
  linked Farcaster FID through `GET /account/connections`.
- Farcaster recovery bearer: verify the pre-existing, session-bound
  `recovery.<ticket>` with the Genycaster media gateway's fixed HTTPS
  `/v1/browser/openproof/connections` route. The gateway validates the
  temporary ticket and queries OpenProof with server-held session cookies.
  A client must not provide a bare FID as evidence.
- Both authenticated routes resolve to the stable owner
  `(issuer='https://openproof.genyleap.com', subject='farcaster:<verified FID>')`.
  This is intentionally consistent across different devices and login paths.
  Never export the temporary bearer to QML, JavaScript or persistent storage.

Changing the current viewer FID cancels pending network operations and
selects a different per-account local outbox and cursor.

## User-visible behavior

Bookmark Save/Unsave always changes the local UI immediately. A durable
per-FID queue with UUID mutation IDs automatically replays on reconnect;
the client reads `GET /bookmarks?since=<cursor>` in bounded batches,
applies tombstones without recursive writes, and advances its cursor only
after local changes are applied. Server retries are idempotent. When an older
device first upgrades, it scans the existing server history (including
deleted bookmarks) before migrating unrecognized local saves, so a stale
local bookmark does not resurrect an existing server deletion.

Profile category/profession/link edits are local-first; a background read
loads the server revision before optimistic PUT. Offline changes remain
marked dirty, with automatic retry and 409 revision handling. An in-flight
older response cannot acknowledge a newer edit.

LinkedIn's manual vanity URL is also saved locally first and replayed
automatically; reads from other devices restore it. Manual URLs stay
distinguished from provider-backed URLs.

Notification read state is per-account, not per-device: the native client
automatically persists its seen watermark and exchanges it with
`GET|PUT /api/v1/cloud/notification_seen`. The API accepts millisecond
timestamps as decimal strings and atomically retains the maximum value across
concurrent devices. The reserved database row is not writable through the
ordinary `notifications` endpoint.

The ordinary rich Cast composer now records per-FID draft edits and deletes
locally before uploading them through the account API. It uses one reserved
per-owner Cast draft ID, an optimistic revision, and a durable dirty flag.
`GET /drafts?id=<UUID>` provides a bounded lookup for that record; PUT with
`is_deleted:true` persists a versioned tombstone (including after a
confirmed publication). Server-side conflicts return 409, and the client
retains its unsent local version instead of overwriting the newer server
version. Existing old local drafts are imported only when no remote draft
conflicts. The transferred editable AST never includes device-private media
management/deletion tokens. Rich draft bodies have a separate 192 KiB
server-side item limit; over-limit edits remain stored locally.

This is a **single active Cast draft** adapter, not a multi-draft manager.
Concurrent conflicting edits on different devices are safely retained but
need a future user-facing draft-version resolution flow; no silent
last-writer-wins merge is claimed.

There are **no explicit sync controls** or sync account-enrollment gates in
the UI. Existing Farcaster/OpenProof authentication is reused.

## API

`GET /api/v1/cloud/app_updates` — public published changelogs.

`GET|PUT /api/v1/cloud/{connections,bookmarks,notifications,drafts,tasks,profile_extras}`
— private verified account data; request bodies are bounded and validated.

`POST /api/v1/cloud/connections/refresh` — refresh provider metadata only
from authenticated OpenProof, never user-provided claims.

Profile extras use revision 0 for create, expected revision for updates;
conflicts return HTTP 409. Bookmarks accept an optional UUID
`mutation_id` and expose a cursor-driven change log. A single ordinary
Cast draft now has an automatic local-first native adapter with versioned
deletion and conflict preservation. Other draft types, multi-draft browsing,
provider notification snapshots and task scheduling/execution do not yet
have full native-client reconciliation. The cross-device *notification seen
watermark* is implemented independently. No background scheduled publishing,
GENY attestation, public user-profile reader or Streak service should
be claimed.

## Testing and operational checks

```bash
node --test services/account-sync/test/*.test.mjs
curl -i https://caster.genyleap.com/api/v1/cloud/app_updates
curl -i https://caster.genyleap.com/api/v1/cloud/profile_extras
```

Expected anonymous responses are `200` JSON and `401` JSON respectively.
Full cross-device acceptance requires two real authenticated sessions:
save/un-save on device A, confirm device B receives the change, then repeat
for profile links and LinkedIn; test offline replay, logout/account switch,
concurrent edits, and database backup/restore.

No actual private user token should appear in logs or be pasted into tickets.
