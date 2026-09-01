module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

module farcaster.storage.entity;

import farcaster.local.entity;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;

namespace farcaster::storage::entity {


class SqliteEntityStore final : public local::entity::IEntityStore {
public:
    [[nodiscard]] static foundation::result::Result<SqliteEntityStore> open(const std::filesystem::path& path);
    ~SqliteEntityStore() override;
    SqliteEntityStore(const SqliteEntityStore&) = delete;
    auto operator=(const SqliteEntityStore&) -> SqliteEntityStore& = delete;
    SqliteEntityStore(SqliteEntityStore&& other) noexcept;
    auto operator=(SqliteEntityStore&& other) noexcept -> SqliteEntityStore&;
    [[nodiscard]] foundation::result::Status putProfile(const domain::profile::Profile& profile) override;
    [[nodiscard]] foundation::result::Result<std::optional<domain::profile::Profile>> findProfile(domain::identity::Fid fid) override;
    [[nodiscard]] foundation::result::Result<std::optional<domain::profile::Profile>> findProfileByUsername(std::string_view username) override;
    [[nodiscard]] foundation::result::Status putCast(const domain::cast::Cast& cast) override;
    [[nodiscard]] foundation::result::Result<std::optional<domain::cast::Cast>> findCast(const domain::cast::CastId& castId) override;
private:
    struct Impl;
    explicit SqliteEntityStore(std::unique_ptr<Impl> impl) noexcept;
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

struct SqliteEntityStore::Impl final {
    explicit Impl(sqlite::Connection openedConnection)
        : connection(std::move(openedConnection))
    {
    }

