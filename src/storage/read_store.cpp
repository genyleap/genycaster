module;

#include "sqlite_boundary.hpp"

#include <chrono>
#include <algorithm>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.storage.read;

import farcaster.capability.feed;
import farcaster.capability.search;
import farcaster.capability.pagination;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.social;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
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
    if (key.kind == local::read::FeedKey::Kind::Conversations) { return "feed:conversations"; }
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
        incoming.socialCountsKnown = false;
        return incoming;
    }

    // Feed/social author objects are summary snapshots. They must not overwrite
    // authoritative profile details or social counts previously obtained from
    // the dedicated profile capability. This prevents a feed refresh from
    // regressing a profile card to stale/partial metadata.
    incoming.followerCount = existing->followerCount;
    incoming.followingCount = existing->followingCount;
    incoming.socialCountsKnown = existing->socialCountsKnown;
    if (!incoming.username && existing->username)
        incoming.username = existing->username;
    if (!incoming.displayName && existing->displayName)
        incoming.displayName = existing->displayName;
    if (!incoming.qualityScore && existing->qualityScore)
        incoming.qualityScore = existing->qualityScore;
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
    [[nodiscard]] foundation::result::Result<std::vector<domain::profile::Profile>>
    searchCachedProfiles(std::string_view query, std::size_t limit) override;
    [[nodiscard]] foundation::result::Result<std::vector<capability::search::ChannelSuggestion>>
    searchCachedChannels(std::string_view query, std::size_t limit) override;
    [[nodiscard]] foundation::result::Status putChannelSuggestions(
        const std::vector<capability::search::ChannelSuggestion>& channels) override;
    [[nodiscard]] foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfile(domain::identity::Fid fid) override;
    [[nodiscard]] foundation::result::Result<std::optional<domain::cast::Cast>>
    findCast(const domain::cast::CastId& castId) override;
    [[nodiscard]] foundation::result::Status
    putProfileSummary(const domain::profile::Profile& profile) override;
    [[nodiscard]] foundation::result::Status
    putProfile(const domain::profile::Profile& profile) override;
    [[nodiscard]] foundation::result::Status
    putCast(const domain::cast::Cast& cast) override;
    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage>
    readFeed(const local::read::FeedKey& key) override;
    [[nodiscard]] foundation::result::Result<local::read::FeedWindow> readFeedWindow(
        const local::read::FeedKey& key,
        std::size_t offset, std::size_t limit) override;
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

    [[nodiscard]] foundation::result::Result<capability::feed::FeedPage>
    hydrateFeed(std::string_view id, std::vector<domain::feed::FeedEntry> entries);
    [[nodiscard]] foundation::result::Result<local::read::FeedWindow>
    readDerivedRecentWindow(std::size_t offset, std::size_t limit);
    [[nodiscard]] foundation::result::Result<local::read::FeedWindow>
    readDerivedFollowingWindow(domain::identity::Fid viewer,
                               std::size_t offset, std::size_t limit);
    [[nodiscard]] foundation::result::Result<bool>
    canDeriveFollowing(domain::identity::Fid viewer);
    [[nodiscard]] foundation::result::Result<bool> hasMaterializedRecentSnapshot();

    /** Best-effort local channel discovery from already persisted canonical casts. */
    void rememberChannelsFromCasts(std::span<const domain::cast::Cast> casts);
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

