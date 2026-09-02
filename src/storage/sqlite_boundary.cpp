#include "sqlite_boundary.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <string>
#include <utility>

namespace farcaster::storage::sqlite {
namespace {

[[nodiscard]] auto tableHasColumn(
    Connection& connection,
    const std::string_view tableName,
    const std::string_view columnName) -> std::expected<bool, Error>
{
    if (tableName.empty() || columnName.empty()) {
        return std::unexpected(Error{SQLITE_MISUSE, "SQLite table/column name must not be empty."});
    }

    // Table names cannot be bound as parameters. Callers pass only internal schema constants.
    const std::string query = "PRAGMA table_info(" + std::string{tableName} + ");";
    auto prepared = Statement::prepare(connection, query);
    if (!prepared) {
        return std::unexpected(prepared.error());
    }
    auto statement = std::move(*prepared);
    while (true) {
        const int step = sqlite3_step(statement.native());
        if (step == SQLITE_DONE) {
            return false;
        }
        if (step != SQLITE_ROW) {
            return std::unexpected(makeError(connection.native(), "Failed to inspect SQLite table schema"));
        }
        if (columnText(statement.native(), 1) == columnName) {
            return true;
        }
    }
}

[[nodiscard]] auto recordMigration(
    Connection& connection,
    const std::int64_t version,
    const std::int64_t appliedAt) -> std::expected<void, Error>
{
    auto prepared = Statement::prepare(
        connection,
        "INSERT OR IGNORE INTO schema_migrations(version, applied_at_ms) VALUES(?, ?);");
    if (!prepared) {
        return std::unexpected(prepared.error());
    }
    auto statement = std::move(*prepared);
    if (sqlite3_bind_int64(statement.native(), 1, version) != SQLITE_OK ||
        sqlite3_bind_int64(statement.native(), 2, appliedAt) != SQLITE_OK) {
        return std::unexpected(makeError(connection.native(), "Failed to bind migration metadata"));
    }
    return stepDone(connection, statement);
}

[[nodiscard]] auto migrationApplied(
    Connection& connection,
    const std::int64_t version) -> std::expected<bool, Error>
{
    auto prepared = Statement::prepare(
        connection,
        "SELECT 1 FROM schema_migrations WHERE version = ? LIMIT 1;");
    if (!prepared) {
        return std::unexpected(prepared.error());
    }
    auto statement = std::move(*prepared);
    if (sqlite3_bind_int64(statement.native(), 1, version) != SQLITE_OK) {
        return std::unexpected(makeError(connection.native(), "Failed to bind migration version"));
    }
    const int step = sqlite3_step(statement.native());
    if (step == SQLITE_ROW) {
        return true;
    }
    if (step == SQLITE_DONE) {
        return false;
    }
    return std::unexpected(makeError(connection.native(), "Failed to inspect migration state"));
}

[[nodiscard]] auto migrate(Connection& connection) -> std::expected<void, Error>
{
    constexpr std::string_view schema = R"SQL(
CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY NOT NULL,
    applied_at_ms INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS profiles (
    fid INTEGER PRIMARY KEY NOT NULL,
    username TEXT,
    display_name TEXT,
    bio TEXT NOT NULL,
    avatar_url TEXT,
    banner_url TEXT,
    location TEXT,
    profile_url TEXT,
    custody_address TEXT,
    follower_count INTEGER NOT NULL DEFAULT 0,
    following_count INTEGER NOT NULL DEFAULT 0,
    quality_score REAL
);
CREATE TABLE IF NOT EXISTS casts (
    id TEXT PRIMARY KEY NOT NULL,
    author_fid INTEGER NOT NULL,
    text TEXT NOT NULL,
    parent_id TEXT,
    parent_url TEXT,
    created_at_ms INTEGER NOT NULL,
    reply_count INTEGER NOT NULL DEFAULT 0,
    recast_count INTEGER NOT NULL DEFAULT 0,
    like_count INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS cast_embeds (
    cast_id TEXT NOT NULL,
    position INTEGER NOT NULL,
    url TEXT NOT NULL,
    PRIMARY KEY (cast_id, position),
    FOREIGN KEY (cast_id) REFERENCES casts(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS cast_embed_items (
    cast_id TEXT NOT NULL,
    position INTEGER NOT NULL,
    kind TEXT NOT NULL,
    url TEXT,
    cast_ref_id TEXT,
    cast_ref_fid INTEGER,
    PRIMARY KEY (cast_id, position),
    FOREIGN KEY (cast_id) REFERENCES casts(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS timeline_entries (
    timeline_id TEXT NOT NULL,
    position INTEGER NOT NULL,
    cast_id TEXT NOT NULL,
    sort_time_ms INTEGER NOT NULL,
    PRIMARY KEY (timeline_id, position)
);
CREATE INDEX IF NOT EXISTS idx_timeline_entries_cast ON timeline_entries(cast_id);
CREATE TABLE IF NOT EXISTS continuations (
    token TEXT PRIMARY KEY NOT NULL,
    capability_id TEXT NOT NULL,
    provider_id TEXT NOT NULL,
    provider_cursor TEXT NOT NULL,
    created_at_ms INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS sync_checkpoints (
    sync_key TEXT PRIMARY KEY NOT NULL,
    sequence INTEGER NOT NULL,
    updated_at_ms INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS read_continuations (
    read_key TEXT PRIMARY KEY NOT NULL,
    token TEXT,
    updated_at_ms INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS social_list_entries (
    list_id TEXT NOT NULL,
    position INTEGER NOT NULL,
    source_fid INTEGER NOT NULL,
    target_fid INTEGER NOT NULL,
    relation_kind TEXT NOT NULL,
    PRIMARY KEY (list_id, position)
);
CREATE INDEX IF NOT EXISTS idx_social_list_target ON social_list_entries(target_fid);
CREATE TABLE IF NOT EXISTS client_session (
    singleton_id INTEGER PRIMARY KEY NOT NULL CHECK (singleton_id = 1),
    fid INTEGER NOT NULL,
    username TEXT,
    display_name TEXT,
    avatar_url TEXT,
    custody_address TEXT,
    auth_method TEXT NOT NULL,
    authenticated_at_ms INTEGER NOT NULL,
    expires_at_ms INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS signer_metadata (
    fid INTEGER PRIMARY KEY NOT NULL,
    signer_id TEXT NOT NULL,
    public_key_hex TEXT NOT NULL,
    created_at_ms INTEGER NOT NULL,
    registration_kind TEXT,
    registration_state TEXT,
    scopes TEXT,
    ttl_seconds INTEGER,
    last_used_at_ms INTEGER,
    request_fid INTEGER,
    verified_at_ms INTEGER
);
CREATE TABLE IF NOT EXISTS write_outbox (
    idempotency_key TEXT PRIMARY KEY NOT NULL,
    kind TEXT NOT NULL,
    cast_id TEXT,
    author_fid INTEGER NOT NULL,
    target_cast_id TEXT,
    target_cast_fid INTEGER,
    target_fid INTEGER,
    depends_on_key TEXT,
    signer_public_key_hex TEXT NOT NULL,
    message_bytes BLOB NOT NULL,
    signed_at_ms INTEGER NOT NULL,
    enqueued_at_ms INTEGER NOT NULL,
    status TEXT NOT NULL,
    attempt_count INTEGER NOT NULL DEFAULT 0,
    next_attempt_at_ms INTEGER,
    last_failure_kind TEXT,
    last_failure_message TEXT,
    confirmed_at_ms INTEGER,
    FOREIGN KEY (cast_id) REFERENCES casts(id) ON DELETE RESTRICT
);
CREATE INDEX IF NOT EXISTS idx_write_outbox_ready
    ON write_outbox(status, next_attempt_at_ms, enqueued_at_ms);
CREATE INDEX IF NOT EXISTS idx_write_outbox_author_ready
    ON write_outbox(author_fid, status, next_attempt_at_ms, enqueued_at_ms);
CREATE TABLE IF NOT EXISTS cast_write_state (
    cast_id TEXT PRIMARY KEY NOT NULL,
    idempotency_key TEXT UNIQUE NOT NULL,
    state TEXT NOT NULL,
    updated_at_ms INTEGER NOT NULL,
    FOREIGN KEY (cast_id) REFERENCES casts(id) ON DELETE CASCADE,
    FOREIGN KEY (idempotency_key) REFERENCES write_outbox(idempotency_key) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS cast_local_tombstones (
    cast_id TEXT PRIMARY KEY NOT NULL,
    idempotency_key TEXT UNIQUE NOT NULL,
    author_fid INTEGER NOT NULL,
    created_at_ms INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS cast_reaction_local_state (
    author_fid INTEGER NOT NULL,
    cast_id TEXT NOT NULL,
    target_cast_fid INTEGER NOT NULL,
    reaction_kind TEXT NOT NULL CHECK (reaction_kind IN ('like', 'recast')),
    is_active INTEGER NOT NULL CHECK (is_active IN (0, 1)),
    idempotency_key TEXT NOT NULL,
    updated_at_ms INTEGER NOT NULL,
    PRIMARY KEY (author_fid, cast_id, reaction_kind),
    FOREIGN KEY (idempotency_key) REFERENCES write_outbox(idempotency_key) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_cast_reaction_local_author
    ON cast_reaction_local_state(author_fid, updated_at_ms);
CREATE TABLE IF NOT EXISTS following_local_state (
    author_fid INTEGER NOT NULL,
    target_fid INTEGER NOT NULL,
    is_following INTEGER NOT NULL CHECK (is_following IN (0, 1)),
    idempotency_key TEXT NOT NULL,
    updated_at_ms INTEGER NOT NULL,
    PRIMARY KEY (author_fid, target_fid),
    FOREIGN KEY (idempotency_key) REFERENCES write_outbox(idempotency_key) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_following_local_author
    ON following_local_state(author_fid, updated_at_ms);
)SQL";

    Transaction transaction{connection};
    if (auto status = transaction.begin(); !status) {
        return status;
    }
    if (auto status = connection.execute(schema); !status) {
        return status;
    }

    const auto appliedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (auto status = recordMigration(connection, 2, appliedAt); !status) {
        return status;
    }

    auto hasParentUrl = tableHasColumn(connection, "casts", "parent_url");
    if (!hasParentUrl) {
        return std::unexpected(hasParentUrl.error());
    }
    if (!*hasParentUrl) {
        if (auto status = connection.execute("ALTER TABLE casts ADD COLUMN parent_url TEXT;"); !status) {
            return status;
        }
    }
    if (auto status = recordMigration(connection, 3, appliedAt); !status) {
        return status;
    }

    auto hasCastReferenceFid = tableHasColumn(connection, "cast_embed_items", "cast_ref_fid");
    if (!hasCastReferenceFid) {
        return std::unexpected(hasCastReferenceFid.error());
    }
    if (!*hasCastReferenceFid) {
        if (auto status = connection.execute(
                "ALTER TABLE cast_embed_items ADD COLUMN cast_ref_fid INTEGER;"); !status) {
            return status;
        }
    }
    if (auto status = recordMigration(connection, 4, appliedAt); !status) {
        return status;
    }
    if (auto status = recordMigration(connection, 5, appliedAt); !status) {
        return status;
    }

    for (const auto column : std::array{std::string_view{"reply_count"}, std::string_view{"recast_count"}, std::string_view{"like_count"}}) {
        auto exists = tableHasColumn(connection, "casts", column);
        if (!exists) { return std::unexpected(exists.error()); }
        if (!*exists) {
            const std::string statement = "ALTER TABLE casts ADD COLUMN " + std::string{column} +
                " INTEGER NOT NULL DEFAULT 0;";
            if (auto status = connection.execute(statement); !status) { return status; }
        }
    }
    if (auto status = recordMigration(connection, 6, appliedAt); !status) {
        return status;
    }
    if (auto status = recordMigration(connection, 7, appliedAt); !status) {
        return status;
    }
    if (auto status = recordMigration(connection, 8, appliedAt); !status) {
        return status;
    }
    if (auto status = recordMigration(connection, 9, appliedAt); !status) {
        return status;
    }
    if (auto status = recordMigration(connection, 10, appliedAt); !status) {
        return status;
    }

    auto hasWriteTarget = tableHasColumn(connection, "write_outbox", "target_cast_id");
    if (!hasWriteTarget) {
        return std::unexpected(hasWriteTarget.error());
    }
    if (!*hasWriteTarget) {
        constexpr std::string_view upgradeWriteOutbox = R"SQL(
CREATE TEMP TABLE cast_write_state_v11_backup AS
SELECT cast_id, idempotency_key, state, updated_at_ms FROM cast_write_state;
CREATE TABLE write_outbox_v11 (
    idempotency_key TEXT PRIMARY KEY NOT NULL,
    kind TEXT NOT NULL,
    cast_id TEXT,
    author_fid INTEGER NOT NULL,
    target_cast_id TEXT,
    target_cast_fid INTEGER,
    target_fid INTEGER,
    signer_public_key_hex TEXT NOT NULL,
    message_bytes BLOB NOT NULL,
    signed_at_ms INTEGER NOT NULL,
    enqueued_at_ms INTEGER NOT NULL,
    status TEXT NOT NULL,
    attempt_count INTEGER NOT NULL DEFAULT 0,
    next_attempt_at_ms INTEGER,
    last_failure_kind TEXT,
    last_failure_message TEXT,
    confirmed_at_ms INTEGER,
    FOREIGN KEY (cast_id) REFERENCES casts(id) ON DELETE RESTRICT
);
INSERT INTO write_outbox_v11(
    idempotency_key, kind, cast_id, author_fid,
    target_cast_id, target_cast_fid, target_fid,
    signer_public_key_hex, message_bytes, signed_at_ms, enqueued_at_ms,
    status, attempt_count, next_attempt_at_ms, last_failure_kind,
    last_failure_message, confirmed_at_ms)
SELECT idempotency_key, kind, cast_id, author_fid,
       NULL, NULL, NULL, signer_public_key_hex, message_bytes,
       signed_at_ms, enqueued_at_ms, status, attempt_count,
       next_attempt_at_ms, last_failure_kind, last_failure_message, confirmed_at_ms
FROM write_outbox;
DROP TABLE cast_write_state;
DROP TABLE write_outbox;
ALTER TABLE write_outbox_v11 RENAME TO write_outbox;
CREATE INDEX idx_write_outbox_ready
    ON write_outbox(status, next_attempt_at_ms, enqueued_at_ms);
CREATE INDEX idx_write_outbox_author_ready
    ON write_outbox(author_fid, status, next_attempt_at_ms, enqueued_at_ms);
CREATE TABLE cast_write_state (
    cast_id TEXT PRIMARY KEY NOT NULL,
    idempotency_key TEXT UNIQUE NOT NULL,
    state TEXT NOT NULL,
    updated_at_ms INTEGER NOT NULL,
    FOREIGN KEY (cast_id) REFERENCES casts(id) ON DELETE CASCADE,
    FOREIGN KEY (idempotency_key) REFERENCES write_outbox(idempotency_key) ON DELETE CASCADE
);
INSERT INTO cast_write_state(cast_id, idempotency_key, state, updated_at_ms)
SELECT cast_id, idempotency_key, state, updated_at_ms FROM cast_write_state_v11_backup;
DROP TABLE cast_write_state_v11_backup;
)SQL";
        if (auto status = connection.execute(upgradeWriteOutbox); !status) {
            return status;
        }
    }
    if (auto status = recordMigration(connection, 11, appliedAt); !status) {
        return status;
    }

    auto hasSessionCustodyAddress = tableHasColumn(connection, "client_session", "custody_address");
    if (!hasSessionCustodyAddress) {
        return std::unexpected(hasSessionCustodyAddress.error());
    }
    if (!*hasSessionCustodyAddress) {
        if (auto status = connection.execute(
                "ALTER TABLE client_session ADD COLUMN custody_address TEXT;"); !status) {
            return status;
        }
    }
    if (auto status = recordMigration(connection, 12, appliedAt); !status) {
        return status;
    }

    // Preserve local delete intent across upgrades. Older Phase 5 databases may
    // already contain remove-cast outbox records that predate the durable
    // tombstone projection, so backfill them before recording schema v13.
    if (auto status = connection.execute(R"SQL(
INSERT OR IGNORE INTO cast_local_tombstones(
    cast_id, idempotency_key, author_fid, created_at_ms)
SELECT target_cast_id, idempotency_key, author_fid, enqueued_at_ms
FROM write_outbox
WHERE kind = 'remove-cast' AND target_cast_id IS NOT NULL;
)SQL"); !status) {
        return status;
    }
    if (auto status = recordMigration(connection, 13, appliedAt); !status) {
        return status;
    }

    // Keep account-scoped reaction intent durable independently of provider
    // feed payloads. Backfill the latest historical Like/Unlike and
    // Recast/Undo-Recast operation for each account/cast/reaction tuple.
    if (auto status = connection.execute(R"SQL(
CREATE TABLE IF NOT EXISTS cast_reaction_local_state (
    author_fid INTEGER NOT NULL,
    cast_id TEXT NOT NULL,
    target_cast_fid INTEGER NOT NULL,
    reaction_kind TEXT NOT NULL CHECK (reaction_kind IN ('like', 'recast')),
    is_active INTEGER NOT NULL CHECK (is_active IN (0, 1)),
    idempotency_key TEXT NOT NULL,
    updated_at_ms INTEGER NOT NULL,
    PRIMARY KEY (author_fid, cast_id, reaction_kind),
    FOREIGN KEY (idempotency_key) REFERENCES write_outbox(idempotency_key) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_cast_reaction_local_author
    ON cast_reaction_local_state(author_fid, updated_at_ms);
INSERT OR REPLACE INTO cast_reaction_local_state(
    author_fid, cast_id, target_cast_fid, reaction_kind,
    is_active, idempotency_key, updated_at_ms)
SELECT
    current.author_fid,
    current.target_cast_id,
    current.target_cast_fid,
    CASE WHEN current.kind IN ('like', 'unlike') THEN 'like' ELSE 'recast' END,
    CASE WHEN current.kind IN ('like', 'recast') THEN 1 ELSE 0 END,
    current.idempotency_key,
    current.enqueued_at_ms
FROM write_outbox AS current
WHERE current.kind IN ('like', 'unlike', 'recast', 'undo-recast')
  AND current.target_cast_id IS NOT NULL
  AND current.target_cast_fid IS NOT NULL
  AND NOT EXISTS (
      SELECT 1
      FROM write_outbox AS newer
      WHERE newer.author_fid = current.author_fid
        AND newer.target_cast_id = current.target_cast_id
        AND newer.kind IN ('like', 'unlike', 'recast', 'undo-recast')
        AND (
            (current.kind IN ('like', 'unlike') AND newer.kind IN ('like', 'unlike'))
            OR
            (current.kind IN ('recast', 'undo-recast') AND newer.kind IN ('recast', 'undo-recast'))
        )
        AND (
            newer.enqueued_at_ms > current.enqueued_at_ms
            OR (newer.enqueued_at_ms = current.enqueued_at_ms
                AND newer.rowid > current.rowid)
        )
  );
)SQL"); !status) {
        return status;
    }
    if (auto status = recordMigration(connection, 14, appliedAt); !status) {
        return status;
    }

    // Keep account-scoped follow intent durable independently of provider
    // profile payloads. Backfill the latest Follow/Unfollow operation for each
    // account/target tuple so upgrades preserve the user's desired state.
    if (auto status = connection.execute(R"SQL(
CREATE TABLE IF NOT EXISTS following_local_state (
    author_fid INTEGER NOT NULL,
    target_fid INTEGER NOT NULL,
    is_following INTEGER NOT NULL CHECK (is_following IN (0, 1)),
    idempotency_key TEXT NOT NULL,
    updated_at_ms INTEGER NOT NULL,
    PRIMARY KEY (author_fid, target_fid),
    FOREIGN KEY (idempotency_key) REFERENCES write_outbox(idempotency_key) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_following_local_author
    ON following_local_state(author_fid, updated_at_ms);
INSERT OR REPLACE INTO following_local_state(
    author_fid, target_fid, is_following, idempotency_key, updated_at_ms)
SELECT
    current.author_fid,
    current.target_fid,
    CASE WHEN current.kind = 'follow' THEN 1 ELSE 0 END,
    current.idempotency_key,
    current.enqueued_at_ms
FROM write_outbox AS current
WHERE current.kind IN ('follow', 'unfollow')
  AND current.target_fid IS NOT NULL
  AND NOT EXISTS (
      SELECT 1
      FROM write_outbox AS newer
      WHERE newer.author_fid = current.author_fid
        AND newer.target_fid = current.target_fid
        AND newer.kind IN ('follow', 'unfollow')
        AND (
            newer.enqueued_at_ms > current.enqueued_at_ms
            OR (newer.enqueued_at_ms = current.enqueued_at_ms
                AND newer.rowid > current.rowid)
        )
  );
)SQL"); !status) {
        return status;
    }
    if (auto status = recordMigration(connection, 15, appliedAt); !status) {
        return status;
    }

    // Preserve canonical social counts with cached profiles. The columns are
    // additive and default to zero for existing local snapshots.
    for (const auto column : std::array{std::string_view{"follower_count"}, std::string_view{"following_count"}}) {
        auto exists = tableHasColumn(connection, "profiles", column);
        if (!exists) { return std::unexpected(exists.error()); }
        if (!*exists) {
            const std::string statement = "ALTER TABLE profiles ADD COLUMN " + std::string{column} +
                " INTEGER NOT NULL DEFAULT 0;";
            if (auto status = connection.execute(statement); !status) { return status; }
        }
    }
    if (auto status = recordMigration(connection, 16, appliedAt); !status) {
        return status;
    }

    // Preserve profile presentation data now exposed by the canonical v2 User
    // object. These are optional and therefore safe additive migrations.
    for (const auto column : std::array{std::string_view{"banner_url"}, std::string_view{"location"}}) {
        auto exists = tableHasColumn(connection, "profiles", column);
        if (!exists) { return std::unexpected(exists.error()); }
        if (!*exists) {
            const std::string statement = "ALTER TABLE profiles ADD COLUMN " + std::string{column} + " TEXT;";
            if (auto status = connection.execute(statement); !status) { return status; }
        }
    }
    if (auto status = recordMigration(connection, 17, appliedAt); !status) {
        return status;
    }

    // Preserve the protocol-native USER_DATA_TYPE_URL separately from the bio.
    // Older databases gain the optional column without rewriting existing rows.
    {
        auto exists = tableHasColumn(connection, "profiles", "profile_url");
        if (!exists) { return std::unexpected(exists.error()); }
        if (!*exists) {
            if (auto status = connection.execute("ALTER TABLE profiles ADD COLUMN profile_url TEXT;"); !status) {
                return status;
            }
        }
    }
    if (auto status = recordMigration(connection, 18, appliedAt); !status) {
        return status;
    }

    // Older builds allowed embedded feed/social author summaries to persist
    // follower/following counts. Those snapshots are not authoritative and can
    // survive long enough to make a profile look fabricated. Clear the polluted
    // cache exactly once; a dedicated profile refresh repopulates current totals.
    auto countsResetApplied = migrationApplied(connection, 19);
    if (!countsResetApplied) {
        return std::unexpected(countsResetApplied.error());
    }
    if (!*countsResetApplied) {
        if (auto status = connection.execute(
                "UPDATE profiles SET follower_count = 0, following_count = 0;"); !status) {
            return status;
        }
        if (auto status = recordMigration(connection, 19, appliedAt); !status) {
            return status;
        }
    }

    // Preserve the provider-neutral normalized profile quality score. The source
    // may currently be Neynar-compatible, but storage keeps only the canonical
    // [0, 1] value so presentation remains provider-independent.
    {
        auto exists = tableHasColumn(connection, "profiles", "quality_score");
        if (!exists) { return std::unexpected(exists.error()); }
        if (!*exists) {
            if (auto status = connection.execute("ALTER TABLE profiles ADD COLUMN quality_score REAL;"); !status) {
                return status;
            }
        }
    }
    if (auto status = recordMigration(connection, 20, appliedAt); !status) {
        return status;
    }

    // Thread children are ordinary CastAdd messages, but their network submit
    // order must survive restart/offline recovery. Persist the parent outbox key
    // independently of the protocol parent_cast_id encoded inside message_bytes.
    {
        auto exists = tableHasColumn(connection, "write_outbox", "depends_on_key");
        if (!exists) { return std::unexpected(exists.error()); }
        if (!*exists) {
            if (auto status = connection.execute(
                    "ALTER TABLE write_outbox ADD COLUMN depends_on_key TEXT;"); !status) {
                return status;
            }
        }
    }
    if (auto status = recordMigration(connection, 21, appliedAt); !status) {
        return status;
    }

    // Copy legacy URL embeds only after all columns required by the current
    // cast_embed_items schema are guaranteed to exist. This ordering keeps
    // upgrades from an intermediate Phase 3 database safe as well as Phase 2.
    if (auto status = connection.execute(
            "INSERT OR IGNORE INTO cast_embed_items("
            "cast_id, position, kind, url, cast_ref_id, cast_ref_fid) "
            "SELECT cast_id, position, 'url', url, NULL, NULL FROM cast_embeds;"); !status) {
        return status;
    }
    return transaction.commit();
}

}

Error makeError(sqlite3* const database, const std::string_view context)
{
    std::string message{context};
    message += ": ";
    message += database != nullptr ? sqlite3_errmsg(database) : "SQLite database is unavailable.";
    return Error{database != nullptr ? sqlite3_errcode(database) : SQLITE_ERROR, std::move(message)};
}

std::expected<Connection, Error> Connection::open(const std::filesystem::path& path)
{
    sqlite3* database{};
    const auto pathText = path.string();
    const auto result = sqlite3_open_v2(
        pathText.c_str(),
        &database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (result != SQLITE_OK) {
        auto error = makeError(database, "Failed to open local database");
        if (database != nullptr) {
            sqlite3_close(database);
        }
        return std::unexpected(std::move(error));
    }

    Connection connection{database};
    if (auto status = connection.execute("PRAGMA foreign_keys = ON;"); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = connection.execute("PRAGMA journal_mode = WAL;"); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = connection.execute("PRAGMA synchronous = NORMAL;"); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = migrate(connection); !status) {
        return std::unexpected(status.error());
    }
    return connection;
}

Connection::Connection(sqlite3* const database) noexcept
    : m_database(database)
{
}

Connection::~Connection()
{
    if (m_database != nullptr) {
        sqlite3_close(m_database);
    }
}

Connection::Connection(Connection&& other) noexcept
    : m_database(std::exchange(other.m_database, nullptr))
{
}

auto Connection::operator=(Connection&& other) noexcept -> Connection&
{
    if (this != &other) {
        if (m_database != nullptr) {
            sqlite3_close(m_database);
        }
        m_database = std::exchange(other.m_database, nullptr);
    }
    return *this;
}

sqlite3* Connection::native() const noexcept
{
    return m_database;
}

std::expected<void, Error> Connection::execute(const std::string_view sql)
{
    char* rawError{};
    const auto result = sqlite3_exec(m_database, sql.data(), nullptr, nullptr, &rawError);
    if (result == SQLITE_OK) {
        return {};
    }

    std::string message{"SQLite statement failed"};
    if (rawError != nullptr) {
        message += ": ";
        message += rawError;
        sqlite3_free(rawError);
    }
    return std::unexpected(Error{result, std::move(message)});
}

std::expected<Statement, Error> Statement::prepare(
    Connection& connection,
    const std::string_view sql)
{
    sqlite3_stmt* statement{};
    const auto result = sqlite3_prepare_v2(
        connection.native(),
        sql.data(),
        static_cast<int>(sql.size()),
        &statement,
        nullptr);
    if (result != SQLITE_OK) {
        return std::unexpected(makeError(connection.native(), "Failed to prepare SQLite statement"));
    }
    return Statement{statement};
}

Statement::Statement(sqlite3_stmt* const statement) noexcept
    : m_statement(statement)
{
}

Statement::~Statement()
{
    if (m_statement != nullptr) {
        sqlite3_finalize(m_statement);
    }
}

Statement::Statement(Statement&& other) noexcept
    : m_statement(std::exchange(other.m_statement, nullptr))
{
}

auto Statement::operator=(Statement&& other) noexcept -> Statement&
{
    if (this != &other) {
        if (m_statement != nullptr) {
            sqlite3_finalize(m_statement);
        }
        m_statement = std::exchange(other.m_statement, nullptr);
    }
    return *this;
}

sqlite3_stmt* Statement::native() const noexcept
{
    return m_statement;
}

void Statement::reset() noexcept
{
    sqlite3_reset(m_statement);
    sqlite3_clear_bindings(m_statement);
}

std::expected<void, Error> bindText(
    sqlite3_stmt* const statement,
    const int index,
    const std::string_view value)
{
    const auto result = sqlite3_bind_text(
        statement,
        index,
        value.data(),
        static_cast<int>(value.size()),
        SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        return std::unexpected(Error{result, "Failed to bind SQLite text parameter."});
    }
    return {};
}

std::expected<void, Error> bindBlob(
    sqlite3_stmt* const statement,
    const int index,
    const std::string_view value)
{
    const auto result = sqlite3_bind_blob(
        statement,
        index,
        value.data(),
        static_cast<int>(value.size()),
        SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        return std::unexpected(Error{result, "Failed to bind SQLite blob parameter."});
    }
    return {};
}

std::expected<void, Error> bindOptionalText(
    sqlite3_stmt* const statement,
    const int index,
    const std::optional<std::string>& value)
{
    if (!value) {
        const auto result = sqlite3_bind_null(statement, index);
        if (result != SQLITE_OK) {
            return std::unexpected(Error{result, "Failed to bind SQLite null parameter."});
        }
        return {};
    }
    return bindText(statement, index, *value);
}

std::string columnBlob(sqlite3_stmt* const statement, const int column)
{
    const auto* bytes = static_cast<const char*>(sqlite3_column_blob(statement, column));
    const int size = sqlite3_column_bytes(statement, column);
    if (bytes == nullptr || size <= 0) {
        return {};
    }
    return std::string{bytes, static_cast<std::size_t>(size)};
}

std::optional<std::string> optionalColumnText(sqlite3_stmt* const statement, const int column)
{
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    const auto* const text = sqlite3_column_text(statement, column);
    const auto length = sqlite3_column_bytes(statement, column);
    if (text == nullptr || length < 0) {
        return std::nullopt;
    }

    std::string result{};
    result.reserve(static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index) {
        result.push_back(static_cast<char>(text[index]));
    }
    return result;
}

std::string columnText(sqlite3_stmt* const statement, const int column)
{
    return optionalColumnText(statement, column).value_or(std::string{});
}

std::expected<void, Error> stepDone(Connection& connection, Statement& statement)
{
    if (sqlite3_step(statement.native()) != SQLITE_DONE) {
        return std::unexpected(makeError(connection.native(), "SQLite statement did not complete"));
    }
    return {};
}

Transaction::Transaction(Connection& connection) noexcept
    : m_connection(connection)
{
}

Transaction::~Transaction()
{
    if (m_isActive) {
        static_cast<void>(m_connection.execute("ROLLBACK;"));
    }
}

std::expected<void, Error> Transaction::begin()
{
    auto status = m_connection.execute("BEGIN IMMEDIATE;");
    if (status) {
        m_isActive = true;
    }
    return status;
}

std::expected<void, Error> Transaction::commit()
{
    auto status = m_connection.execute("COMMIT;");
    if (status) {
        m_isActive = false;
    }
    return status;
}

} // namespace farcaster::storage::sqlite
