module;

// The reviewed third-party BIP39 dictionary is a textual data boundary.
#include "custody_bip39_english.hpp"

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

module farcaster.platform.custody_recovery;

namespace farcaster::platform::custody_recovery {
namespace {

template <std::size_t N>
struct Sensitive final {
    std::array<unsigned char, N> bytes{};
    ~Sensitive() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
    Sensitive() = default;
    Sensitive(const Sensitive&) = delete;
    Sensitive& operator=(const Sensitive&) = delete;
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
using BigNumber = std::unique_ptr<BIGNUM, ClearBn>;
using Context = std::unique_ptr<BN_CTX, FreeCtx>;
using Group = std::unique_ptr<EC_GROUP, FreeGroup>;
using Point = std::unique_ptr<EC_POINT, FreePoint>;

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
} // namespace

std::expected<std::string, Error>
derivePublicCustodyAddress(const std::string_view phrase)
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
    return lowerHex(std::span<const unsigned char>{addressHash}.last(20));
}
} // namespace farcaster::platform::custody_recovery
