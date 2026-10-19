module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.storage.read;

import farcaster.capability.feed;
import farcaster.capability.pagination;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.social;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.local.entity;
import farcaster.local.read;
import farcaster.local.timeline;
import farcaster.storage.entity;
import farcaster.storage.timeline;

namespace farcaster::storage::read {
namespace {

[[nodiscard]] foundation::error::Error toFoundationError(sqlite::Error error)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::StorageFailure, std::move(error.message));
}

[[nodiscard]] std::string feedKey(const local::read::FeedKey& key)
{
    if (key.kind == local::read::FeedKey::Kind::Following) {
        return "feed:following:" + std::to_string(key.fid ? key.fid->value : 0);
    }
    if (key.kind == local::read::FeedKey::Kind::Channel) {
        return "feed:channel:" + (key.channel ? key.channel->value : std::string{});
    }
    if (key.kind == local::read::FeedKey::Kind::UserPosts) {
        return "feed:user:posts:" + std::to_string(key.fid ? key.fid->value : 0);
    }
    if (key.kind == local::read::FeedKey::Kind::UserQuotes) {
        return "feed:user:quotes:" + std::to_string(key.fid ? key.fid->value : 0);
    }
    if (key.kind == local::read::FeedKey::Kind::UserReplies) {
        return "feed:user:replies:" + std::to_string(key.fid ? key.fid->value : 0);
    }
    if (key.kind == local::read::FeedKey::Kind::UserLikes) {
        return "feed:user:likes:" + std::to_string(key.fid ? key.fid->value : 0);
    }
    if (key.kind == local::read::FeedKey::Kind::UserPopular) {
        return "feed:user:popular:" + std::to_string(key.fid ? key.fid->value : 0);
    }
    if (key.kind == local::read::FeedKey::Kind::Recent) { return "feed:recent"; }
    if (key.kind == local::read::FeedKey::Kind::Nfts) { return "feed:nfts"; }
    if (key.kind == local::read::FeedKey::Kind::Developers) { return "feed:developers"; }
    return "feed:trending";
}

[[nodiscard]] std::string socialKey(const local::read::SocialKey& key)
{
    return std::string{key.kind == local::read::SocialKey::Kind::Followers
        ? "social:followers:" : "social:following:"} + std::to_string(key.fid.value);
}

[[nodiscard]] std::int64_t nowMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

[[nodiscard]] domain::profile::Profile mergeEmbeddedProfileSummary(
    const std::optional<domain::profile::Profile>& existing,
    domain::profile::Profile incoming)
{
    if (!existing) {
        // Embedded feed/social author objects are not authoritative sources for
        // follower/following totals. Persist identity/presentation fields so
        // list rows remain useful offline, but leave social counts unknown until
        // the dedicated profile capability refreshes them.
        incoming.followerCount = 0;
        incoming.followingCount = 0;
        return incoming;
    }

    // Feed/social author objects are summary snapshots. They must not overwrite
    // authoritative profile details or social counts previously obtained from
    // the dedicated profile capability. This prevents a feed refresh from
    // regressing a profile card to stale/partial metadata.
    incoming.followerCount = existing->followerCount;
    incoming.followingCount = existing->followingCount;
    if (!existing->bio.empty()) {
        incoming.bio = existing->bio;
    }
    if (existing->avatarUrl) {
        incoming.avatarUrl = existing->avatarUrl;
    }
    if (existing->bannerUrl) {
        incoming.bannerUrl = existing->bannerUrl;
    }
    if (existing->location) {
        incoming.location = existing->location;
    }
    if (existing->profileUrl) {
        incoming.profileUrl = existing->profileUrl;
    }
    if (existing->custodyAddress) {
        incoming.custodyAddress = existing->custodyAddress;
    }
    if (!existing->verifiedEthAddresses.empty()) {
        incoming.verifiedEthAddresses = existing->verifiedEthAddresses;
    }
    if (!existing->verifiedSolAddresses.empty()) {
        incoming.verifiedSolAddresses = existing->verifiedSolAddresses;
    }
    return incoming;
}

