module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

module farcaster.storage.session;

import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.session;

namespace farcaster::storage::session {
namespace {

[[nodiscard]] foundation::error::Error toFoundationError(sqlite::Error error)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::StorageFailure,
        std::move(error.message));
}

[[nodiscard]] std::string methodText(const domain::session::AuthenticationMethod method)
{
    return method == domain::session::AuthenticationMethod::AuthAddress
        ? "auth-address" : "custody";
}

[[nodiscard]] foundation::result::Result<domain::session::AuthenticationMethod>
parseMethod(const std::string& value)
{
    if (value == "auth-address") {
        return domain::session::AuthenticationMethod::AuthAddress;
    }
    if (value == "custody") {
        return domain::session::AuthenticationMethod::Custody;
    }
    return std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::StorageFailure,
        "Persisted client session contains an unknown authentication method."));
}

class SqliteSessionStore final : public local::session::IClientSessionStore {
public:
    explicit SqliteSessionStore(sqlite::Connection connection) noexcept
        : m_connection(std::move(connection))
    {
    }

    [[nodiscard]] foundation::result::Result<std::optional<domain::session::ClientSession>>
    load() override
    {
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
SELECT fid, username, display_name, avatar_url, auth_method,
       authenticated_at_ms, expires_at_ms
FROM client_session WHERE singleton_id = 1;
)SQL");
        if (!prepared) {
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        }
        auto statement = std::move(*prepared);
        const int step = sqlite3_step(statement.native());
        if (step == SQLITE_DONE) {
            return std::optional<domain::session::ClientSession>{};
        }
        if (step != SQLITE_ROW) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to read client session")));
        }
        const auto fidValue = sqlite3_column_int64(statement.native(), 0);
        const auto authenticatedAt = sqlite3_column_int64(statement.native(), 5);
        const auto expiresAt = sqlite3_column_int64(statement.native(), 6);
        if (fidValue <= 0 || authenticatedAt < 0 || expiresAt < 0) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::StorageFailure,
                "Persisted client session contains invalid scalar values."));
        }
        auto method = parseMethod(sqlite::columnText(statement.native(), 4));
        if (!method) {
            return std::unexpected(std::move(method.error()));
        }
        domain::session::ClientSession session{
            .fid = domain::identity::Fid{static_cast<std::uint64_t>(fidValue)},
            .username = sqlite::optionalColumnText(statement.native(), 1),
            .displayName = sqlite::optionalColumnText(statement.native(), 2),
            .avatarUrl = sqlite::optionalColumnText(statement.native(), 3),
            .method = *method,
            .authenticatedAt = foundation::time::Timestamp{
                std::chrono::milliseconds{authenticatedAt}},
            .expiresAt = foundation::time::Timestamp{
                std::chrono::milliseconds{expiresAt}},
        };
        return std::optional<domain::session::ClientSession>{std::move(session)};
    }

    [[nodiscard]] foundation::result::Status save(
        const domain::session::ClientSession& session) override
    {
        if (session.fid.value == 0 || session.expiresAt <= session.authenticatedAt) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Client session requires a non-zero FID and a future expiration."));
        }
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
INSERT INTO client_session(
    singleton_id, fid, username, display_name, avatar_url, auth_method,
    authenticated_at_ms, expires_at_ms)
VALUES(1, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(singleton_id) DO UPDATE SET
    fid = excluded.fid,
    username = excluded.username,
    display_name = excluded.display_name,
    avatar_url = excluded.avatar_url,
    auth_method = excluded.auth_method,
    authenticated_at_ms = excluded.authenticated_at_ms,
    expires_at_ms = excluded.expires_at_ms;
)SQL");
        if (!prepared) {
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        }
        auto statement = std::move(*prepared);
        if (sqlite3_bind_int64(
                statement.native(), 1, static_cast<sqlite3_int64>(session.fid.value)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to bind client-session FID")));
        }
        if (auto status = sqlite::bindOptionalText(statement.native(), 2, session.username); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindOptionalText(statement.native(), 3, session.displayName); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindOptionalText(statement.native(), 4, session.avatarUrl); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindText(statement.native(), 5, methodText(session.method)); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(
                statement.native(), 6,
                static_cast<sqlite3_int64>(session.authenticatedAt.time_since_epoch().count())) != SQLITE_OK ||
            sqlite3_bind_int64(
                statement.native(), 7,
                static_cast<sqlite3_int64>(session.expiresAt.time_since_epoch().count())) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to bind client-session timestamps")));
        }
        if (auto status = sqlite::stepDone(m_connection, statement); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        return {};
    }

    [[nodiscard]] foundation::result::Status clear() override
    {
        if (auto status = m_connection.execute(
                "DELETE FROM client_session WHERE singleton_id = 1;"); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        return {};
    }

private:
    sqlite::Connection m_connection;
};

}

foundation::result::Result<std::unique_ptr<local::session::IClientSessionStore>>
openSessionStore(const std::filesystem::path& path)
{
    auto connection = sqlite::Connection::open(path);
    if (!connection) {
        return std::unexpected(toFoundationError(std::move(connection.error())));
    }
    std::unique_ptr<local::session::IClientSessionStore> store =
        std::make_unique<SqliteSessionStore>(std::move(*connection));
    return store;
}

}
