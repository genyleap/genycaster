module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.storage.write;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.write;

namespace farcaster::storage::write {
namespace {

[[nodiscard]] foundation::error::Error toFoundationError(sqlite::Error error)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::StorageFailure,
        std::move(error.message));
}

[[nodiscard]] foundation::error::Error storageFailure(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::StorageFailure,
        std::move(message));
}

[[nodiscard]] foundation::error::Error notFound(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::NotFound,
        std::move(message));
}

[[nodiscard]] foundation::error::Error conflict(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::Conflict,
        std::move(message));
}

[[nodiscard]] std::int64_t toMilliseconds(const foundation::time::Timestamp timestamp)
{
    return timestamp.time_since_epoch().count();
}

[[nodiscard]] foundation::time::Timestamp fromMilliseconds(const std::int64_t value)
{
    return foundation::time::Timestamp{std::chrono::milliseconds{value}};
}

[[nodiscard]] foundation::result::Result<domain::write::OutboxStatus> parseStatus(
    const std::string_view text)
{
    if (text == "pending") return domain::write::OutboxStatus::Pending;
    if (text == "confirmed") return domain::write::OutboxStatus::Confirmed;
    if (text == "failed") return domain::write::OutboxStatus::Failed;
    return std::unexpected(storageFailure("Persisted write outbox contains an unknown status."));
}

[[nodiscard]] std::string failureKindText(const domain::write::FailureKind kind)
{
    switch (kind) {
    case domain::write::FailureKind::Authorization: return "authorization";
    case domain::write::FailureKind::RateLimited: return "rate-limited";
    case domain::write::FailureKind::Unavailable: return "unavailable";
    case domain::write::FailureKind::Conflict: return "conflict";
    case domain::write::FailureKind::ProtocolRejected: return "protocol-rejected";
    case domain::write::FailureKind::Unsupported: return "unsupported";
    case domain::write::FailureKind::Internal: return "internal";
    }
    return "internal";
}

[[nodiscard]] foundation::result::Result<domain::write::FailureKind> parseFailureKind(
    const std::string_view text)
{
    if (text == "authorization") return domain::write::FailureKind::Authorization;
    if (text == "rate-limited") return domain::write::FailureKind::RateLimited;
    if (text == "unavailable") return domain::write::FailureKind::Unavailable;
    if (text == "conflict") return domain::write::FailureKind::Conflict;
    if (text == "protocol-rejected") return domain::write::FailureKind::ProtocolRejected;
    if (text == "unsupported") return domain::write::FailureKind::Unsupported;
    if (text == "internal") return domain::write::FailureKind::Internal;
    return std::unexpected(storageFailure("Persisted write outbox contains an unknown failure kind."));
}

[[nodiscard]] foundation::result::Status bindInt64(
    sqlite::Connection& connection,
    sqlite3_stmt* statement,
    const int index,
    const std::int64_t value,
    const std::string_view context)
{
    if (sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(value)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(connection.native(), context)));
    }
    return {};
}