    sqlite::Connection connection;
    std::mutex mutex{};
};

foundation::result::Result<SqliteEntityStore> SqliteEntityStore::open(const std::filesystem::path& path)
{
    auto connection = sqlite::Connection::open(path);
    if (!connection) {
        return std::unexpected(toFoundationError(std::move(connection.error())));
    }
    return SqliteEntityStore{std::make_unique<Impl>(std::move(*connection))};
}

SqliteEntityStore::SqliteEntityStore(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{
}

SqliteEntityStore::~SqliteEntityStore() = default;
SqliteEntityStore::SqliteEntityStore(SqliteEntityStore&& other) noexcept = default;
auto SqliteEntityStore::operator=(SqliteEntityStore&& other) noexcept -> SqliteEntityStore& = default;

foundation::result::Status SqliteEntityStore::putProfile(const domain::profile::Profile& profile)
{
    std::scoped_lock lock{m_impl->mutex};
    auto prepared = sqlite::Statement::prepare(m_impl->connection, R"SQL(
INSERT INTO profiles(
    fid, username, display_name, bio, avatar_url, banner_url, location, profile_url, custody_address,
    follower_count, following_count, quality_score)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(fid) DO UPDATE SET
    username = excluded.username,
    display_name = excluded.display_name,
    bio = excluded.bio,
    avatar_url = excluded.avatar_url,
    banner_url = excluded.banner_url,
    location = excluded.location,
    profile_url = excluded.profile_url,
    custody_address = excluded.custody_address,
    follower_count = excluded.follower_count,
    following_count = excluded.following_count,
    quality_score = excluded.quality_score;
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);

    const auto username = profile.username
        ? std::optional<std::string>{profile.username->value}
        : std::nullopt;
    const auto displayName = profile.displayName
        ? std::optional<std::string>{profile.displayName->value}
        : std::nullopt;

    if (sqlite3_bind_int64(statement.native(), 1, static_cast<sqlite3_int64>(profile.fid.value)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind profile fid")));
    }
    if (auto status = sqlite::bindOptionalText(statement.native(), 2, username); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(statement.native(), 3, displayName); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindText(statement.native(), 4, profile.bio); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(statement.native(), 5, profile.avatarUrl); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(statement.native(), 6, profile.bannerUrl); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(statement.native(), 7, profile.location); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(statement.native(), 8, profile.profileUrl); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(statement.native(), 9, profile.custodyAddress); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_bind_int64(statement.native(), 10, static_cast<sqlite3_int64>(profile.followerCount)) != SQLITE_OK ||
        sqlite3_bind_int64(statement.native(), 11, static_cast<sqlite3_int64>(profile.followingCount)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind profile social counts")));
    }
    const int qualityStatus = profile.qualityScore
        ? sqlite3_bind_double(statement.native(), 12, *profile.qualityScore)
        : sqlite3_bind_null(statement.native(), 12);
    if (qualityStatus != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind profile quality score")));
    }
    if (auto status = sqlite::stepDone(m_impl->connection, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

foundation::result::Result<std::optional<domain::profile::Profile>>
SqliteEntityStore::findProfile(const domain::identity::Fid fid)
{
    std::scoped_lock lock{m_impl->mutex};
    auto prepared = sqlite::Statement::prepare(
        m_impl->connection,
        "SELECT username, display_name, bio, avatar_url, banner_url, location, profile_url, custody_address, "
        "follower_count, following_count, quality_score FROM profiles WHERE fid = ?;");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (sqlite3_bind_int64(statement.native(), 1, static_cast<sqlite3_int64>(fid.value)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind profile fid")));
    }

    const auto step = sqlite3_step(statement.native());
    if (step == SQLITE_DONE) {
        return std::optional<domain::profile::Profile>{};
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to load profile")));
    }

    domain::profile::Profile profile{};
    profile.fid = fid;
    if (auto value = sqlite::optionalColumnText(statement.native(), 0)) {
        profile.username = domain::identity::Username{std::move(*value)};
    }
    if (auto value = sqlite::optionalColumnText(statement.native(), 1)) {
        profile.displayName = domain::identity::DisplayName{std::move(*value)};
    }
    profile.bio = sqlite::columnText(statement.native(), 2);
    profile.avatarUrl = sqlite::optionalColumnText(statement.native(), 3);
    profile.bannerUrl = sqlite::optionalColumnText(statement.native(), 4);
    profile.location = sqlite::optionalColumnText(statement.native(), 5);
    profile.profileUrl = sqlite::optionalColumnText(statement.native(), 6);
    profile.custodyAddress = sqlite::optionalColumnText(statement.native(), 7);
    profile.followerCount = static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 8));
    profile.followingCount = static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 9));
    if (sqlite3_column_type(statement.native(), 10) != SQLITE_NULL) {
        profile.qualityScore = sqlite3_column_double(statement.native(), 10);
    }
    return std::optional<domain::profile::Profile>{std::move(profile)};
}

foundation::result::Result<std::optional<domain::profile::Profile>>
SqliteEntityStore::findProfileByUsername(const std::string_view username)
{
    std::scoped_lock lock{m_impl->mutex};
    auto prepared = sqlite::Statement::prepare(
        m_impl->connection,
        "SELECT fid, username, display_name, bio, avatar_url, banner_url, location, profile_url, custody_address, "
        "follower_count, following_count, quality_score "
        "FROM profiles WHERE username = ? COLLATE NOCASE LIMIT 1;");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, username); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    const auto step = sqlite3_step(statement.native());
    if (step == SQLITE_DONE) {
        return std::optional<domain::profile::Profile>{};
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to load profile by username")));
    }
    domain::profile::Profile profile{};
    profile.fid = domain::identity::Fid{
        static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 0))};
    if (auto value = sqlite::optionalColumnText(statement.native(), 1)) {
        profile.username = domain::identity::Username{std::move(*value)};
    }
    if (auto value = sqlite::optionalColumnText(statement.native(), 2)) {
        profile.displayName = domain::identity::DisplayName{std::move(*value)};
    }
    profile.bio = sqlite::columnText(statement.native(), 3);
    profile.avatarUrl = sqlite::optionalColumnText(statement.native(), 4);
    profile.bannerUrl = sqlite::optionalColumnText(statement.native(), 5);
    profile.location = sqlite::optionalColumnText(statement.native(), 6);
    profile.profileUrl = sqlite::optionalColumnText(statement.native(), 7);
    profile.custodyAddress = sqlite::optionalColumnText(statement.native(), 8);
    profile.followerCount = static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 9));
    profile.followingCount = static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 10));
    if (sqlite3_column_type(statement.native(), 11) != SQLITE_NULL) {
        profile.qualityScore = sqlite3_column_double(statement.native(), 11);
    }
    return std::optional<domain::profile::Profile>{std::move(profile)};
}

