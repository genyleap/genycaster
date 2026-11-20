module;

#include <QByteArray>
#include <QEventLoop>
#include <QObject>
#include <QString>
#if defined(Q_OS_ANDROID)
#include <QThread>
#include <QThreadPool>
#include <future>
#endif
#if defined(Q_OS_WIN)
#include <QCoreApplication>
#include <QSettings>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

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

#if !defined(Q_OS_WIN)
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

#endif

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

#if !defined(Q_OS_WIN)
void runJob(QKeychain::Job& job)
{
    QEventLoop loop{};
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec(QEventLoop::ExcludeUserInputEvents);
}

#if defined(Q_OS_ANDROID)
// QtKeychain 0.17 binds its process-wide JobExecutor to its first caller.
// Keep one non-expiring worker for every job type and every vault instance.
QThreadPool& keychainWorker()
{
    struct Worker final : QThreadPool {
        Worker()
        {
            setMaxThreadCount(1);
            setExpiryTimeout(-1);
        }
    };
    static Worker worker;
    return worker;
}
#endif

template<typename Job, typename Operation>
auto withKeychainJob(const std::string& service, domain::identity::Fid fid, Operation operation)
{
    const auto execute = [&] {
        Job job{QString::fromStdString(service)};
        job.setKey(QString::fromStdString(accountFor(fid)));
        return operation(job);
    };
#if defined(Q_OS_ANDROID)
    auto& worker = keychainWorker();
    if (!worker.contains(QThread::currentThread())) {
        // Both the job and its private QObject must be constructed and
        // destroyed on the executor thread. Wait only for this owned worker.
        std::packaged_task<decltype(execute())()> task{execute};
        auto result = task.get_future();
        worker.start(std::move(task));
        return result.get();
    }
#endif
    return execute();
}

#else
// QtKeychain 0.17 on Windows stores a CRED_TYPE_GENERIC entry named
// "key@service" with the raw 32-byte binary payload. The Qt Job's nested
// event loop is not safe for the signer executor on affected Windows hosts.
// Use the same OS vault and key identity directly, preserving existing keys.
[[nodiscard]] bool qtKeychainCompatibilityFlag(const QString& name)
{
    QSettings user{};
    if (user.value(name, false).toBool()) return true;
    QSettings system{QSettings::SystemScope,
                     QCoreApplication::organizationName(),
                     QCoreApplication::applicationName()};
    return system.value(name, false).toBool();
}

[[nodiscard]] std::wstring credentialTarget(
    const std::string& service, const domain::identity::Fid fid)
{
    const QString key = QString::fromStdString(accountFor(fid));
    if (qtKeychainCompatibilityFlag(QStringLiteral("qtkeychainCompatNamingScheme")))
        return key.toStdWString();
    return (key + QLatin1Char('@') + QString::fromStdString(service)).toStdWString();
}

[[nodiscard]] foundation::error::Error windowsCredentialError(
    const DWORD code, const std::string_view action)
{
    using foundation::error::ErrorCode;
    ErrorCode category = ErrorCode::StorageFailure;
    if (code == ERROR_ACCESS_DENIED) category = ErrorCode::Unauthorized;
    if (code == ERROR_NO_SUCH_LOGON_SESSION) category = ErrorCode::Unavailable;
    return foundation::error::Error::make(
        category,
        "Windows Credential Manager could not " + std::string{action}
            + " the protocol signer (system error " + std::to_string(code) + ").");
}

struct CredentialFree final {
    void operator()(CREDENTIALW* value) const noexcept
    {
        if (value) {
            if (value->CredentialBlob && value->CredentialBlobSize)
                SecureZeroMemory(value->CredentialBlob, value->CredentialBlobSize);
            CredFree(value);
        }
    }
};