[[nodiscard]] foundation::result::Status insertCast(
    sqlite::Connection& connection,
    const domain::cast::Cast& cast)
{
    auto prepared = sqlite::Statement::prepare(connection, R"SQL(
INSERT OR IGNORE INTO casts(
    id, author_fid, text, parent_id, parent_url, created_at_ms,
    reply_count, recast_count, like_count)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, cast.id.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = bindInt64(connection, statement.native(), 2,
            static_cast<std::int64_t>(cast.author.value), "Failed to bind optimistic cast author"); !status) {
        return status;
    }
    if (auto status = sqlite::bindText(statement.native(), 3, cast.text); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    const std::optional<std::string> parentId = cast.parent
        ? std::optional<std::string>{cast.parent->value} : std::nullopt;
    if (auto status = sqlite::bindOptionalText(statement.native(), 4, parentId); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(statement.native(), 5, cast.parentUrl); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = bindInt64(connection, statement.native(), 6, toMilliseconds(cast.createdAt),
            "Failed to bind optimistic cast timestamp"); !status) {
        return status;
    }
    if (auto status = bindInt64(connection, statement.native(), 7,
            static_cast<std::int64_t>(cast.engagement.replies), "Failed to bind optimistic reply count"); !status) {
        return status;
    }
    if (auto status = bindInt64(connection, statement.native(), 8,
            static_cast<std::int64_t>(cast.engagement.recasts), "Failed to bind optimistic recast count"); !status) {
        return status;
    }
    if (auto status = bindInt64(connection, statement.native(), 9,
            static_cast<std::int64_t>(cast.engagement.likes), "Failed to bind optimistic like count"); !status) {
        return status;
    }
    if (auto status = sqlite::stepDone(connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    if (sqlite3_changes(connection.native()) == 0) {
        return {};
    }

    auto embedPrepared = sqlite::Statement::prepare(connection, R"SQL(
INSERT INTO cast_embed_items(cast_id, position, kind, url, cast_ref_id, cast_ref_fid)
VALUES(?, ?, ?, ?, ?, ?);
)SQL");
    if (!embedPrepared) {
        return std::unexpected(toFoundationError(std::move(embedPrepared.error())));
    }
    auto embedStatement = std::move(*embedPrepared);
    for (std::size_t index{}; index < cast.embeds.size(); ++index) {
        embedStatement.reset();
        const auto& embed = cast.embeds[index];
        if (auto status = sqlite::bindText(embedStatement.native(), 1, cast.id.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = bindInt64(connection, embedStatement.native(), 2,
                static_cast<std::int64_t>(index), "Failed to bind optimistic embed position"); !status) {
            return status;
        }
        if (embed.kind == domain::cast::EmbedKind::Url) {
            if (auto status = sqlite::bindText(embedStatement.native(), 3, "url"); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (auto status = sqlite::bindText(embedStatement.native(), 4, embed.value); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (sqlite3_bind_null(embedStatement.native(), 5) != SQLITE_OK ||
                sqlite3_bind_null(embedStatement.native(), 6) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    connection.native(), "Failed to bind optimistic URL embed nulls")));
            }
        } else {
            if (auto status = sqlite::bindText(embedStatement.native(), 3, "cast"); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (sqlite3_bind_null(embedStatement.native(), 4) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    connection.native(), "Failed to bind optimistic cast-reference URL")));
            }
            if (auto status = sqlite::bindText(embedStatement.native(), 5, embed.value); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (!embed.referencedAuthor) {
                return std::unexpected(storageFailure(
                    "Optimistic cast-reference embed is missing its referenced-author FID."));
            }
            if (auto status = bindInt64(connection, embedStatement.native(), 6,
                    static_cast<std::int64_t>(embed.referencedAuthor->value),
                    "Failed to bind optimistic cast-reference FID"); !status) {
                return status;
            }
        }
        if (auto status = sqlite::stepDone(connection, embedStatement); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
    }
    return {};
}

[[nodiscard]] foundation::result::Status insertCastTombstone(
    sqlite::Connection& connection,
    const domain::write::SignedOperation& operation,
    const foundation::time::Timestamp createdAt)
{
    if (operation.kind != domain::write::OperationKind::RemoveCast || !operation.target.cast) {
        return {};
    }

    auto prepared = sqlite::Statement::prepare(connection, R"SQL(
INSERT OR IGNORE INTO cast_local_tombstones(
    cast_id, idempotency_key, author_fid, created_at_ms)
VALUES(?, ?, ?, ?);
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(
            statement.native(), 1, operation.target.cast->id.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindText(
            statement.native(), 2, operation.idempotencyKey.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (operation.author.value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::unexpected(storageFailure("FID is outside SQLite INTEGER range."));
    }
    if (auto status = bindInt64(
            connection, statement.native(), 3, static_cast<std::int64_t>(operation.author.value),
            "Failed to bind local cast tombstone author"); !status) {
        return status;
    }
    if (auto status = bindInt64(
            connection, statement.native(), 4, toMilliseconds(createdAt),
            "Failed to bind local cast tombstone timestamp"); !status) {
        return status;
    }
    if (auto status = sqlite::stepDone(connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

[[nodiscard]] foundation::result::Status upsertReactionProjection(
    sqlite::Connection& connection,
    const domain::write::SignedOperation& operation,
    const foundation::time::Timestamp updatedAt)
{
    using domain::write::OperationKind;
    const bool isLike = operation.kind == OperationKind::Like ||
        operation.kind == OperationKind::Unlike;
    const bool isRecast = operation.kind == OperationKind::Recast ||
        operation.kind == OperationKind::UndoRecast;
    if ((!isLike && !isRecast) || !operation.target.cast) {
        return {};
    }

    auto prepared = sqlite::Statement::prepare(connection, R"SQL(
INSERT INTO cast_reaction_local_state(
    author_fid, cast_id, target_cast_fid, reaction_kind,
    is_active, idempotency_key, updated_at_ms)
VALUES(?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(author_fid, cast_id, reaction_kind) DO UPDATE SET
    target_cast_fid = excluded.target_cast_fid,
    is_active = excluded.is_active,
    idempotency_key = excluded.idempotency_key,
    updated_at_ms = excluded.updated_at_ms;
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (operation.author.value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        operation.target.cast->author.value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::unexpected(storageFailure("FID is outside SQLite INTEGER range."));
    }
    if (auto status = bindInt64(connection, statement.native(), 1,
            static_cast<std::int64_t>(operation.author.value),
            "Failed to bind local reaction author"); !status) {
        return status;
    }
    if (auto status = sqlite::bindText(
            statement.native(), 2, operation.target.cast->id.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = bindInt64(connection, statement.native(), 3,
            static_cast<std::int64_t>(operation.target.cast->author.value),
            "Failed to bind local reaction target author"); !status) {
        return status;
    }
    if (auto status = sqlite::bindText(
            statement.native(), 4, isLike ? "like" : "recast"); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    const bool isActive = operation.kind == OperationKind::Like ||
        operation.kind == OperationKind::Recast;
    if (sqlite3_bind_int(statement.native(), 5, isActive ? 1 : 0) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            connection.native(), "Failed to bind local reaction desired state")));
    }
    if (auto status = sqlite::bindText(
            statement.native(), 6, operation.idempotencyKey.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = bindInt64(connection, statement.native(), 7, toMilliseconds(updatedAt),
            "Failed to bind local reaction timestamp"); !status) {
        return status;
    }
    if (auto status = sqlite::stepDone(connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

[[nodiscard]] foundation::result::Status upsertFollowingProjection(
    sqlite::Connection& connection,
    const domain::write::SignedOperation& operation,
    const foundation::time::Timestamp updatedAt)
{
    using domain::write::OperationKind;
    const bool isFollowingOperation = operation.kind == OperationKind::Follow ||
        operation.kind == OperationKind::Unfollow;
    if (!isFollowingOperation || !operation.target.fid) {
        return {};
    }
    if (operation.author.value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        operation.target.fid->value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::unexpected(storageFailure("FID is outside SQLite INTEGER range."));
    }

    auto prepared = sqlite::Statement::prepare(connection, R"SQL(
INSERT INTO following_local_state(
    author_fid, target_fid, is_following, idempotency_key, updated_at_ms)
VALUES(?, ?, ?, ?, ?)
ON CONFLICT(author_fid, target_fid) DO UPDATE SET
    is_following = excluded.is_following,
    idempotency_key = excluded.idempotency_key,
    updated_at_ms = excluded.updated_at_ms;
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = bindInt64(connection, statement.native(), 1,
            static_cast<std::int64_t>(operation.author.value),
            "Failed to bind local following author"); !status) {
        return status;
    }
    if (auto status = bindInt64(connection, statement.native(), 2,
            static_cast<std::int64_t>(operation.target.fid->value),
            "Failed to bind local following target"); !status) {
        return status;
    }
    if (sqlite3_bind_int(
            statement.native(), 3, operation.kind == OperationKind::Follow ? 1 : 0) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            connection.native(), "Failed to bind local following desired state")));
    }
    if (auto status = sqlite::bindText(
            statement.native(), 4, operation.idempotencyKey.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = bindInt64(connection, statement.native(), 5, toMilliseconds(updatedAt),
            "Failed to bind local following timestamp"); !status) {
        return status;
    }
    if (auto status = sqlite::stepDone(connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

[[nodiscard]] foundation::result::Result<domain::cast::Cast> loadCast(
    sqlite::Connection& connection,
    const std::string_view castId)
{
    auto prepared = sqlite::Statement::prepare(connection, R"SQL(
SELECT author_fid, text, parent_id, parent_url, created_at_ms,
       reply_count, recast_count, like_count
FROM casts WHERE id = ?;
)SQL");
    if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, castId); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    const int step = sqlite3_step(statement.native());
    if (step == SQLITE_DONE) {
        return std::unexpected(storageFailure("Write outbox references a missing optimistic cast."));
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(connection.native(), "Failed to load outbox cast")));
    }

    domain::cast::Cast cast{};
    cast.id = domain::cast::CastId{std::string{castId}};
    cast.author = domain::identity::Fid{static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 0))};
    cast.text = sqlite::columnText(statement.native(), 1);
    if (auto parent = sqlite::optionalColumnText(statement.native(), 2)) {
        cast.parent = domain::cast::CastId{std::move(*parent)};
    }
    cast.parentUrl = sqlite::optionalColumnText(statement.native(), 3);
    cast.createdAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 4));
    cast.engagement.replies = static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 5));
    cast.engagement.recasts = static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 6));
    cast.engagement.likes = static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 7));

    auto embedsPrepared = sqlite::Statement::prepare(connection, R"SQL(
SELECT kind, url, cast_ref_id, cast_ref_fid
FROM cast_embed_items WHERE cast_id = ? ORDER BY position ASC;
)SQL");
    if (!embedsPrepared) return std::unexpected(toFoundationError(std::move(embedsPrepared.error())));
    auto embeds = std::move(*embedsPrepared);
    if (auto status = sqlite::bindText(embeds.native(), 1, castId); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    while (true) {
        const int embedStep = sqlite3_step(embeds.native());
        if (embedStep == SQLITE_DONE) break;
        if (embedStep != SQLITE_ROW) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                connection.native(), "Failed to load outbox cast embeds")));
        }
        const std::string kind = sqlite::columnText(embeds.native(), 0);
        if (kind == "url") {
            auto url = sqlite::optionalColumnText(embeds.native(), 1);
            if (!url) return std::unexpected(storageFailure("Stored optimistic URL embed is missing its URL."));
            cast.embeds.push_back(domain::cast::Embed{
                .kind = domain::cast::EmbedKind::Url,
                .value = std::move(*url),
            });
        } else if (kind == "cast") {
            auto reference = sqlite::optionalColumnText(embeds.native(), 2);
            if (!reference || sqlite3_column_type(embeds.native(), 3) == SQLITE_NULL) {
                return std::unexpected(storageFailure(
                    "Stored optimistic cast-reference embed is incomplete."));
            }
            cast.embeds.push_back(domain::cast::Embed{
                .kind = domain::cast::EmbedKind::CastReference,
                .value = std::move(*reference),
                .referencedAuthor = domain::identity::Fid{
                    static_cast<std::uint64_t>(sqlite3_column_int64(embeds.native(), 3))},
            });
        } else {
            return std::unexpected(storageFailure("Stored optimistic cast embed has an unknown kind."));
        }
    }
    return cast;
}

