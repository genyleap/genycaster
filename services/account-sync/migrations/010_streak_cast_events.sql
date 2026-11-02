-- Streak v1: verified, immutable per-cast evidence scoped to the account.
-- The server owns the UTC credit date; the client cannot backdate progress.
BEGIN;
CREATE TABLE IF NOT EXISTS streak_cast_events (
    issuer text NOT NULL CHECK (length(issuer) BETWEEN 8 AND 2048),
    subject text NOT NULL CHECK (length(subject) BETWEEN 1 AND 512),
    cast_hash bytea NOT NULL CHECK (octet_length(cast_hash)=20),
    kind text NOT NULL CHECK (kind IN ('cast','reply')),
    credited_on date NOT NULL DEFAULT (now() AT TIME ZONE 'UTC')::date,
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY(issuer,subject,cast_hash)
);
CREATE INDEX IF NOT EXISTS streak_cast_events_recent
    ON streak_cast_events(issuer,subject,credited_on DESC);
GRANT SELECT, INSERT ON TABLE streak_cast_events TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(10) ON CONFLICT DO NOTHING;
COMMIT;
