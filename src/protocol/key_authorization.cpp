module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

module farcaster.protocol.key_authorization;

import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::protocol::key_authorization {
namespace {

constexpr std::uint32_t KeyTypeEd25519 = 1U;
constexpr std::size_t PublicKeyHexSize = 64U;
constexpr std::size_t EthereumAddressSize = 20U;
constexpr std::size_t EcdsaSignatureSize = 65U;
constexpr std::size_t AbiWordSize = 32U;
constexpr std::uint64_t MetadataHeadSize = 4U * AbiWordSize;

[[nodiscard]] foundation::error::Error invalid(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::InvalidArgument,
        std::move(message));
}

[[nodiscard]] bool isLowerHex(const std::string_view value)
{
    return std::all_of(value.begin(), value.end(), [](const char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    });
}

[[nodiscard]] foundation::result::Status validate(const KeyAddInput& input)
{
    if (input.fid.value == 0U || input.requestFid.value == 0U) {
        return std::unexpected(invalid("KEY_ADD EIP-712 authorization requires non-zero FIDs."));
    }
    if (input.publicKey.hex.size() != PublicKeyHexSize || !isLowerHex(input.publicKey.hex)) {
        return std::unexpected(invalid("KEY_ADD EIP-712 authorization requires a lowercase 32-byte Ed25519 key."));
    }
    if (input.scopes.empty() || input.scopes.size() > 16U) {
        return std::unexpected(invalid("KEY_ADD EIP-712 authorization requires 1..16 explicit scopes."));
    }
    if (!std::all_of(input.scopes.begin(), input.scopes.end(), [](const auto scope) {
            return domain::signer::isAdmissibleScope(scope);
        })) {
        return std::unexpected(invalid("KEY_ADD EIP-712 authorization contains a non-admissible scope."));
    }
    auto sorted = input.scopes;
    std::ranges::sort(sorted, {}, [](const auto scope) {
        return static_cast<std::int32_t>(scope);
    });
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
        return std::unexpected(invalid("KEY_ADD EIP-712 authorization must not contain duplicate scopes."));
    }
    if (input.ttlSeconds == 0U || input.ttlSeconds > domain::signer::MaxGaslessTtlSeconds) {
        return std::unexpected(invalid("KEY_ADD EIP-712 authorization TTL must be within the protocol 90-day bound."));
    }
    if (input.nonce == 0U) {
        return std::unexpected(invalid("KEY_ADD EIP-712 authorization nonce must be positive."));
    }
    if (input.deadline == 0U) {
        return std::unexpected(invalid("KEY_ADD EIP-712 authorization deadline must be positive."));
    }
    return {};
}

[[nodiscard]] std::string scopesJson(const std::vector<domain::signer::Scope>& scopes)
{
    std::string result{"["};
    for (std::size_t index{}; index < scopes.size(); ++index) {
        if (index != 0U) {
            result += ',';
        }
        result += std::to_string(static_cast<std::uint32_t>(scopes[index]));
    }
    result += ']';
    return result;
}

[[nodiscard]] std::string domainJson()
{
    return R"json({"name":"Farcaster KeyAdd","version":"1","chainId":1})json";
}

[[nodiscard]] std::string signedKeyRequestJson(const KeyAddInput& input)
{
    std::string json = R"json({"types":{"EIP712Domain":[{"name":"name","type":"string"},{"name":"version","type":"string"},{"name":"chainId","type":"uint256"}],"SignedKeyRequest":[{"name":"requestFid","type":"uint256"},{"name":"key","type":"bytes"},{"name":"deadline","type":"uint256"}]},"primaryType":"SignedKeyRequest","domain":)json";
    json += domainJson();
    json += R"json(,"message":{"requestFid":")json";
    json += std::to_string(input.requestFid.value);
    json += R"json(","key":"0x)json";
    json += input.publicKey.hex;
    json += R"json(","deadline":")json";
    json += std::to_string(input.deadline);
    json += R"json("}})json";
    return json;
}

