module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <limits>
#include <string_view>
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
import farcaster.capability.signer;

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

// Separate vault namespace and fixed slot, NEVER the social signer key.
// Length framing prevents ambiguity even with arbitrary provider subjects.
constexpr domain::identity::Fid deviceSlot{1U};
[[nodiscard]] std::string sessionSealPayload(const domain::session::ClientSession& session)
{
    std::string bytes{"genycaster-device-session-v1\n"};
    const auto append = [&bytes](const std::string_view value) {
        bytes += std::to_string(value.size()) + ":";
        bytes.append(value);
    };
    const auto optional = [&append](const std::optional<std::string>& value) {
        append(value ? "1" : "0");
        if (value) append(*value);
    };
    optional(session.identityIssuer);
    optional(session.identitySubject);
    append(std::to_string(session.fid.value));
    optional(session.custodyAddress);
    append(methodText(session.method));
    append(std::to_string(session.authenticatedAt.time_since_epoch().count()));
    append(std::to_string(session.expiresAt.time_since_epoch().count()));
    return bytes;
}

[[nodiscard]] foundation::error::Error deviceUnavailable()
{
    return foundation::error::Error::make(foundation::error::ErrorCode::Unavailable,
        "The secure device credential is unavailable.");
}

class SqliteSessionStore final : public local::session::IClientSessionStore {
public:
    explicit SqliteSessionStore(sqlite::Connection connection,
        bool requireDevice = false,
        std::unique_ptr<capability::signer::ISignerVault> deviceVault = {}) noexcept
        : m_connection(std::move(connection)), m_requireDevice(requireDevice),
          m_deviceVault(std::move(deviceVault)) {}

    [[nodiscard]] bool isDeviceBound() const noexcept override { return m_requireDevice; }

    [[nodiscard]] foundation::result::Result<std::optional<domain::session::ClientSession>>
    load() override
    {
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
SELECT identity_issuer, identity_subject, fid, username, display_name, avatar_url,
       custody_address, auth_method, authenticated_at_ms, expires_at_ms
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
        const auto fidValue = sqlite3_column_int64(statement.native(), 2);
        const auto authenticatedAt = sqlite3_column_int64(statement.native(), 8);
        const auto expiresAt = sqlite3_column_int64(statement.native(), 9);
        if (fidValue <= 0 || authenticatedAt < 0 || expiresAt < 0) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::StorageFailure,
                "Persisted client session contains invalid scalar values."));
        }
        auto method = parseMethod(sqlite::columnText(statement.native(), 7));
        if (!method) {
            return std::unexpected(std::move(method.error()));
        }
        domain::session::ClientSession session{
            .identityIssuer = sqlite::optionalColumnText(statement.native(), 0),
            .identitySubject = sqlite::optionalColumnText(statement.native(), 1),
            .fid = domain::identity::Fid{static_cast<std::uint64_t>(fidValue)},
            .username = sqlite::optionalColumnText(statement.native(), 3),
            .displayName = sqlite::optionalColumnText(statement.native(), 4),
            .avatarUrl = sqlite::optionalColumnText(statement.native(), 5),
            .custodyAddress = sqlite::optionalColumnText(statement.native(), 6),
            .method = *method,
            .authenticatedAt = foundation::time::Timestamp{
                std::chrono::milliseconds{authenticatedAt}},
            .expiresAt = foundation::time::Timestamp{
                std::chrono::milliseconds{expiresAt}},
        };
        if (m_requireDevice) {
            if (!m_deviceVault) return std::unexpected(deviceUnavailable());
            auto seal = sqlite::Statement::prepare(m_connection,
                "SELECT signature FROM device_session_seal WHERE singleton_id = 1;");
            if (!seal) return std::unexpected(toFoundationError(std::move(seal.error())));
            const int stepSeal = sqlite3_step(seal->native());
            // Legacy metadata alone is not proof. Require one normal sign-in;
            // do not delete the social signer or the user's cached data.
            if (stepSeal == SQLITE_DONE) return std::optional<domain::session::ClientSession>{};
            if (stepSeal != SQLITE_ROW) return std::unexpected(toFoundationError(
                sqlite::makeError(m_connection.native(), "Failed to read device session seal")));
            const auto stored = sqlite::columnBlob(seal->native(), 0);
            if (stored.size() != 64U) return std::optional<domain::session::ClientSession>{};
            auto key = m_deviceVault->load(deviceSlot);
            if (!key) return std::unexpected(std::move(key.error()));
            if (!*key) return std::optional<domain::session::ClientSession>{};
            const auto proof = (*key)->sign(sessionSealPayload(session));
            if (!proof) return std::unexpected(proof.error());
            if (proof->bytes != stored) return std::optional<domain::session::ClientSession>{};
            session.deviceBound = true;
        }
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
        std::string signature;
        sqlite::Transaction transaction{m_connection};
        if (m_requireDevice) {
            if (!m_deviceVault) return std::unexpected(deviceUnavailable());
            // Rotate on every verified commit. A copied older SQLite record
            // cannot resurrect an earlier account with a surviving device key.
            if (auto erased = m_deviceVault->erase(deviceSlot); !erased)
                return std::unexpected(std::move(erased.error()));
            auto key = m_deviceVault->ensure(deviceSlot);
            if (!key) return std::unexpected(std::move(key.error()));
            if (!*key) return std::unexpected(deviceUnavailable());
            auto proof = (*key)->sign(sessionSealPayload(session));
            if (!proof) return std::unexpected(std::move(proof.error()));
            if (proof->bytes.size() != 64U) return std::unexpected(deviceUnavailable());
            signature = std::move(proof->bytes);
            if (auto begun = transaction.begin(); !begun)
                return std::unexpected(toFoundationError(std::move(begun.error())));
        }
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
INSERT INTO client_session(
    singleton_id, identity_issuer, identity_subject, fid, username, display_name,
    avatar_url, custody_address, auth_method, authenticated_at_ms, expires_at_ms)
