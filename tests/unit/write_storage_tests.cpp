#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include <sqlite3.h>

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.time;
import farcaster.storage.write;

namespace {

namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace storage = farcaster::storage;

struct DatabaseFixture final {
    DatabaseFixture()
    {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
            ("genycaster-write-outbox-" + std::to_string(unique) + ".sqlite3");
    }

    ~DatabaseFixture()
    {
        std::error_code error{};
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.string() + "-wal", error);
        std::filesystem::remove(path.string() + "-shm", error);
    }

    std::filesystem::path path{};
};

[[nodiscard]] foundation::time::Timestamp timestamp(const std::int64_t milliseconds)
{
    return foundation::time::Timestamp{std::chrono::milliseconds{milliseconds}};
}

[[nodiscard]] domain::write::SignedOperation operation()
{
    constexpr auto castId = "0x00112233445566778899aabbccddeeff00112233";
    return domain::write::SignedOperation{
        .kind = domain::write::OperationKind::PublishCast,
        .idempotencyKey = domain::write::IdempotencyKey{castId},
        .author = domain::identity::Fid{42U},
        .optimisticCast = domain::cast::Cast{
            .id = domain::cast::CastId{castId},
            .author = domain::identity::Fid{42U},
            .text = "durable hello",
            .embeds = {
                domain::cast::Embed{
                    .kind = domain::cast::EmbedKind::Url,
                    .value = "https://example.com",
                },
                domain::cast::Embed{
                    .kind = domain::cast::EmbedKind::CastReference,
                    .value = "0x102132435465768798a9bacbdcedfe0f10213243",
                    .referencedAuthor = domain::identity::Fid{7U},
                },
            },
            .createdAt = timestamp(1'700'000'000'000LL),
        },
        .signerPublicKey = domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"},
        .messageBytes = std::string{"\x00\x01signed\x00protobuf", 17U},
        .signedAt = timestamp(1'700'000'000'100LL),
    };
}

[[nodiscard]] domain::write::SignedOperation removeOperation()
{
    return domain::write::SignedOperation{
        .kind = domain::write::OperationKind::RemoveCast,
        .idempotencyKey = domain::write::IdempotencyKey{
            "0x9999888877776666555544443333222211110000"},
        .author = domain::identity::Fid{42U},
        .target = domain::write::OperationTarget{
            .cast = domain::cast::CastReference{
                .author = domain::identity::Fid{42U},
                .id = domain::cast::CastId{
                    "0x00112233445566778899aabbccddeeff00112233"},
            },
        },
        .signerPublicKey = domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"},
        .messageBytes = "signed-remove-envelope",
        .signedAt = timestamp(1'700'000'000'300LL),
    };
}

[[nodiscard]] domain::write::SignedOperation reactionOperation(
    const domain::write::OperationKind kind,
    std::string idempotencyKey)
{
    return domain::write::SignedOperation{
        .kind = kind,
        .idempotencyKey = domain::write::IdempotencyKey{std::move(idempotencyKey)},
        .author = domain::identity::Fid{42U},
        .target = domain::write::OperationTarget{
            .cast = domain::cast::CastReference{
                .author = domain::identity::Fid{7U},
                .id = domain::cast::CastId{
                    "0x000102030405060708090a0b0c0d0e0f10111213"},
            },
        },
        .signerPublicKey = domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"},
        .messageBytes = "signed-reaction-envelope-" + std::to_string(static_cast<int>(kind)),
        .signedAt = timestamp(1'700'000'000'100LL),
    };
}

[[nodiscard]] domain::write::SignedOperation followingOperation(
    const domain::write::OperationKind kind,
    std::string idempotencyKey,
    const domain::identity::Fid author = domain::identity::Fid{42U},
    const domain::identity::Fid target = domain::identity::Fid{99U})
{
    return domain::write::SignedOperation{
        .kind = kind,
        .idempotencyKey = domain::write::IdempotencyKey{std::move(idempotencyKey)},
        .author = author,
        .target = domain::write::OperationTarget{.fid = target},
        .signerPublicKey = domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"},
        .messageBytes = "signed-following-envelope-" + std::to_string(static_cast<int>(kind)),
        .signedAt = timestamp(1'700'000'000'100LL),
    };
}

