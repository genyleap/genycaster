-- Extend Pro quote ownership to verified HyperSnap-native accounts.
-- Migration 012 constrained every quote to a Farcaster subject. Wallet proof
-- already accepts both protocols, but a verified HyperSnap payer could not
-- insert a quote. Retain the original Farcaster format for non-native issuers.
-- The native issuer must be paired with a safe, canonical HyperSnap FID.
-- No existing rows or payment receipts are modified.
BEGIN;
SET LOCAL lock_timeout = '5s';
SET LOCAL statement_timeout = '30s';

ALTER TABLE pro_quotes
  DROP CONSTRAINT pro_quotes_subject_check;

ALTER TABLE pro_quotes
  ADD CONSTRAINT pro_quotes_subject_check CHECK (
    CASE
      WHEN issuer = 'hypersnap:native-registry' THEN
        CASE
          WHEN subject ~ '^hypersnap:[1-9][0-9]{9,15}$' THEN
            split_part(subject, ':', 2)::numeric
              BETWEEN 4294967296 AND 9007199254740991
          ELSE false
        END
      ELSE
        subject ~ '^farcaster:[1-9][0-9]{0,18}$'
    END
  );

INSERT INTO schema_migrations(version) VALUES (16)
  ON CONFLICT DO NOTHING;
COMMIT;