[[nodiscard]] std::string operationKindText(const domain::write::OperationKind kind)
{
    using domain::write::OperationKind;
    switch (kind) {
    case OperationKind::PublishCast: return "publish-cast";
    case OperationKind::RemoveCast: return "remove-cast";
    case OperationKind::Like: return "like";
    case OperationKind::Unlike: return "unlike";
    case OperationKind::Recast: return "recast";
    case OperationKind::UndoRecast: return "undo-recast";
    case OperationKind::Follow: return "follow";
    case OperationKind::Unfollow: return "unfollow";
    case OperationKind::Block: return "block";
    case OperationKind::Unblock: return "unblock";
    }
    return "publish-cast";
}

[[nodiscard]] foundation::result::Result<domain::write::OperationKind> parseOperationKind(
    const std::string_view text)
{
    using domain::write::OperationKind;
    if (text == "publish-cast") return OperationKind::PublishCast;
    if (text == "remove-cast") return OperationKind::RemoveCast;
    if (text == "like") return OperationKind::Like;
    if (text == "unlike") return OperationKind::Unlike;
    if (text == "recast") return OperationKind::Recast;
    if (text == "undo-recast") return OperationKind::UndoRecast;
    if (text == "follow") return OperationKind::Follow;
    if (text == "unfollow") return OperationKind::Unfollow;
    if (text == "block") return OperationKind::Block;
    if (text == "unblock") return OperationKind::Unblock;
    return std::unexpected(storageFailure("Write outbox contains an unsupported operation kind."));
}

[[nodiscard]] foundation::result::Status bindOptionalFid(
    sqlite::Connection& connection,
    sqlite3_stmt* const statement,
    const int index,
    const std::optional<domain::identity::Fid> fid,
    const std::string_view context)
{
    if (!fid) {
        if (sqlite3_bind_null(statement, index) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(connection.native(), context)));
        }
        return {};
    }
    if (fid->value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::unexpected(storageFailure("FID is outside SQLite INTEGER range."));
    }
    return bindInt64(connection, statement, index, static_cast<std::int64_t>(fid->value), context);
}

[[nodiscard]] foundation::result::Status validateOperationShape(
    const domain::write::SignedOperation& operation)
{
    using domain::write::OperationKind;
    if (operation.idempotencyKey.value.empty() || operation.author.value == 0U ||
        operation.messageBytes.empty() || operation.signerPublicKey.hex.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Durable outbox enqueue requires a complete signed protocol operation."));
    }
    if (operation.dependsOn &&
        (operation.dependsOn->value.empty() || operation.dependsOn->value == operation.idempotencyKey.value)) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Durable outbox dependency must reference a distinct non-empty operation key."));
    }
    if (operation.dependsOn && operation.kind != domain::write::OperationKind::PublishCast) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Only PublishCast operations may currently depend on another outbox write."));
    }
    const bool hasCastTarget = operation.target.cast.has_value();
    const bool hasFidTarget = operation.target.fid.has_value();
    const bool hasOptimisticCast = operation.optimisticCast.has_value();
    switch (operation.kind) {
    case OperationKind::PublishCast:
        if (!hasOptimisticCast || hasCastTarget || hasFidTarget ||
            operation.optimisticCast->id.value.empty() ||
            operation.optimisticCast->author.value != operation.author.value ||
            operation.idempotencyKey.value != operation.optimisticCast->id.value) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "PublishCast requires one matching optimistic cast and no external target."));
        }
        return {};
    case OperationKind::RemoveCast:
        if (hasOptimisticCast || !hasCastTarget || hasFidTarget ||
            operation.target.cast->author.value != operation.author.value) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "RemoveCast requires one cast target owned by the operation author."));
        }
        return {};
    case OperationKind::Like:
    case OperationKind::Unlike:
    case OperationKind::Recast:
    case OperationKind::UndoRecast:
        if (hasOptimisticCast || !hasCastTarget || hasFidTarget ||
            operation.target.cast->id.value.empty() || operation.target.cast->author.value == 0U) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Reaction operations require exactly one valid cast target."));
        }
        return {};
    case OperationKind::Follow:
    case OperationKind::Unfollow:
    case OperationKind::Block:
    case OperationKind::Unblock:
        if (hasOptimisticCast || hasCastTarget || !hasFidTarget ||
            operation.target.fid->value == 0U || operation.target.fid->value == operation.author.value) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Social link operations require exactly one non-self FID target."));
        }
        return {};
    }
    return std::unexpected(foundation::error::Error::make(
        foundation::error::ErrorCode::InvalidArgument,
        "Unsupported protocol operation kind."));
}

