# GenyCaster Topics

Topics classify casts independently of Farcaster channels. No `parent_url`,
`channel_id`, or extra protocol fields are generated for topic selection.

## Database

Apply `migrations/006_cast_topics.sql` to the **GenyCaster-owned** PostgreSQL
database after schema v5, using an authorized database operator. It is additive
and seeds Art, Development and Photography plus seven additional topics.\n\nMigration `009_profile_roles_cast_trading.sql` additionally activates the\n`trading` topic. The catalog remains independent of the user's professional\ncategory and role selections; this database migration must be run by the\nPostgreSQL table owner before the topic is offered to clients.

`topics` is the server-managed catalog: `slug`, ordering, active status.
`cast_topics` stores a many-to-many relationship from the immutable cast hash
to the selected topic IDs, with the verified session FID as an ownership claim.
The same cast can have up to three topics, enforced by the API.

Adding a topic never requires rebuilding the client:

```sql
INSERT INTO topics (slug, sort_order) VALUES ('architecture', 110)
ON CONFLICT (slug) DO NOTHING;
```

A client displays an unknown slug with a readable title-cased fallback.\nTrading has a bundled localized label in the desktop/mobile composer and\nExplorer/Profile filter pickers.
Localized strings for the ten built-in topics are bundled in the UI catalogs.

## Account API

- `GET /api/v1/cloud/topics` — authenticated active catalog and cast counts.
- `PUT /api/v1/cloud/cast_topics` — authenticated account-scoped assignment,
  e.g. `{"cast_hash":"0x...","topics":["art","photography"]}`.
- `GET /api/v1/cloud/topic_casts?topic=art&limit=30&offset=0` — paginated
  topic cast hashes plus the session-authenticated claiming author FID.

The client queues an association locally until the Farcaster CastAdd is
**confirmed with a canonical hash**. Network failures retain account-scoped
outbox entries for retry. This is GenyCaster metadata, **not part of Farcaster
CastAdd protocol data**.

**Security note:** topic assignment requires the verified account FID and
an independent read of the canonical `/v1/castById` response from the trusted
Snapchain node. The returned hash, author FID and CastAdd body must all match.
Unindexed, deleted, unavailable or malformed records are rejected rather than
being accepted as client-declared identity. A topic is application metadata,
not part of the signed protocol message or a separate identity attestation.
