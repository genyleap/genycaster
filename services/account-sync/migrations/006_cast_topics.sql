-- GenyCaster-managed cast topics. Independent of Farcaster channel/parent_url.
-- Topics are global metadata; owners are independently authenticated by FID.
BEGIN;
CREATE TABLE IF NOT EXISTS topics (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    slug varchar(64) NOT NULL UNIQUE
      CHECK (slug ~ '^[a-z][a-z0-9-]{1,63}$'),
    sort_order integer NOT NULL DEFAULT 1000,
    is_active boolean NOT NULL DEFAULT true,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS topics_discovery
  ON topics (is_active, sort_order, id);
CREATE TABLE IF NOT EXISTS cast_topics (
    cast_hash bytea NOT NULL CHECK (octet_length(cast_hash)=20),
    topic_id bigint NOT NULL REFERENCES topics(id) ON DELETE RESTRICT,
    author_fid bigint NOT NULL CHECK (author_fid>0),
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (cast_hash, topic_id)
);
CREATE INDEX IF NOT EXISTS cast_topics_filter
  ON cast_topics (topic_id, created_at DESC, cast_hash DESC);
CREATE INDEX IF NOT EXISTS cast_topics_author
  ON cast_topics (author_fid, created_at DESC);
INSERT INTO topics (slug,sort_order) VALUES
 ('art',10),('development',20),('photography',30),
 ('design',40),('technology',50),('music',60),('writing',70),
 ('science',80),('business',90),('gaming',100)
 ON CONFLICT (slug) DO NOTHING;
-- The application role can read and associate verified casts but cannot
-- create, rename or deactivate topics or change the schema.
GRANT SELECT ON TABLE topics TO genycaster_api;
GRANT SELECT, INSERT, DELETE ON TABLE cast_topics TO genycaster_api;
INSERT INTO schema_migrations(version) VALUES(6) ON CONFLICT DO NOTHING;
COMMIT;