foundation::result::Result<std::vector<domain::profile::Profile>>
SqliteReadStore::searchCachedProfiles(std::string_view query, const std::size_t limit)
{
    // This is an exact/prefix index over *cached* identity metadata, not a
    // network-wide search service. Accept SQL wildcard characters literally.
    while (!query.empty() && (query.front() == ' ' || query.front() == '\t'
                               || query.front() == '\r' || query.front() == '\n'))
        query.remove_prefix(1);
    while (!query.empty() && (query.back() == ' ' || query.back() == '\t'
                               || query.back() == '\r' || query.back() == '\n'))
        query.remove_suffix(1);
    if (query.starts_with("@")) query.remove_prefix(1);
    if (query.empty() || query.size() > 128U || query.find('\0') != std::string_view::npos
        || limit == 0U || limit > 30U) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Local profile search requires 1..128 literal bytes and a limit of 1..30."));
    }

    std::string pattern{};
    pattern.reserve(query.size() * 2U + 1U);
    for (const char character : query) {
        if (character == '%' || character == '_' || character == '!')
            pattern.push_back('!');
        pattern.push_back(character);
    }
    pattern.push_back('%');

    std::vector<domain::identity::Fid> matches{};
    {
        std::scoped_lock lock{m_mutex};
        auto prepared = sqlite::Statement::prepare(m_metadata, R"SQL(
SELECT fid FROM profiles
WHERE username LIKE ? ESCAPE '!' COLLATE NOCASE
   OR display_name LIKE ? ESCAPE '!' COLLATE NOCASE
ORDER BY CASE
    WHEN username = ? COLLATE NOCASE THEN 0
    WHEN username LIKE ? ESCAPE '!' COLLATE NOCASE THEN 1
    ELSE 2
END, follower_count DESC, fid ASC
LIMIT ?;
)SQL");
        if (!prepared)
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        for (const auto& [index, value] :
             std::initializer_list<std::pair<int, std::string_view>>{
                 {1, pattern}, {2, pattern}, {3, query}, {4, pattern}}) {
            if (auto bound = sqlite::bindText(statement.native(), index, value); !bound)
                return std::unexpected(toFoundationError(std::move(bound.error())));
        }
        if (sqlite3_bind_int64(statement.native(), 5, static_cast<std::int64_t>(limit))
            != SQLITE_OK)
            return std::unexpected(toFoundationError(
                sqlite::makeError(m_metadata.native(), "Binding cached search limit")));
        while (true) {
            const int result = sqlite3_step(statement.native());
            if (result == SQLITE_DONE) break;
            if (result != SQLITE_ROW)
                return std::unexpected(toFoundationError(
                    sqlite::makeError(m_metadata.native(), "Reading cached profile index")));
            const auto fid = sqlite3_column_int64(statement.native(), 0);
            if (fid > 0)
                matches.push_back(domain::identity::Fid{
                    static_cast<std::uint64_t>(fid)});
        }
    }
    std::vector<domain::profile::Profile> profiles{};
    profiles.reserve(matches.size());
    for (const auto fid : matches) {
        auto profile = m_entities->findProfile(fid);
        if (!profile) return std::unexpected(std::move(profile.error()));
        if (*profile) profiles.push_back(std::move(**profile));
    }
    return profiles;
}

