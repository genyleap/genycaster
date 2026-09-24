#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.time;
import farcaster.protocol.hash;
import farcaster.protocol.key_authorization;
import farcaster.protocol.message;

namespace {

using namespace farcaster;

[[nodiscard]] std::string hex(const std::string_view bytes)
{
    constexpr std::string_view digits{"0123456789abcdef"};
    std::string output{};
    output.reserve(bytes.size() * 2U);
    for (const char rawValue : bytes) {
        const auto value = static_cast<unsigned char>(rawValue);
        output.push_back(digits[value >> 4U]);
        output.push_back(digits[value & 0x0FU]);
    }
    return output;
}

[[nodiscard]] std::string hex(const protocol::hash::Hash20& bytes)
{
    std::string raw{};
    raw.reserve(bytes.size());
    for (const auto value : bytes) {
        raw.push_back(static_cast<char>(std::to_integer<unsigned char>(value)));
    }
    return hex(raw);
}

[[nodiscard]] std::vector<std::byte> blakeVectorInput(const std::size_t size)
{
    std::vector<std::byte> input(size);
    for (std::size_t index{}; index < size; ++index) {
        input[index] = static_cast<std::byte>(index % 251U);
    }
    return input;
}

void testBlake3OfficialVectors()
{
    struct Vector final {
        std::size_t size{};
        std::string_view hash20{};
    };

    constexpr std::array vectors{
        Vector{0U, "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9"},
        Vector{1U, "2d3adedff11b61f14c886e35afa036736dcd87a7"},
        Vector{1023U, "10108970eeda3eb932baac1428c7a2163b0e924c"},
        Vector{1024U, "42214739f095a406f3fc83deb889744ac00df831"},
        Vector{1025U, "d00278ae47eb27b34faecf67b4fe263f82d54129"},
        Vector{2048U, "e776b6028c7cd22a4d0ba182a8bf62205d2ef576"},
        Vector{2049U, "5f4d72f40d7a5f82b15ca2b2e44b1de3c2ef86c4"},
    };

    for (const auto& vector : vectors) {
        const auto input = blakeVectorInput(vector.size);
        assert(hex(protocol::hash::blake3Digest20(input)) == vector.hash20);
    }
}

[[nodiscard]] domain::signer::PublicKey fixturePublicKey()
{
    return domain::signer::PublicKey{
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f"};
}

[[nodiscard]] std::string increasingBytes(const std::size_t size)
{
    std::string bytes(size, '\0');
    for (std::size_t index{}; index < size; ++index) {
        bytes[index] = static_cast<char>(index & 0xFFU);
    }
    return bytes;
}


void testCastAddProtobufFixture()
{
    const protocol::message::CastAddInput input{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
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
        .timestamp = 123'000U,
        .network = protocol::message::Network::Mainnet,
    };

    const auto result = protocol::message::encodeCastAdd(input);
    assert(result);
    assert(hex(result->dataBytes) ==
        "0801102a18f8c00720012a3a220568656c6c6f32150a1368747470733a2f2f6578616d706c652e636f6d321a121808071214000102030405060708090a0b0c0d0e0f10111213");
    assert(result->hashBytes.size() == 20U);
    assert(result->signer.hex == fixturePublicKey().hex);
}


void testReplyCastAddProtobufFixture()
{
    const protocol::message::CastAddInput input{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .text = "hello",
        .parent = domain::cast::CastReference{
            .author = domain::identity::Fid{7U},
            .id = domain::cast::CastId{
                "0x000102030405060708090a0b0c0d0e0f10111213"},
        },
        .timestamp = 123'000U,
        .network = protocol::message::Network::Mainnet,
    };

    const auto result = protocol::message::encodeCastAdd(input);
    assert(result);
    assert(hex(result->dataBytes) ==
        "0801102a18f8c00720012a211a1808071214000102030405060708090a0b0c0d0e0f10111213220568656c6c6f");
    assert(result->hashBytes.size() == 20U);
}

void testCastRejectsMalformedUtf8()
{
    const protocol::message::CastAddInput input{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .text = std::string{"\xF0\x28\x8C\x28", 4},
        .timestamp = 123U,
        .network = protocol::message::Network::Mainnet,
    };
    assert(!protocol::message::encodeCastAdd(input));
}

void testLongCastEncodesTypeDiscriminant()
{
    protocol::message::CastAddInput input{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .text = std::string(321U, 'L'),
        .type = protocol::message::CastType::LongCast,
        .timestamp = 123U,
        .network = protocol::message::Network::Mainnet,
    };
    const auto encoded = protocol::message::encodeCastAdd(input);
    assert(encoded);
    const std::string typeField{"\x40\x01", 2U};
    assert(encoded->dataBytes.find(typeField) != std::string::npos);
}

void testCastAddFailsClosed()
{
    auto input = protocol::message::CastAddInput{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .text = "hello",
        .timestamp = 123U,
        .network = protocol::message::Network::Mainnet,
    };

    input.text.clear();
    assert(!protocol::message::encodeCastAdd(input));

    input.text = std::string(321U, 'a');
    assert(!protocol::message::encodeCastAdd(input));

    input.type = protocol::message::CastType::LongCast;
    assert(protocol::message::encodeCastAdd(input));
    input.text = std::string(1024U, 'a');
    assert(protocol::message::encodeCastAdd(input));
    input.text.push_back('a');
    assert(!protocol::message::encodeCastAdd(input));
    input.text = "short";
    assert(!protocol::message::encodeCastAdd(input));

    input.type = protocol::message::CastType::Cast;
    input.text = "hello";
    input.mentions = {domain::identity::Fid{7U}};
    input.mentionPositions = {2U};
    assert(protocol::message::encodeCastAdd(input));
    input.mentionPositions.clear();
    assert(!protocol::message::encodeCastAdd(input));
    input.mentionPositions = {6U};
    assert(!protocol::message::encodeCastAdd(input));
    input.mentionPositions = {2U};
    input.mentions = {domain::identity::Fid{}};
    assert(!protocol::message::encodeCastAdd(input));
    input.mentions.clear();
    input.mentionPositions.clear();

    input.text = "hello";
    input.embeds = {
        domain::cast::Embed{.kind = domain::cast::EmbedKind::Url, .value = "https://one.example"},
        domain::cast::Embed{.kind = domain::cast::EmbedKind::Url, .value = "https://two.example"},
        domain::cast::Embed{.kind = domain::cast::EmbedKind::Url, .value = "https://three.example"},
    };
    assert(!protocol::message::encodeCastAdd(input));

    input.embeds = {domain::cast::Embed{
        .kind = domain::cast::EmbedKind::Url,
        .value = std::string(257U, 'u'),
    }};
    assert(!protocol::message::encodeCastAdd(input));

    input.embeds = {domain::cast::Embed{
        .kind = domain::cast::EmbedKind::CastReference,
        .value = "0x000102030405060708090a0b0c0d0e0f10111213",
    }};
    assert(!protocol::message::encodeCastAdd(input));

    input.embeds = {domain::cast::Embed{
        .kind = domain::cast::EmbedKind::CastReference,
        .value = "0xdeadbeef",
        .referencedAuthor = domain::identity::Fid{7U},
    }};
    assert(!protocol::message::encodeCastAdd(input));

    input.embeds.clear();
    input.parent = domain::cast::CastReference{
        .author = domain::identity::Fid{},
        .id = domain::cast::CastId{
            "0x000102030405060708090a0b0c0d0e0f10111213"},
    };
    assert(!protocol::message::encodeCastAdd(input));

    input.parent = domain::cast::CastReference{
        .author = domain::identity::Fid{7U},
        .id = domain::cast::CastId{"0xdeadbeef"},
    };
    assert(!protocol::message::encodeCastAdd(input));

    input.parent.reset();
    input.network = static_cast<protocol::message::Network>(99U);
    assert(!protocol::message::encodeCastAdd(input));
}

void testKeyAddProtobufFixture()
{
    const protocol::message::KeyAddInput input{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .scopes = {
            domain::signer::Scope::CastAdd,
            domain::signer::Scope::CastRemove,
            domain::signer::Scope::ReactionAdd,
            domain::signer::Scope::ReactionRemove,
            domain::signer::Scope::LinkAdd,
            domain::signer::Scope::LinkRemove,
        },
        .ttlSeconds = 2'592'000U,
        .nonce = 7U,
        .deadline = 123'456U,
        .timestamp = 123'000U,
        .network = protocol::message::Network::Mainnet,
        .custodySignature = increasingBytes(65U),
        .signedKeyRequestMetadata = increasingBytes(96U),
    };

    const auto result = protocol::message::encodeKeyAdd(input);
    assert(result);
    assert(hex(result->dataBytes) ==
        "0810102a18f8c00720019a01de010a20000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f10011a41000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f4020c0c40728073260000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f38014a0601020304050650809a9e01");
    assert(result->hashBytes.size() == 20U);
}

void testKeyRemoveSelfRevokeProtobufFixture()
{
    const protocol::message::KeyRemoveSelfRevokeInput input{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .nonce = 9U,
        .deadline = 123'456U,
        .timestamp = 123'001U,
        .network = protocol::message::Network::Mainnet,
    };

    const auto result = protocol::message::encodeKeyRemoveSelfRevoke(input);
    assert(result);
    assert(hex(result->dataBytes) ==
        "0811102a18f9c0072001a2012a0a20000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f180220c0c4072809");
}

void testEnvelopeProtobufFixture()
{
    protocol::message::UnsignedMessage message{
        .dataBytes = {},
        .hashBytes = increasingBytes(20U),
        .signer = fixturePublicKey(),
    };

    const auto keyAdd = protocol::message::encodeKeyAdd(protocol::message::KeyAddInput{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .scopes = {
            domain::signer::Scope::CastAdd,
            domain::signer::Scope::CastRemove,
            domain::signer::Scope::ReactionAdd,
            domain::signer::Scope::ReactionRemove,
            domain::signer::Scope::LinkAdd,
            domain::signer::Scope::LinkRemove,
        },
        .ttlSeconds = 2'592'000U,
        .nonce = 7U,
        .deadline = 123'456U,
        .timestamp = 123'000U,
        .network = protocol::message::Network::Mainnet,
        .custodySignature = increasingBytes(65U),
        .signedKeyRequestMetadata = increasingBytes(96U),
    });
    assert(keyAdd);
    message.dataBytes = keyAdd->dataBytes;

    const domain::signer::Signature signature{std::string(64U, static_cast<char>(0xAA))};
    const auto sealed = protocol::message::seal(message, signature);
    assert(sealed);
    assert(hex(*sealed) ==
        "1214000102030405060708090a0b0c0d0e0f1011121318012240aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa28013220000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f3aec010810102a18f8c00720019a01de010a20000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f10011a41000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f4020c0c40728073260000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f38014a0601020304050650809a9e01");
}

void testFarcasterTimestampConversion()
{
    using namespace std::chrono;
    const auto timestamp = foundation::time::Timestamp{
        duration_cast<milliseconds>(seconds{1'609'459'323})};
    const auto converted = protocol::message::toFarcasterTimestamp(timestamp);
    assert(converted && *converted == 123U);
    assert(protocol::message::fromFarcasterTimestamp(*converted) == timestamp);

    const auto beforeEpoch = foundation::time::Timestamp{
        duration_cast<milliseconds>(seconds{1'609'459'199})};
    assert(!protocol::message::toFarcasterTimestamp(beforeEpoch));
}

void testKeyAddFailsClosed()
{
    auto input = protocol::message::KeyAddInput{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .scopes = {domain::signer::Scope::CastAdd},
        .ttlSeconds = 3'600U,
        .nonce = 1U,
        .deadline = 200U,
        .timestamp = 100U,
        .network = protocol::message::Network::Mainnet,
        .custodySignature = increasingBytes(65U),
        .signedKeyRequestMetadata = increasingBytes(96U),
    };

    input.scopes.push_back(domain::signer::Scope::CastAdd);
    assert(!protocol::message::encodeKeyAdd(input));

    input.scopes = {domain::signer::Scope::CastAdd};
    input.custodySignature.resize(64U);
    assert(!protocol::message::encodeKeyAdd(input));

    input.custodySignature = increasingBytes(65U);
    input.signedKeyRequestMetadata.resize(95U);
    assert(!protocol::message::encodeKeyAdd(input));

    input.signedKeyRequestMetadata = increasingBytes(96U);
    input.deadline = 99U;
    assert(!protocol::message::encodeKeyAdd(input));
}


void testKeyAuthorizationTypedDataFixture()
{
    const protocol::key_authorization::KeyAddInput input{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .scopes = {
            domain::signer::Scope::CastAdd,
            domain::signer::Scope::CastRemove,
        },
        .ttlSeconds = 3'600U,
        .nonce = 7U,
        .deadline = 123'456U,
        .requestFid = domain::identity::Fid{43U},
    };

    const auto documents = protocol::key_authorization::buildTypedData(input);
    assert(documents);
    assert(documents->signedKeyRequestJson ==
        R"json({"types":{"EIP712Domain":[{"name":"name","type":"string"},{"name":"version","type":"string"},{"name":"chainId","type":"uint256"}],"SignedKeyRequest":[{"name":"requestFid","type":"uint256"},{"name":"key","type":"bytes"},{"name":"deadline","type":"uint256"}]},"primaryType":"SignedKeyRequest","domain":{"name":"Farcaster KeyAdd","version":"1","chainId":1},"message":{"requestFid":"43","key":"0x000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f","deadline":"123456"}})json");
    assert(documents->keyAddJson ==
        R"json({"types":{"EIP712Domain":[{"name":"name","type":"string"},{"name":"version","type":"string"},{"name":"chainId","type":"uint256"}],"KeyAdd":[{"name":"fid","type":"uint256"},{"name":"key","type":"bytes"},{"name":"keyType","type":"uint32"},{"name":"scopes","type":"uint32[]"},{"name":"ttl","type":"uint32"},{"name":"nonce","type":"uint32"},{"name":"deadline","type":"uint256"}]},"primaryType":"KeyAdd","domain":{"name":"Farcaster KeyAdd","version":"1","chainId":1},"message":{"fid":"42","key":"0x000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f","keyType":1,"scopes":[1,2],"ttl":3600,"nonce":7,"deadline":"123456"}})json");
}

void testSignedKeyRequestMetadataAbiFixture()
{
    const auto metadata = protocol::key_authorization::encodeSignedKeyRequestMetadata(
        domain::identity::Fid{43U},
        increasingBytes(20U),
        increasingBytes(65U),
        123'456U);
    assert(metadata);
    assert(metadata->size() == 288U);
    assert(hex(*metadata) ==
        // Top-level ABI argument offset: SignedKeyRequestMetadata is a dynamic tuple.
        "0000000000000000000000000000000000000000000000000000000000000020"
        "000000000000000000000000000000000000000000000000000000000000002b"
        "000000000000000000000000000102030405060708090a0b0c0d0e0f10111213"
        "0000000000000000000000000000000000000000000000000000000000000080"
        "000000000000000000000000000000000000000000000000000000000001e240"
        "0000000000000000000000000000000000000000000000000000000000000041"
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
        "4000000000000000000000000000000000000000000000000000000000000000");
}

void testKeyAuthorizationFailsClosed()
{
    auto input = protocol::key_authorization::KeyAddInput{
        .fid = domain::identity::Fid{42U},
        .publicKey = fixturePublicKey(),
        .scopes = {domain::signer::Scope::CastAdd},
        .ttlSeconds = 3'600U,
        .nonce = 1U,
        .deadline = 100U,
        .requestFid = domain::identity::Fid{42U},
    };
    input.scopes.push_back(domain::signer::Scope::CastAdd);
    assert(!protocol::key_authorization::buildTypedData(input));
    input.scopes = {domain::signer::Scope::CastAdd};
    input.requestFid = domain::identity::Fid{};
    assert(!protocol::key_authorization::buildTypedData(input));

    assert(!protocol::key_authorization::encodeSignedKeyRequestMetadata(
        domain::identity::Fid{42U}, std::string(19U, '\0'), increasingBytes(65U), 100U));
    assert(!protocol::key_authorization::encodeSignedKeyRequestMetadata(
        domain::identity::Fid{42U}, increasingBytes(20U), std::string(64U, '\0'), 100U));
}

void testSealRejectsWrongSignatureLength()
{
    protocol::message::UnsignedMessage message{
        .dataBytes = "data",
        .hashBytes = std::string(20U, '\0'),
        .signer = fixturePublicKey(),
    };
    assert(!protocol::message::seal(message, domain::signer::Signature{std::string(63U, '\0')}));
}

}

int main()
{
    testBlake3OfficialVectors();
    testCastAddProtobufFixture();
    testReplyCastAddProtobufFixture();
    testCastRejectsMalformedUtf8();
    testLongCastEncodesTypeDiscriminant();
    testCastAddFailsClosed();
    testKeyAddProtobufFixture();
    testKeyRemoveSelfRevokeProtobufFixture();
    testEnvelopeProtobufFixture();
    testFarcasterTimestampConversion();
    testKeyAddFailsClosed();
    testKeyAuthorizationTypedDataFixture();
    testSignedKeyRequestMetadataAbiFixture();
    testKeyAuthorizationFailsClosed();
    testSealRejectsWrongSignatureLength();
    return 0;
}