[[nodiscard]] foundation::result::Status putEmbeddedProfileSummary(
    local::entity::IEntityStore& store,
    const domain::profile::Profile& profile)
{
    auto existing = store.findProfile(profile.fid);
    if (!existing) {
        return std::unexpected(std::move(existing.error()));
    }
    return store.putProfile(mergeEmbeddedProfileSummary(*existing, profile));
}

class SqliteReadStore final : public local::read::IReadStore {
public:
    [[nodiscard]] static foundation::result::Result<std::unique_ptr<SqliteReadStore>>
    open(const std::filesystem::path& path);

    SqliteReadStore(const SqliteReadStore&) = delete;
    auto operator=(const SqliteReadStore&) -> SqliteReadStore& = delete;
    ~SqliteReadStore() override = default;

    [[nodiscard]] foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfileByUsername(std::string_view username) override;
    [[nodiscard]] foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfile(domain::identity::Fid fid) override;
    [[nodiscard]] foundation::result::Result<std::optional<domain::cast::Cast>>
    findCast(const domain::cast::CastId& castId) override;
    [[nodiscard]] foundation::result::Status
    putProfile(const domain::profile::Profile& profile) override;
    [[nodiscard]] foundation::result::Status
    putCast(const domain::cast::Cast& cast) override;
    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage>
    readFeed(const local::read::FeedKey& key) override;
    [[nodiscard]] foundation::result::Status writeFeed(
        const local::read::FeedKey& key,
        const capability::feed::FeedPage& page,
        bool append) override;
    [[nodiscard]] foundation::result::Result<capability::social::SocialGraphPage>
    readSocial(const local::read::SocialKey& key) override;
    [[nodiscard]] foundation::result::Status writeSocial(
        const local::read::SocialKey& key,
        const capability::social::SocialGraphPage& page,
        bool append) override;

private:
    SqliteReadStore(
        std::unique_ptr<local::entity::IEntityStore> entities,
        std::unique_ptr<local::timeline::ITimelineStore> timelines,
        sqlite::Connection metadata) noexcept;

    [[nodiscard]] foundation::result::Result<std::optional<capability::pagination::ContinuationToken>>
    readContinuation(std::string_view key);
    [[nodiscard]] foundation::result::Status writeContinuationLocked(
        std::string_view key,
        const std::optional<capability::pagination::ContinuationToken>& token);

    std::unique_ptr<local::entity::IEntityStore> m_entities;
    std::unique_ptr<local::timeline::ITimelineStore> m_timelines;
    sqlite::Connection m_metadata;
    std::mutex m_mutex{};
};

SqliteReadStore::SqliteReadStore(
    std::unique_ptr<local::entity::IEntityStore> entities,
    std::unique_ptr<local::timeline::ITimelineStore> timelines,
    sqlite::Connection metadata) noexcept
    : m_entities(std::move(entities)),
      m_timelines(std::move(timelines)),
      m_metadata(std::move(metadata))
{
}

foundation::result::Result<std::unique_ptr<SqliteReadStore>> SqliteReadStore::open(
    const std::filesystem::path& path)
{
    auto entities = entity::openEntityStore(path);
    if (!entities) {
        return std::unexpected(std::move(entities.error()));
    }
    auto timelines = timeline::openTimelineStore(path);
    if (!timelines) {
        return std::unexpected(std::move(timelines.error()));
    }
    auto metadata = sqlite::Connection::open(path);
    if (!metadata) {
        return std::unexpected(toFoundationError(std::move(metadata.error())));
    }
    return std::unique_ptr<SqliteReadStore>{new SqliteReadStore{
        std::move(*entities), std::move(*timelines), std::move(*metadata)}};
}

foundation::result::Result<std::optional<domain::profile::Profile>>
SqliteReadStore::findProfileByUsername(const std::string_view username)
{
    return m_entities->findProfileByUsername(username);
}

foundation::result::Result<std::optional<domain::profile::Profile>>
SqliteReadStore::findProfile(const domain::identity::Fid fid)
{
    return m_entities->findProfile(fid);
}

foundation::result::Result<std::optional<domain::cast::Cast>>
SqliteReadStore::findCast(const domain::cast::CastId& castId)
{
    return m_entities->findCast(castId);
}