VALUES(1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(singleton_id) DO UPDATE SET
    identity_issuer = excluded.identity_issuer,
    identity_subject = excluded.identity_subject,
    fid = excluded.fid,
    username = excluded.username,
    display_name = excluded.display_name,
    avatar_url = excluded.avatar_url,
    custody_address = excluded.custody_address,
    auth_method = excluded.auth_method,
    authenticated_at_ms = excluded.authenticated_at_ms,
    expires_at_ms = excluded.expires_at_ms;
)SQL");
        if (!prepared) {
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        }
        auto statement = std::move(*prepared);
        if (auto status = sqlite::bindOptionalText(
                statement.native(), 1, session.identityIssuer); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindOptionalText(
                statement.native(), 2, session.identitySubject); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(
                statement.native(), 3, static_cast<sqlite3_int64>(session.fid.value)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to bind client-session FID")));
        }
        if (auto status = sqlite::bindOptionalText(statement.native(), 4, session.username); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindOptionalText(statement.native(), 5, session.displayName); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindOptionalText(statement.native(), 6, session.avatarUrl); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindOptionalText(statement.native(), 7, session.custodyAddress); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindText(statement.native(), 8, methodText(session.method)); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(
                statement.native(), 9,
                static_cast<sqlite3_int64>(session.authenticatedAt.time_since_epoch().count())) != SQLITE_OK ||
            sqlite3_bind_int64(
                statement.native(), 10,
                static_cast<sqlite3_int64>(session.expiresAt.time_since_epoch().count())) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to bind client-session timestamps")));
        }
        if (auto status = sqlite::stepDone(m_connection, statement); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (m_requireDevice) {
            auto seal = sqlite::Statement::prepare(m_connection,
                "INSERT INTO device_session_seal(singleton_id,signature) VALUES(1,?) "
                "ON CONFLICT(singleton_id) DO UPDATE SET signature=excluded.signature;");
            if (!seal) return std::unexpected(toFoundationError(std::move(seal.error())));
            if (auto bound = sqlite::bindBlob(seal->native(), 1, signature); !bound)
                return std::unexpected(toFoundationError(std::move(bound.error())));
            if (auto done = sqlite::stepDone(m_connection, *seal); !done)
                return std::unexpected(toFoundationError(std::move(done.error())));
            if (auto committed = transaction.commit(); !committed)
                return std::unexpected(toFoundationError(std::move(committed.error())));
        }
        return {};
    }

    [[nodiscard]] foundation::result::Status clear() override
    {
        // Invalidate the durable device key before clearing metadata. Even an
        // old database copy cannot restore the signed-out identity.
        std::optional<foundation::error::Error> eraseError;
        if (m_requireDevice) {
            if (!m_deviceVault) eraseError = deviceUnavailable();
            else if (auto erased = m_deviceVault->erase(deviceSlot); !erased)
                eraseError = std::move(erased.error());
            if (auto deleted = m_connection.execute("DELETE FROM device_session_seal;"); !deleted)
                return std::unexpected(toFoundationError(std::move(deleted.error())));
        }
        if (auto status = m_connection.execute(
                "DELETE FROM client_session WHERE singleton_id = 1;"); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (eraseError) return std::unexpected(std::move(*eraseError));
        return {};
    }

private:
    sqlite::Connection m_connection;
    bool m_requireDevice{};
    std::unique_ptr<capability::signer::ISignerVault> m_deviceVault{};
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

foundation::result::Result<std::unique_ptr<local::session::IClientSessionStore>>
openDeviceBoundSessionStore(const std::filesystem::path& path,
    std::unique_ptr<capability::signer::ISignerVault> deviceVault)
{
    auto connection = sqlite::Connection::open(path);
    if (!connection) return std::unexpected(toFoundationError(std::move(connection.error())));
    if (auto schema = connection->execute("CREATE TABLE IF NOT EXISTS device_session_seal ("
            "singleton_id INTEGER PRIMARY KEY CHECK(singleton_id=1), signature BLOB NOT NULL);"); !schema)
        return std::unexpected(toFoundationError(std::move(schema.error())));
    std::unique_ptr<local::session::IClientSessionStore> store =
        std::make_unique<SqliteSessionStore>(std::move(*connection), true, std::move(deviceVault));
    return store;
}

}