[[nodiscard]] bool hasLocalCastTombstone(
    const std::filesystem::path& databasePath,
    const std::string& castId)
{
    sqlite3* database{};
    const auto pathText = databasePath.string();
    assert(sqlite3_open_v2(pathText.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);

    sqlite3_stmt* statement{};
    constexpr auto sql =
        "SELECT 1 FROM cast_local_tombstones WHERE cast_id = ? LIMIT 1;";
    assert(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK);
    assert(sqlite3_bind_text(statement, 1, castId.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK);
    const bool found = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return found;
}

void durableRoundTripAndDuplicateIdempotency()
{
    DatabaseFixture database{};
    const auto original = operation();
    {
        auto store = storage::write::openOutboxStore(database.path);
        assert(store);
        auto queued = (*store)->enqueue(original, timestamp(1'700'000'000'200LL));
        assert(queued);
        assert(queued->status == domain::write::OutboxStatus::Pending);
        assert(queued->attemptCount == 0U);
        assert(queued->operation.messageBytes == original.messageBytes);
        assert(queued->operation.optimisticCast->embeds.size() == 2U);

        auto attempted = (*store)->beginAttempt(
            original.idempotencyKey,
            timestamp(1'700'000'000'300LL));
        assert(attempted && attempted->attemptCount == 1U);

        auto duplicate = (*store)->enqueue(original, timestamp(1'700'000'000'400LL));
        assert(duplicate);
        assert(duplicate->attemptCount == 1U);
        assert(duplicate->enqueuedAt == timestamp(1'700'000'000'200LL));
    }

    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto record = (*reopened)->find(original.idempotencyKey);
    assert(record && *record);
    assert((*record)->operation.optimisticCast->id.value == original.optimisticCast->id.value);
    assert((*record)->operation.optimisticCast->text == original.optimisticCast->text);
    assert((*record)->operation.optimisticCast->embeds.size() == 2U);
    assert((*record)->operation.messageBytes == original.messageBytes);
    assert((*record)->attemptCount == 1U);
}


void castProjectionsSurviveRestartAndRetainRetryStatus()
{
    DatabaseFixture database{};
    auto original = operation();
    original.optimisticCast->parent = domain::cast::CastId{
        "0x9999888877776666555544443333222211110000"};
    {
        auto store = storage::write::openOutboxStore(database.path);
        assert(store);
        assert((*store)->enqueue(original, timestamp(1000)));
        auto attempted = (*store)->beginAttempt(original.idempotencyKey, timestamp(1001));
        assert(attempted);
        assert((*store)->markRetry(
            original.idempotencyKey,
            domain::write::OutboxFailure{},
            timestamp(9000)));
    }
    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto casts = (*reopened)->listCastProjections(domain::identity::Fid{42U}, 20);
    assert(casts && casts->size() == 1U);
    assert(casts->front().status == domain::write::OutboxStatus::Pending);
    assert(casts->front().nextAttemptAt == timestamp(9000));
    assert(casts->front().operation.optimisticCast);
    assert(casts->front().operation.optimisticCast->parent);
    assert(casts->front().operation.optimisticCast->text == "durable hello");
    auto isolated = (*reopened)->listCastProjections(domain::identity::Fid{9U}, 20);
    assert(isolated && isolated->empty());
    assert((*reopened)->markFailed(original.idempotencyKey, domain::write::OutboxFailure{}, timestamp(10000)));
    (*reopened).reset();
    auto again = storage::write::openOutboxStore(database.path);
    assert(again);
    auto failed = (*again)->listCastProjections(domain::identity::Fid{42U}, 20);
    assert(failed && failed->size() == 1U);
    assert(failed->front().status == domain::write::OutboxStatus::Failed);
}

void socialTargetsSurviveStoreReopen()
{
    DatabaseFixture database{};
    auto store = storage::write::openOutboxStore(database.path);
    assert(store);

    const domain::write::SignedOperation like{
        .kind = domain::write::OperationKind::Like,
        .idempotencyKey = domain::write::IdempotencyKey{
            "0x1111222233334444555566667777888899990000"},
        .author = domain::identity::Fid{42U},
        .target = domain::write::OperationTarget{
            .cast = domain::cast::CastReference{
                .author = domain::identity::Fid{7U},
                .id = domain::cast::CastId{
                    "0x000102030405060708090a0b0c0d0e0f10111213"},
            },
        },
        .signerPublicKey = domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"},
        .messageBytes = "signed-like-envelope",
        .signedAt = timestamp(1'700'000'000'100LL),
    };
    const domain::write::SignedOperation follow{
        .kind = domain::write::OperationKind::Follow,
        .idempotencyKey = domain::write::IdempotencyKey{
            "0xaaaabbbbccccddddeeeeffff0000111122223333"},
        .author = domain::identity::Fid{42U},
        .target = domain::write::OperationTarget{
            .fid = domain::identity::Fid{99U},
        },
        .signerPublicKey = like.signerPublicKey,
        .messageBytes = "signed-follow-envelope",
        .signedAt = timestamp(1'700'000'000'200LL),
    };

    assert((*store)->enqueue(like, timestamp(1'000)));
    assert((*store)->enqueue(follow, timestamp(1'001)));
    (*store).reset();

    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto storedLike = (*reopened)->find(like.idempotencyKey);
    assert(storedLike && *storedLike);
    assert((*storedLike)->operation.kind == domain::write::OperationKind::Like);
    assert((*storedLike)->operation.author.value == 42U);
    assert((*storedLike)->operation.target.cast);
    assert((*storedLike)->operation.target.cast->author.value == 7U);
    assert((*storedLike)->operation.target.cast->id.value ==
        "0x000102030405060708090a0b0c0d0e0f10111213");
    assert(!(*storedLike)->operation.optimisticCast);

    auto storedFollow = (*reopened)->find(follow.idempotencyKey);
    assert(storedFollow && *storedFollow);
    assert((*storedFollow)->operation.kind == domain::write::OperationKind::Follow);
    assert((*storedFollow)->operation.target.fid);
    assert((*storedFollow)->operation.target.fid->value == 99U);
    assert(!(*storedFollow)->operation.optimisticCast);
}

void reactionProjectionSurvivesRefreshReopenAndOldDuplicate()
{
    DatabaseFixture database{};
    const auto like = reactionOperation(
        domain::write::OperationKind::Like,
        "0x1111222233334444555566667777888899990000");
    const auto unlike = reactionOperation(
        domain::write::OperationKind::Unlike,
        "0x2222333344445555666677778888999900001111");
    const auto recast = reactionOperation(
        domain::write::OperationKind::Recast,
        "0x3333444455556666777788889999000011112222");

    {
        auto store = storage::write::openOutboxStore(database.path);
        assert(store);
        assert((*store)->enqueue(like, timestamp(1'000)));
        auto first = (*store)->listReactionProjections(domain::identity::Fid{42U});
        assert(first && first->size() == 1U);
        assert(first->front().reaction == domain::write::ReactionKind::Like);
        assert(first->front().isActive);

        assert((*store)->enqueue(unlike, timestamp(1'001)));
        assert((*store)->enqueue(recast, timestamp(1'002)));

        auto current = (*store)->listReactionProjections(domain::identity::Fid{42U});
        assert(current && current->size() == 2U);
        assert(current->at(0).reaction == domain::write::ReactionKind::Like);
        assert(!current->at(0).isActive);
        assert(current->at(1).reaction == domain::write::ReactionKind::Recast);
        assert(current->at(1).isActive);

        // Re-enqueueing an older idempotent Like must not overwrite the newer
        // Unlike projection because no new outbox row is inserted.
        assert((*store)->enqueue(like, timestamp(2'000)));
        auto afterDuplicate = (*store)->listReactionProjections(domain::identity::Fid{42U});
        assert(afterDuplicate && afterDuplicate->size() == 2U);
        assert(afterDuplicate->at(0).reaction == domain::write::ReactionKind::Like);
        assert(!afterDuplicate->at(0).isActive);
    }

    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto restored = (*reopened)->listReactionProjections(domain::identity::Fid{42U});
    assert(restored && restored->size() == 2U);
    assert(restored->at(0).target.id.value ==
        "0x000102030405060708090a0b0c0d0e0f10111213");
    assert(restored->at(0).reaction == domain::write::ReactionKind::Like);
    assert(!restored->at(0).isActive);
    assert(restored->at(1).reaction == domain::write::ReactionKind::Recast);
    assert(restored->at(1).isActive);

    auto otherAccount = (*reopened)->listReactionProjections(domain::identity::Fid{99U});
    assert(otherAccount && otherAccount->empty());
}


void followingProjectionSurvivesRefreshReopenAndOldDuplicate()
{
    DatabaseFixture database{};
    const auto follow = followingOperation(
        domain::write::OperationKind::Follow,
        "0x4444555566667777888899990000111122223333");
    const auto unfollow = followingOperation(
        domain::write::OperationKind::Unfollow,
        "0x5555666677778888999900001111222233334444");

    {
        auto store = storage::write::openOutboxStore(database.path);
        assert(store);
        assert((*store)->enqueue(follow, timestamp(1'000)));
        auto first = (*store)->listFollowingProjections(domain::identity::Fid{42U});
        assert(first && first->size() == 1U);
        assert(first->front().target.value == 99U);
        assert(first->front().isFollowing);

        assert((*store)->enqueue(unfollow, timestamp(1'001)));
        auto current = (*store)->listFollowingProjections(domain::identity::Fid{42U});
        assert(current && current->size() == 1U);
        assert(!current->front().isFollowing);

        // Re-enqueueing the old deterministic Follow envelope is idempotent and
        // must not overwrite the newer Unfollow projection.
        assert((*store)->enqueue(follow, timestamp(2'000)));
        auto afterDuplicate = (*store)->listFollowingProjections(domain::identity::Fid{42U});
        assert(afterDuplicate && afterDuplicate->size() == 1U);
        assert(!afterDuplicate->front().isFollowing);

        const auto otherAccountFollow = followingOperation(
            domain::write::OperationKind::Follow,
            "0x6666777788889999000011112222333344445555",
            domain::identity::Fid{77U},
            domain::identity::Fid{99U});
        assert((*store)->enqueue(otherAccountFollow, timestamp(1'002)));
    }

    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto restored = (*reopened)->listFollowingProjections(domain::identity::Fid{42U});
    assert(restored && restored->size() == 1U);
    assert(restored->front().target.value == 99U);
    assert(!restored->front().isFollowing);

    auto otherAccount = (*reopened)->listFollowingProjections(domain::identity::Fid{77U});
    assert(otherAccount && otherAccount->size() == 1U);
    assert(otherAccount->front().isFollowing);
}

void followingProjectionBackfillsExistingOutbox()
{
    DatabaseFixture database{};
    sqlite3* rawDatabase{};
    const auto pathText = database.path.string();
    assert(sqlite3_open_v2(
        pathText.c_str(), &rawDatabase,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    constexpr auto seedSchema = R"SQL(
CREATE TABLE write_outbox (
    idempotency_key TEXT PRIMARY KEY NOT NULL,
    kind TEXT NOT NULL,
    cast_id TEXT,
    author_fid INTEGER NOT NULL,
    target_cast_id TEXT,
    target_cast_fid INTEGER,
    target_fid INTEGER,
    signer_public_key_hex TEXT NOT NULL,
    message_bytes BLOB NOT NULL,
    signed_at_ms INTEGER NOT NULL,
    enqueued_at_ms INTEGER NOT NULL,
    status TEXT NOT NULL,
    attempt_count INTEGER NOT NULL DEFAULT 0,
    next_attempt_at_ms INTEGER,
    last_failure_kind TEXT,
    last_failure_message TEXT,
    confirmed_at_ms INTEGER
);
INSERT INTO write_outbox(
    idempotency_key, kind, author_fid, target_fid, signer_public_key_hex,
    message_bytes, signed_at_ms, enqueued_at_ms, status, attempt_count)
VALUES(
    '0xcccccccccccccccccccccccccccccccccccccccc', 'follow', 42, 99,
    '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    X'01', 1000, 2000, 'confirmed', 1);
INSERT INTO write_outbox(
    idempotency_key, kind, author_fid, target_fid, signer_public_key_hex,
    message_bytes, signed_at_ms, enqueued_at_ms, status, attempt_count)
VALUES(
    '0xdddddddddddddddddddddddddddddddddddddddd', 'unfollow', 42, 99,
    '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    X'02', 1001, 2001, 'confirmed', 1);
)SQL";
    char* errorMessage{};
    assert(sqlite3_exec(rawDatabase, seedSchema, nullptr, nullptr, &errorMessage) == SQLITE_OK);
    sqlite3_free(errorMessage);
    sqlite3_close(rawDatabase);

    auto migrated = storage::write::openOutboxStore(database.path);
    assert(migrated);
    auto projections = (*migrated)->listFollowingProjections(domain::identity::Fid{42U});
    assert(projections && projections->size() == 1U);
    assert(projections->front().target.value == 99U);
    assert(!projections->front().isFollowing);
}

void reactionProjectionBackfillsExistingOutbox()
{
    DatabaseFixture database{};
    sqlite3* rawDatabase{};
    const auto pathText = database.path.string();
    assert(sqlite3_open_v2(
        pathText.c_str(), &rawDatabase,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    constexpr auto seedSchema = R"SQL(
CREATE TABLE write_outbox (
    idempotency_key TEXT PRIMARY KEY NOT NULL,
    kind TEXT NOT NULL,
    cast_id TEXT,
    author_fid INTEGER NOT NULL,
    target_cast_id TEXT,
    target_cast_fid INTEGER,
    target_fid INTEGER,
    signer_public_key_hex TEXT NOT NULL,
    message_bytes BLOB NOT NULL,
    signed_at_ms INTEGER NOT NULL,
    enqueued_at_ms INTEGER NOT NULL,
    status TEXT NOT NULL,
    attempt_count INTEGER NOT NULL DEFAULT 0,
    next_attempt_at_ms INTEGER,
    last_failure_kind TEXT,
    last_failure_message TEXT,
    confirmed_at_ms INTEGER
);
INSERT INTO write_outbox(
    idempotency_key, kind, author_fid, target_cast_id, target_cast_fid,
    signer_public_key_hex, message_bytes, signed_at_ms, enqueued_at_ms,
    status, attempt_count)
VALUES(
    '0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 'like', 42,
    '0x000102030405060708090a0b0c0d0e0f10111213', 7,
    '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    X'01', 1000, 2000, 'confirmed', 1);
INSERT INTO write_outbox(
    idempotency_key, kind, author_fid, target_cast_id, target_cast_fid,
    signer_public_key_hex, message_bytes, signed_at_ms, enqueued_at_ms,
    status, attempt_count)
VALUES(
    '0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb', 'unlike', 42,
    '0x000102030405060708090a0b0c0d0e0f10111213', 7,
    '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    X'02', 1001, 2000, 'confirmed', 1);
)SQL";
    char* errorMessage{};
    assert(sqlite3_exec(rawDatabase, seedSchema, nullptr, nullptr, &errorMessage) == SQLITE_OK);
    sqlite3_free(errorMessage);
    sqlite3_close(rawDatabase);

    auto migrated = storage::write::openOutboxStore(database.path);
    assert(migrated);
    auto projections = (*migrated)->listReactionProjections(domain::identity::Fid{42U});
    assert(projections && projections->size() == 1U);
    assert(projections->front().reaction == domain::write::ReactionKind::Like);
    assert(!projections->front().isActive);
}

void replyParentSurvivesStoreReopen()
{
    DatabaseFixture database{};
    auto reply = operation();
    reply.idempotencyKey.value = "0xabcdefabcdefabcdefabcdefabcdefabcdefabcd";
    reply.optimisticCast->id.value = reply.idempotencyKey.value;
    reply.optimisticCast->text = "durable reply";
    reply.optimisticCast->parent = domain::cast::CastId{
        "0x000102030405060708090a0b0c0d0e0f10111213"};

    {
        auto store = storage::write::openOutboxStore(database.path);
        assert(store);
        assert((*store)->enqueue(reply, timestamp(1'000)));
    }

    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto record = (*reopened)->find(reply.idempotencyKey);
    assert(record && *record);
    assert((*record)->operation.optimisticCast->parent);
    assert((*record)->operation.optimisticCast->parent->value ==
        "0x000102030405060708090a0b0c0d0e0f10111213");
}

void retrySchedulingIsDurableAndDueOrdered()
{
    DatabaseFixture database{};
    auto store = storage::write::openOutboxStore(database.path);
    assert(store);
    const auto original = operation();
    assert((*store)->enqueue(original, timestamp(1'000)));
    assert((*store)->beginAttempt(original.idempotencyKey, timestamp(1'100)));
    assert((*store)->markRetry(
        original.idempotencyKey,
        domain::write::OutboxFailure{
            .kind = domain::write::FailureKind::Unavailable,
            .message = "Protocol write endpoint is temporarily unavailable.",
        },
        timestamp(2'000)));

    auto tooEarly = (*store)->listReady(domain::identity::Fid{42U}, timestamp(1'999), 8U);
    assert(tooEarly && tooEarly->empty());
    auto due = (*store)->listReady(domain::identity::Fid{42U}, timestamp(2'000), 8U);
    assert(due && due->size() == 1U);
    assert(due->front().attemptCount == 1U);
    assert(due->front().lastFailure);
    assert(due->front().lastFailure->kind == domain::write::FailureKind::Unavailable);
}


void readyListIsAccountScoped()
{
    DatabaseFixture database{};
    auto store = storage::write::openOutboxStore(database.path);
    assert(store);

    const auto first = operation();
    auto second = operation();
    constexpr auto secondCastId = "0xffeeddccbbaa99887766554433221100ffeeddcc";
    second.idempotencyKey.value = secondCastId;
    second.optimisticCast->id.value = secondCastId;
    second.author = domain::identity::Fid{99U};
    second.optimisticCast->author = domain::identity::Fid{99U};

    assert((*store)->enqueue(first, timestamp(1'000)));
    assert((*store)->enqueue(second, timestamp(1'001)));

    auto firstReady = (*store)->listReady(domain::identity::Fid{42U}, timestamp(2'000), 8U);
    assert(firstReady && firstReady->size() == 1U);
    assert(firstReady->front().operation.optimisticCast->author.value == 42U);
    assert(firstReady->front().operation.idempotencyKey.value == first.idempotencyKey.value);

    auto secondReady = (*store)->listReady(domain::identity::Fid{99U}, timestamp(2'000), 8U);
    assert(secondReady && secondReady->size() == 1U);
    assert(secondReady->front().operation.optimisticCast->author.value == 99U);
    assert(secondReady->front().operation.idempotencyKey.value == second.idempotencyKey.value);

    auto invalid = (*store)->listReady(domain::identity::Fid{}, timestamp(2'000), 8U);
    assert(!invalid);
    assert(invalid.error().code == foundation::error::ErrorCode::InvalidArgument);
}

void terminalFailureRetainsOptimisticProjection()
{
    DatabaseFixture database{};
    const auto original = operation();
    {
        auto store = storage::write::openOutboxStore(database.path);
        assert(store);
        assert((*store)->enqueue(original, timestamp(1'000)));
        assert((*store)->markFailed(
            original.idempotencyKey,
            domain::write::OutboxFailure{
                .kind = domain::write::FailureKind::ProtocolRejected,
                .message = "Canonical protocol write was rejected or malformed.",
            },
            timestamp(1'100)));
    }

    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto record = (*reopened)->find(original.idempotencyKey);
    assert(record && *record);
    assert((*record)->status == domain::write::OutboxStatus::Failed);
    assert((*record)->operation.optimisticCast->text == "durable hello");
    assert((*record)->lastFailure);
    assert((*record)->lastFailure->kind == domain::write::FailureKind::ProtocolRejected);
}

void removeCastPersistsLocalTombstoneAcrossFailureAndReopen()
{
    DatabaseFixture database{};
    const auto remove = removeOperation();
    {
        auto store = storage::write::openOutboxStore(database.path);
        assert(store);
        auto queued = (*store)->enqueue(remove, timestamp(1'000));
        assert(queued);
        assert(queued->operation.kind == domain::write::OperationKind::RemoveCast);
        assert(hasLocalCastTombstone(database.path, remove.target.cast->id.value));

        assert((*store)->markFailed(
            remove.idempotencyKey,
            domain::write::OutboxFailure{
                .kind = domain::write::FailureKind::ProtocolRejected,
                .message = "Canonical protocol write was rejected or malformed.",
            },
            timestamp(1'100)));
    }

    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto record = (*reopened)->find(remove.idempotencyKey);
    assert(record && *record);
    assert((*record)->status == domain::write::OutboxStatus::Failed);
    assert(hasLocalCastTombstone(database.path, remove.target.cast->id.value));
}

void confirmationClearsFailureAndPersists()
{
    DatabaseFixture database{};
    const auto original = operation();
    auto store = storage::write::openOutboxStore(database.path);
    assert(store);
    assert((*store)->enqueue(original, timestamp(1'000)));
    assert((*store)->beginAttempt(original.idempotencyKey, timestamp(1'050)));
    assert((*store)->markRetry(
        original.idempotencyKey,
        domain::write::OutboxFailure{
            .kind = domain::write::FailureKind::Unavailable,
            .message = "Protocol write endpoint is temporarily unavailable.",
        },
        timestamp(1'100)));
    assert((*store)->markConfirmed(original.idempotencyKey, timestamp(1'200)));
    auto confirmed = (*store)->find(original.idempotencyKey);
    assert(confirmed && *confirmed);
    assert((*confirmed)->status == domain::write::OutboxStatus::Confirmed);
    assert(!(*confirmed)->lastFailure);
    assert((*confirmed)->confirmedAt == timestamp(1'200));
}

void invalidIdentityNeverEnqueues()
{
    DatabaseFixture database{};
    auto store = storage::write::openOutboxStore(database.path);
    assert(store);
    auto invalid = operation();
    invalid.idempotencyKey.value = "0xdeadbeef";
    auto result = (*store)->enqueue(invalid, timestamp(1'000));
    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::InvalidArgument);
    auto absent = (*store)->find(invalid.idempotencyKey);
    assert(absent && !*absent);
}

} // namespace

int main()
{
    durableRoundTripAndDuplicateIdempotency();
    castProjectionsSurviveRestartAndRetainRetryStatus();
    socialTargetsSurviveStoreReopen();
    reactionProjectionSurvivesRefreshReopenAndOldDuplicate();
    followingProjectionSurvivesRefreshReopenAndOldDuplicate();
    reactionProjectionBackfillsExistingOutbox();
    followingProjectionBackfillsExistingOutbox();
    replyParentSurvivesStoreReopen();
    retrySchedulingIsDurableAndDueOrdered();
    readyListIsAccountScoped();
    terminalFailureRetainsOptimisticProjection();
    removeCastPersistsLocalTombstoneAcrossFailureAndReopen();
    confirmationClearsFailureAndPersists();
    invalidIdentityNeverEnqueues();
    return 0;
}
