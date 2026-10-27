-- Extend owner-scoped account profiles with multiple professional roles,
-- and add Trading to the global GenyCaster Cast topic catalog.
-- Existing role and topic associations remain intact.
BEGIN;
ALTER TABLE profile_extras
  ADD COLUMN IF NOT EXISTS roles jsonb NOT NULL DEFAULT '[]'::jsonb;
UPDATE profile_extras
  SET roles = jsonb_build_array(profession)
  WHERE profession <> '' AND roles = '[]'::jsonb;
ALTER TABLE profile_extras
  DROP CONSTRAINT IF EXISTS profile_roles_bounded;
ALTER TABLE profile_extras
  ADD CONSTRAINT profile_roles_bounded
  CHECK (jsonb_typeof(roles) = 'array' AND jsonb_array_length(roles) <= 5);
INSERT INTO topics(slug,sort_order,is_active) VALUES ('trading',105,true)
  ON CONFLICT (slug) DO UPDATE
    SET is_active=true,sort_order=EXCLUDED.sort_order;
INSERT INTO schema_migrations(version) VALUES(9) ON CONFLICT DO NOTHING;
COMMIT;
