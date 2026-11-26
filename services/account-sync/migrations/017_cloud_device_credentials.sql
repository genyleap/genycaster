-- Device credentials are independent of protocol social signing authority.
BEGIN;
CREATE TABLE IF NOT EXISTS cloud_device_credentials (
    issuer text NOT NULL,
    subject text NOT NULL,
    public_key char(64) NOT NULL CHECK(public_key ~ '^[0-9a-f]{64}$'),
    created_at timestamptz NOT NULL DEFAULT now(),
    last_used_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer,subject,public_key),
    UNIQUE(public_key)
);
ALTER TABLE account_sessions ADD COLUMN IF NOT EXISTS device_key char(64);
ALTER TABLE account_sessions ADD COLUMN IF NOT EXISTS auth_source text NOT NULL DEFAULT 'legacy';
DO $$ BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conname='account_sessions_device_credential_fk'
        AND conrelid='account_sessions'::regclass) THEN
        ALTER TABLE account_sessions ADD CONSTRAINT account_sessions_device_credential_fk
          FOREIGN KEY(issuer,subject,device_key)
          REFERENCES cloud_device_credentials(issuer,subject,public_key) ON DELETE CASCADE;
    END IF;
END $$;
CREATE INDEX IF NOT EXISTS account_sessions_device_idx
    ON account_sessions(issuer,subject,device_key) WHERE device_key IS NOT NULL;
GRANT SELECT,INSERT,UPDATE,DELETE ON cloud_device_credentials TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(17) ON CONFLICT DO NOTHING;
COMMIT;