[[nodiscard]] foundation::result::Result<std::optional<Seed>> windowsLoadSeed(
    const std::string& service, const domain::identity::Fid fid)
{
    const std::wstring target = credentialTarget(service, fid);
    CREDENTIALW* raw{};
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &raw)) {
        const DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND)
            return std::unexpected(windowsCredentialError(error, "read"));
        // Match the pinned QtKeychain 0.17 optional migration setting.
        if (qtKeychainCompatibilityFlag(
                QStringLiteral("qtkeychainCompatNamingScheme"))
            || !qtKeychainCompatibilityFlag(
                QStringLiteral("qtkeychainFallbackToCompatNamingScheme"))) {
            return std::optional<Seed>{};
        }
        const std::wstring legacy = QString::fromStdString(accountFor(fid)).toStdWString();
        if (!CredReadW(legacy.c_str(), CRED_TYPE_GENERIC, 0, &raw)) {
            const DWORD legacyError = GetLastError();
            if (legacyError == ERROR_NOT_FOUND) return std::optional<Seed>{};
            return std::unexpected(windowsCredentialError(legacyError, "read"));
        }
    }
    const std::unique_ptr<CREDENTIALW, CredentialFree> stored{raw};
    // A GenyCaster Ed25519 seed is exactly 32 bytes; attributes represent
    // QtKeychain's large-data DPAPI format and are never used for our seeds.
    if (stored->CredentialBlobSize != SeedSize || stored->CredentialBlob == nullptr
        || stored->AttributeCount != 0) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::StorageFailure,
            "Windows secure signer entry does not contain a valid Ed25519 seed."));
    }
    Seed seed{};
    std::copy_n(stored->CredentialBlob, SeedSize, seed.bytes.begin());
    return std::optional<Seed>{std::move(seed)};
}

[[nodiscard]] foundation::result::Status windowsSaveSeed(
    const std::string& service, const domain::identity::Fid fid, const Seed& seed)
{
    std::wstring target = credentialTarget(service, fid);
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = target.data();
    credential.CredentialBlob = const_cast<BYTE*>(seed.bytes.data());
    credential.CredentialBlobSize = static_cast<DWORD>(SeedSize);
    credential.Persist = CRED_PERSIST_ENTERPRISE;
    if (!CredWriteW(&credential, 0))
        return std::unexpected(windowsCredentialError(GetLastError(), "store"));
    return {};
}

[[nodiscard]] foundation::result::Status windowsDeleteSeed(
    const std::string& service, const domain::identity::Fid fid)
{
    const std::wstring target = credentialTarget(service, fid);
    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        const DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND)
            return std::unexpected(windowsCredentialError(error, "delete"));
    }
    // If QtKeychain was configured to allow fallback reads from the legacy
    // key-only name, explicit signer revocation must erase that entry too.
    // Otherwise a subsequent restore could silently resurrect the local key.
    if (!qtKeychainCompatibilityFlag(QStringLiteral("qtkeychainCompatNamingScheme"))
        && qtKeychainCompatibilityFlag(
            QStringLiteral("qtkeychainFallbackToCompatNamingScheme"))) {
        const std::wstring legacy = QString::fromStdString(accountFor(fid)).toStdWString();
        if (!CredDeleteW(legacy.c_str(), CRED_TYPE_GENERIC, 0)) {
            const DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND)
                return std::unexpected(windowsCredentialError(error, "delete legacy"));
        }
    }
    return {};
}
#endif

[[nodiscard]] foundation::result::Result<std::optional<Seed>> loadSeed(
    const std::string& service,
    const domain::identity::Fid fid)
{
#if defined(Q_OS_WIN)
    return windowsLoadSeed(service, fid);
#else
    return withKeychainJob<QKeychain::ReadPasswordJob>(service, fid,
        [](QKeychain::ReadPasswordJob& job) -> foundation::result::Result<std::optional<Seed>> {
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
        });
#endif
}

[[nodiscard]] foundation::result::Status saveSeed(
    const std::string& service,
    const domain::identity::Fid fid,
    const Seed& seed)
{
#if defined(Q_OS_WIN)
    return windowsSaveSeed(service, fid, seed);
#else
    return withKeychainJob<QKeychain::WritePasswordJob>(service, fid,
        [&seed](QKeychain::WritePasswordJob& job) -> foundation::result::Status {
            QByteArray data{
                reinterpret_cast<const char*>(seed.bytes.data()),
                static_cast<qsizetype>(seed.bytes.size())};
            job.setBinaryData(data);
            runJob(job);
            OPENSSL_cleanse(data.data(), static_cast<std::size_t>(data.size()));
            if (job.error() != QKeychain::NoError) {
                return std::unexpected(makeStorageError(job.error(), job.errorString(), "store"));
            }
            return {};
        });
#endif
}

[[nodiscard]] foundation::result::Status deleteSeed(
    const std::string& service,
    const domain::identity::Fid fid)
{
#if defined(Q_OS_WIN)
    return windowsDeleteSeed(service, fid);
#else
    return withKeychainJob<QKeychain::DeletePasswordJob>(service, fid,
        [](QKeychain::DeletePasswordJob& job) -> foundation::result::Status {
            runJob(job);
            if (job.error() == QKeychain::NoError || job.error() == QKeychain::EntryNotFound) {
                return {};
            }
            return std::unexpected(makeStorageError(job.error(), job.errorString(), "delete"));
        });
#endif
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
