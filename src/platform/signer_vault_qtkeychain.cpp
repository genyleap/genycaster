module;

#include <QByteArray>
#include <QEventLoop>
#include <QObject>
#include <QString>

// QtKeychain installs its public header as qt6keychain/keychain.h, while
// a source-tree FetchContent build exposes qtkeychain/keychain.h. Both use
// the same QKeychain API; preserve the platform's secure-store backend.
#if __has_include(<qt6keychain/keychain.h>)
#include <qt6keychain/keychain.h>
#elif __has_include(<qtkeychain/keychain.h>)
#include <qtkeychain/keychain.h>
#else
#error "QtKeychain public header is missing; provide Qt6Keychain >= 0.17"
#endif

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
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

constexpr std::size_t SeedSize = 32U;
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

[[nodiscard]] foundation::error::Error makeStorageError(
    const QKeychain::Error code,
    const QString& detail,
    const std::string_view action)
{
    using foundation::error::ErrorCode;
    ErrorCode mapped = ErrorCode::StorageFailure;
    switch (code) {
    case QKeychain::AccessDeniedByUser:
    case QKeychain::AccessDenied:
        mapped = ErrorCode::Unauthorized;
        break;
    case QKeychain::NoBackendAvailable:
    case QKeychain::NotImplemented:
        mapped = ErrorCode::Unavailable;
        break;
    case QKeychain::NoError:
    case QKeychain::EntryNotFound:
    case QKeychain::CouldNotDeleteEntry:
    case QKeychain::OtherError:
        mapped = ErrorCode::StorageFailure;
        break;
    }

    std::string message{"QtKeychain failed to "};
    message += action;
    message += " the protocol signer seed";
    if (!detail.isEmpty()) {
        message += ": ";
        message += detail.toStdString();
    }
    return foundation::error::Error::make(mapped, std::move(message));
}

[[nodiscard]] foundation::error::Error cryptoError(const std::string_view context)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::InternalFailure,
        std::string{context});
}

[[nodiscard]] foundation::error::Error invalidFidError()
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::InvalidArgument,
        "Protocol signer vault requires a non-zero FID.");
}

[[nodiscard]] std::string accountFor(const domain::identity::Fid fid)
{
    return "fid:" + std::to_string(fid.value);
}

void runJob(QKeychain::Job& job)
{
    QEventLoop loop{};
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec(QEventLoop::ExcludeUserInputEvents);
}

[[nodiscard]] foundation::result::Result<std::optional<Seed>> loadSeed(
    const std::string& service,
    const domain::identity::Fid fid)
{
    QKeychain::ReadPasswordJob job{QString::fromStdString(service)};
    job.setKey(QString::fromStdString(accountFor(fid)));
    runJob(job);
    if (job.error() == QKeychain::EntryNotFound) {
        return std::optional<Seed>{};
    }
    if (job.error() != QKeychain::NoError) {
        return std::unexpected(makeStorageError(job.error(), job.errorString(), "read"));
    }

    QByteArray data = job.binaryData();
    if (data.size() != static_cast<qsizetype>(SeedSize)) {
        if (!data.isEmpty()) {
            OPENSSL_cleanse(data.data(), static_cast<std::size_t>(data.size()));
        }
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::StorageFailure,
            "Secure protocol signer entry contains an invalid Ed25519 seed length."));
    }

    Seed seed{};
    std::copy_n(
        reinterpret_cast<const unsigned char*>(data.constData()),
        SeedSize,
        seed.bytes.begin());
    OPENSSL_cleanse(data.data(), static_cast<std::size_t>(data.size()));
    return std::optional<Seed>{std::move(seed)};
}

[[nodiscard]] foundation::result::Status saveSeed(
    const std::string& service,
    const domain::identity::Fid fid,
    const Seed& seed)
{
    QByteArray data{
        reinterpret_cast<const char*>(seed.bytes.data()),
        static_cast<qsizetype>(seed.bytes.size())};
    QKeychain::WritePasswordJob job{QString::fromStdString(service)};
    job.setKey(QString::fromStdString(accountFor(fid)));
    job.setBinaryData(data);
    runJob(job);
    OPENSSL_cleanse(data.data(), static_cast<std::size_t>(data.size()));
    if (job.error() != QKeychain::NoError) {
        return std::unexpected(makeStorageError(job.error(), job.errorString(), "store"));
    }
    return {};
}