foundation::result::Result<std::vector<capability::search::ChannelSuggestion>>
SqliteReadStore::searchCachedChannels(std::string_view query, const std::size_t limit)
{
    while (!query.empty() && (query.front() == ' ' || query.front() == '\t'
        || query.front() == '\n' || query.front() == '\r'))
        query.remove_prefix(1);
    while (!query.empty() && (query.back() == ' ' || query.back() == '\t'
        || query.back() == '\n' || query.back() == '\r'))
        query.remove_suffix(1);
    if (query.empty() || query.size() > 128U
        || query.find('\0') != std::string_view::npos
        || limit == 0U || limit > 30U) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Local channel search requires 1..128 literal bytes and a limit of 1..30."));
    }
    std::string prefix{};
    prefix.reserve(query.size() * 2U + 1U);
    for (const char character : query) {
        if (character == '%' || character == '_' || character == '!')
            prefix.push_back('!');
        prefix.push_back(character);
    }
    prefix.push_back('%');

    std::scoped_lock lock{m_mutex};
    auto prepared = sqlite::Statement::prepare(m_metadata, R"SQL(
SELECT id, name, parent_url, image_url, description, follower_count
FROM cached_channel_suggestions
WHERE id LIKE ? ESCAPE '!' COLLATE NOCASE
   OR name LIKE ? ESCAPE '!' COLLATE NOCASE
ORDER BY CASE WHEN id = ? COLLATE NOCASE THEN 0
              WHEN id LIKE ? ESCAPE '!' COLLATE NOCASE THEN 1
              ELSE 2 END,
         follower_count DESC, id ASC
LIMIT ?;
)SQL");
    if (!prepared)
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    auto statement = std::move(*prepared);
    for (const auto& [index, value] :
         std::initializer_list<std::pair<int, std::string_view>>{
             {1, prefix}, {2, prefix}, {3, query}, {4, prefix}}) {
        if (auto status = sqlite::bindText(statement.native(), index, value); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (sqlite3_bind_int64(statement.native(), 5, static_cast<sqlite3_int64>(limit))
        != SQLITE_OK)
        return std::unexpected(toFoundationError(
            sqlite::makeError(m_metadata.native(), "Binding local channel search limit")));

    std::vector<capability::search::ChannelSuggestion> channels{};
    channels.reserve(limit);
    while (true) {
        const int result = sqlite3_step(statement.native());
        if (result == SQLITE_DONE) break;
        if (result != SQLITE_ROW)
            return std::unexpected(toFoundationError(
                sqlite::makeError(m_metadata.native(), "Reading local channel suggestions")));
        const auto optionalColumn = [&](const int index) -> std::optional<std::string> {
            if (sqlite3_column_type(statement.native(), index) == SQLITE_NULL)
                return std::nullopt;
            return sqlite::columnText(statement.native(), index);
        };
        channels.push_back(capability::search::ChannelSuggestion{
            .id = domain::channel::ChannelId{sqlite::columnText(statement.native(), 0)},
            .name = sqlite::columnText(statement.native(), 1),
            .parentUrl = optionalColumn(2),
            .imageUrl = optionalColumn(3),
            .description = optionalColumn(4),
            .followerCount = static_cast<std::uint64_t>(
                std::max<sqlite3_int64>(0, sqlite3_column_int64(statement.native(), 5))),
        });
    }
    return channels;
}

foundation::result::Status SqliteReadStore::putChannelSuggestions(
    const std::vector<capability::search::ChannelSuggestion>& channels)
{
    if (channels.size() > 30U)
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Channel search snapshot exceeds the bounded suggestion limit."));
    for (const auto& channel : channels) {
        if (channel.id.value.empty() || channel.id.value.size() > 256U
            || channel.name.empty() || channel.name.size() > 512U
            || channel.id.value.find('\0') != std::string::npos
            || channel.name.find('\0') != std::string::npos
            || channel.followerCount > static_cast<std::uint64_t>(
                std::numeric_limits<sqlite3_int64>::max())) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Invalid local channel suggestion identity or follower count."));
        }
        for (const auto* field : {&channel.parentUrl, &channel.imageUrl,
                                  &channel.description}) {
            if (*field && ((*field)->size() > 4096U
                || (*field)->find('\0') != std::string::npos)) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::InvalidArgument,
                    "Invalid or oversized channel metadata."));
            }
        }
    }
    std::scoped_lock lock{m_mutex};
    sqlite::Transaction transaction{m_metadata};
    if (auto status = transaction.begin(); !status)
        return std::unexpected(toFoundationError(std::move(status.error())));
    auto prepared = sqlite::Statement::prepare(m_metadata, R"SQL(
INSERT INTO cached_channel_suggestions
(id, name, parent_url, image_url, description, follower_count)
VALUES(?, ?, ?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    name = CASE WHEN excluded.name = excluded.id
                 AND cached_channel_suggestions.name != cached_channel_suggestions.id
                THEN cached_channel_suggestions.name ELSE excluded.name END,
    parent_url = COALESCE(excluded.parent_url, cached_channel_suggestions.parent_url),
    image_url = COALESCE(excluded.image_url, cached_channel_suggestions.image_url),
    description = COALESCE(excluded.description, cached_channel_suggestions.description),
    follower_count = CASE WHEN excluded.follower_count > 0
                          THEN excluded.follower_count
                          ELSE cached_channel_suggestions.follower_count END;
)SQL");
    if (!prepared)
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    auto statement = std::move(*prepared);
    for (const auto& channel : channels) {
        sqlite3_reset(statement.native());
        sqlite3_clear_bindings(statement.native());
        if (auto status = sqlite::bindText(statement.native(), 1, channel.id.value); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        if (auto status = sqlite::bindText(statement.native(), 2, channel.name); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        for (const auto& [index, field] :
             std::initializer_list<std::pair<int, const std::optional<std::string>*>>{
                 {3, &channel.parentUrl}, {4, &channel.imageUrl},
                 {5, &channel.description}}) {
            if (*field) {
                if (auto status = sqlite::bindText(statement.native(), index, **field); !status)
                    return std::unexpected(toFoundationError(std::move(status.error())));
            } else if (sqlite3_bind_null(statement.native(), index) != SQLITE_OK) {
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_metadata.native(), "Binding optional local channel metadata")));
            }
        }
        if (sqlite3_bind_int64(statement.native(), 6,
            static_cast<sqlite3_int64>(channel.followerCount)) != SQLITE_OK)
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_metadata.native(), "Binding local channel followers")));
        if (auto status = sqlite::stepDone(m_metadata, statement); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
    }
    if (auto status = transaction.commit(); !status)
        return std::unexpected(toFoundationError(std::move(status.error())));
    return {};
}