foundation::result::Status SqliteReadStore::putProfile(const domain::profile::Profile& profile)
{
    return m_entities->putProfile(profile);
}

foundation::result::Status SqliteReadStore::putCast(const domain::cast::Cast& cast)
{
    return m_entities->putCast(cast);
}

foundation::result::Result<std::optional<capability::pagination::ContinuationToken>>
SqliteReadStore::readContinuation(const std::string_view key)
{
    std::scoped_lock lock{m_mutex};
    auto prepared = sqlite::Statement::prepare(
        m_metadata, "SELECT token FROM read_continuations WHERE read_key = ?;");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, key); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    const int step = sqlite3_step(statement.native());
    if (step == SQLITE_DONE) {
        return std::optional<capability::pagination::ContinuationToken>{};
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_metadata.native(), "Failed to read local continuation")));
    }
    if (sqlite3_column_type(statement.native(), 0) == SQLITE_NULL) {
        return std::optional<capability::pagination::ContinuationToken>{};
    }
    return std::optional<capability::pagination::ContinuationToken>{
        capability::pagination::ContinuationToken{sqlite::columnText(statement.native(), 0)}};
}

foundation::result::Status SqliteReadStore::writeContinuationLocked(
    const std::string_view key,
    const std::optional<capability::pagination::ContinuationToken>& token)
{
    auto prepared = sqlite::Statement::prepare(m_metadata, R"SQL(
INSERT INTO read_continuations(read_key, token, updated_at_ms) VALUES(?, ?, ?)
ON CONFLICT(read_key) DO UPDATE SET token = excluded.token, updated_at_ms = excluded.updated_at_ms;
)SQL");
    if (!prepared) {
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    }
    auto statement = std::move(*prepared);
    if (auto status = sqlite::bindText(statement.native(), 1, key); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    const std::optional<std::string> tokenValue = token
        ? std::optional<std::string>{token->value} : std::nullopt;
    if (auto status = sqlite::bindOptionalText(statement.native(), 2, tokenValue); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_bind_int64(statement.native(), 3, nowMilliseconds()) != SQLITE_OK) {
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_metadata.native(), "Failed to bind local continuation time")));
    }
    if (auto status = sqlite::stepDone(m_metadata, statement); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

foundation::result::Result<capability::feed::FeedPage> SqliteReadStore::readFeed(
    const local::read::FeedKey& key)
{
    const std::string id = feedKey(key);
    // `key.limit` is the provider page size, not the size of the already
    // materialized local snapshot. Pagination appends each fetched page to this
    // timeline, so returning only the first network page makes load-more appear
    // to succeed while the UI never sees the accumulated rows.
    auto entries = m_timelines->readAllTimeline(local::timeline::TimelineId{id});
    if (!entries) {
        return std::unexpected(std::move(entries.error()));
    }
    capability::feed::FeedPage page{};
    page.entries = std::move(*entries);
    std::unordered_set<std::uint64_t> seenProfiles{};
    for (const auto& entry : page.entries) {
        auto cast = m_entities->findCast(entry.castId);
        if (!cast) {
            return std::unexpected(std::move(cast.error()));
        }
        if (!*cast) {
            continue;
        }
        if (seenProfiles.insert((*cast)->author.value).second) {
            auto profile = m_entities->findProfile((*cast)->author);
            if (!profile) {
                return std::unexpected(std::move(profile.error()));
            }
            if (*profile) {
                page.profiles.push_back(std::move(**profile));
            }
        }
        page.casts.push_back(std::move(**cast));
    }
    auto next = readContinuation(id);
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    page.next = std::move(*next);
    return page;
}

foundation::result::Status SqliteReadStore::writeFeed(
    const local::read::FeedKey& key,
    const capability::feed::FeedPage& page,
    const bool append)
{
    for (const auto& profile : page.profiles) {
        if (auto status = putEmbeddedProfileSummary(*m_entities, profile); !status) {
            return status;
        }
    }
    for (const auto& cast : page.casts) {
        if (auto status = m_entities->putCast(cast); !status) {
            return status;
        }
    }

    const std::string id = feedKey(key);
    std::scoped_lock lock{m_mutex};
    sqlite::Transaction transaction{m_metadata};
    if (auto status = transaction.begin(); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    sqlite3_int64 nextPosition = 0;
    if (append) {
        auto positionPrepared = sqlite::Statement::prepare(
            m_metadata,
            "SELECT COALESCE(MAX(position) + 1, 0) FROM timeline_entries WHERE timeline_id = ?;");
        if (!positionPrepared) {
            return std::unexpected(toFoundationError(std::move(positionPrepared.error())));
        }
        auto position = std::move(*positionPrepared);
        if (auto status = sqlite::bindText(position.native(), 1, id); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_step(position.native()) != SQLITE_ROW) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_metadata.native(), "Failed to read next feed position")));
        }
        nextPosition = sqlite3_column_int64(position.native(), 0);
    } else {
        auto clearPrepared = sqlite::Statement::prepare(
            m_metadata, "DELETE FROM timeline_entries WHERE timeline_id = ?;");
        if (!clearPrepared) {
            return std::unexpected(toFoundationError(std::move(clearPrepared.error())));
        }
        auto clear = std::move(*clearPrepared);
        if (auto status = sqlite::bindText(clear.native(), 1, id); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::stepDone(m_metadata, clear); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
    }

    auto insertPrepared = sqlite::Statement::prepare(m_metadata, R"SQL(
INSERT OR IGNORE INTO timeline_entries(timeline_id, position, cast_id, sort_time_ms)
VALUES(?, ?, ?, ?);
)SQL");
    if (!insertPrepared) {
        return std::unexpected(toFoundationError(std::move(insertPrepared.error())));
    }
    auto insert = std::move(*insertPrepared);
    for (const auto& entry : page.entries) {
        insert.reset();
        if (auto status = sqlite::bindText(insert.native(), 1, id); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(insert.native(), 2, nextPosition) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_metadata.native(), "Failed to bind feed entry position")));
        }
        if (auto status = sqlite::bindText(insert.native(), 3, entry.castId.value); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(
                insert.native(), 4,
                static_cast<sqlite3_int64>(entry.sortTime.time_since_epoch().count())) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_metadata.native(), "Failed to bind feed entry timestamp")));
        }
        if (auto status = sqlite::stepDone(m_metadata, insert); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_changes(m_metadata.native()) > 0) {
            ++nextPosition;
        }
    }

    if (auto status = writeContinuationLocked(id, page.next); !status) {
        return status;
    }
    if (auto status = transaction.commit(); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

foundation::result::Result<capability::social::SocialGraphPage>
SqliteReadStore::readSocial(const local::read::SocialKey& key)
{
    const std::string id = socialKey(key);
    capability::social::SocialGraphPage page{};
    {
        std::scoped_lock lock{m_mutex};
        auto prepared = sqlite::Statement::prepare(m_metadata, R"SQL(
SELECT source_fid, target_fid, relation_kind FROM social_list_entries
WHERE list_id = ? ORDER BY position ASC;
)SQL");
        if (!prepared) {
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        }
        auto statement = std::move(*prepared);
        if (auto status = sqlite::bindText(statement.native(), 1, id); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        // Social pagination follows the same materialized-snapshot contract as
        // feeds: the network page size must never truncate accumulated local rows.
        while (true) {
            const int step = sqlite3_step(statement.native());
            if (step == SQLITE_DONE) {
                break;
            }
            if (step != SQLITE_ROW) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_metadata.native(), "Failed to read social list")));
            }
            page.relations.push_back(domain::social::SocialRelation{
                .source = domain::identity::Fid{static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 0))},
                .target = domain::identity::Fid{static_cast<std::uint64_t>(sqlite3_column_int64(statement.native(), 1))},
                .kind = sqlite::columnText(statement.native(), 2) == "blocks"
                    ? domain::social::SocialRelationKind::Blocks
                    : domain::social::SocialRelationKind::Follows,
            });
        }
    }
    std::unordered_set<std::uint64_t> seen{};
    for (const auto& relation : page.relations) {
        const auto fid = key.kind == local::read::SocialKey::Kind::Followers
            ? relation.source : relation.target;
        if (!seen.insert(fid.value).second) {
            continue;
        }
        auto profile = m_entities->findProfile(fid);
        if (!profile) {
            return std::unexpected(std::move(profile.error()));
        }
        if (*profile) {
            page.profiles.push_back(std::move(**profile));
        }
    }
    auto next = readContinuation(id);
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    page.next = std::move(*next);
    return page;
}

