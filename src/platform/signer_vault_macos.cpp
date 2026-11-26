module;

#include "signer_keychain_boundary.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

module farcaster.platform.signer_vault;

import farcaster.capability.signer;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::platform::signer_vault {
namespace {

constexpr std::size_t SeedSize = keychain_boundary::seedSize;
constexpr std::size_t PublicKeySize = 32U;
constexpr std::size_t SignatureSize = 64U;

struct Seed final {
    std::array<unsigned char, SeedSize> bytes{};
    ~Seed() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
    Seed() = default;
    Seed(const Seed&) = delete;
    auto operator=(const Seed&) -> Seed& = delete;
    Seed(Seed&& other) noexcept : bytes(other.bytes)
    {
        OPENSSL_cleanse(other.bytes.data(), other.bytes.size());
    }
    auto operator=(Seed&& other) noexcept -> Seed&
    {
        if (this != &other) {
            OPENSSL_cleanse(bytes.data(), bytes.size());
            bytes = other.bytes;
            OPENSSL_cleanse(other.bytes.data(), other.bytes.size());
        }
        return *this;
    }
};

[[nodiscard]] foundation::error::Error keychainError(
    const keychain_boundary::Failure& failure)
{
    std::string message{failure.context};
    if (failure.nativeStatus) {
        message += " (Security.framework status ";
        message += std::to_string(*failure.nativeStatus);
        message += ").";
    }
    return foundation::error::Error::make(
        failure.authorizationDenied ? foundation::error::ErrorCode::Unauthorized
                                    : foundation::error::ErrorCode::StorageFailure,
        std::move(message));
}

[[nodiscard]] foundation::error::Error cryptoError(const std::string_view context)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::InternalFailure,
        std::string{context});
}

[[nodiscard]] std::string accountFor(const domain::identity::Fid fid)
{
    return "fid:" + std::to_string(fid.value);
}

[[nodiscard]] foundation::result::Result<std::optional<Seed>> loadSeed(
    const std::string& service,
    const domain::identity::Fid fid)
{
    Seed seed{};
    auto loaded = keychain_boundary::load(service, accountFor(fid), seed.bytes);
    if (!loaded) {
        return std::unexpected(keychainError(loaded.error()));
    }
    if (*loaded == keychain_boundary::LoadState::NotFound) {
        return std::optional<Seed>{};
    }
    return std::optional<Seed>{std::move(seed)};
}

[[nodiscard]] foundation::result::Status saveSeed(
    const std::string& service,
    const domain::identity::Fid fid,
    const Seed& seed)
{
    auto saved = keychain_boundary::save(service, accountFor(fid), seed.bytes);
    if (!saved) {
        return std::unexpected(keychainError(saved.error()));
    }
    return {};
}

[[nodiscard]] foundation::result::Status deleteSeed(
    const std::string& service,
    const domain::identity::Fid fid)
{
    auto erased = keychain_boundary::erase(service, accountFor(fid));
    if (!erased) {
        return std::unexpected(keychainError(erased.error()));
    }
    return {};
}

[[nodiscard]] foundation::result::Result<Seed> createSeed()
{
    Seed seed{};
    if (RAND_bytes(seed.bytes.data(), static_cast<int>(seed.bytes.size())) != 1) {
        return std::unexpected(cryptoError("OpenSSL failed to generate an Ed25519 signer seed."));
    }
    return seed;
}

struct PkeyDeleter final {
    void operator()(EVP_PKEY* key) const noexcept { EVP_PKEY_free(key); }
};
using PkeyPtr = std::unique_ptr<EVP_PKEY, PkeyDeleter>;

struct MdContextDeleter final {
    void operator()(EVP_MD_CTX* context) const noexcept { EVP_MD_CTX_free(context); }
};
using MdContextPtr = std::unique_ptr<EVP_MD_CTX, MdContextDeleter>;

[[nodiscard]] foundation::result::Result<PkeyPtr> makePrivateKey(const Seed& seed)
{
    PkeyPtr key{EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519,
        nullptr,
        seed.bytes.data(),
        seed.bytes.size())};
    if (!key) return std::unexpected(cryptoError("OpenSSL failed to construct Ed25519 signer."));
    return key;
}

[[nodiscard]] std::string toHex(const unsigned char* bytes, const std::size_t size)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result(size * 2U, '0');
    for (std::size_t index{}; index < size; ++index) {
        result[index * 2U] = digits[(bytes[index] >> 4U) & 0x0FU];
        result[index * 2U + 1U] = digits[bytes[index] & 0x0FU];
    }
    return result;
}

class KeychainNativeSigner final : public capability::signer::INativeSigner {
public:
    KeychainNativeSigner(std::string service, const domain::identity::Fid fid,
                         PkeyPtr oneShotDeviceKey = {})
        : m_service(std::move(service)), m_fid(fid),
          m_oneShotDeviceKey(std::move(oneShotDeviceKey))
    {
    }