[[nodiscard]] std::string keyAddJson(const KeyAddInput& input)
{
    std::string json = R"json({"types":{"EIP712Domain":[{"name":"name","type":"string"},{"name":"version","type":"string"},{"name":"chainId","type":"uint256"}],"KeyAdd":[{"name":"fid","type":"uint256"},{"name":"key","type":"bytes"},{"name":"keyType","type":"uint32"},{"name":"scopes","type":"uint32[]"},{"name":"ttl","type":"uint32"},{"name":"nonce","type":"uint32"},{"name":"deadline","type":"uint256"}]},"primaryType":"KeyAdd","domain":)json";
    json += domainJson();
    json += R"json(,"message":{"fid":")json";
    json += std::to_string(input.fid.value);
    json += R"json(","key":"0x)json";
    json += input.publicKey.hex;
    json += R"json(","keyType":)json";
    json += std::to_string(KeyTypeEd25519);
    json += R"json(,"scopes":)json";
    json += scopesJson(input.scopes);
    json += R"json(,"ttl":)json";
    json += std::to_string(input.ttlSeconds);
    json += R"json(,"nonce":)json";
    json += std::to_string(input.nonce);
    json += R"json(,"deadline":")json";
    json += std::to_string(input.deadline);
    json += R"json("}})json";
    return json;
}

void appendUint256(std::string& output, std::uint64_t value)
{
    const auto start = output.size();
    output.append(AbiWordSize, '\0');
    for (std::size_t byte{}; byte < sizeof(value); ++byte) {
        output[start + AbiWordSize - 1U - byte] = static_cast<char>(value & 0xFFU);
        value >>= 8U;
    }
}

void appendAddress(std::string& output, const std::string& address)
{
    output.append(AbiWordSize - EthereumAddressSize, '\0');
    output.append(address);
}

void appendDynamicBytes(std::string& output, const std::string& bytes)
{
    appendUint256(output, bytes.size());
    output.append(bytes);
    const auto remainder = bytes.size() % AbiWordSize;
    if (remainder != 0U) {
        output.append(AbiWordSize - remainder, '\0');
    }
}

}

foundation::result::Result<TypedDataDocuments> buildTypedData(const KeyAddInput& input)
{
    if (auto status = validate(input); !status) {
        return std::unexpected(std::move(status.error()));
    }
    return TypedDataDocuments{
        .signedKeyRequestJson = signedKeyRequestJson(input),
        .keyAddJson = keyAddJson(input),
    };
}

foundation::result::Result<std::string> encodeSignedKeyRequestMetadata(
    const domain::identity::Fid requestFid,
    const std::string& requestSignerAddress,
    const std::string& signedKeyRequestSignature,
    const std::uint32_t deadline)
{
    if (requestFid.value == 0U) {
        return std::unexpected(invalid("SignedKeyRequest metadata requires a non-zero request FID."));
    }
    if (requestSignerAddress.size() != EthereumAddressSize) {
        return std::unexpected(invalid("SignedKeyRequest metadata requires a raw 20-byte Ethereum address."));
    }
    if (signedKeyRequestSignature.size() != EcdsaSignatureSize) {
        return std::unexpected(invalid("SignedKeyRequest metadata requires a raw 65-byte ECDSA signature."));
    }
    if (deadline == 0U) {
        return std::unexpected(invalid("SignedKeyRequest metadata requires a positive deadline."));
    }

    std::string encoded{};
    // `metadata` is ABI-encoding of one dynamic tuple argument, matching
    // Solidity `abi.encode(SignedKeyRequestMetadata(...))` and the official
    // SDK's `encodeAbiParameters([{ type: "tuple", ... }], [metadata])`.
    // Because the tuple contains dynamic `bytes`, the top-level argument head
    // is an offset to the tuple payload. Omitting this 0x20 word produces a
    // structurally plausible 32-byte-aligned blob that Snapchain rejects before
    // consuming the user nonce.
    encoded.reserve(AbiWordSize + MetadataHeadSize + AbiWordSize * 4U);
    appendUint256(encoded, AbiWordSize);
    appendUint256(encoded, requestFid.value);
    appendAddress(encoded, requestSignerAddress);
    appendUint256(encoded, MetadataHeadSize);
    appendUint256(encoded, deadline);
    appendDynamicBytes(encoded, signedKeyRequestSignature);
    return encoded;
}

}
