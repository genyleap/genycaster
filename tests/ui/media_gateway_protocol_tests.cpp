/* Hostile multipart response regressions through the REAL Qt media adapter. */
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

import farcaster.capability.media;
import farcaster.capability.write_authority;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.transport.async_http;
import farcaster.transport.http;
import farcaster.transport.qtnetwork.media;

namespace {
namespace media = farcaster::transport::qtnetwork::media;
namespace http = farcaster::transport::http;
namespace async = farcaster::transport::async_http;
namespace core = farcaster::foundation;
namespace domain = farcaster::domain;

class FakeAuthority final : public farcaster::capability::write::IWriteAuthority {
public:
    [[nodiscard]] core::result::Result<domain::signer::PublicKey> requireSigner(
        domain::identity::Fid, domain::signer::Scope) const override
    {
        return domain::signer::PublicKey{.hex = std::string(64, 'a')};
    }

    [[nodiscard]] core::result::Result<domain::signer::Signature> sign(
        domain::signer::Scope, const std::string&) override
    {
        return domain::signer::Signature{.bytes = std::string(64, 's')};
    }

    [[nodiscard]] core::result::Status recordConfirmedWrite(
        domain::identity::Fid, core::time::Timestamp) override
    {
        return {};
    }
};

[[nodiscard]] std::string ticket(std::string_view partSize, std::string_view partCount)
{
    return R"({"uploadId":"u1","mode":"gateway-multipart","assetUrl":"https://media.example/a.png","partSizeBytes":)"
        + std::string{partSize} + R"(,"partCount":)" + std::string{partCount} + "}";
}

[[nodiscard]] std::string completion(std::string_view sizeField)
{
    return R"({"assetUrl":"https://media.example/a.png","managementToken":"opaque")"
        + (sizeField.empty() ? std::string{} : R"(,"sizeBytes":)" + std::string{sizeField}) + "}";
}

class FakeTransport final : public async::IAsyncHttpTransport {
public:
    std::string creation = ticket("8", "1");
    std::string part = R"({"partNumber":1,"sizeBytes":8,"etag":"good"})";
    std::string finished = completion("8");
    int calls{};
    int puts{};
    int completes{};
    int aborts{};

    [[nodiscard]] core::result::Result<async::Operation> start(
        async::Request request, async::Completion callback) override
    {
        ++calls;
        std::string body{};
        if (request.method == async::Method::Delete) {
            ++aborts;
            body = "{}";
        } else if (request.method == async::Method::Put) {
            ++puts;
            body = part;
        } else if (request.url.find("/v1/uploads/complete") != std::string::npos) {
            ++completes;
            body = finished;
        } else {
            body = creation;
        }
        callback(http::HttpResponse{.statusCode = 200U, .body = std::move(body)});
        return async::Operation{};
    }
};

void require(const bool passed, const char* why)
{
    if (!passed) qFatal("Media gateway protocol regression: %s", why);
}

[[nodiscard]] core::result::Result<farcaster::capability::media::ManagedAsset>
upload(FakeAuthority& authority, FakeTransport& transport, const std::filesystem::path& path)
{
    media::Config config{};
    config.baseUrl = "https://media.example";
    auto gateway = media::makeGateway(authority, transport, config);
    require(gateway.has_value(), "cannot construct real media gateway");
    constexpr std::uint64_t BasicImageLimit = 2U * 1024U * 1024U;
    return (*gateway)->upload(domain::identity::Fid{42U}, path,
                              BasicImageLimit, {}, {});
}
}

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};
    QTemporaryDir temporary{};
    require(temporary.isValid(), "cannot create temporary image directory");
    const QString imagePath = temporary.filePath(QStringLiteral("fixture.png"));
    {
        QFile file{imagePath};
        require(file.open(QIODevice::WriteOnly), "cannot create PNG input");
        const QByteArray header = QByteArray::fromHex("89504e470d0a1a0a");
        require(file.write(header) == header.size(), "cannot write PNG signature");
    }
    const auto path = std::filesystem::path{imagePath.toStdString()};
    FakeAuthority authority{};

    // Positive control: complete authenticated 3-step upload remains valid.
    {
        FakeTransport transport{};
        const auto result = upload(authority, transport, path);
        require(result && result->asset.sizeBytes == 8U, "valid upload failed");
        require(transport.calls == 3 && transport.puts == 1 && transport.completes == 1,
                "valid multipart sequence not preserved");
    }
    // Invalid ticket fields must fail BEFORE issuing a PUT. A hostile JSON
    // number must never reach the old unchecked floating-to-uint64 cast.
    for (const char* value : {"-1", "-0.5", "1.5", "1e100", "null", R"("8")", "67108865"}) {
        FakeTransport transport{};
        transport.creation = ticket(value, "1");
        const auto result = upload(authority, transport, path);
        require(!result && result.error().code == core::error::ErrorCode::ProtocolFailure,
                "invalid part size was accepted");
        require(transport.calls == 1 && transport.puts == 0,
                "invalid ticket sent an upload request");
    }
    for (const char* value : {"-1", "0.5", "1e100", "10001", "null"}) {
        FakeTransport transport{};
        transport.creation = ticket("8", value);
        const auto result = upload(authority, transport, path);
        require(!result && result.error().code == core::error::ErrorCode::ProtocolFailure,
                "invalid part count was accepted");
        require(transport.calls == 1, "invalid count started multipart transfer");
    }
    // The upload part reply and finalization are separate trust boundaries.
    for (const char* value : {"-1", "1e100", "0.5", "9"}) {
        FakeTransport transport{};
        transport.part = std::string{R"({"partNumber":1,"sizeBytes":)"}
            + value + R"(,"etag":"good"})";
        const auto result = upload(authority, transport, path);
        require(!result && result.error().code == core::error::ErrorCode::ProtocolFailure,
                "invalid returned size was accepted");
        require(transport.aborts == 1, "invalid part was not aborted");
    }
    for (const char* value : {"-1", "1e100", "0.5", "9"}) {
        FakeTransport transport{};
        transport.finished = completion(value);
        const auto result = upload(authority, transport, path);
        require(!result && result.error().code == core::error::ErrorCode::ProtocolFailure,
                "invalid final size was accepted");
    }
    // Protocol permits omission of sizeBytes on the terminal reply: it then
    // inherits the local, previously verified byte count.
    {
        FakeTransport transport{};
        transport.finished = completion("");
        const auto result = upload(authority, transport, path);
        require(result && result->asset.sizeBytes == 8U, "valid legacy final size fallback failed");
    }
    return 0;
}
