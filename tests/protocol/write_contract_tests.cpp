#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

import farcaster.capability.write;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.time;
import farcaster.protocol.write;

namespace {

namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace protocol = farcaster::protocol;

[[nodiscard]] std::string hex(const std::string_view bytes)
{
    constexpr std::string_view digits{"0123456789abcdef"};
    std::string output{"0x"};
    output.reserve(2U + bytes.size() * 2U);
    for (const char raw : bytes) {
        const auto value = static_cast<unsigned char>(raw);
        output.push_back(digits[value >> 4U]);
        output.push_back(digits[value & 0x0FU]);
    }
    return output;
}

[[nodiscard]] foundation::time::Timestamp fixtureTime()
{
    using namespace std::chrono;
    return foundation::time::Timestamp{
        duration_cast<milliseconds>(seconds{1'609'459'200LL + 123'000LL})};
}

void canonicalIdentityMatchesProtocolHash()
{
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto prepared = codec->preparePublishCast(
        domain::write::PublishCastIntent{
            .author = domain::identity::Fid{42U},
            .text = "hello",
            .embeds = {
                domain::cast::Embed{
                    .kind = domain::cast::EmbedKind::Url,
                    .value = "https://example.com",
                },
                domain::cast::Embed{
                    .kind = domain::cast::EmbedKind::CastReference,
                    .value = "0x000102030405060708090a0b0c0d0e0f10111213",
                    .referencedAuthor = domain::identity::Fid{7U},
                },
            },
            .createdAt = fixtureTime(),
        },
        domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"});
    assert(prepared);
    assert(prepared->hashBytes.size() == 20U);
    assert(prepared->idempotencyKey.value == hex(prepared->hashBytes));
    assert(prepared->optimisticCast->id.value == prepared->idempotencyKey.value);
    assert(prepared->optimisticCast->author.value == 42U);
    assert(prepared->optimisticCast->text == "hello");
    assert(prepared->optimisticCast->embeds.size() == 2U);
    assert(prepared->dataBytes.size() == 70U);
}


void replyIdentityUsesCanonicalParent()
{
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto prepared = codec->preparePublishCast(
        domain::write::PublishCastIntent{
            .author = domain::identity::Fid{42U},
            .text = "reply",
            .parent = domain::cast::CastReference{
                .author = domain::identity::Fid{7U},
                .id = domain::cast::CastId{
                    "0x000102030405060708090a0b0c0d0e0f10111213"},
            },
            .createdAt = fixtureTime(),
        },
        domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"});
    assert(prepared);
    assert(prepared->optimisticCast->parent);
    assert(prepared->optimisticCast->parent->value ==
        "0x000102030405060708090a0b0c0d0e0f10111213");
    assert(prepared->idempotencyKey.value == hex(prepared->hashBytes));
    assert(prepared->optimisticCast->id.value == prepared->idempotencyKey.value);
}

void socialOperationsUseCanonicalTargetsAndKinds()
{
    auto codec = protocol::write::makeCanonicalWriteCodec();
    const domain::signer::PublicKey key{
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"};
    const domain::cast::CastReference target{
        .author = domain::identity::Fid{7U},
        .id = domain::cast::CastId{
            "0x000102030405060708090a0b0c0d0e0f10111213"},
    };

    auto like = codec->prepareReaction(
        domain::write::ReactionIntent{
            .author = domain::identity::Fid{42U},
            .target = target,
            .reaction = domain::write::ReactionKind::Like,
            .isActive = true,
            .createdAt = fixtureTime(),
        },
        key);
    assert(like);
    assert(like->kind == domain::write::OperationKind::Like);
    assert(like->author.value == 42U);
    assert(like->target.cast);
    assert(like->target.cast->author.value == 7U);
    assert(like->target.cast->id.value == target.id.value);
    assert(!like->optimisticCast);
    assert(like->hashBytes.size() == 20U);
    assert(like->idempotencyKey.value == hex(like->hashBytes));

    auto unlike = codec->prepareReaction(
        domain::write::ReactionIntent{
            .author = domain::identity::Fid{42U},
            .target = target,
            .reaction = domain::write::ReactionKind::Like,
            .isActive = false,
            .createdAt = fixtureTime(),
        },
        key);
    assert(unlike && unlike->kind == domain::write::OperationKind::Unlike);

    auto recast = codec->prepareReaction(
        domain::write::ReactionIntent{
            .author = domain::identity::Fid{42U},
            .target = target,
            .reaction = domain::write::ReactionKind::Recast,
            .isActive = true,
            .createdAt = fixtureTime(),
        },
        key);
    assert(recast && recast->kind == domain::write::OperationKind::Recast);

    auto undoRecast = codec->prepareReaction(
        domain::write::ReactionIntent{
            .author = domain::identity::Fid{42U},
            .target = target,
            .reaction = domain::write::ReactionKind::Recast,
            .isActive = false,
            .createdAt = fixtureTime(),
        },
        key);
    assert(undoRecast && undoRecast->kind == domain::write::OperationKind::UndoRecast);

    auto follow = codec->prepareFollow(
        domain::write::FollowIntent{
            .author = domain::identity::Fid{42U},
            .target = domain::identity::Fid{99U},
            .isFollowing = true,
            .createdAt = fixtureTime(),
        },
        key);
    assert(follow);
    assert(follow->kind == domain::write::OperationKind::Follow);
    assert(follow->target.fid && follow->target.fid->value == 99U);
    assert(!follow->optimisticCast);

    auto unfollow = codec->prepareFollow(
        domain::write::FollowIntent{
            .author = domain::identity::Fid{42U},
            .target = domain::identity::Fid{99U},
            .isFollowing = false,
            .createdAt = fixtureTime(),
        },
        key);
    assert(unfollow && unfollow->kind == domain::write::OperationKind::Unfollow);

    auto sealed = codec->seal(*like, domain::signer::Signature{std::string(64U, 's')});
    assert(sealed);
    assert(sealed->kind == domain::write::OperationKind::Like);
    assert(sealed->author.value == 42U);
    assert(sealed->target.cast && sealed->target.cast->id.value == target.id.value);
    assert(!sealed->messageBytes.empty());
}

void removeCastRequiresTheAuthenticatedOwner()
{
    auto codec = protocol::write::makeCanonicalWriteCodec();
    const domain::signer::PublicKey key{
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"};
    const domain::cast::CastReference owned{
        .author = domain::identity::Fid{42U},
        .id = domain::cast::CastId{
            "0x000102030405060708090a0b0c0d0e0f10111213"},
    };
    auto prepared = codec->prepareRemoveCast(
        domain::write::RemoveCastIntent{
            .author = domain::identity::Fid{42U},
            .target = owned,
            .createdAt = fixtureTime(),
        },
        key);
    assert(prepared);
    assert(prepared->kind == domain::write::OperationKind::RemoveCast);
    assert(prepared->target.cast && prepared->target.cast->author.value == 42U);
    assert(!prepared->optimisticCast);

    auto denied = codec->prepareRemoveCast(
        domain::write::RemoveCastIntent{
            .author = domain::identity::Fid{42U},
            .target = domain::cast::CastReference{
                .author = domain::identity::Fid{7U},
                .id = owned.id,
            },
            .createdAt = fixtureTime(),
        },
        key);
    assert(!denied);
}

void invalidReplyParentFailsBeforeSigning()
{
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto prepared = codec->preparePublishCast(
        domain::write::PublishCastIntent{
            .author = domain::identity::Fid{42U},
            .text = "reply",
            .parent = domain::cast::CastReference{
                .author = domain::identity::Fid{7U},
                .id = domain::cast::CastId{"0xdeadbeef"},
            },
            .createdAt = fixtureTime(),
        },
        domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"});
    assert(!prepared);
}

void sealingRetainsCanonicalIdentity()
{
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto prepared = codec->preparePublishCast(
        domain::write::PublishCastIntent{
            .author = domain::identity::Fid{42U},
            .text = "hello",
            .createdAt = fixtureTime(),
        },
        domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"});
    assert(prepared);
    auto sealed = codec->seal(
        *prepared,
        domain::signer::Signature{std::string(64U, static_cast<char>(0x5A))});
    assert(sealed);
    assert(sealed->idempotencyKey.value == prepared->idempotencyKey.value);
    assert(sealed->optimisticCast->id.value == prepared->optimisticCast->id.value);
    assert(sealed->signerPublicKey.hex == prepared->signerPublicKey.hex);
    assert(!sealed->messageBytes.empty());
}

void longCastIntentUsesCanonicalLongCast()
{
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto prepared = codec->preparePublishCast(
        domain::write::PublishCastIntent{
            .author = domain::identity::Fid{42U},
            .text = std::string(321U, 'L'),
            .createdAt = fixtureTime(),
        },
        domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"});
    assert(prepared);
    assert(prepared->optimisticCast);
    assert(prepared->optimisticCast->text.size() == 321U);
    const std::string typeField{"\x40\x01", 2U};
    assert(prepared->dataBytes.find(typeField) != std::string::npos);
}

void invalidIntentFailsBeforeSigning()
{
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto prepared = codec->preparePublishCast(
        domain::write::PublishCastIntent{
            .author = domain::identity::Fid{42U},
            .text = std::string(1025U, 'x'),
            .createdAt = fixtureTime(),
        },
        domain::signer::PublicKey{
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"});
    assert(!prepared);
}

} // namespace

int main()
{
    canonicalIdentityMatchesProtocolHash();
    socialOperationsUseCanonicalTargetsAndKinds();
    removeCastRequiresTheAuthenticatedOwner();
    replyIdentityUsesCanonicalParent();
    sealingRetainsCanonicalIdentity();
    longCastIntentUsesCanonicalLongCast();
    invalidIntentFailsBeforeSigning();
    invalidReplyParentFailsBeforeSigning();
    return 0;
}