void SqliteReadStore::rememberChannelsFromCasts(
    const std::span<const domain::cast::Cast> casts)
{
    // A raw/canonical Cast may already contain channel identity without an
    // indexed search response. Learn only explicitly resolved channel IDs;
    // never invent an ID from an arbitrary parent URL or a provider hostname.
    std::vector<const domain::cast::Cast*> pending{};
    pending.reserve(casts.size());
    for (const auto& cast : casts) pending.push_back(&cast);
    std::vector<capability::search::ChannelSuggestion> batch{};
    batch.reserve(30U);
    const auto valid = [](const std::string& text, const std::size_t maximum) {
        return !text.empty() && text.size() <= maximum
            && text.find('\0') == std::string::npos;
    };
    const auto safeOptional = [&](const std::optional<std::string>& value)
        -> std::optional<std::string> {
        return value && valid(*value, 4096U) ? value : std::nullopt;
    };
    const auto flush = [&] {
        if (!batch.empty()) {
            // Discovery is strictly secondary to a successfully persisted
            // Cast/feed. SQLite errors cannot make canonical reads fail.
            (void)putChannelSuggestions(batch);
            batch.clear();
        }
    };
    while (!pending.empty()) {
        const auto* cast = pending.back();
        pending.pop_back();
        for (const auto& reply : cast->directReplies)
            pending.push_back(&reply);
        if (!cast->channelId || !valid(*cast->channelId, 256U))
            continue;
        capability::search::ChannelSuggestion suggestion{
            .id = domain::channel::ChannelId{*cast->channelId},
            .name = cast->channelName && valid(*cast->channelName, 512U)
                ? *cast->channelName : *cast->channelId,
            .parentUrl = !cast->parent ? safeOptional(cast->parentUrl) : std::nullopt,
            .imageUrl = safeOptional(cast->channelImageUrl),
        };
        batch.push_back(std::move(suggestion));
        if (batch.size() == 30U) flush();
    }
    flush();
}

foundation::result::Result<std::optional<domain::profile::Profile>>
SqliteReadStore::findProfile(const domain::identity::Fid fid)
{
    return m_entities->findProfile(fid);
}

foundation::result::Result<std::optional<domain::cast::Cast>>
SqliteReadStore::findCast(const domain::cast::CastId& castId)
{
    auto loaded = m_entities->findCast(castId);
    if (!loaded || !*loaded) {
        return loaded;
    }
    // Restore embedded author summaries for replies as well as the normalized
    // cast graph. Reuse each FID once even in large fan-out conversations.
    std::unordered_map<std::uint64_t, std::optional<domain::profile::Profile>> authors{};
    std::vector<domain::cast::Cast*> pending{&**loaded};
    while (!pending.empty()) {
        auto* cast = pending.back();
        pending.pop_back();
        if (!cast->authorProfile && cast->author.value != 0) {
            const auto [it, fresh] = authors.try_emplace(cast->author.value);
            if (fresh) {
                auto profile = m_entities->findProfile(cast->author);
                if (!profile) {
                    return std::unexpected(std::move(profile.error()));
                }
                it->second = std::move(*profile);
            }
            if (it->second) {
                cast->authorProfile = *it->second;
            }
        }
        for (auto& reply : cast->directReplies) {
            pending.push_back(&reply);
        }
    }
    return loaded;
}

foundation::result::Status SqliteReadStore::putProfileSummary(
    const domain::profile::Profile& profile)
{
    return putEmbeddedProfileSummary(*m_entities, profile);
}

foundation::result::Status SqliteReadStore::putProfile(const domain::profile::Profile& profile)
{
    return m_entities->putProfile(profile);
}