    [[nodiscard]] domain::signer::SignerId signerId() const override
    {
        return domain::signer::SignerId{"macos-keychain:" + accountFor(m_fid)};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> publicKey() const override
    {
        auto loaded = loadSeed(m_service, m_fid);
        if (!loaded) return std::unexpected(std::move(loaded.error()));
        if (!*loaded) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::NotFound,
                "Protocol signer is no longer present in Apple Keychain."));
        }
        auto key = makePrivateKey(**loaded);
        if (!key) return std::unexpected(std::move(key.error()));
        std::array<unsigned char, PublicKeySize> publicBytes{};
        std::size_t size = publicBytes.size();
        if (EVP_PKEY_get_raw_public_key(key->get(), publicBytes.data(), &size) != 1 ||
            size != PublicKeySize) {
            return std::unexpected(cryptoError("OpenSSL failed to derive Ed25519 public key."));
        }
        return domain::signer::PublicKey{toHex(publicBytes.data(), publicBytes.size())};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::Signature>
    sign(const std::string& bytes) override
    {
        // Device-session sealing immediately follows the authorized vault read.
        // Consume that key once; it dies on return even when signing fails.
        // Ordinary protocol signers still check Keychain on every operation.
        PkeyPtr key = std::move(m_oneShotDeviceKey);
        if (!key) {
            auto loaded = loadSeed(m_service, m_fid);
            if (!loaded) return std::unexpected(std::move(loaded.error()));
            if (!*loaded) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::NotFound,
                    "Protocol signer is no longer present in Apple Keychain."));
            }
            auto derived = makePrivateKey(**loaded);
            if (!derived) return std::unexpected(std::move(derived.error()));
            key = std::move(*derived);
        }
        MdContextPtr context{EVP_MD_CTX_new()};
        if (!context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1) {
            return std::unexpected(cryptoError("OpenSSL failed to initialize Ed25519 signing."));
        }
        std::array<unsigned char, SignatureSize> signature{};
        std::size_t signatureSize = signature.size();
        if (EVP_DigestSign(
                context.get(),
                signature.data(),
                &signatureSize,
                reinterpret_cast<const unsigned char*>(bytes.data()),
                bytes.size()) != 1 || signatureSize != SignatureSize) {
            return std::unexpected(cryptoError("OpenSSL failed to sign canonical Farcaster bytes."));
        }
        return domain::signer::Signature{
            std::string{reinterpret_cast<const char*>(signature.data()), signatureSize}};
    }

private:
    std::string m_service;
    domain::identity::Fid m_fid{};
    PkeyPtr m_oneShotDeviceKey{};
};

class KeychainSignerVault final : public capability::signer::ISignerVault {
public:
    explicit KeychainSignerVault(std::string service) : m_service(std::move(service)) {}

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    load(const domain::identity::Fid fid) override
    {
        if (fid.value == 0) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Protocol signer vault requires a non-zero FID."));
        }
        auto seed = loadSeed(m_service, fid);
        if (!seed) return std::unexpected(std::move(seed.error()));
        if (!*seed) return std::unique_ptr<capability::signer::INativeSigner>{};
        return makeSigner(fid, **seed);
    }

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    ensure(const domain::identity::Fid fid) override
    {
        auto existing = load(fid);
        if (!existing) return std::unexpected(std::move(existing.error()));
        if (*existing) return existing;

        auto seed = createSeed();
        if (!seed) return std::unexpected(std::move(seed.error()));
        if (auto saved = saveSeed(m_service, fid, *seed); !saved) {
            return std::unexpected(std::move(saved.error()));
        }
        return makeSigner(fid, *seed);
    }

    [[nodiscard]] foundation::result::Status erase(const domain::identity::Fid fid) override
    {
        return deleteSeed(m_service, fid);
    }

private:
    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    makeSigner(const domain::identity::Fid fid, const Seed& seed)
    {
        PkeyPtr oneShot{};
        // The session seal store owns this handle only within load/save.
        if (m_service == "org.genycaster.device-session" && fid.value == 1U) {
            auto key = makePrivateKey(seed);
            if (!key) return std::unexpected(std::move(key.error()));
            oneShot = std::move(*key);
        }
        return std::unique_ptr<capability::signer::INativeSigner>{
            std::make_unique<KeychainNativeSigner>(m_service, fid, std::move(oneShot))};
    }

    std::string m_service;
};

}

foundation::result::Result<std::unique_ptr<capability::signer::ISignerVault>>
open(std::string serviceName)
{
    if (serviceName.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Apple protocol signer vault requires a non-empty service name."));
    }
    std::unique_ptr<capability::signer::ISignerVault> vault =
        std::make_unique<KeychainSignerVault>(std::move(serviceName));
    return vault;
}

}
