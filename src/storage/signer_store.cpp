module;

#include "sqlite_boundary.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.storage.signer;

import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.signer;

namespace farcaster::storage::signer {
namespace {

[[nodiscard]] foundation::error::Error toFoundationError(sqlite::Error error)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::StorageFailure,
        std::move(error.message));
}

[[nodiscard]] std::string registrationKindText(const domain::signer::RegistrationKind kind)
{
    return kind == domain::signer::RegistrationKind::LegacyOnchain ? "legacy-onchain" : "gasless";
}

[[nodiscard]] foundation::result::Result<domain::signer::RegistrationKind>
parseRegistrationKind(const std::string& text)
{
    if (text == "gasless") {
        return domain::signer::RegistrationKind::Gasless;
    }
    if (text == "legacy-onchain") {
        return domain::signer::RegistrationKind::LegacyOnchain;
    }
    return std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::StorageFailure,
        "Persisted signer metadata contains an unknown registration kind."));
}

[[nodiscard]] std::string registrationStateText(const domain::signer::RegistrationState state)
{
    switch (state) {
    case domain::signer::RegistrationState::Missing: return "missing";
    case domain::signer::RegistrationState::Active: return "active";
    case domain::signer::RegistrationState::Revoked: return "revoked";
    case domain::signer::RegistrationState::Expired: return "expired";
    }
    return "missing";
}

[[nodiscard]] foundation::result::Result<domain::signer::RegistrationState>
parseRegistrationState(const std::string& text)
{
    if (text == "missing") return domain::signer::RegistrationState::Missing;
    if (text == "active") return domain::signer::RegistrationState::Active;
    if (text == "revoked") return domain::signer::RegistrationState::Revoked;
    if (text == "expired") return domain::signer::RegistrationState::Expired;
    return std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::StorageFailure,
        "Persisted signer metadata contains an unknown registration state."));
}

[[nodiscard]] std::string scopesText(const std::vector<domain::signer::Scope>& scopes)
{
    std::string text{};
    for (const auto scope : scopes) {
        if (!text.empty()) {
            text.push_back(',');
        }
        text += std::to_string(static_cast<std::int32_t>(scope));
    }
    return text;
}

[[nodiscard]] foundation::result::Result<std::vector<domain::signer::Scope>>
parseScopes(const std::string& text)
{
    std::vector<domain::signer::Scope> scopes{};
    if (text.empty()) {
        return scopes;
    }
    std::size_t start{};
    while (start <= text.size()) {
        const auto end = text.find(',', start);
        const auto token = std::string_view{text}.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        std::int32_t raw{};
        const auto* begin = token.data();
        const auto* finish = token.data() + token.size();
        const auto parsed = std::from_chars(begin, finish, raw);
        if (parsed.ec != std::errc{} || parsed.ptr != finish) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::StorageFailure,
                "Persisted signer scope list contains an invalid value."));
        }
        const auto scope = static_cast<domain::signer::Scope>(raw);
        if (!domain::signer::isAdmissibleScope(scope)) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::StorageFailure,
                "Persisted signer scope is not admissible by the current protocol contract."));
        }
        scopes.push_back(scope);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return scopes;
}

[[nodiscard]] bool isLowerHex64(const std::string_view value)
{
    if (value.size() != 64U) return false;
    for (const char ch : value) {
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
    }
    return true;
}

class SqliteSignerMetadataStore final : public local::signer::ISignerMetadataStore {
public:
    explicit SqliteSignerMetadataStore(sqlite::Connection connection) noexcept
        : m_connection(std::move(connection))
    {
    }

