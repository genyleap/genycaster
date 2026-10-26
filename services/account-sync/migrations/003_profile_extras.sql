-- Add owner-managed profile presentation data to Genycaster's own database.
-- OpenProof verifies the principal. These fields are NOT provider-verified.
-- The account database is canonical; an offline device cache grants no identity.
BEGIN;

CREATE TABLE IF NOT EXISTS profile_extras (
    issuer text NOT NULL CHECK (length(issuer) BETWEEN 8 AND 2048),
    subject text NOT NULL CHECK (length(subject) BETWEEN 1 AND 512),
    category text NOT NULL DEFAULT '' CHECK (length(category) <= 80),
    profession text NOT NULL DEFAULT '' CHECK (length(profession) <= 80),
    links jsonb NOT NULL DEFAULT '[]'::jsonb
        CHECK (jsonb_typeof(links) = 'array' AND jsonb_array_length(links) <= 5),
    publicly_visible boolean NOT NULL DEFAULT false,
    revision bigint NOT NULL DEFAULT 1 CHECK (revision > 0),
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer, subject)
);
COMMENT ON TABLE profile_extras IS
    'Owner-authored Genycaster profile category, profession and link labels. Private by default; not an OpenProof attestation.';

INSERT INTO schema_migrations(version) VALUES (3) ON CONFLICT DO NOTHING;
COMMIT;