[[nodiscard]] foundation::result::Status deleteSeed(
    const std::string& service,
    const domain::identity::Fid fid)
{
    QKeychain::DeletePasswordJob job{QString::fromStdString(service)};
    job.setKey(QString::fromStdString(accountFor(fid)));
    runJob(job);
    if (job.error() == QKeychain::NoError || job.error() == QKeychain::EntryNotFound) {
        return {};
    }
    return std::unexpected(makeStorageError(job.error(), job.errorString(), "delete"));
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
    if (!key) {
        return std::unexpected(cryptoError("OpenSSL failed to construct Ed25519 signer."));
    }
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

class QtKeychainNativeSigner final : public capability::signer::INativeSigner {
public:
    QtKeychainNativeSigner(std::string service, const domain::identity::Fid fid)
        : m_service(std::move(service)), m_fid(fid)
    {
    }

    [[nodiscard]] domain::signer::SignerId signerId() const override
    {
        return domain::signer::SignerId{"qtkeychain:" + accountFor(m_fid)};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> publicKey() const override
    {
        auto loaded = loadSeed(m_service, m_fid);
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        if (!*loaded) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::NotFound,
                "Protocol signer is no longer present in the platform secure store."));
        }
        auto key = makePrivateKey(**loaded);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
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
        auto loaded = loadSeed(m_service, m_fid);
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        if (!*loaded) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::NotFound,
                "Protocol signer is no longer present in the platform secure store."));
        }
        auto key = makePrivateKey(**loaded);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        MdContextPtr context{EVP_MD_CTX_new()};
        if (!context ||
            EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key->get()) != 1) {
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
};

class QtKeychainSignerVault final : public capability::signer::ISignerVault {
public:
    explicit QtKeychainSignerVault(std::string service) : m_service(std::move(service)) {}

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    load(const domain::identity::Fid fid) override
    {
        if (fid.value == 0) {
            return std::unexpected(invalidFidError());
        }
        auto seed = loadSeed(m_service, fid);
        if (!seed) {
            return std::unexpected(std::move(seed.error()));
        }
        if (!*seed) {
            return std::unique_ptr<capability::signer::INativeSigner>{};
        }
        return std::unique_ptr<capability::signer::INativeSigner>{
            std::make_unique<QtKeychainNativeSigner>(m_service, fid)};
    }

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    ensure(const domain::identity::Fid fid) override
    {
        auto existing = load(fid);
        if (!existing) {
            return std::unexpected(std::move(existing.error()));
        }
        if (*existing) {
            return existing;
        }

        auto seed = createSeed();
        if (!seed) {
            return std::unexpected(std::move(seed.error()));
        }
        if (auto saved = saveSeed(m_service, fid, *seed); !saved) {
            return std::unexpected(std::move(saved.error()));
        }
        return std::unique_ptr<capability::signer::INativeSigner>{
            std::make_unique<QtKeychainNativeSigner>(m_service, fid)};
    }

    [[nodiscard]] foundation::result::Status erase(const domain::identity::Fid fid) override
    {
        if (fid.value == 0) {
            return std::unexpected(invalidFidError());
        }
        return deleteSeed(m_service, fid);
    }

private:
    std::string m_service;
};

}

foundation::result::Result<std::unique_ptr<capability::signer::ISignerVault>>
open(std::string serviceName)
{
    if (serviceName.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Platform protocol signer vault requires a non-empty service name."));
    }
    if (!QKeychain::isAvailable()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::Unavailable,
            "No secure QtKeychain backend is available on this platform session."));
    }
    std::unique_ptr<capability::signer::ISignerVault> vault =
        std::make_unique<QtKeychainSignerVault>(std::move(serviceName));
    return vault;
}

}
