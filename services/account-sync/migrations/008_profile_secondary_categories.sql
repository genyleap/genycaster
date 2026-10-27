-- Up to three owner-managed, canonical secondary professional categories.
-- Distinct from Cast topics. Existing records keep an empty selection.
BEGIN;
ALTER TABLE profile_extras
  ADD COLUMN IF NOT EXISTS secondary_categories jsonb NOT NULL DEFAULT '[]'::jsonb;
ALTER TABLE profile_extras
  DROP CONSTRAINT IF EXISTS profile_secondary_categories_bounded;
ALTER TABLE profile_extras
  ADD CONSTRAINT profile_secondary_categories_bounded
    CHECK (jsonb_typeof(secondary_categories) = 'array'
       AND jsonb_array_length(secondary_categories) <= 3);
INSERT INTO schema_migrations(version) VALUES(8) ON CONFLICT DO NOTHING;
COMMIT;