[[nodiscard]] foundation::result::Result<domain::write::OutboxRecord> loadRecord(
    sqlite::Connection& connection,
    const std::string_view key)
{
    auto prepared = sqlite::Statement::prepare(connection, R"SQL(
SELECT kind, cast_id, author_fid, target_cast_id, target_cast_fid, target_fid,
       signer_public_key_hex, message_bytes, signed_at_ms, enqueued_at_ms,
       status, attempt_count, next_attempt_at_ms, last_failure_kind,
       last_failure_message, confirmed_at_ms, depends_on_key
FROM write_outbox WHERE idempotency_key = ?;
)SQL");
    if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, key); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    const int step = sqlite3_step(statement.native());
    if (step == SQLITE_DONE) {
        return std::unexpected(notFound("Write outbox record was not found."));
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            connection.native(), "Failed to load write outbox record")));
    }

    auto kind = parseOperationKind(sqlite::columnText(statement.native(), 0));
    if (!kind) return std::unexpected(std::move(kind.error()));
    const bool hasCastId = sqlite3_column_type(statement.native(), 1) != SQLITE_NULL;
    const std::string castId = hasCastId ? sqlite::columnText(statement.native(), 1) : std::string{};
    const auto rawAuthor = sqlite3_column_int64(statement.native(), 2);
    if (rawAuthor <= 0) {
        return std::unexpected(storageFailure("Write outbox contains an invalid author FID."));
    }
    const domain::identity::Fid author{static_cast<std::uint64_t>(rawAuthor)};

    domain::write::OperationTarget target{};
    const bool hasTargetCastId = sqlite3_column_type(statement.native(), 3) != SQLITE_NULL;
    const std::string targetCastId = hasTargetCastId ? sqlite::columnText(statement.native(), 3) : std::string{};
    const bool hasTargetCastFid = sqlite3_column_type(statement.native(), 4) != SQLITE_NULL;
    if (hasTargetCastId != hasTargetCastFid) {
        return std::unexpected(storageFailure("Write outbox contains a partial cast target."));
    }
    if (hasTargetCastId) {
        const auto rawFid = sqlite3_column_int64(statement.native(), 4);
        if (rawFid <= 0) {
            return std::unexpected(storageFailure("Write outbox contains an invalid target cast FID."));
        }
        target.cast = domain::cast::CastReference{
            .author = domain::identity::Fid{static_cast<std::uint64_t>(rawFid)},
            .id = domain::cast::CastId{targetCastId},
        };
    }
    if (sqlite3_column_type(statement.native(), 5) != SQLITE_NULL) {
        const auto rawFid = sqlite3_column_int64(statement.native(), 5);
        if (rawFid <= 0) {
            return std::unexpected(storageFailure("Write outbox contains an invalid target FID."));
        }
        target.fid = domain::identity::Fid{static_cast<std::uint64_t>(rawFid)};
    }

    std::optional<domain::cast::Cast> optimisticCast{};
    if (hasCastId) {
        auto cast = loadCast(connection, castId);
        if (!cast) return std::unexpected(std::move(cast.error()));
        if (cast->author.value != author.value) {
            return std::unexpected(storageFailure("Write outbox author does not match its optimistic cast."));
        }
        optimisticCast = std::move(*cast);
    }

    std::string messageBytes = sqlite::columnBlob(statement.native(), 7);
    if (messageBytes.empty()) {
        return std::unexpected(storageFailure("Write outbox contains an empty canonical message envelope."));
    }
    auto persistedStatus = parseStatus(sqlite::columnText(statement.native(), 10));
    if (!persistedStatus) return std::unexpected(std::move(persistedStatus.error()));
    const auto rawAttempts = sqlite3_column_int64(statement.native(), 11);
    if (rawAttempts < 0 || rawAttempts > static_cast<sqlite3_int64>(std::numeric_limits<std::uint32_t>::max())) {
        return std::unexpected(storageFailure("Write outbox attempt count is outside uint32 range."));
    }

    domain::write::OutboxRecord record{
        .operation = domain::write::SignedOperation{
            .kind = *kind,
            .idempotencyKey = domain::write::IdempotencyKey{std::string{key}},
            .author = author,
            .target = std::move(target),
            .optimisticCast = std::move(optimisticCast),
            .dependsOn = sqlite3_column_type(statement.native(), 16) != SQLITE_NULL
                ? std::optional<domain::write::IdempotencyKey>{domain::write::IdempotencyKey{
                    sqlite::columnText(statement.native(), 16)}}
                : std::nullopt,
            .signerPublicKey = domain::signer::PublicKey{sqlite::columnText(statement.native(), 6)},
            .messageBytes = std::move(messageBytes),
            .signedAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 8)),
        },
        .status = *persistedStatus,
        .attemptCount = static_cast<std::uint32_t>(rawAttempts),
        .enqueuedAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 9)),
    };
    if (auto valid = validateOperationShape(record.operation); !valid) {
        return std::unexpected(storageFailure("Persisted write outbox operation violates its canonical shape."));
    }
    if (sqlite3_column_type(statement.native(), 12) != SQLITE_NULL) {
        record.nextAttemptAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 12));
    }
    if (sqlite3_column_type(statement.native(), 13) != SQLITE_NULL) {
        auto parsedKind = parseFailureKind(sqlite::columnText(statement.native(), 13));
        if (!parsedKind) return std::unexpected(std::move(parsedKind.error()));
        record.lastFailure = domain::write::OutboxFailure{
            .kind = *parsedKind,
            .message = sqlite::optionalColumnText(statement.native(), 14).value_or(std::string{}),
        };
    } else if (sqlite3_column_type(statement.native(), 14) != SQLITE_NULL) {
        return std::unexpected(storageFailure("Write outbox failure message exists without a failure kind."));
    }
    if (sqlite3_column_type(statement.native(), 15) != SQLITE_NULL) {
        record.confirmedAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 15));
    }
    return record;
}

