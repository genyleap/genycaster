-- Server-only Neynar FID reservations and paid registration attempts.
-- No public caller may INSERT or UPDATE these tables. The application role
-- can mutate only through explicit, verified first-party request handlers.
BEGIN;
CREATE TABLE IF NOT EXISTS neynar_registration_leases (
  id uuid PRIMARY KEY,
  principal text NOT NULL UNIQUE CHECK (length(principal) BETWEEN 10 AND 160),
  custody varchar(42) NOT NULL UNIQUE CHECK
    (custody ~ '^0x[0-9a-f]{40}$'),
  status text NOT NULL DEFAULT 'reserving'
    CHECK (status IN ('reserving','reservation_unknown','reserved',
      'submitting','submission_unknown','submitted','rejected',
      'already_registered','confirmed')),
  started boolean NOT NULL DEFAULT false,
  fid bigint UNIQUE CHECK (fid IS NULL OR (fid>0 AND fid<=9007199254740991)),
  created_ms bigint NOT NULL CHECK (created_ms>0),
  reserved_ms bigint CHECK (reserved_ms IS NULL OR reserved_ms>=created_ms),
  transaction_hash varchar(66) CHECK
    (transaction_hash IS NULL OR transaction_hash ~ '^0x[0-9a-f]{64}$'),
  updated_at timestamptz NOT NULL DEFAULT now(),
  -- One custody wallet and one paid invoice are each globally single-use.
  -- Prevents concurrent paid FID allocations for the same wallet.
  UNIQUE (principal,custody),
  CHECK (status NOT IN ('reserved','submitting','submitted','confirmed')
         OR (fid IS NOT NULL AND reserved_ms IS NOT NULL))
);
CREATE INDEX IF NOT EXISTS neynar_registration_leases_status
  ON neynar_registration_leases(status,updated_at);
GRANT SELECT,INSERT ON neynar_registration_leases TO genycaster_api;
GRANT UPDATE(started,status,fid,reserved_ms,transaction_hash,updated_at)
  ON neynar_registration_leases TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(13) ON CONFLICT DO NOTHING;
COMMIT;
