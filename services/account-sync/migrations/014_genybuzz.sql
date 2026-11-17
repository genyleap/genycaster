-- GenyBuzz is an application-owned, opt-in attention signal, NOT a Cast,
-- protocol message, wallet operation, or a future DM transport.
-- Both parties are identified by verified application account authorities.
BEGIN;
CREATE TABLE IF NOT EXISTS genybuzz_preferences (
  issuer text NOT NULL CHECK (length(issuer) BETWEEN 8 AND 2048),
  subject text NOT NULL CHECK (subject ~ '^(farcaster|hypersnap):[1-9][0-9]{0,18}$'),
  enabled boolean NOT NULL DEFAULT false,
  revision bigint NOT NULL DEFAULT 1 CHECK (revision>0),
  updated_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY(issuer,subject)
);
CREATE TABLE IF NOT EXISTS genybuzz_events (
  id uuid PRIMARY KEY,
  sender_issuer text NOT NULL,
  sender_subject text NOT NULL,
  recipient_issuer text NOT NULL,
  recipient_subject text NOT NULL,
  context text NOT NULL CHECK (context IN ('profile','dm')),
  created_at timestamptz NOT NULL DEFAULT now(),
  expires_at timestamptz NOT NULL DEFAULT (now()+interval '24 hours'),
  seen_at timestamptz,
  CONSTRAINT genybuzz_not_self CHECK
    ((sender_issuer,sender_subject)<>(recipient_issuer,recipient_subject)),
  CONSTRAINT genybuzz_valid_expiry CHECK (expires_at>created_at)
);
CREATE INDEX IF NOT EXISTS genybuzz_inbox
  ON genybuzz_events(recipient_issuer,recipient_subject,created_at DESC)
  WHERE seen_at IS NULL;
CREATE INDEX IF NOT EXISTS genybuzz_sender_recent
  ON genybuzz_events(sender_issuer,sender_subject,created_at DESC);
CREATE INDEX IF NOT EXISTS genybuzz_recipient_recent
  ON genybuzz_events(recipient_issuer,recipient_subject,created_at DESC);
CREATE INDEX IF NOT EXISTS genybuzz_expiry
  ON genybuzz_events(expires_at);
GRANT SELECT,INSERT,UPDATE ON TABLE genybuzz_preferences TO genycaster_api;
GRANT SELECT,INSERT,UPDATE(seen_at),DELETE ON TABLE genybuzz_events TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(14) ON CONFLICT DO NOTHING;
COMMIT;
