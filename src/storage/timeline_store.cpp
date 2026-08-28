module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

module farcaster.storage.timeline;

import farcaster.local.timeline;
import farcaster.domain.cast;
import farcaster.domain.feed;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;

namespace farcaster::storage::timeline {


class SqliteTimelineStore final : public local::timeline::ITimelineStore {
public:
    [[nodiscard]] static foundation::result::Result<SqliteTimelineStore> open(const std::filesystem::path& path);
    ~SqliteTimelineStore() override;
    SqliteTimelineStore(const SqliteTimelineStore&) = delete;
    auto operator=(const SqliteTimelineStore&) -> SqliteTimelineStore& = delete;
    SqliteTimelineStore(SqliteTimelineStore&& other) noexcept;
    auto operator=(SqliteTimelineStore&& other) noexcept -> SqliteTimelineStore&;
    [[nodiscard]] foundation::result::Status replaceTimeline(const local::timeline::TimelineId& timelineId, std::span<const domain::feed::FeedEntry> entries) override;
    [[nodiscard]] foundation::result::Result<std::vector<domain::feed::FeedEntry>> readTimeline(const local::timeline::TimelineId& timelineId, std::size_t limit) override;
private:
    struct Impl;
    explicit SqliteTimelineStore(std::unique_ptr<Impl> impl) noexcept;
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

struct SqliteTimelineStore::Impl final {
    explicit Impl(sqlite::Connection openedConnection)
        : connection(std::move(openedConnection))
    {
    }

    sqlite::Connection connection;
    std::mutex mutex{};
};

foundation::result::Result<SqliteTimelineStore> SqliteTimelineStore::open(const std::filesystem::path& path)
{
    auto connection = sqlite::Connection::open(path);
    if (!connection) {
        return std::unexpected(toFoundationError(std::move(connection.error())));
    }
    return SqliteTimelineStore{std::make_unique<Impl>(std::move(*connection))};
}

SqliteTimelineStore::SqliteTimelineStore(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{
}

SqliteTimelineStore::~SqliteTimelineStore() = default;
SqliteTimelineStore::SqliteTimelineStore(SqliteTimelineStore&& other) noexcept = default;
auto SqliteTimelineStore::operator=(SqliteTimelineStore&& other) noexcept -> SqliteTimelineStore& = default;

foundation::result::Status SqliteTimelineStore::replaceTimeline(
    const local::timeline::TimelineId& timelineId,
    const std::span<const domain::feed::FeedEntry> entries)
{
    if (timelineId.value.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Timeline id must not be empty."));
    }

    std::scoped_lock lock{m_impl->mutex};
    sqlite::Transaction transaction{m_impl->connection};
    if (auto status = transaction.begin(); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    auto preparedClear = sqlite::Statement::prepare(
        m_impl->connection,
        "DELETE FROM timeline_entries WHERE timeline_id = ?;");
    if (!preparedClear) {
        return std::unexpected(toFoundationError(std::move(preparedClear.error())));
    }
    auto clear = std::move(*preparedClear);
    if (auto status = sqlite::bindText(clear.native(), 1, timelineId.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::stepDone(m_impl->connection, clear); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    auto preparedInsert = sqlite::Statement::prepare(m_impl->connection, R"SQL(
INSERT INTO timeline_entries(timeline_id, position, cast_id, sort_time_ms)
VALUES(?, ?, ?, ?);
)SQL");
    if (!preparedInsert) {
        return std::unexpected(toFoundationError(std::move(preparedInsert.error())));
    }
    auto insert = std::move(*preparedInsert);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        insert.reset();
        if (auto status = sqlite::bindText(insert.native(), 1, timelineId.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(insert.native(), 2, static_cast<sqlite3_int64>(index)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_impl->connection.native(), "Failed to bind timeline position")));
        }
        if (auto status = sqlite::bindText(insert.native(), 3, entries[index].castId.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(insert.native(), 4, toMilliseconds(entries[index].sortTime)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_impl->connection.native(), "Failed to bind timeline timestamp")));
        }
        if (auto status = sqlite::stepDone(m_impl->connection, insert); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
    }

    if (auto status = transaction.commit(); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

foundation::result::Result<std::vector<domain::feed::FeedEntry>> SqliteTimelineStore::readTimeline(
    const local::timeline::TimelineId& timelineId,
    const std::size_t limit)
{
    std::scoped_lock lock{m_impl->mutex};
    auto prepared = sqlite::Statement::prepare(m_impl->connection, R"SQL(
SELECT entry.cast_id, entry.sort_time_ms
FROM timeline_entries AS entry
WHERE entry.timeline_id = ?
  AND NOT EXISTS (
      SELECT 1 FROM cast_local_tombstones AS tombstone
      WHERE tombstone.cast_id = entry.cast_id
  )
ORDER BY entry.position ASC
LIMIT ?;
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, timelineId.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_bind_int64(statement.native(), 2, static_cast<sqlite3_int64>(limit)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind timeline limit")));
    }

    std::vector<domain::feed::FeedEntry> result{};
    result.reserve(limit);
    while (true) {
        const auto step = sqlite3_step(statement.native());
        if (step == SQLITE_DONE) {
            break;
        }
        if (step != SQLITE_ROW) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_impl->connection.native(), "Failed to read timeline")));
        }
        result.push_back(domain::feed::FeedEntry{
            .castId = domain::cast::CastId{sqlite::columnText(statement.native(), 0)},
            .sortTime = fromMilliseconds(sqlite3_column_int64(statement.native(), 1)),
        });
    }
    return result;
}


foundation::result::Result<std::unique_ptr<local::timeline::ITimelineStore>>
openTimelineStore(const std::filesystem::path& path)
{
    auto opened = SqliteTimelineStore::open(path);
    if (!opened) {
        return std::unexpected(opened.error());
    }
    std::unique_ptr<local::timeline::ITimelineStore> result =
        std::make_unique<SqliteTimelineStore>(std::move(*opened));
    return result;
}

}
