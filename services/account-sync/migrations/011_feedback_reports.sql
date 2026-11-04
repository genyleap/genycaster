-- Account-scoped feedback and server-reviewed rewards. Client role can
-- submit and read its own reports through the authenticated API, but cannot
-- alter moderation status, reward points or ownership.
BEGIN;
CREATE TABLE IF NOT EXISTS feedback_reports (
    id uuid PRIMARY KEY,
    issuer text NOT NULL CHECK (length(issuer) BETWEEN 8 AND 2048),
    subject text NOT NULL CHECK (subject ~ '^farcaster:[1-9][0-9]{0,18}$'),
    category text NOT NULL CHECK (category IN ('bug','performance','improvement','idea')),
    sentiment smallint NOT NULL CHECK (sentiment BETWEEN 1 AND 5),
    message text NOT NULL CHECK (length(message) BETWEEN 10 AND 16000),
    platform varchar(32) NOT NULL DEFAULT '',
    app_version varchar(40) NOT NULL DEFAULT '',
    route varchar(96) NOT NULL DEFAULT '',
    status text NOT NULL DEFAULT 'submitted'
        CHECK (status IN ('submitted','triaged','accepted','resolved','rejected')),
    points_awarded integer NOT NULL DEFAULT 0 CHECK (points_awarded BETWEEN 0 AND 100000),
    review_note text NOT NULL DEFAULT '',
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS feedback_reports_owner_recent
    ON feedback_reports (issuer,subject,created_at DESC);
GRANT SELECT, INSERT ON TABLE feedback_reports TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(11) ON CONFLICT DO NOTHING;
COMMIT;
