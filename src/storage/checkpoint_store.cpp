module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <limits>
#include <string_view>
#include <optional>
#include <utility>

module farcaster.storage.checkpoint;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.sync.checkpoint;
import farcaster.sync.model;

namespace farcaster::storage::checkpoint {


class SqliteCheckpointStore final : public sync::checkpoint::ICheckpointStore {
public:
    [[nodiscard]] static foundation::result::Result<SqliteCheckpointStore> open(const std::filesystem::path& path);
    ~SqliteCheckpointStore() override;
    SqliteCheckpointStore(const SqliteCheckpointStore&) = delete;
    auto operator=(const SqliteCheckpointStore&) -> SqliteCheckpointStore& = delete;
    SqliteCheckpointStore(SqliteCheckpointStore&& other) noexcept;
    auto operator=(SqliteCheckpointStore&& other) noexcept -> SqliteCheckpointStore&;
    [[nodiscard]] foundation::result::Status saveCheckpoint(const sync::model::SyncKey& key, const sync::model::SyncCheckpoint& checkpoint) override;
    [[nodiscard]] foundation::result::Result<std::optional<sync::model::SyncCheckpoint>> loadCheckpoint(const sync::model::SyncKey& key) override;
private:
    [[nodiscard]] foundation::result::Result<std::optional<sync::model::SyncCheckpoint>> loadCheckpointLocked(const sync::model::SyncKey& key);
    struct Impl;
    explicit SqliteCheckpointStore(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl{};
};

namespace {

[[nodiscard]] foundation::error::Error toFoundationError(sqlite::Error error)
{
    return foundation::error::Error::make(foundation::error::ErrorCode::StorageFailure, std::move(error.message));
}

[[nodiscard]] std::int64_t toMilliseconds(const foundation::time::Timestamp timestamp)
{
    return timestamp.time_since_epoch().count();
}

[[nodiscard]] foundation::time::Timestamp fromMilliseconds(const std::int64_t value)
{
    return foundation::time::Timestamp{std::chrono::milliseconds{value}};
}

}

struct SqliteCheckpointStore::Impl final {
    explicit Impl(sqlite::Connection openedConnection)
        : connection(std::move(openedConnection))
    {
    }

    sqlite::Connection connection;
    std::mutex mutex{};
};

foundation::result::Result<SqliteCheckpointStore> SqliteCheckpointStore::open(
    const std::filesystem::path& path)
{
    auto connection = sqlite::Connection::open(path);
    if (!connection) {
        return std::unexpected(toFoundationError(std::move(connection.error())));
    }
    return SqliteCheckpointStore{std::make_unique<Impl>(std::move(*connection))};
}

SqliteCheckpointStore::SqliteCheckpointStore(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{
}

SqliteCheckpointStore::~SqliteCheckpointStore() = default;
SqliteCheckpointStore::SqliteCheckpointStore(SqliteCheckpointStore&& other) noexcept = default;
auto SqliteCheckpointStore::operator=(SqliteCheckpointStore&& other) noexcept
    -> SqliteCheckpointStore& = default;

foundation::result::Status SqliteCheckpointStore::saveCheckpoint(
    const sync::model::SyncKey& key,
    const sync::model::SyncCheckpoint& checkpoint)
{
    constexpr auto maximumSequence =
        static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max());
    if (key.value.empty() || key.value.size() > 512U
        || key.value.find('\0') != std::string::npos
        || checkpoint.sequence > maximumSequence) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Sync checkpoint requires a valid key and a signed-64-bit sequence."));
    }

    // IMMEDIATE serializes independent SQLite connections before inspecting
    // the current waterline. A late/stale worker can never rewind progress.
    std::scoped_lock lock{m_impl->mutex};
    sqlite::Transaction transaction{m_impl->connection};
    if (auto status = transaction.begin(); !status)
        return std::unexpected(toFoundationError(std::move(status.error())));

    auto current = loadCheckpointLocked(key);
    if (!current) return std::unexpected(std::move(current.error()));
    if (*current) {
        if (checkpoint.sequence < (*current)->sequence) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Conflict,
                "Stale sync checkpoint cannot rewind an indexed stream."));
        }
        if (checkpoint.sequence == (*current)->sequence) {
            // An idempotent replay does not rewrite the commit timestamp.
            if (auto status = transaction.commit(); !status)
                return std::unexpected(toFoundationError(std::move(status.error())));
            return {};
        }
    }

    auto prepared = sqlite::Statement::prepare(m_impl->connection, R"SQL(
INSERT INTO sync_checkpoints(sync_key, sequence, updated_at_ms)
VALUES(?, ?, ?)
ON CONFLICT(sync_key) DO UPDATE SET
    sequence = excluded.sequence,
    updated_at_ms = excluded.updated_at_ms;
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, key.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_bind_int64(statement.native(), 2, static_cast<sqlite3_int64>(checkpoint.sequence)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind checkpoint sequence")));
    }
    if (sqlite3_bind_int64(statement.native(), 3, toMilliseconds(checkpoint.updatedAt)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind checkpoint timestamp")));
    }
    if (auto status = sqlite::stepDone(m_impl->connection, statement); !status)
        return std::unexpected(toFoundationError(std::move(status.error())));
    if (auto status = transaction.commit(); !status)
        return std::unexpected(toFoundationError(std::move(status.error())));
    return {};
}

foundation::result::Result<std::optional<sync::model::SyncCheckpoint>> SqliteCheckpointStore::loadCheckpoint(
    const sync::model::SyncKey& key)
{
    if (key.value.empty() || key.value.size() > 512U
        || key.value.find('\0') != std::string::npos) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Sync checkpoint lookup requires a valid key."));
    }
    std::scoped_lock lock{m_impl->mutex};
    return loadCheckpointLocked(key);
}

foundation::result::Result<std::optional<sync::model::SyncCheckpoint>> SqliteCheckpointStore::loadCheckpointLocked(
    const sync::model::SyncKey& key)
{
    auto prepared = sqlite::Statement::prepare(
        m_impl->connection,
        "SELECT sequence, updated_at_ms FROM sync_checkpoints WHERE sync_key = ?;");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, key.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    const auto step = sqlite3_step(statement.native());
    if (step == SQLITE_DONE) {
        return std::optional<sync::model::SyncCheckpoint>{};
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to load sync checkpoint")));
    }

    const auto signedSequence = sqlite3_column_int64(statement.native(), 0);
    if (signedSequence < 0)
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::StorageFailure,
            "Negative indexed-stream sequence in persisted checkpoint."));
    return std::optional<sync::model::SyncCheckpoint>{sync::model::SyncCheckpoint{
        .sequence = static_cast<std::uint64_t>(signedSequence),
        .updatedAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 1)),
    }};
}


foundation::result::Result<std::unique_ptr<sync::checkpoint::ICheckpointStore>>
openCheckpointStore(const std::filesystem::path& path)
{
    auto opened = SqliteCheckpointStore::open(path);
    if (!opened) {
        return std::unexpected(opened.error());
    }
    std::unique_ptr<sync::checkpoint::ICheckpointStore> result =
        std::make_unique<SqliteCheckpointStore>(std::move(*opened));
    return result;
}

}
