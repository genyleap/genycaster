# Genycaster application-owned account state

Genycaster stores the user features **not provided by Farcaster** in its own
PostgreSQL database. The account user interface must **not**
show any "Cloud Sync", "Enable Sync", "Upload" or "Restore" controls.
Bookmarking, editing profile links, writing Cast drafts and changing account URLs
are ordinary product actions. The Genycaster database is canonical for account
state; local device data serves only as a responsive and offline cache.

[OpenProof](https://github.com/genyleap/openproof) is the independently
managed identity and connection authority; its server process is [`opp`](https://github.com/genyleap/openproof/tree/main/apps/opp)
and the configured first-party issuer is `https://openproof.genyleap.com`.
GenyCaster does **not** share OpenProof's PostgreSQL database or copy its
provider credentials. The GenyCaster application database stores only
application-specific account state. See [identity and service architecture](../../docs/OPENPROOF_AND_SERVICES.md).
A user-entered
LinkedIn URL is *self-asserted*; it does not become OpenProof-verified.
Farcaster/Snapchain remain the protocol source for casts, signers and likes.

## Schema

Apply migrations `001_bookmarks.sql` through `007_bookmark_collections.sql` in
version order inside the GenyCaster PostgreSQL database with an authorized database-admin
role. Migration 006 creates app-owned topics and their Cast associations.
Migration 007 introduces private per-account bookmark collections, memberships,
and a tombstone trigger that removes memberships when a bookmark is unsaved.
Each migration grants only the table operations needed by the restricted
runtime role.

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
| `account_sessions` | Verified per-browser sessions, stored by token digest |
| `topics` | Global ordered, active topic slugs (editable by DB administrator, not users) |
| `cast_topics` | Up to three topic references per protocol-verified, author-owned cast; independent of channels |
| `bookmark_collections` | Private user-created, account-owned named reading-list collections |
| `bookmark_collection_items` | Private many-to-many links to existing, saved bookmarks |

The API runtime must connect with a restricted application role; no
connection to or grants on OpenProof's PostgreSQL database are required.

## Production infrastructure

As of October 26, 2026, the operator has provisioned its independent
PostgreSQL datastore, applied migrations 1–3, configured `GENYCASTER_DATABASE_URL`
outside Git, and enabled HTTPS `/api/v1/cloud/` through Nginx. Live probes
confirmed `200 {"items":[]}` for `app_updates` and `401
{"error":"authentication_required"}` for unauthenticated private reads.

If the DB is unavailable, the client must keep the local edit/outbox rather
than implying that a remote write succeeded. The API responds with JSON,
never its browser app shell. Browser clients only contact the same-origin
`api.example.com` API; they do not see PostgreSQL credentials.

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
  `(issuer='https://identity.example.com', subject='farcaster:<verified FID>')`.
  This is intentionally consistent across different devices and login paths.
  Never export the temporary bearer to QML, JavaScript or persistent storage.

Changing the current viewer FID cancels pending network operations and
selects a different per-account local outbox and cursor.

## Persistent account access

Genycaster supports two independently verified ways to establish a
30-day browser account session:

- An explicit OpenProof/Farcaster grant uses POST /api/v1/cloud/session.
- An already **Snapchain-registered** device signer uses POST
  /api/v1/cloud/session/challenge followed by POST /api/v1/cloud/session/proof.
  The 90-second nonce is one-time and domain-separated, signed with the
  non-extractable Ed25519 browser key. The API validates the signature
  and the **live FID-scoped signer registration**, including scopes and
  expiry, directly with the fixed Snapchain endpoint. A remembered FID,
  an unregistered signer and a mere local session marker grant nothing.
  The browser performs this ceremony silently on registered signer
  restoration; no private key or custody phrase is sent to the server.

The account database remains canonical. PostgreSQL stores only a SHA-256
session-token digest. A Secure, HttpOnly, SameSite=Strict host-only cookie
belongs to the official Genycaster origin. A same-origin GET /session confirms
the browser retained the cookie and returns the canonical FID. The Qt client
receives only the matching FID and an in-memory CSRF value, never the
HttpOnly cookie or a persistent OpenProof bearer. Cookie-authorized writes
require the CSRF header. DELETE /session revokes the browser session.

The session-proof routes require browser same-origin requests, enforce
single-use nonce replay prevention, strict signer-key matching and bounded
Snapchain reads. Signer scopes for Farcaster protocol writes remain separate:
the account proof signs **only** a Genycaster account challenge, not a Cast.

The updated backend remains schema-v3 compatible: it continues to serve
existing account data if PM2 restarts before migration 004, while the new
session endpoints fail closed. Migration 004 is still mandatory to **enable**
persisted account sessions. Apply it with the DB administrator, verify the
restricted API role, then restart the server and test same-origin browser
cookies before publishing the new WASM client.

The production Nginx `/api/v1/cloud/` proxy currently limits request bodies
to 24 KiB while the API explicitly supports 192 KiB rich drafts. Increase
`client_max_body_size` for **that location only** to at least `208k`,
validate with `nginx -t`, and reload Nginx before claiming full-size draft
support. Do not alter unrelated upload or API limits.

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

Profile-specific device caches use the stable `farcaster:<FID>` key regardless
of sign-in route. Existing route-scoped QSettings entries are copied without
erasure; a previously queued, unsent edit is preferred over an older clean
local cache. The server remains canonical for saved profile data. Account
switching waits for the old session's cookie revocation before requesting a
new session, avoiding a late sign-out response clearing the new account.

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

`GET /api/v1/cloud/topics` — **authenticated GenyCaster account**
ordered topic slugs and counts; the API returns no per-language display names.
Clients localize the slug locally.

`GET /api/v1/cloud/topic_casts?topic=art&limit=30&offset=0` — **authenticated**
bounded filter returning matching Cast hashes and FIDs. The client's Farcaster
reader remains responsible for resolving the protocol Cast payloads.
An optional `author_fid` restricts results to a single author. Both reads
require either a server-verified, linked-FID bearer grant or a valid same-origin
GenyCaster HttpOnly account session; unauthenticated access returns HTTP 401.
Origin, User-Agent and embedded shared client keys do **not** prove a particular
native application and must not be treated as authorization. The protected API
is available to verified account holders; exclusive executable identity would
require independently validated platform/device attestation where supported.

`PUT /api/v1/cloud/cast_topics` — authenticated post-publication topic assignment
with `{ "cast_hash": "0x…", "topics": ["art", "photography"] }`. The server
checks the Cast hash and verified owner against Snapchain before writing.
Validation is strict, maximum three unique active slugs, no channel or
`parent_url` dependency. A locally queued assignment may retry while the
Cast is waiting for Snapchain indexing. Adding a topic is an administrative
catalog operation, never an unauthenticated or user-supplied new slug.

`GET|PUT /api/v1/cloud/bookmark_collections` — authenticated private
collection listing and revision-safe create/rename/delete. Create requires
`{ "action":"create","id":"<uuid>","name":"Inspiration" }`;
rename/delete require the current `expected_revision`. A collection deletion
only removes its membership links, never the saved Cast itself. Name uniqueness,
count limits and ownership are enforced in PostgreSQL.

`GET /api/v1/cloud/bookmark_collection_items?collection_id=<uuid>`
and `PUT /api/v1/cloud/bookmark_collection_items` — authenticated listing
and attaching/removing saved Cast hashes using
`{ "collection_id":"<uuid>","cast_hash":"0x<40 hex>","remove":false }`.
One bookmark can belong to multiple collections. Unsaving a bookmark removes
its collection memberships in the same DB transaction via a trigger.
Migration 007 is required; while unapplied, signed-in requests get 503.
The Composer's public Cast Topics and personal bookmark collections
remain independent entities, and no manual account-sync UI is required.

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

The URLs below are documentation placeholders. Replace `api.example.com` with your own authorized API origin.

```bash
node --test services/account-sync/test/*.test.mjs
curl -i https://api.example.com/api/v1/cloud/app_updates
curl -i https://api.example.com/api/v1/cloud/profile_extras
```

Expected anonymous responses are `200` JSON and `401` JSON respectively.
Full cross-device acceptance requires two real authenticated sessions:
save/un-save on device A, confirm device B receives the change, then repeat
for profile links and LinkedIn; test offline replay, logout/account switch,
concurrent edits, and database backup/restore.

No actual private user token should appear in logs or be pasted into tickets.

### GenyCaster user-selected profile status

`GET /api/v1/cloud/profile_status?fid=<verified-farcaster-fid>` exposes
only an explicitly public, user-selected status. It never claims to reflect
live socket presence or online availability. Private or cleared statuses
return `{"item":null}`, with no authentication or social-data scraping.

`GET /api/v1/cloud/profile_status` requires the verified owner's account
session and includes private status. `PUT /api/v1/cloud/profile_status`
requires the same account session (and CSRF for cookie sessions). Body:
`{mood,note,visible,revision}`. Mood is a bounded curated identifier;
`note` is user-authored (up to 72 Unicode scalars). Expected `revision`
protects against conflicting writes on another device. The server ignores
any caller-supplied FID; ownership is derived from the verified session.

Data is persisted as a reserved GenyCaster-owned `notifications` row
(ID `00000000-0000-4000-8000-000000000003`). Generic notification
reads and writes exclude that row and cannot impersonate the internal
record. This needs no new database role, schema privileges, or migrations.


### Market snapshot (read-only)

The Stats → Market dashboard reads `GET /api/v1/cloud/market` through the
existing first-party HTTPS API proxy. This endpoint is intentionally **public,
read-only**, unlike authenticated account state; no account cookie, FID, or
wallet information is needed. Each market metric is fetched **once per server
cache interval** across every connected desktop, mobile and WASM client.

For production CoinMarketCap market indicators configure
`GENYCASTER_CMC_API_KEY` in the account API process environment (for example
`/etc/genycaster/account-api.env`, never a shipped QML/WASM asset), using
a plan whose terms permit GenyCaster's application use. Restart the
`genycaster-account-api` PM2 service after configuration. The implementation
uses the authenticated CMC Pro API, not the rate-limited keyless evaluation API.

Without a key, the endpoint returns an honestly **partial** snapshot: the
Fear & Greed indicator from Alternative.me, credited and linked in the UI;
CMC-specific cards show unavailable, never fabricated values. Each indicator
has independent TTL/single-flight caching and stale-data fallback; upstream
exceptions and credentials are not forwarded to clients. Add provider-supplied
attribution and respect CMC API subscription and redistribution restrictions.
