# Genycaster Account API — optional first-party extension

This optional Node.js service is **not** OpenProof and does not own any linked
identities. It offers a bounded, cache-coalesced proxy for the public OpenProof
provider catalog at `GET /api/v1/connections/providers`, and `GET /healthz`.

Configuration: `GENYCASTER_OPENPROOF_PROVIDER_CATALOG_URL` (required HTTPS
public catalog), `GENYCASTER_API_BIND_ADDRESS` (default loopback),
`GENYCASTER_API_PORT` (default 8797). Reverse-proxy only the versioned API
prefix through the same user-facing Genycaster origin. No production routes
should be switched before the service has startup, security and HTTPS tests.

Migration files under `migrations/` prepare the separately provisioned
PostgreSQL **genycaster** database for future bookmark synchronization. They
do not store OpenProof accounts or connections. Migration 000 requires DBA
authorization; migration 001 belongs to the restricted schema owner. The
database and any private bookmark endpoint MUST NOT be claimed active
until server-verified OpenProof subject binding, migration, optimistic conflict
handling and device sync integration are implemented and tested. The API
currently returns 404 for bookmark endpoints by design.

No OAuth access/refresh token is logged, cached, persisted or forwarded by the
public catalog API. Account linking itself remains at the authenticated
OpenProof browser account portal, never in this public API.