foundation::result::Status SqliteReadStore::writeSocial(
    const local::read::SocialKey& key,
    const capability::social::SocialGraphPage& page,
    const bool append)
{
    for (const auto& profile : page.profiles) {
        if (auto status = putEmbeddedProfileSummary(*m_entities, profile); !status) {
            return status;
        }
    }

    const std::string id = socialKey(key);
    std::scoped_lock lock{m_mutex};
    sqlite::Transaction transaction{m_metadata};
    if (auto status = transaction.begin(); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }

    sqlite3_int64 nextPosition = 0;
    if (append) {
        auto positionPrepared = sqlite::Statement::prepare(
            m_metadata,
            "SELECT COALESCE(MAX(position) + 1, 0) FROM social_list_entries WHERE list_id = ?;");
        if (!positionPrepared) {
            return std::unexpected(toFoundationError(std::move(positionPrepared.error())));
        }
        auto position = std::move(*positionPrepared);
        if (auto status = sqlite::bindText(position.native(), 1, id); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_step(position.native()) != SQLITE_ROW) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_metadata.native(), "Failed to read next social-list position")));
        }
        nextPosition = sqlite3_column_int64(position.native(), 0);
    } else {
        auto clearPrepared = sqlite::Statement::prepare(
            m_metadata, "DELETE FROM social_list_entries WHERE list_id = ?;");
        if (!clearPrepared) {
            return std::unexpected(toFoundationError(std::move(clearPrepared.error())));
        }
        auto clear = std::move(*clearPrepared);
        if (auto status = sqlite::bindText(clear.native(), 1, id); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::stepDone(m_metadata, clear); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
    }

    auto insertPrepared = sqlite::Statement::prepare(m_metadata, R"SQL(
INSERT OR IGNORE INTO social_list_entries(list_id, position, source_fid, target_fid, relation_kind)
VALUES(?, ?, ?, ?, ?);
)SQL");
    if (!insertPrepared) {
        return std::unexpected(toFoundationError(std::move(insertPrepared.error())));
    }
    auto insert = std::move(*insertPrepared);
    for (const auto& relation : page.relations) {
        insert.reset();
        if (auto status = sqlite::bindText(insert.native(), 1, id); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_bind_int64(insert.native(), 2, nextPosition) != SQLITE_OK ||
            sqlite3_bind_int64(insert.native(), 3, static_cast<sqlite3_int64>(relation.source.value)) != SQLITE_OK ||
            sqlite3_bind_int64(insert.native(), 4, static_cast<sqlite3_int64>(relation.target.value)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_metadata.native(), "Failed to bind social relation")));
        }
        if (auto status = sqlite::bindText(
                insert.native(), 5,
                relation.kind == domain::social::SocialRelationKind::Blocks
                    ? std::string_view{"blocks"} : std::string_view{"follows"}); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (auto status = sqlite::stepDone(m_metadata, insert); !status) {
            return std::unexpected(toFoundationError(std::move(status.error())));
        }
        if (sqlite3_changes(m_metadata.native()) > 0) {
            ++nextPosition;
        }
    }

    if (auto status = writeContinuationLocked(id, page.next); !status) {
        return status;
    }
    if (auto status = transaction.commit(); !status) {
        return std::unexpected(toFoundationError(std::move(status.error())));
    }
    return {};
}

} // namespace

foundation::result::Result<std::unique_ptr<local::read::IReadStore>>
openReadStore(const std::filesystem::path& path)
{
    auto opened = SqliteReadStore::open(path);
    if (!opened) {
        return std::unexpected(std::move(opened.error()));
    }
    std::unique_ptr<local::read::IReadStore> result = std::move(*opened);
    return result;
}

} // namespace farcaster::storage::read
