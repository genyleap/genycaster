# GenyCaster Topics

Topics classify casts independently of Farcaster channels. No `parent_url`,
`channel_id`, or extra protocol fields are generated for topic selection.

## Database

Apply `migrations/006_cast_topics.sql` to the **GenyCaster-owned** PostgreSQL
database after schema v5, using an authorized database operator. It is additive
and seeds Art, Development and Photography plus seven additional topics.

`topics` is the server-managed catalog: `slug`, ordering, active status.
`cast_topics` stores a many-to-many relationship from the immutable cast hash
to the selected topic IDs, with the verified session FID as an ownership claim.
The same cast can have up to three topics, enforced by the API.

Adding a topic never requires rebuilding the client:

```sql
INSERT INTO topics (slug, sort_order) VALUES ('architecture', 110)
ON CONFLICT (slug) DO NOTHING;
```

A client displays an unknown slug with a readable title-cased fallback.
Localized strings for the ten built-in topics are bundled in the UI catalogs.

## Account API

- `GET /api/v1/cloud/topics` — public active catalog and cast counts.
- `PUT /api/v1/cloud/cast_topics` — authenticated account-scoped assignment,
  e.g. `{"cast_hash":"0x...","topics":["art","photography"]}`.
- `GET /api/v1/cloud/topic_casts?topic=art&limit=30&offset=0` — paginated
  topic cast hashes plus the session-authenticated claiming author FID.

The client queues an association locally until the Farcaster CastAdd is
**confirmed with a canonical hash**. Network failures retain account-scoped
outbox entries for retry. This is GenyCaster metadata, **not part of Farcaster
CastAdd protocol data**.

**Security note:** the topic API verifies the account FID and prevents a
different FID from overwriting an existing cast's topic assignment. It does
not yet independently attest that a claimed cast hash was authored by that FID.
Before displaying results in discovery, consumers must compare the returned
`author_fid` against the authoritative Farcaster cast author; a server-side
cast-authorship verifier is a prerequisite to considering stored counts fully
authoritative. Do not use topic records as identity or protocol proof.