[[nodiscard]] foundation::result::Status updateProjectionState(
    sqlite::Connection& connection,
    const domain::write::IdempotencyKey& key,
    const std::string_view state,
    const foundation::time::Timestamp updatedAt)
{
    auto prepared = sqlite::Statement::prepare(connection, R"SQL(
UPDATE cast_write_state SET state = ?, updated_at_ms = ? WHERE idempotency_key = ?;
)SQL");
    if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, state); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = bindInt64(connection, statement.native(), 2, toMilliseconds(updatedAt),
            "Failed to bind optimistic projection update timestamp"); !status) {
        return status;
    }
    if (auto status = sqlite::bindText(statement.native(), 3, key.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::stepDone(connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_changes(connection.native()) != 0) {
        return {};
    }

    auto checkPrepared = sqlite::Statement::prepare(
        connection, "SELECT cast_id FROM write_outbox WHERE idempotency_key = ?;");
    if (!checkPrepared) return std::unexpected(toFoundationError(std::move(checkPrepared.error())));
    auto check = std::move(*checkPrepared);
    if (auto status = sqlite::bindText(check.native(), 1, key.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    const int step = sqlite3_step(check.native());
    if (step == SQLITE_DONE) {
        return std::unexpected(notFound("Write outbox record disappeared during projection reconciliation."));
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            connection.native(), "Failed to inspect write projection ownership")));
    }
    if (sqlite3_column_type(check.native(), 0) == SQLITE_NULL) {
        return {};
    }
    return std::unexpected(storageFailure("Write projection state was missing during reconciliation."));
}

class SqliteOutboxStore final : public local::write::IOutboxStore {
public:
    [[nodiscard]] static foundation::result::Result<std::unique_ptr<SqliteOutboxStore>> open(
        const std::filesystem::path& path)
    {
        auto connection = sqlite::Connection::open(path);
        if (!connection) return std::unexpected(toFoundationError(std::move(connection.error())));
        return std::make_unique<SqliteOutboxStore>(std::move(*connection));
    }

    explicit SqliteOutboxStore(sqlite::Connection connection)
        : m_connection(std::move(connection))
    {
    }