foundation::result::Status SqliteReadStore::putCast(const domain::cast::Cast& cast)
{
    // Thread responses embed their authors. Persist each identity once, with
    // the same non-regressing summary policy used by feed hydration.
    std::vector<const domain::cast::Cast*> pending{&cast};
    std::unordered_set<std::uint64_t> seenProfiles{};
    while (!pending.empty()) {
        const auto* current = pending.back();
        pending.pop_back();
        if (current->authorProfile && current->author.value != 0
            && seenProfiles.insert(current->author.value).second) {
            if (auto status = putEmbeddedProfileSummary(*m_entities, *current->authorProfile);
                !status) {
                return status;
            }
        }
        for (const auto& reply : current->directReplies) {
            pending.push_back(&reply);
        }
    }
    if (auto status = m_entities->putCast(cast); !status)
        return status;
    rememberChannelsFromCasts(std::span{&cast, 1U});
    return {};
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
    // Legacy complete reads remain available until the Qt presentation has
    // explicitly migrated to local-window pagination.
    auto entries = m_timelines->readAllTimeline(local::timeline::TimelineId{id});
    if (!entries) {
        return std::unexpected(std::move(entries.error()));
    }
    if (key.kind == local::read::FeedKey::Kind::Following && key.fid
        && entries->empty()) {
        auto eligible = canDeriveFollowing(*key.fid);
        if (!eligible) return std::unexpected(std::move(eligible.error()));
        if (*eligible) {
            auto derived = readDerivedFollowingWindow(
                *key.fid, 0U, std::clamp(key.limit, std::size_t{1}, std::size_t{256}));
            if (!derived) return std::unexpected(std::move(derived.error()));
            return std::move(derived->page);
        }
    }
    if (key.kind == local::read::FeedKey::Kind::Recent && entries->empty()) {
        auto materialized = hasMaterializedRecentSnapshot();
        if (!materialized) return std::unexpected(std::move(materialized.error()));
        // An authoritative *empty* provider Recent snapshot wins too.
        // Never reclassify a valid empty network result as a local fallback.
        if (!*materialized) {
            auto derived = readDerivedRecentWindow(
                0U, std::clamp(key.limit, std::size_t{1}, std::size_t{256}));
            if (!derived) return std::unexpected(std::move(derived.error()));
            return std::move(derived->page);
        }
    }
    return hydrateFeed(id, std::move(*entries));
}

foundation::result::Result<local::read::FeedWindow> SqliteReadStore::readFeedWindow(
    const local::read::FeedKey& key, const std::size_t offset,
    const std::size_t limit)
{
    const std::string id = feedKey(key);
    auto window = m_timelines->readTimelineWindow(
        local::timeline::TimelineId{id}, offset, limit);
    if (!window) {
        return std::unexpected(std::move(window.error()));
    }
    if (key.kind == local::read::FeedKey::Kind::Following && key.fid
        && window->entries.empty()) {
        // Never start local-derived pagination after a materialized provider
        // feed has ended: the two datasets do not share a cursor or order.
        auto first = m_timelines->readTimelineWindow(
            local::timeline::TimelineId{id}, 0U, 1U);
        if (!first) return std::unexpected(std::move(first.error()));
        if (first->entries.empty()) {
            auto eligible = canDeriveFollowing(*key.fid);
            if (!eligible) return std::unexpected(std::move(eligible.error()));
            if (*eligible) return readDerivedFollowingWindow(*key.fid, offset, limit);
        }
    }
    if (key.kind == local::read::FeedKey::Kind::Recent && window->entries.empty()) {
        // An offset beyond a populated provider timeline must stay empty:
        // do not switch into a different dataset during scrolling.
        auto first = m_timelines->readTimelineWindow(
            local::timeline::TimelineId{id}, 0U, 1U);
        if (!first) return std::unexpected(std::move(first.error()));
        if (first->entries.empty()) {
            auto materialized = hasMaterializedRecentSnapshot();
            if (!materialized) return std::unexpected(std::move(materialized.error()));
            if (!*materialized) return readDerivedRecentWindow(offset, limit);
        }
    }
    const bool hasMore = window->hasMore;
    auto hydrated = hydrateFeed(id, std::move(window->entries));
    if (!hydrated) {
        return std::unexpected(std::move(hydrated.error()));
    }
    return local::read::FeedWindow{
        .page = std::move(*hydrated),
        .hasMore = hasMore,
    };
}