foundation::result::Status SqliteEntityStore::putCast(const domain::cast::Cast& cast)
{
    std::scoped_lock lock{m_impl->mutex};
    sqlite::Transaction transaction{m_impl->connection};
    if (auto status = transaction.begin(); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    auto preparedCast = sqlite::Statement::prepare(m_impl->connection, R"SQL(
INSERT INTO casts(id, author_fid, text, parent_id, parent_url, created_at_ms, reply_count, recast_count, like_count)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    author_fid = excluded.author_fid,
    text = excluded.text,
    parent_id = excluded.parent_id,
    parent_url = excluded.parent_url,
    created_at_ms = excluded.created_at_ms,
    reply_count = excluded.reply_count,
    recast_count = excluded.recast_count,
    like_count = excluded.like_count;
)SQL");
    if (!preparedCast) {
        return std::unexpected(toFoundationError(std::move(preparedCast.error())));
    }
    auto castStatement = std::move(*preparedCast);
    const std::optional<std::string> parentId = cast.parent
        ? std::optional<std::string>{cast.parent->value}
        : std::nullopt;

    if (auto status = sqlite::bindText(castStatement.native(), 1, cast.id.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_bind_int64(castStatement.native(), 2, static_cast<sqlite3_int64>(cast.author.value)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind cast author")));
    }
    if (auto status = sqlite::bindText(castStatement.native(), 3, cast.text); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(castStatement.native(), 4, parentId); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::bindOptionalText(castStatement.native(), 5, cast.parentUrl); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_bind_int64(castStatement.native(), 6, toMilliseconds(cast.createdAt)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind cast timestamp")));
    }
    if (sqlite3_bind_int64(castStatement.native(), 7, static_cast<sqlite3_int64>(cast.engagement.replies)) != SQLITE_OK ||
        sqlite3_bind_int64(castStatement.native(), 8, static_cast<sqlite3_int64>(cast.engagement.recasts)) != SQLITE_OK ||
        sqlite3_bind_int64(castStatement.native(), 9, static_cast<sqlite3_int64>(cast.engagement.likes)) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to bind cast engagement")));
    }
    if (auto status = sqlite::stepDone(m_impl->connection, castStatement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    auto preparedDelete = sqlite::Statement::prepare(
        m_impl->connection,
        "DELETE FROM cast_embed_items WHERE cast_id = ?;");
    if (!preparedDelete) {
        return std::unexpected(toFoundationError(std::move(preparedDelete.error())));
    }
    auto deleteEmbeds = std::move(*preparedDelete);
    if (auto status = sqlite::bindText(deleteEmbeds.native(), 1, cast.id.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = sqlite::stepDone(m_impl->connection, deleteEmbeds); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    auto preparedEmbed = sqlite::Statement::prepare(
        m_impl->connection,
        "INSERT INTO cast_embed_items(cast_id, position, kind, url, cast_ref_id, cast_ref_fid) "
        "VALUES(?, ?, ?, ?, ?, ?);");
    if (!preparedEmbed) {
        return std::unexpected(toFoundationError(std::move(preparedEmbed.error())));
    }
    auto insertEmbed = std::move(*preparedEmbed);
    for (std::size_t index = 0; index < cast.embeds.size(); ++index) {
        insertEmbed.reset();
        if (auto status = sqlite::bindText(insertEmbed.native(), 1, cast.id.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(insertEmbed.native(), 2, static_cast<sqlite3_int64>(index)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_impl->connection.native(), "Failed to bind cast embed position")));
        }
        const domain::cast::Embed& embed = cast.embeds[index];
        if (embed.kind == domain::cast::EmbedKind::Url) {
            if (auto status = sqlite::bindText(insertEmbed.native(), 3, "url"); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (auto status = sqlite::bindText(insertEmbed.native(), 4, embed.value); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (sqlite3_bind_null(insertEmbed.native(), 5) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_impl->connection.native(), "Failed to bind URL embed cast reference")));
            }
            if (sqlite3_bind_null(insertEmbed.native(), 6) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_impl->connection.native(), "Failed to bind URL embed cast-reference FID")));
            }
        } else {
            if (auto status = sqlite::bindText(insertEmbed.native(), 3, "cast"); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (sqlite3_bind_null(insertEmbed.native(), 4) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_impl->connection.native(), "Failed to bind cast-reference URL")));
            }
            if (auto status = sqlite::bindText(insertEmbed.native(), 5, embed.value); !status) {
                return std::unexpected(toFoundationError(std::move(status.error())));
            }
            if (embed.referencedAuthor) {
                if (sqlite3_bind_int64(
                        insertEmbed.native(),
                        6,
                        static_cast<sqlite3_int64>(embed.referencedAuthor->value)) != SQLITE_OK) {
                    return std::unexpected(toFoundationError(sqlite::makeError(
                        m_impl->connection.native(), "Failed to bind cast-reference FID")));
                }
            } else if (sqlite3_bind_null(insertEmbed.native(), 6) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_impl->connection.native(), "Failed to bind nullable cast-reference FID")));
            }
        }
        if (auto status = sqlite::stepDone(m_impl->connection, insertEmbed); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
    }

    if (auto status = transaction.commit(); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

foundation::result::Result<std::optional<domain::cast::Cast>>
SqliteEntityStore::findCast(const domain::cast::CastId& castId)
{
    std::scoped_lock lock{m_impl->mutex};
    auto prepared = sqlite::Statement::prepare(
        m_impl->connection,
        R"SQL(
SELECT c.author_fid, c.text, c.parent_id, c.parent_url, c.created_at_ms,
       c.reply_count, c.recast_count, c.like_count
FROM casts AS c
WHERE c.id = ?
  AND NOT EXISTS (
      SELECT 1 FROM cast_local_tombstones AS tombstone
      WHERE tombstone.cast_id = c.id
  );
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, castId.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    const auto step = sqlite3_step(statement.native());
    if (step == SQLITE_DONE) {
        return std::optional<domain::cast::Cast>{};
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_impl->connection.native(), "Failed to load cast")));
    }

    domain::cast::Cast cast{};
    cast.id = castId;
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

    auto preparedEmbeds = sqlite::Statement::prepare(
        m_impl->connection,
        "SELECT kind, url, cast_ref_id, cast_ref_fid FROM cast_embed_items "
        "WHERE cast_id = ? ORDER BY position ASC;");
    if (!preparedEmbeds) {
        return std::unexpected(toFoundationError(std::move(preparedEmbeds.error())));
    }
    auto embeds = std::move(*preparedEmbeds);
    if (auto status = sqlite::bindText(embeds.native(), 1, castId.value); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    while (true) {
        const auto embedStep = sqlite3_step(embeds.native());
        if (embedStep == SQLITE_DONE) {
            break;
        }
        if (embedStep != SQLITE_ROW) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_impl->connection.native(), "Failed to load cast embeds")));
        }
        const std::string kind = sqlite::columnText(embeds.native(), 0);
        if (kind == "url") {
            if (sqlite3_column_type(embeds.native(), 1) == SQLITE_NULL) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::StorageFailure,
                    "Stored URL embed is missing its URL."));
            }
            std::string url = sqlite::columnText(embeds.native(), 1);
            cast.embeds.push_back(domain::cast::Embed{
                .kind = domain::cast::EmbedKind::Url,
                .value = std::move(url),
            });
        } else if (kind == "cast") {
            if (sqlite3_column_type(embeds.native(), 2) == SQLITE_NULL) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::StorageFailure,
                    "Stored cast-reference embed is missing its cast id."));
            }
            std::string castReference = sqlite::columnText(embeds.native(), 2);
            std::optional<domain::identity::Fid> referencedAuthor{};
            if (sqlite3_column_type(embeds.native(), 3) != SQLITE_NULL) {
                referencedAuthor = domain::identity::Fid{
                    static_cast<std::uint64_t>(sqlite3_column_int64(embeds.native(), 3))};
            }
            cast.embeds.push_back(domain::cast::Embed{
                .kind = domain::cast::EmbedKind::CastReference,
                .value = std::move(castReference),
                .referencedAuthor = referencedAuthor,
            });
        } else {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::StorageFailure,
                "Stored cast embed has an unknown kind."));
        }
    }
    return std::optional<domain::cast::Cast>{std::move(cast)};
}


foundation::result::Result<std::unique_ptr<local::entity::IEntityStore>>
openEntityStore(const std::filesystem::path& path)
{
    auto opened = SqliteEntityStore::open(path);
    if (!opened) {
        return std::unexpected(opened.error());
    }
    std::unique_ptr<local::entity::IEntityStore> result =
        std::make_unique<SqliteEntityStore>(std::move(*opened));
    return result;
}

}
