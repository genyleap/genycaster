module;

#include "signer_keychain_boundary.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <mutex>
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

[[nodiscard]] foundation::result::Result<domain::signer::PublicKey>
derivePublicKey(EVP_PKEY* key)
{
    std::array<unsigned char, PublicKeySize> publicBytes{};
    std::size_t size = publicBytes.size();
    if (EVP_PKEY_get_raw_public_key(key, publicBytes.data(), &size) != 1 ||
        size != PublicKeySize) {
        return std::unexpected(cryptoError("OpenSSL failed to derive Ed25519 public key."));
    }
    return domain::signer::PublicKey{toHex(publicBytes.data(), publicBytes.size())};
}

// Non-secret revocation state shared by a vault and its handles. Erasure
// invalidates outstanding ceremonies even if a worker still holds a handle.
struct AccessState final {
    std::mutex mutex;
    std::uint64_t generation{0};
};

// Share revocation across independently opened vaults for the same item.
[[nodiscard]] std::shared_ptr<AccessState> accessFor(
    const std::string& service, domain::identity::Fid fid)
{
    static std::mutex mutex;
    static std::map<std::pair<std::string, std::uint64_t>,
                    std::weak_ptr<AccessState>> registry;
    const std::scoped_lock lock{mutex};
    std::erase_if(registry, [](const auto& entry) { return entry.second.expired(); });
    auto& slot = registry[{service, fid.value}];
    if (auto existing = slot.lock()) return existing;
    auto state = std::make_shared<AccessState>();
    slot = state;
    return state;
}

class KeychainNativeSigner final : public capability::signer::INativeSigner {
public:
    KeychainNativeSigner(std::string service, const domain::identity::Fid fid,
                         domain::signer::PublicKey publicKey,
                         std::shared_ptr<AccessState> access, std::uint64_t generation,
                         PkeyPtr oneShotDeviceKey = {})
        : m_service(std::move(service)), m_fid(fid), m_publicKey(std::move(publicKey)),
          m_access(std::move(access)), m_generation(generation),
          m_oneShotRequired(static_cast<bool>(oneShotDeviceKey)),
          m_oneShotDeviceKey(std::move(oneShotDeviceKey)),
          m_oneShotExpires(std::chrono::steady_clock::now() + std::chrono::seconds{20})
    {
    }

    [[nodiscard]] domain::signer::SignerId signerId() const override
    {
        return domain::signer::SignerId{"macos-keychain:" + accountFor(m_fid)};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> publicKey() const override
    {
        // The authorized vault read already established this handle's public
        // identity. This is public metadata, not a cache of the seed.
        return m_publicKey;
    }

    [[nodiscard]] foundation::result::Result<domain::signer::Signature>
    sign(const std::string& bytes) override
    {
        // A bounded device/account proof consumes its authorized read once.
        // Never fall back to a second Keychain prompt on an expired/used proof.
        // Ordinary long-lived social signers still check Keychain on every sign.
        const std::scoped_lock lock{m_access->mutex};
        PkeyPtr key = std::move(m_oneShotDeviceKey);
        if (m_generation != m_access->generation)
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unauthorized, "Secure signing handle was revoked."));
        if (m_oneShotRequired && (!key || std::chrono::steady_clock::now() >= m_oneShotExpires))
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unauthorized,
                "Single-use secure signing authorization expired or was consumed."));
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
        // A replaced key must never sign under an older public/registry identity.
        const auto actual = derivePublicKey(key.get());
        if (!actual || actual->hex != m_publicKey.hex)
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unauthorized,
                "Protocol signer identity changed; restore authority again."));
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
    domain::signer::PublicKey m_publicKey;
    std::shared_ptr<AccessState> m_access;
    std::uint64_t m_generation{};
    bool m_oneShotRequired{false};
    PkeyPtr m_oneShotDeviceKey{};
    std::chrono::steady_clock::time_point m_oneShotExpires;
};

class KeychainSignerVault final : public capability::signer::ISignerVault {
public:
    explicit KeychainSignerVault(std::string service) : m_service(std::move(service)) {}

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    load(const domain::identity::Fid fid) override
    {
        return loadWithPurpose(fid, false, false);
    }

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    ensure(const domain::identity::Fid fid) override
    {
        return loadWithPurpose(fid, true, false);
    }

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    prepareSignature(const domain::identity::Fid fid, const bool createIfMissing) override
    {
        return loadWithPurpose(fid, createIfMissing, true);
    }

    [[nodiscard]] foundation::result::Status erase(const domain::identity::Fid fid) override
    {
        const auto access = accessFor(m_service, fid);
        const std::scoped_lock lock{access->mutex};
        ++access->generation;
        return deleteSeed(m_service, fid);
    }

private:
    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    loadWithPurpose(const domain::identity::Fid fid, const bool createIfMissing,
                    const bool singleSignature)
    {
        if (fid.value == 0) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Protocol signer vault requires a non-zero FID."));
        }
        const auto access = accessFor(m_service, fid);
        const std::scoped_lock lock{access->mutex};
        auto seed = loadSeed(m_service, fid);
        if (!seed) return std::unexpected(std::move(seed.error()));
        if (*seed) return makeSigner(fid, **seed, singleSignature, access);
        if (!createIfMissing) return std::unique_ptr<capability::signer::INativeSigner>{};

        auto created = createSeed();
        if (!created) return std::unexpected(std::move(created.error()));
        if (auto saved = saveSeed(m_service, fid, *created); !saved)
            return std::unexpected(std::move(saved.error()));
        return makeSigner(fid, *created, singleSignature, access);
    }

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    makeSigner(const domain::identity::Fid fid, const Seed& seed, const bool singleSignature,
               const std::shared_ptr<AccessState>& access)
    {
        auto key = makePrivateKey(seed);
        if (!key) return std::unexpected(std::move(key.error()));
        auto publicKey = derivePublicKey(key->get());
        if (!publicKey) return std::unexpected(std::move(publicKey.error()));
        PkeyPtr oneShot{};
        // Long-lived social handles contain only public metadata. The device
        // seal and explicit proof ceremony may retain one authorized operation.
        if (singleSignature || (m_service == "org.genycaster.device-session" && fid.value == 1U))
            oneShot = std::move(*key);
        return std::unique_ptr<capability::signer::INativeSigner>{
            std::make_unique<KeychainNativeSigner>(m_service, fid, std::move(*publicKey),
                access, access->generation, std::move(oneShot))};
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
