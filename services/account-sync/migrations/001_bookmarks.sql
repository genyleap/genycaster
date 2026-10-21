-- Apply only inside a separately created PostgreSQL database named "genycaster".
-- No OpenProof identities, providers, verified claims, OAuth credentials, FIDs,
-- protocol profiles or token metadata are copied into these tables.
BEGIN;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version integer PRIMARY KEY,
    applied_at timestamptz NOT NULL DEFAULT now()
);

-- Client-specific bookmarks: a delete is an explicit tombstone, not physical
-- removal, so disconnected devices cannot resurrect a deleted bookmark.
CREATE TABLE IF NOT EXISTS bookmarks (
    openproof_subject text NOT NULL CHECK (length(openproof_subject) BETWEEN 1 AND 512),
    cast_hash bytea NOT NULL CHECK (octet_length(cast_hash) = 20),
    is_deleted boolean NOT NULL DEFAULT false,
    changed_seq bigint NOT NULL,
    changed_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (openproof_subject, cast_hash)
);

-- Append-only application-level mutation receipts. The unique idempotency
-- key is scoped to the verified OpenProof subject, never to a self-declared FID.
CREATE TABLE IF NOT EXISTS bookmark_changes (
    sequence bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    openproof_subject text NOT NULL CHECK (length(openproof_subject) BETWEEN 1 AND 512),
    mutation_id uuid NOT NULL,
    cast_hash bytea NOT NULL CHECK (octet_length(cast_hash) = 20),
    is_deleted boolean NOT NULL,
    changed_at timestamptz NOT NULL DEFAULT now(),
    UNIQUE (openproof_subject, mutation_id)
);

CREATE INDEX IF NOT EXISTS bookmark_changes_by_account
    ON bookmark_changes (openproof_subject, sequence);

-- The bookmark row points to an immutable change receipt. There are no
-- foreign keys into OpenProof or any external provider database.
DO $do$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM pg_constraint
        WHERE conname = 'bookmarks_change_sequence_fk'
          AND conrelid = 'bookmarks'::regclass
    ) THEN
        ALTER TABLE bookmarks
            ADD CONSTRAINT bookmarks_change_sequence_fk
            FOREIGN KEY (changed_seq) REFERENCES bookmark_changes(sequence);
    END IF;
END
$do$;

INSERT INTO schema_migrations (version) VALUES (1)
ON CONFLICT (version) DO NOTHING;

COMMIT;
