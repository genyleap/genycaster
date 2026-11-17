-- Verified app-owned payer addresses. Never accepts addresses merely reported
-- by eth_requestAccounts; only a one-use EIP-191 signature may insert.
BEGIN;
CREATE TABLE IF NOT EXISTS pro_wallet_challenges (
  id uuid PRIMARY KEY,
  issuer text NOT NULL,
  subject text NOT NULL,
  address text NOT NULL CHECK (address ~ '^0x[0-9a-f]{40}$'),
  message text NOT NULL CHECK (length(message) BETWEEN 100 AND 2500),
  created_at timestamptz NOT NULL DEFAULT now(),
  expires_at timestamptz NOT NULL DEFAULT (now()+interval '5 minutes'),
  consumed_at timestamptz,
  CHECK (expires_at > created_at)
);
CREATE INDEX IF NOT EXISTS pro_wallet_challenges_owner
  ON pro_wallet_challenges(issuer,subject,created_at DESC);
CREATE INDEX IF NOT EXISTS pro_wallet_challenges_expiry
  ON pro_wallet_challenges(expires_at);
CREATE TABLE IF NOT EXISTS pro_wallets (
  issuer text NOT NULL,
  subject text NOT NULL,
  address text NOT NULL CHECK(address ~ '^0x[0-9a-f]{40}$'),
  verified_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY(issuer,subject,address)
);
GRANT SELECT,INSERT,UPDATE(consumed_at),DELETE ON TABLE pro_wallet_challenges TO genycaster_api;
GRANT SELECT,INSERT,UPDATE(verified_at) ON TABLE pro_wallets TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(15) ON CONFLICT DO NOTHING;
COMMIT;