foundation::result::Result<bool> SqliteReadStore::canDeriveFollowing(
    const domain::identity::Fid viewer)
{
    if (viewer.value == 0U) return false;
    std::scoped_lock lock{m_mutex};
    // A successfully fetched empty provider feed is authoritative.
    // Equally, avoid pretending to know a user's Following graph when its
    // own graph page has not yet been persisted locally.
    auto prepared = sqlite::Statement::prepare(m_metadata, R"SQL(
SELECT EXISTS(SELECT 1 FROM read_continuations WHERE read_key = ?),
       EXISTS(SELECT 1 FROM read_continuations WHERE read_key = ?);
)SQL");
    if (!prepared)
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    auto statement = std::move(*prepared);
    const auto feed = "feed:following:" + std::to_string(viewer.value);
    const auto graph = "social:following:" + std::to_string(viewer.value);
    if (auto status = sqlite::bindText(statement.native(), 1, feed); !status)
        return std::unexpected(toFoundationError(std::move(status.error())));
    if (auto status = sqlite::bindText(statement.native(), 2, graph); !status)
        return std::unexpected(toFoundationError(std::move(status.error())));
    if (sqlite3_step(statement.native()) != SQLITE_ROW)
        return std::unexpected(toFoundationError(sqlite::makeError(
            m_metadata.native(), "Inspecting local Following coverage")));
    return sqlite3_column_int(statement.native(), 0) == 0
        && sqlite3_column_int(statement.native(), 1) == 1;
}

foundation::result::Result<local::read::FeedWindow>
SqliteReadStore::readDerivedFollowingWindow(
    const domain::identity::Fid viewer,
    const std::size_t offset, const std::size_t limit)
{
    constexpr auto maximumSqliteInteger =
        static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max());
    if (viewer.value == 0U || viewer.value > maximumSqliteInteger
        || limit == 0U || limit > 256U
        || static_cast<std::uint64_t>(offset) > maximumSqliteInteger) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Locally derived Following needs a viewer FID, limit 1..256 and safe offset."));
    }
    std::vector<domain::feed::FeedEntry> entries{};
    {
        std::scoped_lock lock{m_mutex};
        // This inventory is intentionally limited to already-observed root
        // and quote Casts from saved Follow edges. It is NOT a live network
        // aggregate. Never read an unrelated account's cached graph.
        auto prepared = sqlite::Statement::prepare(m_metadata, R"SQL(
SELECT casts.id, casts.created_at_ms FROM casts
WHERE casts.parent_id IS NULL
  AND EXISTS (
    SELECT 1 FROM social_list_entries AS links
    WHERE links.list_id = ? AND links.source_fid = ?
      AND links.target_fid = casts.author_fid
      AND links.relation_kind = 'follows'
  )
  AND NOT EXISTS (
    SELECT 1 FROM cast_local_tombstones AS tombstone
    WHERE tombstone.cast_id = casts.id
  )
ORDER BY casts.created_at_ms DESC, casts.id DESC
LIMIT ? OFFSET ?;
)SQL");
        if (!prepared)
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        const auto graph = "social:following:" + std::to_string(viewer.value);
        if (auto status = sqlite::bindText(statement.native(), 1, graph); !status)
            return std::unexpected(toFoundationError(std::move(status.error())));
        if (sqlite3_bind_int64(statement.native(), 2,
                static_cast<sqlite3_int64>(viewer.value)) != SQLITE_OK
            || sqlite3_bind_int64(statement.native(), 3,
                static_cast<sqlite3_int64>(limit + 1U)) != SQLITE_OK
            || sqlite3_bind_int64(statement.native(), 4,
                static_cast<sqlite3_int64>(offset)) != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_metadata.native(), "Binding local Following window")));
        }
        entries.reserve(limit + 1U);
        while (true) {
            const int step = sqlite3_step(statement.native());
            if (step == SQLITE_DONE) break;
            if (step != SQLITE_ROW)
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_metadata.native(), "Reading local Following window")));
            entries.push_back(domain::feed::FeedEntry{
                .castId = domain::cast::CastId{sqlite::columnText(statement.native(), 0)},
                .sortTime = foundation::time::Timestamp{
                    std::chrono::milliseconds{sqlite3_column_int64(statement.native(), 1)}},
            });
        }
    }
    const bool hasMore = entries.size() > limit;
    if (hasMore) entries.pop_back();
    auto page = hydrateFeed(
        "feed:following:" + std::to_string(viewer.value), std::move(entries));
    if (!page) return std::unexpected(std::move(page.error()));
    page->next.reset(); // Local-only ordering must never expose a provider cursor.
    return local::read::FeedWindow{
        .page = std::move(*page),
        .hasMore = hasMore,
        .isDerivedFollowing = true,
    };
}

