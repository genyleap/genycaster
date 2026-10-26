-- Genycaster account-owned bookmark preview + explicit priority.
-- Public Farcaster content here is a bounded display snapshot, never proof.
-- Run as postgres in genycaster_db, after schema v4.
BEGIN;
ALTER TABLE bookmarks
  ADD COLUMN IF NOT EXISTS preview jsonb,
  ADD COLUMN IF NOT EXISTS priority smallint NOT NULL DEFAULT 0;
ALTER TABLE bookmark_changes
  ADD COLUMN IF NOT EXISTS preview jsonb,
  ADD COLUMN IF NOT EXISTS priority smallint NOT NULL DEFAULT 0;
DO $$
BEGIN
  IF NOT EXISTS (SELECT 1 FROM pg_constraint
                 WHERE conname='genycaster_bookmark_preview_object') THEN
    ALTER TABLE bookmarks ADD CONSTRAINT genycaster_bookmark_preview_object
      CHECK (preview IS NULL OR (jsonb_typeof(preview)='object'
                                 AND pg_column_size(preview)<=8192));
  END IF;
  IF NOT EXISTS (SELECT 1 FROM pg_constraint
                 WHERE conname='genycaster_bookmark_priority_range') THEN
    ALTER TABLE bookmarks ADD CONSTRAINT genycaster_bookmark_priority_range
      CHECK (priority BETWEEN 0 AND 2);
  END IF;
  IF NOT EXISTS (SELECT 1 FROM pg_constraint
                 WHERE conname='genycaster_bookmark_change_priority_range') THEN
    ALTER TABLE bookmark_changes ADD CONSTRAINT genycaster_bookmark_change_priority_range
      CHECK (priority BETWEEN 0 AND 2);
  END IF;
END $$;
CREATE INDEX IF NOT EXISTS bookmarks_account_priority
 ON bookmarks(openproof_issuer,openproof_subject,priority DESC,changed_at DESC)
 WHERE is_deleted=false;
INSERT INTO schema_migrations(version) VALUES(5) ON CONFLICT DO NOTHING;
COMMIT;