    [[nodiscard]] foundation::result::Result<domain::write::OutboxRecord> enqueue(
        const domain::write::SignedOperation& operation,
        const foundation::time::Timestamp enqueuedAt) override
    {
        std::scoped_lock lock{m_mutex};
        if (auto valid = validateOperationShape(operation); !valid) {
            return std::unexpected(std::move(valid.error()));
        }

        sqlite::Transaction transaction{m_connection};
        if (auto status = transaction.begin(); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (operation.optimisticCast) {
            if (auto status = insertCast(m_connection, *operation.optimisticCast); !status) {
                return std::unexpected(std::move(status.error()));
            }
        }

        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
INSERT OR IGNORE INTO write_outbox(
    idempotency_key, kind, cast_id, author_fid,
    target_cast_id, target_cast_fid, target_fid,
    signer_public_key_hex, message_bytes, signed_at_ms, enqueued_at_ms,
    depends_on_key, status, attempt_count)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pending', 0);
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = sqlite::bindText(statement.native(), 1, operation.idempotencyKey.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindText(statement.native(), 2, operationKindText(operation.kind)); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        const std::optional<std::string> optimisticCastId = operation.optimisticCast
            ? std::optional<std::string>{operation.optimisticCast->id.value} : std::nullopt;
        if (auto status = sqlite::bindOptionalText(statement.native(), 3, optimisticCastId); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = bindInt64(m_connection, statement.native(), 4,
                static_cast<std::int64_t>(operation.author.value), "Failed to bind outbox author"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        const std::optional<std::string> targetCastId = operation.target.cast
            ? std::optional<std::string>{operation.target.cast->id.value} : std::nullopt;
        if (auto status = sqlite::bindOptionalText(statement.native(), 5, targetCastId); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        const std::optional<domain::identity::Fid> targetCastFid = operation.target.cast
            ? std::optional<domain::identity::Fid>{operation.target.cast->author} : std::nullopt;
        if (auto status = bindOptionalFid(m_connection, statement.native(), 6, targetCastFid,
                "Failed to bind outbox target-cast FID"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        if (auto status = bindOptionalFid(m_connection, statement.native(), 7, operation.target.fid,
                "Failed to bind outbox target FID"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        if (auto status = sqlite::bindText(statement.native(), 8, operation.signerPublicKey.hex); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindBlob(statement.native(), 9, operation.messageBytes); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = bindInt64(m_connection, statement.native(), 10, toMilliseconds(operation.signedAt),
                "Failed to bind outbox signing timestamp"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        if (auto status = bindInt64(m_connection, statement.native(), 11, toMilliseconds(enqueuedAt),
                "Failed to bind outbox enqueue timestamp"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        const std::optional<std::string> dependencyKey = operation.dependsOn
            ? std::optional<std::string>{operation.dependsOn->value} : std::nullopt;
        if (auto status = sqlite::bindOptionalText(statement.native(), 12, dependencyKey); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::stepDone(m_connection, statement); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        const bool inserted = sqlite3_changes(m_connection.native()) != 0;

        if (inserted) {
            if (auto status = insertCastTombstone(m_connection, operation, enqueuedAt); !status) {
                return std::unexpected(std::move(status.error()));
            }
            if (auto status = upsertReactionProjection(m_connection, operation, enqueuedAt); !status) {
                return std::unexpected(std::move(status.error()));
            }
            if (auto status = upsertFollowingProjection(m_connection, operation, enqueuedAt); !status) {
                return std::unexpected(std::move(status.error()));
            }
        }

        if (inserted && operation.optimisticCast) {
            auto projectionPrepared = sqlite::Statement::prepare(m_connection, R"SQL(
INSERT INTO cast_write_state(cast_id, idempotency_key, state, updated_at_ms)
VALUES(?, ?, 'pending', ?);
)SQL");
            if (!projectionPrepared) {
                return std::unexpected(toFoundationError(std::move(projectionPrepared.error())));
            }
            auto projection = std::move(*projectionPrepared);
            if (auto status = sqlite::bindText(
                    projection.native(), 1, operation.optimisticCast->id.value); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (auto status = sqlite::bindText(projection.native(), 2, operation.idempotencyKey.value); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (auto status = bindInt64(m_connection, projection.native(), 3, toMilliseconds(enqueuedAt),
                    "Failed to bind optimistic projection timestamp"); !status) {
                return std::unexpected(std::move(status.error()));
            }
            if (auto status = sqlite::stepDone(m_connection, projection); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
        }

        auto record = loadRecord(m_connection, operation.idempotencyKey.value);
        if (!record) return std::unexpected(std::move(record.error()));
        if (record->operation.kind != operation.kind ||
            record->operation.author.value != operation.author.value ||
            record->operation.signerPublicKey.hex != operation.signerPublicKey.hex ||
            record->operation.messageBytes != operation.messageBytes ||
            record->operation.dependsOn.has_value() != operation.dependsOn.has_value() ||
            (record->operation.dependsOn && operation.dependsOn &&
             record->operation.dependsOn->value != operation.dependsOn->value)) {
            return std::unexpected(conflict(
                "Idempotency key is already bound to a different protocol operation."));
        }
        if (auto status = transaction.commit(); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        return record;
    }

    [[nodiscard]] foundation::result::Result<std::optional<domain::write::OutboxRecord>> find(
        const domain::write::IdempotencyKey& key) override
    {
        std::scoped_lock lock{m_mutex};
        auto record = loadRecord(m_connection, key.value);
        if (!record && record.error().code == foundation::error::ErrorCode::NotFound) {
            return std::optional<domain::write::OutboxRecord>{};
        }
        if (!record) return std::unexpected(std::move(record.error()));
        return std::optional<domain::write::OutboxRecord>{std::move(*record)};
    }

    [[nodiscard]] foundation::result::Result<std::vector<local::write::ReactionProjection>>
    listReactionProjections(const domain::identity::Fid author) override
    {
        std::scoped_lock lock{m_mutex};
        if (author.value == 0U ||
            author.value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Reaction projection lookup requires a valid account FID."));
        }

        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
SELECT cast_id, target_cast_fid, reaction_kind, is_active, updated_at_ms
FROM cast_reaction_local_state
WHERE author_fid = ?
ORDER BY updated_at_ms ASC, cast_id ASC, reaction_kind ASC;
)SQL");
        if (!prepared) {
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        }
        auto statement = std::move(*prepared);
        if (auto status = bindInt64(m_connection, statement.native(), 1,
                static_cast<std::int64_t>(author.value),
                "Failed to bind local reaction account FID"); !status) {
            return std::unexpected(std::move(status.error()));
        }

        std::vector<local::write::ReactionProjection> projections{};
        while (true) {
            const int step = sqlite3_step(statement.native());
            if (step == SQLITE_DONE) {
                break;
            }
            if (step != SQLITE_ROW) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_connection.native(), "Failed to list local reaction projections")));
            }
            const auto targetFidRaw = sqlite3_column_int64(statement.native(), 1);
            if (targetFidRaw <= 0) {
                return std::unexpected(storageFailure(
                    "Local reaction projection contains an invalid target FID."));
            }
            const std::string reactionKind = sqlite::columnText(statement.native(), 2);
            domain::write::ReactionKind reaction{};
            if (reactionKind == "like") {
                reaction = domain::write::ReactionKind::Like;
            } else if (reactionKind == "recast") {
                reaction = domain::write::ReactionKind::Recast;
            } else {
                return std::unexpected(storageFailure(
                    "Local reaction projection contains an unknown reaction kind."));
            }
            projections.push_back(local::write::ReactionProjection{
                .target = domain::cast::CastReference{
                    .author = domain::identity::Fid{static_cast<std::uint64_t>(targetFidRaw)},
                    .id = domain::cast::CastId{sqlite::columnText(statement.native(), 0)},
                },
                .reaction = reaction,
                .isActive = sqlite3_column_int(statement.native(), 3) != 0,
                .updatedAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 4)),
            });
        }
        return projections;
    }

    [[nodiscard]] foundation::result::Result<std::vector<local::write::FollowingProjection>>
    listFollowingProjections(const domain::identity::Fid author) override
    {
        std::scoped_lock lock{m_mutex};
        if (author.value == 0U ||
            author.value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Following projection lookup requires a valid account FID."));
        }

        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
SELECT target_fid, is_following, updated_at_ms
FROM following_local_state
WHERE author_fid = ?
ORDER BY updated_at_ms ASC, target_fid ASC;
)SQL");
        if (!prepared) {
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        }
        auto statement = std::move(*prepared);
        if (auto status = bindInt64(m_connection, statement.native(), 1,
                static_cast<std::int64_t>(author.value),
                "Failed to bind local following account FID"); !status) {
            return std::unexpected(std::move(status.error()));
        }

        std::vector<local::write::FollowingProjection> projections{};
        while (true) {
            const int step = sqlite3_step(statement.native());
            if (step == SQLITE_DONE) {
                break;
            }
            if (step != SQLITE_ROW) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_connection.native(), "Failed to list local following projections")));
            }
            const auto targetFidRaw = sqlite3_column_int64(statement.native(), 0);
            if (targetFidRaw <= 0) {
                return std::unexpected(storageFailure(
                    "Local following projection contains an invalid target FID."));
            }
            projections.push_back(local::write::FollowingProjection{
                .target = domain::identity::Fid{static_cast<std::uint64_t>(targetFidRaw)},
                .isFollowing = sqlite3_column_int(statement.native(), 1) != 0,
                .updatedAt = fromMilliseconds(sqlite3_column_int64(statement.native(), 2)),
            });
        }
        return projections;
    }

    [[nodiscard]] foundation::result::Result<std::vector<domain::write::OutboxRecord>>
    listCastProjections(const domain::identity::Fid author, const std::size_t limit) override
    {
        std::scoped_lock lock{m_mutex};
        if (author.value == 0U || limit == 0U ||
            limit > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Local cast projection requires a valid FID and positive limit."));
        }
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
SELECT idempotency_key FROM write_outbox
WHERE author_fid = ? AND kind = 'publish-cast'
  AND status IN ('pending', 'failed', 'confirmed')
ORDER BY enqueued_at_ms DESC, idempotency_key DESC LIMIT ?;
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = bindInt64(m_connection, statement.native(), 1,
                static_cast<std::int64_t>(author.value), "Local cast projection FID"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        if (sqlite3_bind_int(statement.native(), 2, static_cast<int>(limit)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Local cast projection limit")));
        }
        std::vector<std::string> keys{};
        while (true) {
            const auto step = sqlite3_step(statement.native());
            if (step == SQLITE_DONE) break;
            if (step != SQLITE_ROW) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_connection.native(), "Local cast projections")));
            }
            keys.push_back(sqlite::columnText(statement.native(), 0));
        }
        std::vector<domain::write::OutboxRecord> records{};
        records.reserve(keys.size());
        for (const auto& key : keys) {
            auto record = loadRecord(m_connection, key);
            if (!record) return std::unexpected(std::move(record.error()));
            records.push_back(std::move(*record));
        }
        return records;
    }

    [[nodiscard]] foundation::result::Result<std::vector<domain::write::OutboxRecord>> listReady(
        const domain::identity::Fid author,
        const foundation::time::Timestamp now,
        const std::size_t limit) override
    {
        std::scoped_lock lock{m_mutex};
        if (author.value == 0U) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Outbox ready-list requires a non-zero author FID."));
        }
        if (limit == 0U || limit > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Outbox ready-list limit must be between 1 and INT_MAX."));
        }
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
SELECT idempotency_key
FROM write_outbox
WHERE author_fid = ?
  AND status = 'pending'
  AND (next_attempt_at_ms IS NULL OR next_attempt_at_ms <= ?)
ORDER BY enqueued_at_ms ASC, idempotency_key ASC
LIMIT ?;
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = bindInt64(m_connection, statement.native(), 1,
                static_cast<std::int64_t>(author.value),
                "Failed to bind outbox author FID"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        if (auto status = bindInt64(m_connection, statement.native(), 2, toMilliseconds(now),
                "Failed to bind outbox ready timestamp"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        if (sqlite3_bind_int(statement.native(), 3, static_cast<int>(limit)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_connection.native(), "Failed to bind outbox ready limit")));
        }
        std::vector<std::string> keys{};
        while (true) {
            const int step = sqlite3_step(statement.native());
            if (step == SQLITE_DONE) break;
            if (step != SQLITE_ROW) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_connection.native(), "Failed to list ready outbox records")));
            }
            keys.push_back(sqlite::columnText(statement.native(), 0));
        }
        std::vector<domain::write::OutboxRecord> records{};
        records.reserve(keys.size());
        for (const auto& key : keys) {
            auto record = loadRecord(m_connection, key);
            if (!record) return std::unexpected(std::move(record.error()));
            records.push_back(std::move(*record));
        }
        return records;
    }

