-- Allow Pro quotes for the verified HyperSnap-native account identity.
-- Migration 012 assumed every account was Farcaster. Keep its accepted
-- Farcaster subjects intact and allow only the canonical native issuer/FID.
-- Wallet ownership remains proved by the independent one-use EIP-191 flow.
--
-- Run under a database-owner role, never grant schema ALTER to the API role.
-- PostgreSQL DDL and schema_migrations insertion are one atomic transaction.
BEGIN;
SET LOCAL lock_timeout = '5s';
SET LOCAL statement_timeout = '30s';

ALTER TABLE pro_quotes
  DROP CONSTRAINT pro_quotes_subject_check;

ALTER TABLE pro_quotes
  ADD CONSTRAINT pro_quotes_subject_check CHECK (
    CASE WHEN issuer = 'hypersnap:native-registry' THEN
      CASE WHEN subject ~ '^hypersnap:[1-9][0-9]{9,15}

INSERT INTO schema_migrations(version) VALUES(16)
  ON CONFLICT DO NOTHING;
COMMIT;
 THEN
        split_part(subject, ':', 2)::numeric
          BETWEEN 4294967296 AND 9007199254740991
      ELSE false END
    ELSE subject ~ '^farcaster:[1-9][0-9]{0,18}

INSERT INTO schema_migrations(version) VALUES(16)
  ON CONFLICT DO NOTHING;
COMMIT;
 END
  );

INSERT INTO schema_migrations(version) VALUES(16)
  ON CONFLICT DO NOTHING;
COMMIT;
