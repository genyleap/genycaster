-- Retain public-key tombstones to make offline revocation replay-safe.
BEGIN;
ALTER TABLE cloud_device_credentials ADD COLUMN IF NOT EXISTS revoked_at timestamptz;
CREATE INDEX IF NOT EXISTS cloud_device_principal_history_idx
    ON cloud_device_credentials(issuer,subject);
INSERT INTO schema_migrations(version) VALUES(18) ON CONFLICT DO NOTHING;
COMMIT;