    [[nodiscard]] foundation::result::Result<std::optional<domain::signer::SignerRecord>>
    load(const domain::identity::Fid fid) override
    {
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
SELECT signer_id, public_key_hex, created_at_ms,
       registration_kind, registration_state, scopes, ttl_seconds,
       last_used_at_ms, request_fid, verified_at_ms
FROM signer_metadata WHERE fid = ?;
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (sqlite3_bind_int64(statement.native(), 1, static_cast<sqlite3_int64>(fid.value)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to bind signer-metadata FID")));
        }
        const int step = sqlite3_step(statement.native());
        if (step == SQLITE_DONE) return std::optional<domain::signer::SignerRecord>{};
        if (step != SQLITE_ROW) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to load signer metadata")));
        }

        const auto signerId = sqlite::columnText(statement.native(), 0);
        const auto publicKey = sqlite::columnText(statement.native(), 1);
        const auto createdAt = sqlite3_column_int64(statement.native(), 2);
        if (signerId.empty() || !isLowerHex64(publicKey) || createdAt < 0) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::StorageFailure,
                "Persisted signer metadata contains invalid local key metadata."));
        }
        domain::signer::SignerRecord record{
            .local = domain::signer::LocalSigner{
                .fid = fid,
                .id = domain::signer::SignerId{signerId},
                .publicKey = domain::signer::PublicKey{publicKey},
                .createdAt = foundation::time::Timestamp{std::chrono::milliseconds{createdAt}},
            },
        };

        if (sqlite3_column_type(statement.native(), 3) == SQLITE_NULL) {
            return std::optional<domain::signer::SignerRecord>{std::move(record)};
        }
        if (sqlite3_column_type(statement.native(), 4) == SQLITE_NULL ||
            sqlite3_column_type(statement.native(), 5) == SQLITE_NULL ||
            sqlite3_column_type(statement.native(), 6) == SQLITE_NULL ||
            sqlite3_column_type(statement.native(), 9) == SQLITE_NULL) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::StorageFailure,
                "Persisted signer registration is only partially populated."));
        }
        auto kind = parseRegistrationKind(sqlite::columnText(statement.native(), 3));
        if (!kind) return std::unexpected(std::move(kind.error()));
        auto state = parseRegistrationState(sqlite::columnText(statement.native(), 4));
        if (!state) return std::unexpected(std::move(state.error()));
        auto scopes = parseScopes(sqlite::columnText(statement.native(), 5));
        if (!scopes) return std::unexpected(std::move(scopes.error()));
        const auto ttl = sqlite3_column_int64(statement.native(), 6);
        const auto verifiedAt = sqlite3_column_int64(statement.native(), 9);
        if (ttl < 0 || ttl > domain::signer::MaxGaslessTtlSeconds || verifiedAt < 0) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::StorageFailure,
                "Persisted signer registration contains invalid scalar metadata."));
        }
        domain::signer::Registration registration{
            .fid = fid,
            .publicKey = record.local.publicKey,
            .kind = *kind,
            .state = *state,
            .scopes = std::move(*scopes),
            .ttlSeconds = static_cast<std::uint32_t>(ttl),
            .verifiedAt = foundation::time::Timestamp{std::chrono::milliseconds{verifiedAt}},
        };
        if (sqlite3_column_type(statement.native(), 7) != SQLITE_NULL) {
            const auto lastUsed = sqlite3_column_int64(statement.native(), 7);
            if (lastUsed < 0) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::StorageFailure,
                    "Persisted signer last-used time is invalid."));
            }
            registration.lastUsedAt = foundation::time::Timestamp{std::chrono::milliseconds{lastUsed}};
        }
        if (sqlite3_column_type(statement.native(), 8) != SQLITE_NULL) {
            const auto requestFid = sqlite3_column_int64(statement.native(), 8);
            if (requestFid <= 0) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::StorageFailure,
                    "Persisted signer request FID is invalid."));
            }
            registration.requestFid = domain::identity::Fid{static_cast<std::uint64_t>(requestFid)};
        }
        record.lastVerifiedRegistration = std::move(registration);
        return std::optional<domain::signer::SignerRecord>{std::move(record)};
    }

    [[nodiscard]] foundation::result::Status save(
        const domain::signer::SignerRecord& record) override
    {
        if (record.local.fid.value == 0 || record.local.id.value.empty() ||
            !isLowerHex64(record.local.publicKey.hex)) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Signer metadata requires a FID, opaque id, and 32-byte lowercase-hex public key."));
        }
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
INSERT INTO signer_metadata(
    fid, signer_id, public_key_hex, created_at_ms,
    registration_kind, registration_state, scopes, ttl_seconds,
    last_used_at_ms, request_fid, verified_at_ms)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(fid) DO UPDATE SET
    signer_id = excluded.signer_id,
    public_key_hex = excluded.public_key_hex,
    created_at_ms = excluded.created_at_ms,
    registration_kind = excluded.registration_kind,
    registration_state = excluded.registration_state,
    scopes = excluded.scopes,
    ttl_seconds = excluded.ttl_seconds,
    last_used_at_ms = excluded.last_used_at_ms,
    request_fid = excluded.request_fid,
    verified_at_ms = excluded.verified_at_ms;
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (sqlite3_bind_int64(statement.native(), 1,
                static_cast<sqlite3_int64>(record.local.fid.value)) != SQLITE_OK ||
            sqlite3_bind_int64(statement.native(), 4,
                static_cast<sqlite3_int64>(record.local.createdAt.time_since_epoch().count())) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to bind signer local metadata")));
        }
        if (auto status = sqlite::bindText(statement.native(), 2, record.local.id.value); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        if (auto status = sqlite::bindText(statement.native(), 3, record.local.publicKey.hex); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));

        if (!record.lastVerifiedRegistration) {
            for (int index = 5; index <= 11; ++index) {
                if (sqlite3_bind_null(statement.native(), index) != SQLITE_OK) {
                    return std::unexpected(toFoundationError(sqlite::makeError(
                        m_connection.native(), "Failed to bind empty signer registration")));
                }
            }
        } else {
            const auto& registration = *record.lastVerifiedRegistration;
            if (registration.fid.value != record.local.fid.value ||
                registration.publicKey.hex != record.local.publicKey.hex) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::InvalidArgument,
                    "Signer registration must describe the same FID/public key as the local signer."));
            }
            if (auto status = sqlite::bindText(statement.native(), 5,
                    registrationKindText(registration.kind)); !status)
                return std::unexpected(toFoundationError(std::move(status.error())));
            if (auto status = sqlite::bindText(statement.native(), 6,
                    registrationStateText(registration.state)); !status)
                return std::unexpected(toFoundationError(std::move(status.error())));
            if (auto status = sqlite::bindText(statement.native(), 7,
                    scopesText(registration.scopes)); !status)
                return std::unexpected(toFoundationError(std::move(status.error())));
            if (sqlite3_bind_int64(statement.native(), 8, registration.ttlSeconds) != SQLITE_OK)
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_connection.native(), "Failed to bind signer TTL")));
            if (registration.lastUsedAt) {
                if (sqlite3_bind_int64(statement.native(), 9,
                        registration.lastUsedAt->time_since_epoch().count()) != SQLITE_OK)
                    return std::unexpected(toFoundationError(sqlite::makeError(
                        m_connection.native(), "Failed to bind signer last-used time")));
            } else if (sqlite3_bind_null(statement.native(), 9) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_connection.native(), "Failed to bind empty signer last-used time")));
            }
            if (registration.requestFid) {
                if (sqlite3_bind_int64(statement.native(), 10,
                        static_cast<sqlite3_int64>(registration.requestFid->value)) != SQLITE_OK)
                    return std::unexpected(toFoundationError(sqlite::makeError(
                        m_connection.native(), "Failed to bind signer request FID")));
            } else if (sqlite3_bind_null(statement.native(), 10) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_connection.native(), "Failed to bind empty signer request FID")));
            }
            if (sqlite3_bind_int64(statement.native(), 11,
                    registration.verifiedAt.time_since_epoch().count()) != SQLITE_OK)
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_connection.native(), "Failed to bind signer verification time")));
        }
        if (auto status = sqlite::stepDone(m_connection, statement); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        return {};
    }

    [[nodiscard]] foundation::result::Status clear(const domain::identity::Fid fid) override
    {
        auto prepared = sqlite::Statement::prepare(
            m_connection, "DELETE FROM signer_metadata WHERE fid = ?;");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (sqlite3_bind_int64(statement.native(), 1, static_cast<sqlite3_int64>(fid.value)) != SQLITE_OK)
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to bind signer-metadata clear FID")));
        if (auto status = sqlite::stepDone(m_connection, statement); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        return {};
    }

private:
    sqlite::Connection m_connection;
};

}

foundation::result::Result<std::unique_ptr<local::signer::ISignerMetadataStore>>
openSignerMetadataStore(const std::filesystem::path& path)
{
    auto connection = sqlite::Connection::open(path);
    if (!connection) return std::unexpected(toFoundationError(std::move(connection.error())));
    std::unique_ptr<local::signer::ISignerMetadataStore> store =
        std::make_unique<SqliteSignerMetadataStore>(std::move(*connection));
    return store;
}

}