    [[nodiscard]] foundation::result::Result<domain::write::OutboxRecord> beginAttempt(
        const domain::write::IdempotencyKey& key,
        const foundation::time::Timestamp attemptedAt) override
    {
        std::scoped_lock lock{m_mutex};
        auto existing = loadRecord(m_connection, key.value);
        if (!existing) return std::unexpected(std::move(existing.error()));
        if (existing->status != domain::write::OutboxStatus::Pending) {
            return std::unexpected(conflict("Only pending outbox records may begin another attempt."));
        }
        if (existing->attemptCount == std::numeric_limits<std::uint32_t>::max()) {
            return std::unexpected(conflict("Outbox attempt count reached the uint32 limit."));
        }
        if (existing->nextAttemptAt && *existing->nextAttemptAt > attemptedAt) {
            return std::unexpected(conflict("Outbox record is not due for another attempt yet."));
        }
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
UPDATE write_outbox
SET attempt_count = attempt_count + 1,
    next_attempt_at_ms = NULL
WHERE idempotency_key = ? AND status = 'pending';
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = sqlite::bindText(statement.native(), 1, key.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::stepDone(m_connection, statement); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_changes(m_connection.native()) == 0) {
            return std::unexpected(conflict("Pending outbox record changed before attempt could begin."));
        }
        return loadRecord(m_connection, key.value);
    }

    [[nodiscard]] foundation::result::Status markRetry(
        const domain::write::IdempotencyKey& key,
        domain::write::OutboxFailure failure,
        const foundation::time::Timestamp nextAttemptAt) override
    {
        std::scoped_lock lock{m_mutex};
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
UPDATE write_outbox
SET status = 'pending', next_attempt_at_ms = ?,
    last_failure_kind = ?, last_failure_message = ?, confirmed_at_ms = NULL
WHERE idempotency_key = ? AND status = 'pending';
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = bindInt64(m_connection, statement.native(), 1, toMilliseconds(nextAttemptAt),
                "Failed to bind outbox retry timestamp"); !status) return status;
        if (auto status = sqlite::bindText(statement.native(), 2, failureKindText(failure.kind)); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindText(statement.native(), 3, failure.message); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindText(statement.native(), 4, key.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::stepDone(m_connection, statement); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_changes(m_connection.native()) == 0) {
            return std::unexpected(notFound("Pending outbox record disappeared before retry scheduling."));
        }
        return {};
    }

