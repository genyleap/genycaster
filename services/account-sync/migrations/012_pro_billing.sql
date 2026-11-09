-- Product-owned GENY Pro receipts. Only an authenticated account can request a
-- quote, but only the server can mark an onchain payment confirmed. Changes
-- are transactionally serialized per owner and protected by a global hash key.
BEGIN;
CREATE TABLE IF NOT EXISTS pro_quotes (
  id uuid PRIMARY KEY,
  issuer text NOT NULL CHECK (length(issuer) BETWEEN 8 AND 2048),
  subject text NOT NULL CHECK (subject ~ '^farcaster:[1-9][0-9]{0,18}$'),
  price_usd_cents integer NOT NULL CHECK (price_usd_cents BETWEEN 1 AND 999999),
  geny_price_usd text NOT NULL CHECK (length(geny_price_usd) BETWEEN 1 AND 48),
  amount_base_units numeric(78,0) NOT NULL CHECK (amount_base_units > 0),
  token varchar(42) NOT NULL,
  treasury varchar(42) NOT NULL,
  chain_id integer NOT NULL DEFAULT 8453 CHECK (chain_id = 8453),
  created_at timestamptz NOT NULL,
  expires_at timestamptz NOT NULL CHECK (expires_at > created_at),
  status text NOT NULL DEFAULT 'quoted' CHECK (status IN ('quoted','paid')),
  tx_hash varchar(66) UNIQUE,
  paid_at timestamptz
);
CREATE INDEX IF NOT EXISTS pro_quotes_owner_recent
  ON pro_quotes(issuer,subject,created_at DESC);
CREATE TABLE IF NOT EXISTS pro_payments (
  tx_hash varchar(66) PRIMARY KEY CHECK (length(tx_hash)=66),
  quote_id uuid NOT NULL UNIQUE REFERENCES pro_quotes(id),
  issuer text NOT NULL,
  subject text NOT NULL,
  price_usd_cents integer NOT NULL,
  amount_base_units numeric(78,0) NOT NULL,
  payer varchar(42) NOT NULL,
  block_number bigint NOT NULL,
  block_hash varchar(66) NOT NULL,
  confirmed_at timestamptz NOT NULL DEFAULT now()
);
CREATE TABLE IF NOT EXISTS pro_subscriptions (
  issuer text NOT NULL,
  subject text NOT NULL,
  expires_at timestamptz NOT NULL,
  updated_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY(issuer,subject)
);
CREATE INDEX IF NOT EXISTS pro_subscriptions_expiry
  ON pro_subscriptions(expires_at);
GRANT SELECT,INSERT ON TABLE pro_quotes TO genycaster_api;
GRANT UPDATE(status,tx_hash,paid_at) ON TABLE pro_quotes TO genycaster_api;
GRANT SELECT,INSERT ON TABLE pro_payments TO genycaster_api;
GRANT SELECT,INSERT,UPDATE ON TABLE pro_subscriptions TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(12) ON CONFLICT DO NOTHING;
COMMIT;
