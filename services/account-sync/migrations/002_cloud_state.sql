-- Application-owned data only. OpenProof remains the identity authority.
-- Apply after 001_bookmarks.sql in the dedicated application PostgreSQL database.
BEGIN;

CREATE TABLE IF NOT EXISTS connections (
    issuer text NOT NULL CHECK (length(issuer) BETWEEN 8 AND 2048),
    subject text NOT NULL CHECK (length(subject) BETWEEN 1 AND 512),
    provider text NOT NULL CHECK (provider ~ '^[a-z][a-z0-9-]{0,39}$'),
    display_name text CHECK (length(display_name) <= 256),
    preferred_username text CHECK (length(preferred_username) <= 128),
    provider_profile_url text CHECK (length(provider_profile_url) <= 2048),
    manual_profile_url text CHECK (length(manual_profile_url) <= 2048),
    publicly_visible boolean NOT NULL DEFAULT false,
    last_confirmed_at timestamptz,
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer, subject, provider)
);
COMMENT ON TABLE connections IS 'Non-authoritative account display cache and manually asserted URLs, never a login or proof store.';
COMMENT ON COLUMN connections.manual_profile_url IS 'Self-asserted user URL; not attested or verified by the external provider.';

CREATE TABLE IF NOT EXISTS notifications (
    issuer text NOT NULL,
    subject text NOT NULL,
    id uuid NOT NULL,
    source text NOT NULL CHECK (length(source) BETWEEN 1 AND 40),
    kind text NOT NULL CHECK (length(kind) BETWEEN 1 AND 64),
    payload jsonb NOT NULL DEFAULT '{}'::jsonb CHECK (jsonb_typeof(payload) = 'object'),
    occurred_at timestamptz NOT NULL,
    read_at timestamptz,
    deleted_at timestamptz,
    revision bigint NOT NULL DEFAULT 1 CHECK (revision > 0),
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer, subject, id)
);
CREATE INDEX IF NOT EXISTS notifications_recent ON notifications (issuer, subject, occurred_at DESC);

CREATE TABLE IF NOT EXISTS app_updates (
    release_id text PRIMARY KEY CHECK (release_id ~ '^[A-Za-z0-9._-]{1,80}$'),
    title text NOT NULL CHECK (length(title) BETWEEN 1 AND 180),
    description text NOT NULL CHECK (length(description) BETWEEN 1 AND 8000),
    changes jsonb NOT NULL DEFAULT '[]'::jsonb CHECK (jsonb_typeof(changes) = 'array'),
    published_at timestamptz NOT NULL,
    visible boolean NOT NULL DEFAULT false
);
COMMENT ON TABLE app_updates IS 'Published by release administrators only; user-facing API must be read-only.';

CREATE TABLE IF NOT EXISTS tasks (
    issuer text NOT NULL,
    subject text NOT NULL,
    id uuid NOT NULL,
    kind text NOT NULL CHECK (kind IN ('scheduled_cast', 'scheduled_article', 'reminder')),
    schedule_at timestamptz NOT NULL,
    timezone text NOT NULL CHECK (length(timezone) BETWEEN 1 AND 100),
    payload jsonb NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
    state text NOT NULL DEFAULT 'draft' CHECK (state IN ('draft', 'scheduled', 'processing', 'published', 'failed', 'cancelled')),
    revision bigint NOT NULL DEFAULT 1 CHECK (revision > 0),
    attempt_count integer NOT NULL DEFAULT 0 CHECK (attempt_count >= 0),
    idempotency_key uuid NOT NULL,
    claimed_at timestamptz,
    executed_at timestamptz,
    deleted_at timestamptz,
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer, subject, id),
    UNIQUE (issuer, subject, idempotency_key)
);
CREATE INDEX IF NOT EXISTS scheduled_tasks_pending ON tasks(schedule_at) WHERE state = 'scheduled' AND deleted_at IS NULL;
COMMENT ON TABLE tasks IS 'Schedule definitions only. No unattended protocol signing is performed until a separate authorized worker exists.';

CREATE TABLE IF NOT EXISTS drafts (
    issuer text NOT NULL,
    subject text NOT NULL,
    id uuid NOT NULL,
    kind text NOT NULL CHECK (kind IN ('cast', 'article', 'thread')),
    title text NOT NULL DEFAULT '' CHECK (length(title) <= 256),
    content jsonb NOT NULL CHECK (jsonb_typeof(content) = 'object'),
    revision bigint NOT NULL DEFAULT 1 CHECK (revision > 0),
    deleted_at timestamptz,
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer, subject, id)
);
CREATE INDEX IF NOT EXISTS drafts_latest ON drafts(issuer, subject, updated_at DESC);

INSERT INTO schema_migrations(version) VALUES (2) ON CONFLICT DO NOTHING;
COMMIT;
