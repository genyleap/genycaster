-- Private user-defined GenyCaster bookmark collections. No Farcaster channel/topic coupling.
BEGIN;
CREATE TABLE IF NOT EXISTS bookmark_collections (
    issuer text NOT NULL CHECK (length(issuer) BETWEEN 8 AND 2048),
    subject text NOT NULL CHECK (length(subject) BETWEEN 1 AND 512),
    id uuid NOT NULL,
    name varchar(80) NOT NULL
        CHECK (name=btrim(name) AND length(name) BETWEEN 1 AND 80
               AND name !~ '[[:cntrl:]]'),
    sort_order integer NOT NULL DEFAULT 1000,
    revision bigint NOT NULL DEFAULT 1 CHECK (revision > 0),
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer,subject,id)
);
CREATE UNIQUE INDEX IF NOT EXISTS bookmark_collections_unique_name
  ON bookmark_collections(issuer,subject,lower(name));
CREATE INDEX IF NOT EXISTS bookmark_collections_order
  ON bookmark_collections(issuer,subject,sort_order,id);

CREATE TABLE IF NOT EXISTS bookmark_collection_items (
    issuer text NOT NULL,
    subject text NOT NULL,
    collection_id uuid NOT NULL,
    cast_hash bytea NOT NULL CHECK (octet_length(cast_hash)=20),
    added_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer,subject,collection_id,cast_hash),
    FOREIGN KEY (issuer,subject,collection_id)
      REFERENCES bookmark_collections(issuer,subject,id) ON DELETE CASCADE,
    FOREIGN KEY (issuer,subject,cast_hash)
      REFERENCES bookmarks(openproof_issuer,openproof_subject,cast_hash)
      ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS bookmark_collection_items_cast
  ON bookmark_collection_items(issuer,subject,cast_hash);
-- Keep existing bookmark tombstone semantics: unsaving a cast removes its
-- personal collection memberships, without deleting any global cast Topic.
CREATE OR REPLACE FUNCTION genycaster_prune_unsaved_collection_items()
RETURNS trigger LANGUAGE plpgsql AS $function$
BEGIN
  IF NEW.is_deleted THEN
    DELETE FROM bookmark_collection_items
    WHERE issuer=NEW.openproof_issuer AND subject=NEW.openproof_subject
      AND cast_hash=NEW.cast_hash;
  END IF;
  RETURN NEW;
END;
$function$;
DROP TRIGGER IF EXISTS bookmark_collections_unbookmark ON bookmarks;
CREATE TRIGGER bookmark_collections_unbookmark
  AFTER INSERT OR UPDATE OF is_deleted ON bookmarks
  FOR EACH ROW EXECUTE FUNCTION genycaster_prune_unsaved_collection_items();
GRANT SELECT, INSERT, UPDATE, DELETE ON TABLE bookmark_collections TO genycaster_api;
GRANT SELECT, INSERT, DELETE ON TABLE bookmark_collection_items TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(7) ON CONFLICT DO NOTHING;
COMMIT;
