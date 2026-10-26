-- Long-lived same-origin account continuity for Genycaster private app data.
-- OpenProof remains the authority at session creation; no OAuth token is
-- persisted. The cookie is random, HttpOnly and its SHA-256 digest alone is stored.
BEGIN;
CREATE TABLE IF NOT EXISTS account_sessions (
    token_hash char(64) PRIMARY KEY CHECK (token_hash ~ '^[0-9a-f]{64}$'),
    issuer text NOT NULL CHECK (length(issuer) BETWEEN 8 AND 2048),
    subject text NOT NULL CHECK (length(subject) BETWEEN 1 AND 512),
    csrf_token char(43) NOT NULL CHECK (csrf_token ~ '^[A-Za-z0-9_-]{43}$'),
    created_at timestamptz NOT NULL DEFAULT now(),
    expires_at timestamptz NOT NULL,
    CHECK (expires_at > created_at)
);
CREATE INDEX IF NOT EXISTS account_sessions_expires_at
    ON account_sessions (expires_at);
COMMENT ON TABLE account_sessions IS
    'Server-verified Genycaster sessions; raw cookie and OpenProof credentials never stored.';
-- The runtime may create, inspect and revoke sessions, but never alter schema
-- or access the OpenProof database. Deployment uses a separate DBA role.
GRANT SELECT, INSERT, DELETE ON TABLE account_sessions TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES (4) ON CONFLICT DO NOTHING;
COMMIT;
