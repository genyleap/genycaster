module;

#include "genycaster_deployment.hpp"

// The reviewed third-party BIP39 dictionary is a textual data boundary.
#include "custody_bip39_english.hpp"

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/core_names.h>
#include <openssl/ecdsa.h>
#include <openssl/param_build.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <bit>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <optional>
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <utility>

module farcaster.platform.custody_recovery;

import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.protocol.key_authorization;
import farcaster.protocol.message;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::platform::custody_recovery {
namespace {

template <std::size_t N>
struct Sensitive final {
    std::array<unsigned char, N> bytes{};
    ~Sensitive() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
    Sensitive() = default;
    Sensitive(const Sensitive&) = delete;
    Sensitive& operator=(const Sensitive&) = delete;
    Sensitive(Sensitive&& other) noexcept : bytes(other.bytes)
    {
        OPENSSL_cleanse(other.bytes.data(), other.bytes.size());
    }
    Sensitive& operator=(Sensitive&& other) noexcept
    {
        if (this != &other) {
            OPENSSL_cleanse(bytes.data(), bytes.size());
            bytes = other.bytes;
            OPENSSL_cleanse(other.bytes.data(), other.bytes.size());
        }
        return *this;
    }
};

struct SensitiveText final {
    std::string value{};
    SensitiveText() = default;
    SensitiveText(const SensitiveText&) = delete;
    SensitiveText& operator=(const SensitiveText&) = delete;
    SensitiveText(SensitiveText&& other) noexcept : value(std::move(other.value))
    {
        if (!other.value.empty()) {
            OPENSSL_cleanse(other.value.data(), other.value.size());
            other.value.clear();
        }
    }
    SensitiveText& operator=(SensitiveText&&) = delete;
    ~SensitiveText()
    {
        if (!value.empty()) OPENSSL_cleanse(value.data(), value.size());
    }
};

struct ClearBn final {
    void operator()(BIGNUM* p) const noexcept { BN_clear_free(p); }
};
struct FreeCtx final {
    void operator()(BN_CTX* p) const noexcept { BN_CTX_free(p); }
};
struct FreeGroup final {
    void operator()(EC_GROUP* p) const noexcept { EC_GROUP_free(p); }
};
struct FreePoint final {
    void operator()(EC_POINT* p) const noexcept { EC_POINT_free(p); }
};
struct FreePkey final {
    void operator()(EVP_PKEY* p) const noexcept { EVP_PKEY_free(p); }
};
struct FreePkeyCtx final {
    void operator()(EVP_PKEY_CTX* p) const noexcept { EVP_PKEY_CTX_free(p); }
};
struct FreeParamBld final {
    void operator()(OSSL_PARAM_BLD* p) const noexcept { OSSL_PARAM_BLD_free(p); }
};
struct FreeParams final {
    void operator()(OSSL_PARAM* p) const noexcept { OSSL_PARAM_free(p); }
};
struct FreeSignature final {
    void operator()(ECDSA_SIG* p) const noexcept { ECDSA_SIG_free(p); }
};
using BigNumber = std::unique_ptr<BIGNUM, ClearBn>;
using Context = std::unique_ptr<BN_CTX, FreeCtx>;
using Group = std::unique_ptr<EC_GROUP, FreeGroup>;
using Point = std::unique_ptr<EC_POINT, FreePoint>;
using Pkey = std::unique_ptr<EVP_PKEY, FreePkey>;
using PkeyContext = std::unique_ptr<EVP_PKEY_CTX, FreePkeyCtx>;
using ParameterBuilder = std::unique_ptr<OSSL_PARAM_BLD, FreeParamBld>;
using Parameters = std::unique_ptr<OSSL_PARAM, FreeParams>;
using Signature = std::unique_ptr<ECDSA_SIG, FreeSignature>;

[[nodiscard]] bool hmac512(std::span<const unsigned char> key,
                           std::span<const unsigned char> message,
                           Sensitive<64>& output)
{
    std::size_t written{};
    return EVP_Q_mac(nullptr, "HMAC", nullptr, "SHA512", nullptr,
                     key.data(), key.size(), message.data(), message.size(),
                     output.bytes.data(), output.bytes.size(), &written) != nullptr
           && written == output.bytes.size();
}

[[nodiscard]] std::array<unsigned char, 32>
keccak256(const std::span<const unsigned char> bytes)
{
    // Ethereum Keccak-256 uses 0x01 domain padding, not FIPS SHA3's 0x06.
    constexpr std::array<unsigned, 25> rotations{
        0, 1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43,
        25, 39, 41, 45, 15, 21, 8, 18, 2, 61, 56, 14,
    };
    constexpr std::array<std::uint64_t, 24> constants{
        0x0000000000000001ULL, 0x0000000000008082ULL,
        0x800000000000808aULL, 0x8000000080008000ULL,
        0x000000000000808bULL, 0x0000000080000001ULL,
        0x8000000080008081ULL, 0x8000000000008009ULL,
        0x000000000000008aULL, 0x0000000000000088ULL,
        0x0000000080008009ULL, 0x000000008000000aULL,
        0x000000008000808bULL, 0x800000000000008bULL,
        0x8000000000008089ULL, 0x8000000000008003ULL,
        0x8000000000008002ULL, 0x8000000000000080ULL,
        0x000000000000800aULL, 0x800000008000000aULL,
        0x8000000080008081ULL, 0x8000000000008080ULL,
        0x0000000080000001ULL, 0x8000000080008008ULL,
    };
    std::array<std::uint64_t, 25> state{};
    const auto permute = [&] {
        for (const auto constant : constants) {
            std::array<std::uint64_t, 5> columns{};
            for (std::size_t x{}; x < 5; ++x)
                for (std::size_t y{}; y < 5; ++y)
                    columns[x] ^= state[x + 5 * y];
            for (std::size_t x{}; x < 5; ++x) {
                const auto theta = columns[(x + 4) % 5]
                                 ^ std::rotl(columns[(x + 1) % 5], 1);
                for (std::size_t y{}; y < 5; ++y) state[x + 5 * y] ^= theta;
            }
            std::array<std::uint64_t, 25> shifted{};
            for (std::size_t x{}; x < 5; ++x) {
                for (std::size_t y{}; y < 5; ++y) {
                    const auto index = x + 5 * y;
                    shifted[y + 5 * ((2 * x + 3 * y) % 5)] =
                        std::rotl(state[index], static_cast<int>(rotations[index]));
                }
            }
            for (std::size_t x{}; x < 5; ++x)
                for (std::size_t y{}; y < 5; ++y)
                    state[x + 5 * y] = shifted[x + 5 * y]
                        ^ (~shifted[(x + 1) % 5 + 5 * y]
                           & shifted[(x + 2) % 5 + 5 * y]);
            state[0] ^= constant;
        }
    };
    constexpr std::size_t rate = 136;
    const auto absorb = [&](std::span<const unsigned char> block) {
        for (std::size_t index{}; index < rate; ++index) {
            state[index / 8] ^= static_cast<std::uint64_t>(block[index])
                                << (8 * (index % 8));
        }
        permute();
    };
    std::size_t offset{};
    while (bytes.size() - offset >= rate) {
        absorb(bytes.subspan(offset, rate));
        offset += rate;
    }
    std::array<unsigned char, rate> tail{};
    std::copy(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
              bytes.end(), tail.begin());
    tail[bytes.size() - offset] = 0x01;
    tail.back() |= 0x80;
    absorb(tail);
    std::array<unsigned char, 32> result{};
    for (std::size_t index{}; index < result.size(); ++index)
        result[index] = static_cast<unsigned char>(state[index / 8]
                                                 >> (8 * (index % 8)));
    return result;
}

[[nodiscard]] std::expected<SensitiveText, Error>
validateMnemonic(const std::string_view phrase)
{
    if (phrase.empty() || phrase.size() > 4095)
        return std::unexpected(Error::InvalidPhrase);
    SensitiveText normalized{};
    normalized.value.reserve(phrase.size());
    std::array<std::string_view, 24> tokens{};
    std::size_t count{};
    std::size_t start{};

    // English BIP39 consists exclusively of ASCII lowercase words. We
    // normalize ASCII case and whitespace but never silently reinterpret
    // arbitrary Unicode bytes as a different mnemonic.
    for (const char raw : phrase) {
        const auto byte = static_cast<unsigned char>(raw);
        if (byte > 127 || byte == 0)
            return std::unexpected(Error::InvalidPhrase);
        if (std::isspace(byte)) {
            if (!normalized.value.empty() && normalized.value.back() != ' ')
                normalized.value.push_back(' ');
        } else if (byte >= 'A' && byte <= 'Z') {
            normalized.value.push_back(static_cast<char>(byte - 'A' + 'a'));
        } else {
            normalized.value.push_back(static_cast<char>(byte));
        }
    }
    if (!normalized.value.empty() && normalized.value.back() == ' ')
        normalized.value.pop_back();
    if (normalized.value.empty()) return std::unexpected(Error::InvalidPhrase);
    const std::string_view canonical{normalized.value};
    for (std::size_t i{}; i <= canonical.size(); ++i) {
        if (i < canonical.size() && canonical[i] != ' ') continue;
        if (count == tokens.size()) return std::unexpected(Error::InvalidWordCount);
        tokens[count++] = canonical.substr(start, i - start);
        start = i + 1;
    }
    if (count != 12 && count != 24)
        return std::unexpected(Error::InvalidWordCount);
    const std::size_t entropyBits = count / 3 * 32;
    const std::size_t checksumBits = count / 3;
    Sensitive<32> entropy{};
    unsigned checksum{};
    for (std::size_t index{}; index < count; ++index) {
        const auto it = std::lower_bound(detail::englishWords.begin(),
                                         detail::englishWords.end(), tokens[index]);
        if (it == detail::englishWords.end() || *it != tokens[index])
            return std::unexpected(Error::UnknownWord);
        const auto wordIndex = static_cast<unsigned>(
            std::distance(detail::englishWords.begin(), it));
        for (unsigned bit{}; bit < 11; ++bit) {
            const bool set = ((wordIndex >> (10U - bit)) & 1U) != 0;
            const std::size_t position = index * 11 + bit;
            if (position < entropyBits) {
                if (set) entropy.bytes[position / 8] |=
                    static_cast<unsigned char>(1U << (7U - (position % 8)));
            } else if (set) {
                checksum |= (1U << (checksumBits - 1U - (position - entropyBits)));
            }
        }
    }
    std::array<unsigned char, 32> digest{};
    unsigned int digestLength{};
    if (EVP_Digest(entropy.bytes.data(), entropyBits / 8,
                   digest.data(), &digestLength, EVP_sha256(), nullptr) != 1
        || digestLength != digest.size()) {
        return std::unexpected(Error::KeyDerivationFailed);
    }
    const auto expectedChecksum = static_cast<unsigned>(digest[0] >> (8U - checksumBits));
    if (checksum != expectedChecksum)
        return std::unexpected(Error::InvalidChecksum);
    return normalized;
}

[[nodiscard]] std::string lowerHex(std::span<const unsigned char> bytes)
{
    constexpr std::string_view digits = "0123456789abcdef";
    std::string result = "0x";
    result.reserve(2 + bytes.size() * 2);
    for (unsigned char byte : bytes) {
        result += digits[byte >> 4U];
        result += digits[byte & 0x0FU];
    }
    return result;
}

struct CustodyMaterial final {
    Sensitive<32> privateKey{};
    std::string address{};
    CustodyMaterial(Sensitive<32>&& key, std::string publicAddress)
        : privateKey(std::move(key)), address(std::move(publicAddress)) {}
    CustodyMaterial(CustodyMaterial&&) noexcept = default;
    CustodyMaterial(const CustodyMaterial&) = delete;
    CustodyMaterial& operator=(const CustodyMaterial&) = delete;
};

[[nodiscard]] std::expected<CustodyMaterial, Error>
deriveMaterial(const std::string_view phrase)
{
    auto normalized = validateMnemonic(phrase);
    if (!normalized) return std::unexpected(normalized.error());

    Sensitive<64> seed{};
    constexpr std::string_view salt = "mnemonic"; // empty optional BIP39 passphrase
    if (PKCS5_PBKDF2_HMAC(normalized->value.data(),
                          static_cast<int>(normalized->value.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          static_cast<int>(salt.size()), 2048, EVP_sha512(),
                          static_cast<int>(seed.bytes.size()), seed.bytes.data()) != 1)
        return std::unexpected(Error::KeyDerivationFailed);
    // In-memory lifetime of the mnemonic ends as soon as the KDF completes.
    OPENSSL_cleanse(normalized->value.data(), normalized->value.size());
    normalized->value.clear();

    constexpr std::string_view bitcoinSeed = "Bitcoin seed";
    Sensitive<64> digest{};
    if (!hmac512({reinterpret_cast<const unsigned char*>(bitcoinSeed.data()),
                  bitcoinSeed.size()}, seed.bytes, digest))
        return std::unexpected(Error::KeyDerivationFailed);
    Sensitive<32> privateKey{};
    Sensitive<32> chainCode{};
    std::copy_n(digest.bytes.begin(), 32, privateKey.bytes.begin());
    std::copy_n(digest.bytes.begin() + 32, 32, chainCode.bytes.begin());
    OPENSSL_cleanse(seed.bytes.data(), seed.bytes.size());

    Group group{EC_GROUP_new_by_curve_name(NID_secp256k1)};
    Context context{BN_CTX_secure_new()};
    BigNumber order{BN_secure_new()};
    Point point{group ? EC_POINT_new(group.get()) : nullptr};
    if (!group || !context || !order || !point
        || EC_GROUP_get_order(group.get(), order.get(), context.get()) != 1)
        return std::unexpected(Error::KeyDerivationFailed);

    constexpr std::uint32_t hardened = 0x80000000U;
    constexpr std::array<std::uint32_t, 5> path{
        44U | hardened, 60U | hardened, hardened, 0U, 0U,
    };
    for (std::uint32_t index : path) {
        BigNumber parent{BN_bin2bn(privateKey.bytes.data(), 32, BN_secure_new())};
        if (!parent || BN_is_zero(parent.get())
            || BN_cmp(parent.get(), order.get()) >= 0)
            return std::unexpected(Error::KeyDerivationFailed);
        Sensitive<37> input{};
        const bool isHardened = (index & hardened) != 0;
        if (isHardened) {
            input.bytes[0] = 0;
            std::copy(privateKey.bytes.begin(), privateKey.bytes.end(),
                      input.bytes.begin() + 1);
        } else {
            if (EC_POINT_mul(group.get(), point.get(), parent.get(), nullptr,
                             nullptr, context.get()) != 1
                || EC_POINT_point2oct(group.get(), point.get(),
                                      POINT_CONVERSION_COMPRESSED,
                                      input.bytes.data(), 33, context.get()) != 33)
                return std::unexpected(Error::KeyDerivationFailed);
        }
        for (std::size_t offset{}; offset < 4; ++offset)
            input.bytes[33 + offset] =
                static_cast<unsigned char>(index >> (24 - 8 * offset));
        if (!hmac512(chainCode.bytes, input.bytes, digest))
            return std::unexpected(Error::KeyDerivationFailed);
        BigNumber left{BN_bin2bn(digest.bytes.data(), 32, BN_secure_new())};
        if (!left || BN_cmp(left.get(), order.get()) >= 0
            || BN_mod_add(parent.get(), parent.get(), left.get(),
                          order.get(), context.get()) != 1
            || BN_is_zero(parent.get())
            || BN_bn2binpad(parent.get(), privateKey.bytes.data(), 32) != 32)
            return std::unexpected(Error::KeyDerivationFailed);
        std::copy_n(digest.bytes.begin() + 32, 32, chainCode.bytes.begin());
    }
    BigNumber leaf{BN_bin2bn(privateKey.bytes.data(), 32, BN_secure_new())};
    Sensitive<65> publicKey{};
    if (!leaf || EC_POINT_mul(group.get(), point.get(), leaf.get(), nullptr,
                              nullptr, context.get()) != 1
        || EC_POINT_point2oct(group.get(), point.get(),
                              POINT_CONVERSION_UNCOMPRESSED,
                              publicKey.bytes.data(), publicKey.bytes.size(),
                              context.get()) != publicKey.bytes.size()
        || publicKey.bytes[0] != 4)
        return std::unexpected(Error::KeyDerivationFailed);
    const auto addressHash = keccak256(
        std::span<const unsigned char>{publicKey.bytes}.subspan(1));
    return CustodyMaterial{std::move(privateKey),
                           lowerHex(std::span<const unsigned char>{addressHash}.last(20))};
}
[[nodiscard]] bool matchesTrustedRecoveryChallenge(
    const std::string_view message, const std::uint64_t fid,
    const std::string_view localAddress)
{
    if (fid == 0U || message.size() < 80 || message.size() > 8192
        || message.find('\0') != std::string_view::npos)
        return false;
    constexpr std::string_view browserBase = genycaster::deployment::openProofBrowserBase;
    constexpr auto authorityEnd = browserBase.find('/', 8);
    const std::string heading = std::string{browserBase.substr(8, authorityEnd - 8)}
        + " wants you to sign in with your Ethereum account:\n";
    if (!message.starts_with(heading)) return false;
    const auto addressEnd = message.find('\n', heading.size());
    if (addressEnd == std::string_view::npos
        || addressEnd - heading.size() != 42)
        return false;
    const auto declared = message.substr(heading.size(), 42);
    if (declared.substr(0, 2) != "0x") return false;
    const auto equivalentAddress = std::equal(
        declared.begin(), declared.end(), localAddress.begin(), localAddress.end(),
        [](const char a, const char b) {
            return std::tolower(static_cast<unsigned char>(a))
                == std::tolower(static_cast<unsigned char>(b));
        });
    if (!equivalentAddress
        || message.find("\n\nFarcaster Auth\n") == std::string_view::npos
        || message.find("\nChain ID: 10\n") == std::string_view::npos
        || message.find("\nURI: https://") == std::string_view::npos
        || message.find("\nNonce: ") == std::string_view::npos
        || message.find("\nIssued At: ") == std::string_view::npos)
        return false;
    const std::string resource =
        "\n- farcaster://fid/" + std::to_string(fid);
    const auto resourceAt = message.find(resource);
    if (resourceAt == std::string_view::npos) return false;
    const auto next = resourceAt + resource.size();
    return next == message.size() || message[next] == '\n' || message[next] == '\r';
}

[[nodiscard]] std::expected<std::string, Error> signDigest(
    const Sensitive<32>& privateKey,
    const std::span<const unsigned char, 32> digest)
{
    Group group{EC_GROUP_new_by_curve_name(NID_secp256k1)};
    Context ctx{BN_CTX_secure_new()};
    BigNumber order{BN_secure_new()};
    BigNumber secret{BN_bin2bn(privateKey.bytes.data(), 32, BN_secure_new())};
    Point publicPoint{group ? EC_POINT_new(group.get()) : nullptr};
    Sensitive<65> publicKey{};
    if (!group || !ctx || !order || !secret || !publicPoint
        || EC_GROUP_get_order(group.get(), order.get(), ctx.get()) != 1
        || EC_POINT_mul(group.get(), publicPoint.get(), secret.get(),
                        nullptr, nullptr, ctx.get()) != 1
        || EC_POINT_point2oct(group.get(), publicPoint.get(),
                              POINT_CONVERSION_UNCOMPRESSED,
                              publicKey.bytes.data(), publicKey.bytes.size(),
                              ctx.get()) != publicKey.bytes.size())
        return std::unexpected(Error::SigningFailed);

    // Provider-backed OpenSSL 3 EC key import (no deprecated EC_KEY API).
    ParameterBuilder paramsBuilder{OSSL_PARAM_BLD_new()};
    if (!paramsBuilder
        || OSSL_PARAM_BLD_push_utf8_string(
               paramsBuilder.get(), OSSL_PKEY_PARAM_GROUP_NAME, "secp256k1", 0) != 1
        || OSSL_PARAM_BLD_push_BN(
               paramsBuilder.get(), OSSL_PKEY_PARAM_PRIV_KEY, secret.get()) != 1
        || OSSL_PARAM_BLD_push_octet_string(
               paramsBuilder.get(), OSSL_PKEY_PARAM_PUB_KEY,
               publicKey.bytes.data(), publicKey.bytes.size()) != 1)
        return std::unexpected(Error::SigningFailed);
    Parameters params{OSSL_PARAM_BLD_to_param(paramsBuilder.get())};
    PkeyContext importContext{EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr)};
    EVP_PKEY* rawKey{};
    if (!params || !importContext
        || EVP_PKEY_fromdata_init(importContext.get()) <= 0
        || EVP_PKEY_fromdata(importContext.get(), &rawKey,
                            EVP_PKEY_KEYPAIR, params.get()) <= 0) {
        EVP_PKEY_free(rawKey);
        return std::unexpected(Error::SigningFailed);
    }
    Pkey signingKey{rawKey};
    PkeyContext signingContext{EVP_PKEY_CTX_new(signingKey.get(), nullptr)};
    if (!signingContext || EVP_PKEY_sign_init(signingContext.get()) <= 0
        || EVP_PKEY_CTX_set_signature_md(signingContext.get(), EVP_sha256()) <= 0)
        return std::unexpected(Error::SigningFailed);

    BigNumber halfOrder{BN_dup(order.get())};
    BigNumber z{BN_bin2bn(digest.data(), 32, BN_secure_new())};
    BigNumber inv{BN_secure_new()};
    BigNumber u1{BN_secure_new()};
    BigNumber u2{BN_secure_new()};
    BigNumber x{BN_new()};
    BigNumber y{BN_new()};
    Point noncePoint{EC_POINT_new(group.get())};
    if (!halfOrder || !z || !inv || !u1 || !u2 || !x || !y || !noncePoint
        || BN_rshift1(halfOrder.get(), halfOrder.get()) != 1)
        return std::unexpected(Error::SigningFailed);

    // Low-S canonicalization is mandatory for Ethereum. Derive v from the
    // actual (r,s,z,Q) elliptic-curve equation, not from a guessed parity.
    for (unsigned attempt{}; attempt < 4U; ++attempt) {
        std::size_t derLength{};
        if (EVP_PKEY_sign(signingContext.get(), nullptr, &derLength,
                          digest.data(), digest.size()) <= 0
            || derLength == 0 || derLength > 256)
            return std::unexpected(Error::SigningFailed);
        std::vector<unsigned char> der(derLength);
        if (EVP_PKEY_sign(signingContext.get(), der.data(), &derLength,
                          digest.data(), digest.size()) <= 0)
            return std::unexpected(Error::SigningFailed);
        const unsigned char* cursor = der.data();
        Signature signature{d2i_ECDSA_SIG(nullptr, &cursor,
                                         static_cast<long>(derLength))};
        const BIGNUM* r{};
        const BIGNUM* s{};
        if (!signature || cursor != der.data() + derLength)
            return std::unexpected(Error::SigningFailed);
        ECDSA_SIG_get0(signature.get(), &r, &s);
        if (!r || !s || BN_is_zero(r) || BN_is_zero(s)
            || BN_is_negative(r) || BN_is_negative(s)
            || BN_cmp(r, order.get()) >= 0 || BN_cmp(s, order.get()) >= 0)
            return std::unexpected(Error::SigningFailed);
        BigNumber lowS{BN_dup(s)};
        if (!lowS) return std::unexpected(Error::SigningFailed);
        if (BN_cmp(lowS.get(), halfOrder.get()) > 0
            && BN_sub(lowS.get(), order.get(), lowS.get()) != 1)
            return std::unexpected(Error::SigningFailed);
        if (!BN_mod_inverse(inv.get(), lowS.get(), order.get(), ctx.get())
            || BN_mod_mul(u1.get(), z.get(), inv.get(), order.get(), ctx.get()) != 1
            || BN_mod_mul(u2.get(), r, inv.get(), order.get(), ctx.get()) != 1
            || EC_POINT_mul(group.get(), noncePoint.get(), u1.get(),
                            publicPoint.get(), u2.get(), ctx.get()) != 1
            || EC_POINT_get_affine_coordinates(group.get(), noncePoint.get(),
                                               x.get(), y.get(), ctx.get()) != 1)
            return std::unexpected(Error::SigningFailed);
        // Recovery IDs 2/3 require x>=n, a negligibly rare case unsupported
        // by 27/28 personal-sign. Resign with fresh CSPRNG entropy instead.
        if (BN_cmp(x.get(), order.get()) >= 0) continue;
        if (BN_cmp(x.get(), r) != 0)
            return std::unexpected(Error::SigningFailed);
        Sensitive<65> bytes{};
        if (BN_bn2binpad(r, bytes.bytes.data(), 32) != 32
            || BN_bn2binpad(lowS.get(), bytes.bytes.data() + 32, 32) != 32)
            return std::unexpected(Error::SigningFailed);
        bytes.bytes[64] = static_cast<unsigned char>(
            27 + BN_is_odd(y.get()));
        return lowerHex(bytes.bytes);
    }
    return std::unexpected(Error::SigningFailed);
}
} // namespace

GeneratedCustodyWallet::GeneratedCustodyWallet(GeneratedCustodyWallet&& other) noexcept
    : recoveryWords(std::move(other.recoveryWords)),
      address(std::move(other.address))
{
    if (!other.recoveryWords.empty()) {
        OPENSSL_cleanse(other.recoveryWords.data(), other.recoveryWords.size());
        other.recoveryWords.clear();
    }
}

GeneratedCustodyWallet&
GeneratedCustodyWallet::operator=(GeneratedCustodyWallet&& other) noexcept
{
    if (this != &other) {
        if (!recoveryWords.empty()) {
            OPENSSL_cleanse(recoveryWords.data(), recoveryWords.size());
            recoveryWords.clear();
        }
        recoveryWords = std::move(other.recoveryWords);
        address = std::move(other.address);
        if (!other.recoveryWords.empty()) {
            OPENSSL_cleanse(other.recoveryWords.data(), other.recoveryWords.size());
            other.recoveryWords.clear();
        }
    }
    return *this;
}

GeneratedCustodyWallet::~GeneratedCustodyWallet()
{
    if (!recoveryWords.empty())
        OPENSSL_cleanse(recoveryWords.data(), recoveryWords.size());
}

std::expected<GeneratedCustodyWallet, Error> generateCustodyWallet()
{
    // 256 cryptographically secure random entropy bits plus eight checksum bits
    // produce exactly 24 BIP39 words. Never use QRandomGenerator or rand().
    Sensitive<32> entropy{};
    if (RAND_priv_bytes(entropy.bytes.data(),
                       static_cast<int>(entropy.bytes.size())) != 1)
        return std::unexpected(Error::KeyDerivationFailed);
    std::array<unsigned char, 32> digest{};
    unsigned int digestLength{};
    if (EVP_Digest(entropy.bytes.data(), entropy.bytes.size(), digest.data(),
                   &digestLength, EVP_sha256(), nullptr) != 1
        || digestLength != digest.size())
        return std::unexpected(Error::KeyDerivationFailed);
    GeneratedCustodyWallet wallet{};
    wallet.recoveryWords.reserve(24 * 9);
    for (std::size_t wordIndex{}; wordIndex < 24; ++wordIndex) {
        unsigned index{};
        for (std::size_t bit{}; bit < 11; ++bit) {
            const std::size_t offset = wordIndex * 11 + bit;
            const unsigned char byte = offset < 256
                ? entropy.bytes[offset / 8] : digest[0];
            const unsigned shift = 7U - static_cast<unsigned>(offset % 8);
            index = (index << 1U) | ((byte >> shift) & 1U);
        }
        if (wordIndex > 0) wallet.recoveryWords.push_back(' ');
        wallet.recoveryWords.append(detail::englishWords[index]);
    }
    auto publicAddress = deriveMaterial(wallet.recoveryWords);
    if (!publicAddress) return std::unexpected(publicAddress.error());
    wallet.address = std::move(publicAddress->address);
    return wallet;
}

std::expected<std::string, Error>
derivePublicCustodyAddress(const std::string_view phrase)
{
    auto custody = deriveMaterial(phrase);
    if (!custody) return std::unexpected(custody.error());
    return std::move(custody->address);
}

std::expected<RecoveryProof, Error>
signOneTimeIdentityProof(const std::string_view phrase,
                         const std::uint64_t fid,
                         const std::string_view challenge)
{
    // Refuse malformed or unrelated messages before the expensive BIP39 KDF.
    constexpr std::string_view browserBase = genycaster::deployment::openProofBrowserBase;
    constexpr auto authorityEnd = browserBase.find('/', 8);
    const std::string expectedHeading = std::string{browserBase.substr(8, authorityEnd - 8)}
        + " wants you to sign in with your Ethereum account:\n";
    if (fid == 0U || challenge.size() < 80 || challenge.size() > 8192
        || !challenge.starts_with(expectedHeading))
        return std::unexpected(Error::InvalidChallenge);
    auto custody = deriveMaterial(phrase);
    if (!custody) return std::unexpected(custody.error());
    if (!matchesTrustedRecoveryChallenge(challenge, fid, custody->address))
        return std::unexpected(Error::InvalidChallenge);

    // EIP-191 personal_sign signs the exact UTF-8 bytes supplied by the
    // verified HTTPS gateway. Neither the phrase nor the custody private key
    // enters the network request, QML properties, a log, or persistent storage.
    std::string prefix{"\x19" "Ethereum Signed Message:\n"};
    prefix += std::to_string(challenge.size());
    std::vector<unsigned char> input{};
    input.reserve(prefix.size() + challenge.size());
    for (const char ch : prefix)
        input.push_back(static_cast<unsigned char>(ch));
    for (const char ch : challenge)
        input.push_back(static_cast<unsigned char>(ch));
    const auto hash = keccak256(input);
    auto signature = signDigest(custody->privateKey, hash);
    if (!signature) return std::unexpected(signature.error());
    return RecoveryProof{
        .address = std::move(custody->address),
        .signature = std::move(*signature),
    };
}


// Strict, single-use EIP-712 signing for a network-approved device signer.
// The typed-data hashes reproduce key_authorization::buildTypedData's domain,
// field ordering and dynamic-array encoding; no JSON parser is trusted here.
namespace {
using Hash32 = std::array<unsigned char, 32>;

[[nodiscard]] Hash32 hashLiteral(std::string_view value)
{
    return keccak256({reinterpret_cast<const unsigned char*>(value.data()), value.size()});
}

void appendHash(std::vector<unsigned char>& destination, const Hash32& hash)
{
    destination.insert(destination.end(), hash.begin(), hash.end());
}

void appendNumber(std::vector<unsigned char>& destination, std::uint64_t value)
{
    std::array<unsigned char, 32> word{};
    for (std::size_t i{}; i < 8; ++i) {
        word[31 - i] = static_cast<unsigned char>(value & 255U);
        value >>= 8U;
    }
    appendHash(destination, word);
}

[[nodiscard]] std::optional<std::vector<unsigned char>>
parseHex(std::string_view value, std::size_t bytes)
{
    if (value.starts_with("0x")) value.remove_prefix(2);
    if (value.size() != bytes * 2) return std::nullopt;
    const auto nibble = [](const char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<unsigned char> decoded{};
    decoded.reserve(bytes);
    for (std::size_t i{}; i < value.size(); i += 2) {
        const auto upper = nibble(value[i]), lower = nibble(value[i + 1]);
        if (upper < 0 || lower < 0) return std::nullopt;
        decoded.push_back(static_cast<unsigned char>((upper << 4) | lower));
    }
    return decoded;
}

[[nodiscard]] Hash32 typedDigest(
    const farcaster::capability::signer::KeyAddRequest& request,
    const std::uint32_t deadline,
    const bool signedKeyRequest)
{
    auto key = parseHex(request.publicKey.hex, 32);
    if (!key) return {};
    const Hash32 domainType = hashLiteral(
        "EIP712Domain(string name,string version,uint256 chainId)");
    std::vector<unsigned char> domain{};
    appendHash(domain, domainType);
    appendHash(domain, hashLiteral("Farcaster KeyAdd"));
    appendHash(domain, hashLiteral("1"));
    appendNumber(domain, 1);
    const auto domainHash = keccak256(domain);

    std::vector<unsigned char> structure{};
    if (signedKeyRequest) {
        appendHash(structure, hashLiteral(
            "SignedKeyRequest(uint256 requestFid,bytes key,uint256 deadline)"));
        appendNumber(structure, request.requestFid.value);
        appendHash(structure, keccak256(*key));
        appendNumber(structure, deadline);
    } else {
        appendHash(structure, hashLiteral(
            "KeyAdd(uint256 fid,bytes key,uint32 keyType,uint32[] scopes,uint32 ttl,uint32 nonce,uint256 deadline)"));
        appendNumber(structure, request.fid.value);
        appendHash(structure, keccak256(*key));
        appendNumber(structure, 1);  // ED25519
        std::vector<unsigned char> scopes{};
        for (const auto scope : request.scopes)
            appendNumber(scopes, static_cast<std::uint32_t>(scope));
        appendHash(structure, keccak256(scopes));
        appendNumber(structure, request.ttlSeconds);
        appendNumber(structure, request.nonce);
        appendNumber(structure, deadline);
    }
    const auto structHash = keccak256(structure);
    std::vector<unsigned char> preimage{0x19, 0x01};
    appendHash(preimage, domainHash);
    appendHash(preimage, structHash);
    return keccak256(preimage);
}

class OneShotRecoveryAuthorizer final
    : public farcaster::capability::signer::ICustodyAuthorizer {
public:
    OneShotRecoveryAuthorizer(Sensitive<32>&& key, std::string address,
                              const std::uint64_t verifiedFid)
        : m_key(std::move(key)), m_address(std::move(address)),
          m_verifiedFid(verifiedFid) {}

    [[nodiscard]] farcaster::foundation::result::Result<
        farcaster::capability::signer::KeyAddAuthorization>
    authorize(const farcaster::capability::signer::KeyAddRequest& request,
        std::optional<farcaster::capability::signer_attestation::SignedKeyRequestAttestation>
            appAttestation) override
    {
        using farcaster::foundation::error::ErrorCode;
        const auto invalid = [](std::string message) {
            return std::unexpected(farcaster::foundation::error::Error::make(
                ErrorCode::Unauthorized, std::move(message)));
        };
        if (m_consumed) return invalid("Recovery custody approval was already consumed.");
        m_consumed = true;
        struct CleanseAfterUse {
            Sensitive<32>& key;
            ~CleanseAfterUse() { OPENSSL_cleanse(key.bytes.data(), key.bytes.size()); }
        } cleanse{m_key};

        // Do not sign unrelated accounts, replayed requests or a silently
        // swapped custody address after the verified recovery ceremony.
        const auto expected = parseHex(m_address, 20);
        const auto supplied = parseHex(request.custodyAddress, 20);
        if (!expected || !supplied || *expected != *supplied
            || request.fid.value == 0 || request.fid.value != m_verifiedFid)
            return invalid("Recovery signer FID/custody differs from the verified account.");
        if (std::chrono::steady_clock::now() - m_createdAt > std::chrono::minutes{3})
            return invalid("One-time signer recovery authorization has expired.");
        const auto deadline = farcaster::protocol::message::toFarcasterTimestamp(request.deadline);
        if (!deadline) return std::unexpected(std::move(deadline.error()));
        const farcaster::protocol::key_authorization::KeyAddInput input{
            .fid = request.fid, .publicKey = request.publicKey,
            .scopes = request.scopes, .ttlSeconds = request.ttlSeconds,
            .nonce = request.nonce, .deadline = *deadline,
            .requestFid = request.requestFid,
        };
        if (auto documents = farcaster::protocol::key_authorization::buildTypedData(input);
            !documents) return std::unexpected(std::move(documents.error()));
        if (request.requestFid.value != request.fid.value && !appAttestation)
            return invalid("An app-attributed signer requires the independent app attestation.");
        const auto custody = signDigest(m_key, typedDigest(request, *deadline, false));
        if (!custody) return invalid("Custody key could not sign the exact KEY_ADD request.");
        auto rawCustody = parseHex(*custody, 65);
        if (!rawCustody) return invalid("Invalid custody signature encoding.");

        std::string requestSignerAddress{};
        std::string signedRequestSignature{};
        if (appAttestation) {
            requestSignerAddress = appAttestation->requestSignerAddress;
            signedRequestSignature = appAttestation->signature;
        } else {
            const auto requestSignature =
                signDigest(m_key, typedDigest(request, *deadline, true));
            if (!requestSignature)
                return invalid("Custody key could not authorize the SignedKeyRequest.");
            const auto raw = parseHex(*requestSignature, 65);
            if (!raw) return invalid("Invalid SignedKeyRequest signature.");
            requestSignerAddress.assign(reinterpret_cast<const char*>(expected->data()),
                                        expected->size());
            signedRequestSignature.assign(
                reinterpret_cast<const char*>(raw->data()), raw->size());
        }
        auto metadata = farcaster::protocol::key_authorization::
            encodeSignedKeyRequestMetadata(request.requestFid, requestSignerAddress,
                                           signedRequestSignature, *deadline);
        if (!metadata) return std::unexpected(std::move(metadata.error()));
        return farcaster::capability::signer::KeyAddAuthorization{
            .custodySignature = std::string{
                reinterpret_cast<const char*>(rawCustody->data()), rawCustody->size()},
            .signedKeyRequestMetadata = std::move(*metadata),
        };
    }

private:
    Sensitive<32> m_key{};
    std::string m_address{};
    std::uint64_t m_verifiedFid{};
    const std::chrono::steady_clock::time_point m_createdAt{
        std::chrono::steady_clock::now()};
    bool m_consumed{false};
};
}

std::expected<FarcasterIdTransferSignature, Error> signFarcasterIdTransfer(
    const std::string_view recoveryWords,
    const std::uint64_t reservedFid,
    const std::uint64_t idRegistryNonce,
    const std::uint64_t deadlineUnixSeconds)
{
    // Neynar "wallet integration" reserved-FID transfer is a chain-specific
    // EIP-712 signature. Never sign an arbitrary domain/contract supplied by
    // a remote registration service. The server must verify FID reservation
    // and the IdRegistry nonce independently before broadcasting.
    constexpr std::string_view idRegistry =
        "0x00000000fc6c5f01fc30151999387bb99a9f489b";
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    if (reservedFid == 0
        || deadlineUnixSeconds <= now + 60
        || deadlineUnixSeconds > now + 3600)
        return std::unexpected(Error::SigningFailed);

    auto contract = parseHex(idRegistry, 20);
    if (!contract) return std::unexpected(Error::SigningFailed);
    auto custody = deriveMaterial(recoveryWords);
    if (!custody) return std::unexpected(custody.error());
    auto receiver = parseHex(custody->address, 20);
    if (!receiver) return std::unexpected(Error::SigningFailed);

    std::vector<unsigned char> domain{};
    appendHash(domain, hashLiteral(
        "EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)"));
    appendHash(domain, hashLiteral("Farcaster IdRegistry"));
    appendHash(domain, hashLiteral("1"));
    appendNumber(domain, 10);
    domain.insert(domain.end(), 12, 0);
    domain.insert(domain.end(), contract->begin(), contract->end());
    const auto domainHash = keccak256(domain);

    std::vector<unsigned char> body{};
    appendHash(body, hashLiteral(
        "Transfer(uint256 fid,address to,uint256 nonce,uint256 deadline)"));
    appendNumber(body, reservedFid);
    body.insert(body.end(), 12, 0);
    body.insert(body.end(), receiver->begin(), receiver->end());
    appendNumber(body, idRegistryNonce);
    appendNumber(body, deadlineUnixSeconds);
    const auto bodyHash = keccak256(body);
    std::vector<unsigned char> typed{0x19, 0x01};
    appendHash(typed, domainHash);
    appendHash(typed, bodyHash);

    auto signature = signDigest(custody->privateKey, keccak256(typed));
    if (!signature) return std::unexpected(Error::SigningFailed);
    return FarcasterIdTransferSignature{
        .custodyAddress = std::move(custody->address),
        .signature = std::move(*signature),
        .fid = reservedFid,
        .nonce = idRegistryNonce,
        .deadline = deadlineUnixSeconds,
    };
}

std::expected<std::string, Error> solveNativeOnboardPow(
    const std::string_view custodyAddress,
    const std::string_view anchorHex,
    const std::uint32_t difficultyBits,
    const std::uint64_t maxAttempts,
    const std::uint64_t startNonce,
    const std::atomic_bool* cancelled)
{
    if (difficultyBits == 0 || difficultyBits > 256
        || maxAttempts == 0 || maxAttempts > 100'000'000ULL
        || startNonce > std::numeric_limits<std::uint64_t>::max() - maxAttempts)
        return std::unexpected(Error::SigningFailed);
    const auto custody = parseHex(custodyAddress, 20);
    const auto anchor = parseHex(anchorHex, 32);
    if (!custody || !anchor) return std::unexpected(Error::SigningFailed);
    constexpr std::string_view prefix = "hyper.onboard.pow:";
    std::vector<unsigned char> challenge(prefix.begin(), prefix.end());
    challenge.insert(challenge.end(), custody->begin(), custody->end());
    challenge.insert(challenge.end(), anchor->begin(), anchor->end());
    const auto noncePosition = challenge.size();
    challenge.resize(noncePosition + 8);
    std::array<unsigned char, 32> digest{};
    for (std::uint64_t index{}; index < maxAttempts; ++index) {
        if ((index & 0x3fffU) == 0 && cancelled
            && cancelled->load(std::memory_order_relaxed))
            return std::unexpected(Error::SigningFailed);
        const auto nonce = startNonce + index;
        // HyperSnap native registration defines an 8-byte BE nonce.
        for (std::size_t index{}; index < 8; ++index)
            challenge[noncePosition + index] = static_cast<unsigned char>(
                nonce >> (56U - static_cast<unsigned>(8U * index)));
        unsigned int written{};
        if (EVP_Digest(challenge.data(), challenge.size(), digest.data(),
                       &written, EVP_sha256(), nullptr) != 1
            || written != digest.size())
            return std::unexpected(Error::SigningFailed);
        bool targetMet = true;
        for (std::uint32_t bit{}; bit < difficultyBits; ++bit) {
            if ((digest[bit / 8] & (0x80U >> (bit % 8))) != 0) {
                targetMet = false;
                break;
            }
        }
        if (targetMet) {
            return lowerHex(std::span{
                challenge.data() + noncePosition, std::size_t{8}});
        }
    }
    // Exhaustion is not registration success. Caller can refresh the anchor.
    return std::unexpected(Error::SigningFailed);
}

std::expected<NativeOnboardSignature, Error> signNativeOnboard(
    const std::string_view recoveryWords,
    const std::uint64_t anchorHeight,
    const std::string_view anchorHex,
    const std::string_view nonceHex,
    const std::uint32_t difficultyBits)
{
    // The native account namespace and EIP-712 domain are different from
    // Optimism IdRegistry. No existing FID or signer is required here.
    if (anchorHeight == 0 || difficultyBits == 0 || difficultyBits > 256)
        return std::unexpected(Error::SigningFailed);
    auto anchor = parseHex(anchorHex, 32);
    auto nonce = parseHex(nonceHex, 8);
    if (!anchor || !nonce) return std::unexpected(Error::SigningFailed);
    auto custody = deriveMaterial(recoveryWords);
    if (!custody) return std::unexpected(custody.error());
    auto address = parseHex(custody->address, 20);
    if (!address) return std::unexpected(Error::SigningFailed);

    const auto sha256 = [](const std::vector<unsigned char>& bytes) {
        Hash32 hash{};
        unsigned int written{};
        if (EVP_Digest(bytes.data(), bytes.size(), hash.data(),
                       &written, EVP_sha256(), nullptr) != 1
            || written != hash.size()) return std::optional<Hash32>{};
        return std::optional<Hash32>{hash};
    };
    constexpr std::string_view powPrefix = "hyper.onboard.pow:";
    std::vector<unsigned char> pow(powPrefix.begin(), powPrefix.end());
    pow.insert(pow.end(), address->begin(), address->end());
    pow.insert(pow.end(), anchor->begin(), anchor->end());
    pow.insert(pow.end(), nonce->begin(), nonce->end());
    const auto powHash = sha256(pow);
    if (!powHash) return std::unexpected(Error::SigningFailed);
    for (std::uint32_t bit{}; bit < difficultyBits; ++bit)
        if (((*powHash)[bit / 8] & (0x80U >> (bit % 8))) != 0)
            return std::unexpected(Error::SigningFailed);

    constexpr std::string_view commitmentPrefix = "hyper.onboard.pow.commitment:";
    std::vector<unsigned char> commitment(commitmentPrefix.begin(), commitmentPrefix.end());
    commitment.insert(commitment.end(), nonce->begin(), nonce->end());
    for (int shift = 24; shift >= 0; shift -= 8)
        commitment.push_back(static_cast<unsigned char>(difficultyBits >> shift));
    const auto commitmentHash = sha256(commitment);
    if (!commitmentHash) return std::unexpected(Error::SigningFailed);

    // Match HyperSnap src/native_registration.rs::onboard_typed_data
    // exactly: EIP712Domain chainId 10, HyperNativeOnboard fields in order.
    std::vector<unsigned char> domain{};
    appendHash(domain, hashLiteral(
        "EIP712Domain(string name,string version,uint256 chainId)"));
    appendHash(domain, hashLiteral("HypersnapNativeOnboard"));
    appendHash(domain, hashLiteral("1"));
    appendNumber(domain, 10);
    const auto domainHash = keccak256(domain);

    std::vector<unsigned char> structure{};
    appendHash(structure, hashLiteral(
        "HyperNativeOnboard(address custody,uint64 anchor_block_height,bytes32 anchor_block_hash,bytes32 gate_commitment)"));
    structure.insert(structure.end(), 12, 0);
    structure.insert(structure.end(), address->begin(), address->end());
    appendNumber(structure, anchorHeight);
    structure.insert(structure.end(), anchor->begin(), anchor->end());
    appendHash(structure, *commitmentHash);
    const auto structureHash = keccak256(structure);
    std::vector<unsigned char> preimage{0x19, 0x01};
    appendHash(preimage, domainHash);
    appendHash(preimage, structureHash);
    const auto signature = signDigest(custody->privateKey, keccak256(preimage));
    if (!signature) return std::unexpected(Error::SigningFailed);
    return NativeOnboardSignature{
        .address = std::move(custody->address),
        .signature = *signature
    };
}

std::expected<std::shared_ptr<farcaster::capability::signer::ICustodyAuthorizer>, Error>
makeOneShotRecoveryAuthorizer(const std::string_view phrase,
                              const std::string_view verifiedCustodyAddress,
                              const std::uint64_t verifiedFid)
{
    if (verifiedFid == 0) return std::unexpected(Error::SigningFailed);
    std::size_t wordCount{};
    bool inWord{false};
    for (const char character : phrase) {
        const auto ch = static_cast<unsigned char>(character);
        if (std::isspace(ch)) {
            inWord = false;
        } else if (!inWord) {
            if (++wordCount > 24) return std::unexpected(Error::InvalidWordCount);
            inWord = true;
        }
    }
    if (wordCount != 24) return std::unexpected(Error::InvalidWordCount);
    auto custody = deriveMaterial(phrase);
    if (!custody) return std::unexpected(custody.error());
    const auto expected = parseHex(verifiedCustodyAddress, 20);
    const auto actual = parseHex(custody->address, 20);
    if (!expected || !actual || *expected != *actual)
        return std::unexpected(Error::SigningFailed);
    std::shared_ptr<capability::signer::ICustodyAuthorizer> authorizer =
        std::make_shared<OneShotRecoveryAuthorizer>(
            std::move(custody->privateKey), std::move(custody->address),
            verifiedFid);
    return authorizer;
}

} // namespace farcaster::platform::custody_recovery
