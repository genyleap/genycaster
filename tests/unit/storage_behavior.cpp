module;

#include <cassert>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <limits>
#include <thread>
#include <optional>
#include <string>
#include <vector>

module farcaster.test.storage;

import farcaster.local.timeline;
import farcaster.capability.catalog;
import farcaster.domain.cast;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.time;
import farcaster.foundation.error;
import farcaster.provider.model;
import farcaster.storage.checkpoint;
import farcaster.storage.continuation;
import farcaster.storage.entity;
import farcaster.storage.timeline;
import farcaster.sync.model;
import farcaster.test.tempdatabase;

namespace farcaster::test::storage {

void runLocalStoreRoundTrip()
{
    using namespace farcaster;

    const tempdatabase::TemporaryDatabase temporaryDatabase{};
    const auto& databasePath = temporaryDatabase.path();

    auto entityStore = farcaster::storage::entity::openEntityStore(databasePath).value();
    auto timelineStore = farcaster::storage::timeline::openTimelineStore(databasePath).value();
    auto continuationStore = farcaster::storage::continuation::openContinuationStore(databasePath).value();
    auto checkpointStore = farcaster::storage::checkpoint::openCheckpointStore(databasePath).value();

    const domain::profile::Profile profile{
        .fid = domain::identity::Fid{42},
        .username = domain::identity::Username{"alice"},
        .displayName = domain::identity::DisplayName{"Alice"},
        .bio = "Local-first profile",
        .avatarUrl = std::string{"https://example.test/avatar.png"},
        .bannerUrl = std::string{"https://example.test/banner.png"},
        .location = std::string{"San Francisco"},
        .profileUrl = std::string{"https://example.test/alice"},
        .custodyAddress = std::string{"0x1234"},
        .verifiedEthAddresses = {
            "0x1111111111111111111111111111111111111111",
            "0x2222222222222222222222222222222222222222",
        },
        .verifiedSolAddresses = {"So11111111111111111111111111111111111111112"},
        .followerCount = 1200,
        .followingCount = 345,
        .qualityScore = 0.83,
    };
    assert(entityStore->putProfile(profile));

    auto loadedProfile = entityStore->findProfile(domain::identity::Fid{42});
    assert(loadedProfile);
    assert(loadedProfile->has_value());
    assert((*loadedProfile)->username->value == "alice");
    assert((*loadedProfile)->displayName->value == "Alice");
    assert((*loadedProfile)->bannerUrl == std::optional<std::string>{"https://example.test/banner.png"});
    assert((*loadedProfile)->location == std::optional<std::string>{"San Francisco"});
    assert((*loadedProfile)->profileUrl == std::optional<std::string>{"https://example.test/alice"});
    assert((*loadedProfile)->verifiedEthAddresses == profile.verifiedEthAddresses);
    assert((*loadedProfile)->verifiedSolAddresses == profile.verifiedSolAddresses);
    assert((*loadedProfile)->followerCount == 1200);
    assert((*loadedProfile)->followingCount == 345);
    assert((*loadedProfile)->socialCountsKnown);
    assert((*loadedProfile)->qualityScore == std::optional<double>{0.83});

    // Raw Snapchain userData has no graph counts. Persist that distinction
    // through both primary-key and username lookup; zero is not a known value.
    auto rawProfile = profile;
    rawProfile.followerCount = 0;
    rawProfile.followingCount = 0;
    rawProfile.socialCountsKnown = false;
    assert(entityStore->putProfile(rawProfile));
    auto unknown = entityStore->findProfile(domain::identity::Fid{42});
    assert(unknown && *unknown && !(*unknown)->socialCountsKnown);
    auto byUsername = entityStore->findProfileByUsername("alice");
    assert(byUsername && *byUsername && !(*byUsername)->socialCountsKnown);
    assert(entityStore->putProfile(profile));

    const auto createdAt = foundation::time::now();
    const domain::cast::Cast cast{
        .id = domain::cast::CastId{"cast-1"},
        .author = domain::identity::Fid{42},
        .text = "Right! 💯  \n\nhttps://x.com/vrypan/status/2113262913767317648?s=46",
        .displayText = std::string{"Right! 💯 @vrypan.eth \n\nhttps://x.com/vrypan/status/2113262913767317648?s=46"},
        .parent = std::nullopt,
        .parentUrl = std::string{"https://farcaster.group/dev/chat"},
        .embeds = {
            domain::cast::Embed{
                .kind = domain::cast::EmbedKind::Url,
                .value = "https://example.test",
            },
            domain::cast::Embed{
                .kind = domain::cast::EmbedKind::CastReference,
                .value = "cast-quoted",
                .referencedAuthor = domain::identity::Fid{91},
            },
        },
        .engagement = domain::cast::Engagement{.replies = 3, .recasts = 2, .likes = 11},
        .createdAt = createdAt,
    };
    assert(entityStore->putCast(cast));

    auto loadedCast = entityStore->findCast(domain::cast::CastId{"cast-1"});
    assert(loadedCast);
    assert(loadedCast->has_value());
    assert((*loadedCast)->text == cast.text);
    assert((*loadedCast)->displayText == cast.displayText);

    // Re-reading the same cast from a sparse search row must not erase an
    // already hydrated mention projection. Canonical signed text stays raw.
    auto sparse = cast;
    sparse.displayText.reset();
    assert(entityStore->putCast(sparse));
    auto warmed = entityStore->findCastShallow(domain::cast::CastId{"cast-1"});
    assert(warmed && warmed->has_value());
    assert((*warmed)->text == cast.text);
    assert((*warmed)->displayText == cast.displayText);

    assert((*loadedCast)->parentUrl == std::optional<std::string>{"https://farcaster.group/dev/chat"});
    assert((*loadedCast)->embeds.size() == 2);
    assert((*loadedCast)->embeds[0].kind == domain::cast::EmbedKind::Url);
    assert((*loadedCast)->embeds[0].value == "https://example.test");
    assert((*loadedCast)->embeds[1].kind == domain::cast::EmbedKind::CastReference);
    assert((*loadedCast)->embeds[1].value == "cast-quoted");
    assert((*loadedCast)->embeds[1].referencedAuthor);
    assert((*loadedCast)->embeds[1].referencedAuthor->value == 91);
    assert((*loadedCast)->engagement.replies == 3);
    assert((*loadedCast)->engagement.recasts == 2);
    assert((*loadedCast)->engagement.likes == 11);

    const local::timeline::TimelineId timelineId{"feed.following:42"};
    const std::vector<domain::feed::FeedEntry> entries{
        domain::feed::FeedEntry{.castId = domain::cast::CastId{"cast-1"}, .sortTime = createdAt},
    };
    assert(timelineStore->replaceTimeline(timelineId, entries));

    auto timeline = timelineStore->readTimeline(timelineId, 30);
    assert(timeline);
    assert(timeline->size() == 1);
    assert(timeline->front().castId.value == "cast-1");

    auto token = continuationStore.bind(
        capability::catalog::feedFollowingRead(),
        provider::model::ProviderId{"provider-a"},
        "provider-private-cursor");
    assert(token);
    assert(token->value.find("provider-private-cursor") == std::string::npos);

    auto binding = continuationStore.resolve(*token);
    assert(binding);
    assert(binding->providerId.value == "provider-a");
    assert(binding->providerCursor == "provider-private-cursor");

    assert(continuationStore.erase(*token));
    auto erasedBinding = continuationStore.resolve(*token);
    assert(!erasedBinding);

    const sync::model::SyncKey syncKey{"feed.following:42"};
    const sync::model::SyncCheckpoint checkpoint{.sequence = 77, .updatedAt = foundation::time::now()};
    assert(checkpointStore->saveCheckpoint(syncKey, checkpoint));
    auto loadedCheckpoint = checkpointStore->loadCheckpoint(syncKey);
    assert(loadedCheckpoint);
    assert(loadedCheckpoint->has_value());
    assert((*loadedCheckpoint)->sequence == 77);

    // Multiple workers/nodes can ingest independently, but a stale completion
    // must not roll back a previously committed checkpoint.
    auto secondStore = farcaster::storage::checkpoint::openCheckpointStore(databasePath).value();
    const auto laterTimestamp = checkpoint.updatedAt + std::chrono::seconds{60};
    const sync::model::SyncCheckpoint sameSequence{
        .sequence = 77, .updatedAt = laterTimestamp};
    assert(secondStore->saveCheckpoint(syncKey, sameSequence));
    auto replay = checkpointStore->loadCheckpoint(syncKey);
    assert(replay && replay->has_value());
    assert((*replay)->updatedAt == checkpoint.updatedAt);

    const sync::model::SyncCheckpoint rewind{
        .sequence = 76, .updatedAt = laterTimestamp};
    auto stale = secondStore->saveCheckpoint(syncKey, rewind);
    assert(!stale && stale.error().code == foundation::error::ErrorCode::Conflict);
    replay = checkpointStore->loadCheckpoint(syncKey);
    assert(replay && (*replay)->sequence == 77);
    assert((*replay)->updatedAt == checkpoint.updatedAt);

    // Two independent SQLite connections race at the same waterline.
    // The lower sequence either commits first or fails with Conflict.
    std::barrier gate{2};
    std::atomic<int> refused{0};
    std::thread lower{[&] {
        gate.arrive_and_wait();
        const auto status = checkpointStore->saveCheckpoint(syncKey,
            sync::model::SyncCheckpoint{.sequence = 101, .updatedAt = laterTimestamp});
        if (!status) {
            assert(status.error().code == foundation::error::ErrorCode::Conflict);
            ++refused;
        }
    }};
    std::thread higher{[&] {
        gate.arrive_and_wait();
        const auto status = secondStore->saveCheckpoint(syncKey,
            sync::model::SyncCheckpoint{.sequence = 103, .updatedAt = laterTimestamp});
        assert(status);
    }};
    lower.join();
    higher.join();
    assert(refused.load() <= 1);
    auto latest = checkpointStore->loadCheckpoint(syncKey);
    assert(latest && (*latest)->sequence == 103);
    assert((*latest)->updatedAt == laterTimestamp);
    assert(!checkpointStore->saveCheckpoint(syncKey, rewind));
    latest = secondStore->loadCheckpoint(syncKey);
    assert(latest && (*latest)->sequence == 103);

    // Source-specific keys remain separate: one raw-node reader must not
    // overwrite a different node's traversal waterline.
    const sync::model::SyncKey otherSource{"index:other-node:recent"};
    assert(secondStore->saveCheckpoint(otherSource,
        sync::model::SyncCheckpoint{.sequence = 4, .updatedAt = checkpoint.updatedAt}));
    auto other = checkpointStore->loadCheckpoint(otherSource);
    assert(other && (*other)->sequence == 4);
    assert((checkpointStore->loadCheckpoint(syncKey))->value().sequence == 103);

    // SQLite INTEGER is signed. Overflowing a u64 must never persist a
    // negative sequence and be mistaken for enormous forward progress.
    constexpr auto sqliteMaximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    const sync::model::SyncCheckpoint oversized{
        .sequence = sqliteMaximum + 1U, .updatedAt = laterTimestamp};
    auto invalid = checkpointStore->saveCheckpoint(syncKey, oversized);
    assert(!invalid && invalid.error().code
        == foundation::error::ErrorCode::InvalidArgument);
    assert(!checkpointStore->saveCheckpoint(
        sync::model::SyncKey{}, checkpoint));
    assert(!checkpointStore->loadCheckpoint(sync::model::SyncKey{}));
    assert(!checkpointStore->saveCheckpoint(
        sync::model::SyncKey{std::string("a\0b", 3)}, checkpoint));

    // Closing/reopening SQLite must preserve the strictly increasing
    // persisted sequence; older late responses cannot resurrect an old state.
    secondStore.reset();
    auto restarted = farcaster::storage::checkpoint::openCheckpointStore(databasePath).value();
    auto recovered = restarted->loadCheckpoint(syncKey);
    assert(recovered && (*recovered)->sequence == 103);
    assert(!restarted->saveCheckpoint(syncKey, rewind));
}

}