    [[nodiscard]] foundation::result::Status markFailed(
        const domain::write::IdempotencyKey& key,
        domain::write::OutboxFailure failure,
        const foundation::time::Timestamp failedAt) override
    {
        std::scoped_lock lock{m_mutex};
        sqlite::Transaction transaction{m_connection};
        if (auto status = transaction.begin(); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
UPDATE write_outbox
SET status = 'failed', next_attempt_at_ms = NULL,
    last_failure_kind = ?, last_failure_message = ?, confirmed_at_ms = NULL
WHERE idempotency_key = ? AND status = 'pending';
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = sqlite::bindText(statement.native(), 1, failureKindText(failure.kind)); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindText(statement.native(), 2, failure.message); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::bindText(statement.native(), 3, key.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::stepDone(m_connection, statement); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_changes(m_connection.native()) == 0) {
            return std::unexpected(notFound("Pending outbox record disappeared before terminal failure."));
        }
        if (auto status = updateProjectionState(m_connection, key, "failed", failedAt); !status) {
            return status;
        }
        if (auto status = transaction.commit(); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        return {};
    }

    [[nodiscard]] foundation::result::Status markConfirmed(
        const domain::write::IdempotencyKey& key,
        const foundation::time::Timestamp confirmedAt) override
    {
        std::scoped_lock lock{m_mutex};
        sqlite::Transaction transaction{m_connection};
        if (auto status = transaction.begin(); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        auto prepared = sqlite::Statement::prepare(m_connection, R"SQL(
UPDATE write_outbox
SET status = 'confirmed', next_attempt_at_ms = NULL,
    last_failure_kind = NULL, last_failure_message = NULL,
    confirmed_at_ms = ?
WHERE idempotency_key = ? AND status IN ('pending', 'confirmed');
)SQL");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = bindInt64(m_connection, statement.native(), 1, toMilliseconds(confirmedAt),
                "Failed to bind outbox confirmation timestamp"); !status) return status;
        if (auto status = sqlite::bindText(statement.native(), 2, key.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::stepDone(m_connection, statement); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_changes(m_connection.native()) == 0) {
            return std::unexpected(notFound("Outbox record disappeared before confirmation."));
        }
        if (auto status = updateProjectionState(m_connection, key, "confirmed", confirmedAt); !status) {
            return status;
        }
        if (auto status = transaction.commit(); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        return {};
    }

private:
    sqlite::Connection m_connection;
    std::mutex m_mutex{};
};

class SqliteWriteRouteBindings final : public local::write::IWriteRouteBindings {
public:
    explicit SqliteWriteRouteBindings(sqlite::Connection connection)
        : m_connection(std::move(connection)) {}

    [[nodiscard]] foundation::result::Result<std::optional<local::write::WriteRouteBinding>> find(
        const domain::write::IdempotencyKey& key) override
    {
        std::scoped_lock lock{m_mutex};
        return findLocked(key.value);
    }

    [[nodiscard]] foundation::result::Result<local::write::WriteRouteBinding> bindOnce(
        const domain::write::IdempotencyKey& key,
        local::write::WriteRouteBinding binding) override
    {
        if (key.value.empty() || binding.providerId.empty() || binding.endpointIdentity.empty()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Canonical message hash and selected provider ID must not be empty."));
        }
        std::scoped_lock lock{m_mutex};
        sqlite::Transaction transaction{m_connection};
        if (auto status = transaction.begin(); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        auto prepared = sqlite::Statement::prepare(m_connection,
            "INSERT OR IGNORE INTO write_route_bindings(idempotency_key, provider_id, endpoint_identity) VALUES(?, ?, ?);");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = sqlite::bindText(statement.native(), 1, key.value); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        if (auto status = sqlite::bindText(statement.native(), 2, binding.providerId); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        if (auto status = sqlite::bindText(statement.native(), 3, binding.endpointIdentity); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        if (auto status = sqlite::stepDone(m_connection, statement); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        auto persisted = findLocked(key.value);
        if (!persisted) return std::unexpected(std::move(persisted.error()));
        if (!*persisted) {
            return std::unexpected(storageFailure("Write route binding was not persisted."));
        }
        if ((**persisted).providerId != binding.providerId
            || (**persisted).endpointIdentity != binding.endpointIdentity) {
            return std::unexpected(conflict(
                "Canonical write already belongs to a different provider or endpoint; rerouting is forbidden."));
        }
        if (auto status = transaction.commit(); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        return **persisted;
    }

private:
    [[nodiscard]] foundation::result::Result<std::optional<local::write::WriteRouteBinding>> findLocked(
        const std::string_view key)
    {
        auto prepared = sqlite::Statement::prepare(m_connection,
            "SELECT provider_id, endpoint_identity FROM write_route_bindings WHERE idempotency_key = ?;");
        if (!prepared) return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (auto status = sqlite::bindText(statement.native(), 1, key); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        const int result = sqlite3_step(statement.native());
        if (result == SQLITE_DONE) return std::optional<local::write::WriteRouteBinding>{};
        if (result != SQLITE_ROW) return std::unexpected(toFoundationError(sqlite::makeError(
            m_connection.native(), "Failed to find durable write route")));
        return std::optional<local::write::WriteRouteBinding>{local::write::WriteRouteBinding{
            .providerId = sqlite::columnText(statement.native(), 0),
            .endpointIdentity = sqlite::columnText(statement.native(), 1),
        }};
    }

    sqlite::Connection m_connection;
    std::mutex m_mutex{};
};

} // namespace

foundation::result::Result<std::unique_ptr<local::write::IOutboxStore>> openOutboxStore(
    const std::filesystem::path& path)
{
    auto opened = SqliteOutboxStore::open(path);
    if (!opened) return std::unexpected(std::move(opened.error()));
    std::unique_ptr<local::write::IOutboxStore> result = std::move(*opened);
    return result;
}

foundation::result::Result<std::unique_ptr<local::write::IWriteRouteBindings>>
openWriteRouteBindings(const std::filesystem::path& path)
{
    auto connection = sqlite::Connection::open(path);
    if (!connection) return std::unexpected(toFoundationError(std::move(connection.error())));
    // A dedicated infrastructure table: provider identities stay out of the
    // protocol/domain outbox schema and canonical message serialization.
    auto created = connection->execute(R"SQL(
CREATE TABLE IF NOT EXISTS write_route_bindings (
    idempotency_key TEXT PRIMARY KEY NOT NULL,
    provider_id TEXT NOT NULL CHECK(length(provider_id) > 0),
    endpoint_identity TEXT NOT NULL CHECK(length(endpoint_identity) > 0),
    FOREIGN KEY (idempotency_key) REFERENCES write_outbox(idempotency_key) ON DELETE CASCADE
);
)SQL");
    if (!created) return std::unexpected(toFoundationError(std::move(created.error())));
    return std::unique_ptr<local::write::IWriteRouteBindings>{
        std::make_unique<SqliteWriteRouteBindings>(std::move(*connection))};
}

} // namespace farcaster::storage::write
