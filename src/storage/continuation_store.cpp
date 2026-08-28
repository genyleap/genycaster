module;

#include "sqlite_boundary.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

module farcaster.storage.continuation;

import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.provider.model;

namespace farcaster::storage::continuation {
namespace {

[[nodiscard]] foundation::error::Error toFoundationError(sqlite::Error error)
{
    return foundation::error::Error::make(foundation::error::ErrorCode::StorageFailure,std::move(error.message));
}

[[nodiscard]] std::int64_t toMilliseconds(const foundation::time::Timestamp timestamp)
{
    return timestamp.time_since_epoch().count();
}

[[nodiscard]] foundation::time::Timestamp fromMilliseconds(const std::int64_t value)
{
    return foundation::time::Timestamp{std::chrono::milliseconds{value}};
}

[[nodiscard]] std::string makeTokenValue()
{
    static std::atomic<std::uint64_t> sequence{0};
    const auto clockValue = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto currentSequence = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    return "continuation-" + std::to_string(clockValue) + "-" + std::to_string(currentSequence);
}

}

struct ContinuationStore::Impl final {
    explicit Impl(sqlite::Connection openedConnection)
        : connection(std::move(openedConnection))
    {
    }

    sqlite::Connection connection;
    std::mutex mutex{};
};

ContinuationStore::ContinuationStore(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{
}

ContinuationStore::~ContinuationStore() = default;
ContinuationStore::ContinuationStore(ContinuationStore&& other) noexcept = default;
auto ContinuationStore::operator=(ContinuationStore&& other) noexcept
    -> ContinuationStore& = default;

foundation::result::Result<capability::pagination::ContinuationToken> ContinuationStore::bind(
    const capability::model::CapabilityId& capabilityId,
    const provider::model::ProviderId& providerId,
    std::string providerCursor)
{
    if (capabilityId.value.empty() || providerId.value.empty() || providerCursor.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Continuation binding requires capability, provider, and cursor values."));
    }

    const capability::pagination::ContinuationToken token{makeTokenValue()};
    const auto createdAt = foundation::time::now();
    std::scoped_lock lock{m_impl->mutex};

    auto prepared = sqlite::Statement::prepare(m_impl->connection, R"SQL(
INSERT INTO continuations(token, capability_id, provider_id, provider_cursor, created_at_ms)
VALUES(?, ?, ?, ?, ?);
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }

    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, token.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindText(statement.native(), 2, capabilityId.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindText(statement.native(), 3, providerId.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindText(statement.native(), 4, providerCursor); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_bind_int64(statement.native(), 5, toMilliseconds(createdAt)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(),
            "Failed to bind continuation timestamp")));
    }
    if (auto status = sqlite::stepDone(m_impl->connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    return token;
}

foundation::result::Result<ContinuationRecord> ContinuationStore::resolve(
    const capability::pagination::ContinuationToken& token)
{
    std::scoped_lock lock{m_impl->mutex};
    auto prepared = sqlite::Statement::prepare(m_impl->connection, R"SQL(
SELECT capability_id, provider_id, provider_cursor, created_at_ms
FROM continuations
WHERE token = ?;
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }

    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, token.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    const auto step = sqlite3_step(statement.native());
    if (step == SQLITE_DONE) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::NotFound,
            "Continuation token was not found."));
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(),
            "Failed to resolve continuation")));
    }

    return ContinuationRecord{
        .token = token,
        .capabilityId = capability::model::CapabilityId{sqlite::columnText(statement.native(), 0)},
        .providerId = provider::model::ProviderId{sqlite::columnText(statement.native(), 1)},
        .providerCursor = sqlite::columnText(statement.native(), 2),
        .createdAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 3)),
    };
}

foundation::result::Status ContinuationStore::erase(const capability::pagination::ContinuationToken& token)
{
    std::scoped_lock lock{m_impl->mutex};
    auto prepared = sqlite::Statement::prepare(
        m_impl->connection,
        "DELETE FROM continuations WHERE token = ?;");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }

    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, token.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::stepDone(m_impl->connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    return {};
}

foundation::result::Result<ContinuationStore>
openContinuationStore(const std::filesystem::path& path)
{
    auto connection = sqlite::Connection::open(path);
    if (!connection) {
        return std::unexpected(toFoundationError(std::move(connection.error())));
    }

    return ContinuationStore{
        std::make_unique<ContinuationStore::Impl>(std::move(*connection))};
}

}