foundation::result::Result<bool> SqliteReadStore::hasMaterializedRecentSnapshot()
{
    std::scoped_lock lock{m_mutex};
    // writeFeed persists this row even when the authoritative provider page
    // is empty or has no continuation. A missing row means never materialized.
    auto prepared = sqlite::Statement::prepare(m_metadata,
        "SELECT 1 FROM read_continuations WHERE read_key = 'feed:recent' LIMIT 1;");
    if (!prepared)
        return std::unexpected(toFoundationError(std::move(prepared.error())));
    auto statement = std::move(*prepared);
    const int step = sqlite3_step(statement.native());
    if (step == SQLITE_ROW) return true;
    if (step == SQLITE_DONE) return false;
    return std::unexpected(toFoundationError(sqlite::makeError(
        m_metadata.native(), "Inspecting authoritative Recent snapshot")));
}

foundation::result::Result<local::read::FeedWindow>
SqliteReadStore::readDerivedRecentWindow(
    const std::size_t offset, const std::size_t limit)
{
    constexpr std::size_t maximumWindow{256U};
    constexpr auto largestSqliteInteger =
        static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max());
    if (limit == 0U || limit > maximumWindow
        || static_cast<std::uint64_t>(offset) > largestSqliteInteger) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Derived local Recent requires a limit 1..256 and valid offset."));
    }
    std::vector<domain::feed::FeedEntry> entries{};
    {
        std::scoped_lock lock{m_mutex};
        // Locally observed casts are a *bounded, partial* dataset. This is
        // not a network Recent API and never manufactures provider cursors.
        // Tombstones are filtered before SQL pagination so offsets are stable.
        auto prepared = sqlite::Statement::prepare(m_metadata, R"SQL(
SELECT casts.id, casts.created_at_ms FROM casts
WHERE NOT EXISTS (
    SELECT 1 FROM cast_local_tombstones AS tombstone
    WHERE tombstone.cast_id = casts.id
)
ORDER BY casts.created_at_ms DESC, casts.id DESC
LIMIT ? OFFSET ?;
)SQL");
        if (!prepared)
            return std::unexpected(toFoundationError(std::move(prepared.error())));
        auto statement = std::move(*prepared);
        if (sqlite3_bind_int64(statement.native(), 1, static_cast<sqlite3_int64>(limit + 1U))
                != SQLITE_OK
            || sqlite3_bind_int64(statement.native(), 2, static_cast<sqlite3_int64>(offset))
                != SQLITE_OK) {
            return std::unexpected(toFoundationError(sqlite::makeError(
                m_metadata.native(), "Failed to bind derived Recent window")));
        }
        entries.reserve(limit + 1U);
        while (true) {
            const int step = sqlite3_step(statement.native());
            if (step == SQLITE_DONE) break;
            if (step != SQLITE_ROW)
                return std::unexpected(toFoundationError(sqlite::makeError(
                    m_metadata.native(), "Failed to read derived Recent")));
            entries.push_back(domain::feed::FeedEntry{
                .castId = domain::cast::CastId{sqlite::columnText(statement.native(), 0)},
                .sortTime = foundation::time::Timestamp{
                    std::chrono::milliseconds{sqlite3_column_int64(statement.native(), 1)}},
            });
        }
    }
    const bool hasMore = entries.size() > limit;
    if (hasMore) entries.pop_back();
    auto page = hydrateFeed("feed:recent", std::move(entries));
    if (!page) return std::unexpected(std::move(page.error()));
    // An unrelated provider token is never valid for this local-only view.
    page->next.reset();
    return local::read::FeedWindow{
        .page = std::move(*page),
        .hasMore = hasMore,
        .isDerivedRecent = true,
    };
}

foundation::result::Result<capability::feed::FeedPage> SqliteReadStore::hydrateFeed(
    const std::string_view id, std::vector<domain::feed::FeedEntry> entries)
{
    capability::feed::FeedPage page{};
    page.entries = std::move(entries);
    std::unordered_set<std::uint64_t> seenProfiles{};
    for (const auto& entry : page.entries) {
        auto cast = m_entities->findCastShallow(entry.castId);
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
    std::unique_lock lock{m_mutex};
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
    lock.unlock(); // Release metadata mutex before the secondary channel-cache transaction.
    rememberChannelsFromCasts(std::span<const domain::cast::Cast>{page.casts});
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
