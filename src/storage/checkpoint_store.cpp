module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
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
    if (key.value.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Sync key must not be empty."));
    }

    std::scoped_lock lock{m_impl->mutex};
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
    if (auto status = sqlite::stepDone(m_impl->connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

foundation::result::Result<std::optional<sync::model::SyncCheckpoint>> SqliteCheckpointStore::loadCheckpoint(
    const sync::model::SyncKey& key)
{
    std::scoped_lock lock{m_impl->mutex};
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

    return std::optional<sync::model::SyncCheckpoint>{sync::model::SyncCheckpoint{
        .sequence = static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 0)),
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
