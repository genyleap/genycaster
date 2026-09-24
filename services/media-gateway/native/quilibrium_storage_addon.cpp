#include <node_api.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

import quilibrium.core;
import quilibrium.qstorage;
import quilibrium.sigv4;

namespace {

struct SignedRequest final {
    std::string url{};
    quilibrium::http_headers headers{};
};

class QStorageSigner final {
public:
    QStorageSigner(
        quilibrium::endpoint endpoint,
        std::string region,
        std::string bucket,
        std::string accessKeyId,
        std::string secretAccessKey,
        std::string sessionToken)
        : m_endpoint(std::move(endpoint)),
          m_client(
              quilibrium::qstorage::config{
                  .endpoints = {m_endpoint},
                  .auth = {
                      .access_key_id = accessKeyId,
                      .secret_access_key = secretAccessKey,
                      .session_token = sessionToken,
                  },
                  .region = region,
              }),
          m_headerSigner(
              quilibrium::auth::sigv4_credentials{
                  .access_key_id = std::move(accessKeyId),
                  .secret_access_key = std::move(secretAccessKey),
                  .session_token = std::move(sessionToken),
              },
              std::move(region),
              "s3"),
          m_bucket(std::move(bucket))
    {
    }

    [[nodiscard]] quilibrium::result<quilibrium::auth::presigned_request> presignPut(
        std::string key,
        std::string contentType,
        std::chrono::seconds expires) const
    {
        return m_client.presign_put_object(
            m_bucket,
            std::move(key),
            std::move(contentType),
            expires);
    }

    [[nodiscard]] quilibrium::result<quilibrium::auth::presigned_request> presignGet(
        std::string key,
        std::chrono::seconds expires) const
    {
        return m_client.presign_get_object(m_bucket, std::move(key), expires);
    }

    [[nodiscard]] quilibrium::result<quilibrium::auth::presigned_request> presignHead(
        std::string key,
        std::chrono::seconds expires) const
    {
        return m_client.presign_head_object(m_bucket, std::move(key), expires);
    }

    [[nodiscard]] quilibrium::result<SignedRequest> signCreateMultipart(
        std::string key,
        std::string contentType) const
    {
        quilibrium::http_headers headers{{"content-type", std::move(contentType)}};
        return signRequest(
            quilibrium::http_method::post,
            pathFor(key) + "?uploads",
            std::move(headers),
            {});
    }

    [[nodiscard]] quilibrium::result<SignedRequest> signUploadPart(
        std::string key,
        std::string uploadId,
        const std::uint32_t partNumber,
        quilibrium::bytes body) const
    {
        std::string target = pathFor(key);
        target += "?partNumber=";
        target += std::to_string(partNumber);
        target += "&uploadId=";
        target += quilibrium::percent_encode(uploadId);
        return signRequest(
            quilibrium::http_method::put,
            std::move(target),
            {},
            std::move(body));
    }

    [[nodiscard]] quilibrium::result<SignedRequest> signCompleteMultipart(
        std::string key,
        std::string uploadId,
        quilibrium::bytes completionXml) const
    {
        std::string target = pathFor(key);
        target += "?uploadId=";
        target += quilibrium::percent_encode(uploadId);
        quilibrium::http_headers headers{{"content-type", "application/xml"}};
        return signRequest(
            quilibrium::http_method::post,
            std::move(target),
            std::move(headers),
            std::move(completionXml));
    }

    [[nodiscard]] quilibrium::result<SignedRequest> signAbortMultipart(
        std::string key,
        std::string uploadId) const
    {
        std::string target = pathFor(key);
        target += "?uploadId=";
        target += quilibrium::percent_encode(uploadId);
        return signRequest(
            quilibrium::http_method::del,
            std::move(target),
            {},
            {});
    }

    [[nodiscard]] quilibrium::result<SignedRequest> signHeadObject(std::string key) const
    {
        return signRequest(
            quilibrium::http_method::head,
            pathFor(key),
            {},
            {});
    }

    [[nodiscard]] quilibrium::result<SignedRequest> signDeleteObject(std::string key) const
    {
        return signRequest(
            quilibrium::http_method::del,
            pathFor(key),
            {},
            {});
    }

private:
    [[nodiscard]] std::string pathFor(const std::string& key) const
    {
        std::string target{"/"};
        target += quilibrium::percent_encode(m_bucket);
        target += "/";
        target += quilibrium::percent_encode(key, true);
        return target;
    }

    [[nodiscard]] quilibrium::result<SignedRequest> signRequest(
        const quilibrium::http_method method,
        std::string target,
        quilibrium::http_headers headers,
        quilibrium::bytes body) const
    {
        quilibrium::http_request request{
            .verb = method,
            .target_endpoint = m_endpoint,
            .target = std::move(target),
            .header_fields = std::move(headers),
            .body = std::move(body),
        };
        auto signedStatus = m_headerSigner.sign(request);
        if (!signedStatus) {
            return std::unexpected(signedStatus.error());
        }

        std::string url = m_endpoint.origin();
        if (!m_endpoint.base_path.empty()) {
            if (!m_endpoint.base_path.starts_with('/')) {
                url.push_back('/');
            }
            url += m_endpoint.base_path;
        }
        if (!request.target.empty() && !request.target.starts_with('/')) {
            url.push_back('/');
        }
        url += request.target;

        request.header_fields.erase("host");
        return SignedRequest{
            .url = std::move(url),
            .headers = std::move(request.header_fields),
        };
    }

    quilibrium::endpoint m_endpoint;
    quilibrium::qstorage::client m_client;
    quilibrium::auth::sigv4_signer m_headerSigner;
    std::string m_bucket;
};

void throwNapiError(napi_env env, const char* message)
{
    napi_throw_error(env, nullptr, message);
}

void requireStatus(napi_env, napi_status status, const char* message)
{
    if (status != napi_ok) {
        throw std::runtime_error{message};
    }
}

[[nodiscard]] napi_value namedProperty(napi_env env, napi_value object, const char* name)
{
    napi_value value{};
    requireStatus(env, napi_get_named_property(env, object, name, &value), "Unable to read native addon argument.");
    return value;
}

[[nodiscard]] std::string stringValue(napi_env env, napi_value value, const char* name)
{
    napi_valuetype type{};
    requireStatus(env, napi_typeof(env, value, &type), "Unable to inspect native addon argument.");
    if (type != napi_string) {
        throw std::runtime_error{std::string{name} + " must be a string."};
    }

    std::size_t size{};
    requireStatus(env, napi_get_value_string_utf8(env, value, nullptr, 0, &size), "Unable to size native addon string.");
    std::string result(size + 1U, '\0');
    std::size_t written{};
    requireStatus(
        env,
        napi_get_value_string_utf8(env, value, result.data(), result.size(), &written),
        "Unable to read native addon string.");
    result.resize(written);
    return result;
}

[[nodiscard]] std::string requiredStringProperty(napi_env env, napi_value object, const char* name)
{
    auto result = stringValue(env, namedProperty(env, object, name), name);
    if (result.empty()) {
        throw std::runtime_error{std::string{name} + " must not be empty."};
    }
    return result;
}

[[nodiscard]] std::string optionalStringProperty(napi_env env, napi_value object, const char* name)
{
    bool hasProperty{};
    requireStatus(env, napi_has_named_property(env, object, name, &hasProperty), "Unable to inspect native addon object.");
    if (!hasProperty) {
        return {};
    }

    napi_value value = namedProperty(env, object, name);
    napi_valuetype type{};
    requireStatus(env, napi_typeof(env, value, &type), "Unable to inspect native addon argument.");
    if (type == napi_undefined || type == napi_null) {
        return {};
    }
    return stringValue(env, value, name);
}

[[nodiscard]] std::uint32_t integerValue(
    napi_env env,
    napi_value value,
    const char* name,
    const std::uint32_t minimum,
    const std::uint32_t maximum)
{
    napi_valuetype type{};
    requireStatus(env, napi_typeof(env, value, &type), "Unable to inspect native addon argument.");
    if (type != napi_number) {
        throw std::runtime_error{std::string{name} + " must be a number."};
    }

    double rawValue{};
    requireStatus(env, napi_get_value_double(env, value, &rawValue), "Unable to read native addon number.");
    if (!std::isfinite(rawValue) || std::floor(rawValue) != rawValue ||
        rawValue < static_cast<double>(minimum) || rawValue > static_cast<double>(maximum)) {
        throw std::runtime_error{
            std::string{name} + " must be an integer between " +
            std::to_string(minimum) + " and " + std::to_string(maximum) + "."};
    }
    return static_cast<std::uint32_t>(rawValue);
}

[[nodiscard]] std::uint32_t expiryValue(napi_env env, napi_value value, const char* name)
{
    return integerValue(env, value, name, 1U, 604800U);
}

[[nodiscard]] quilibrium::bytes bufferValue(napi_env env, napi_value value, const char* name)
{
    bool isBuffer{};
    requireStatus(env, napi_is_buffer(env, value, &isBuffer), "Unable to inspect native addon buffer.");
    if (!isBuffer) {
        throw std::runtime_error{std::string{name} + " must be a Buffer."};
    }

    void* data{};
    std::size_t size{};
    requireStatus(env, napi_get_buffer_info(env, value, &data, &size), "Unable to read native addon buffer.");
    quilibrium::bytes result(size);
    if (size > 0U) {
        std::memcpy(result.data(), data, size);
    }
    return result;
}

[[nodiscard]] QStorageSigner* signerFromValue(napi_env env, napi_value value)
{
    void* pointer{};
    requireStatus(env, napi_get_value_external(env, value, &pointer), "Invalid Quilibrium QStorage signer handle.");
    if (pointer == nullptr) {
        throw std::runtime_error{"Invalid Quilibrium QStorage signer handle."};
    }
    return static_cast<QStorageSigner*>(pointer);
}

void finalizeSigner(napi_env, void* data, void*)
{
    delete static_cast<QStorageSigner*>(data);
}

[[nodiscard]] napi_value stringResult(napi_env env, const std::string& value)
{
    napi_value result{};
    requireStatus(
        env,
        napi_create_string_utf8(env, value.data(), value.size(), &result),
        "Unable to create native addon string result.");
    return result;
}

void setNamedString(napi_env env, napi_value object, const char* name, const std::string& value)
{
    requireStatus(
        env,
        napi_set_named_property(env, object, name, stringResult(env, value)),
        "Unable to create native addon response.");
}

[[nodiscard]] napi_value headersResult(napi_env env, const quilibrium::http_headers& headers)
{
    napi_value result{};
    requireStatus(env, napi_create_object(env, &result), "Unable to create native addon headers object.");
    for (const auto& [name, value] : headers) {
        requireStatus(
            env,
            napi_set_named_property(env, result, name.c_str(), stringResult(env, value)),
            "Unable to add native addon response header.");
    }
    return result;
}

[[nodiscard]] napi_value signedRequestResult(
    napi_env env,
    quilibrium::result<SignedRequest> result)
{
    if (!result) {
        throw std::runtime_error{result.error().message};
    }

    napi_value object{};
    requireStatus(env, napi_create_object(env, &object), "Unable to create native addon response object.");
    setNamedString(env, object, "url", result->url);
    requireStatus(
        env,
        napi_set_named_property(env, object, "headers", headersResult(env, result->headers)),
        "Unable to attach native addon response headers.");
    return object;
}

[[nodiscard]] napi_value presignedResult(
    napi_env env,
    quilibrium::result<quilibrium::auth::presigned_request> result)
{
    if (!result) {
        throw std::runtime_error{result.error().message};
    }

    napi_value object{};
    requireStatus(env, napi_create_object(env, &object), "Unable to create native addon response object.");
    setNamedString(env, object, "url", result->url);
    requireStatus(
        env,
        napi_set_named_property(env, object, "requiredHeaders", headersResult(env, result->required_headers)),
        "Unable to attach native addon response headers.");

    const auto expiresAtMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        result->expires_at.time_since_epoch()).count();
    napi_value expiresValue{};
    requireStatus(
        env,
        napi_create_int64(env, expiresAtMs, &expiresValue),
        "Unable to create native addon expiration value.");
    requireStatus(
        env,
        napi_set_named_property(env, object, "expiresAtMs", expiresValue),
        "Unable to attach native addon expiration value.");
    return object;
}

[[nodiscard]] napi_value createQStorageSigner(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 1U;
        napi_value argv[1]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 1U) {
            throw std::runtime_error{"createQStorageSigner expects one configuration object."};
        }

        const std::string endpointText = requiredStringProperty(env, argv[0], "endpoint");
        const std::string region = requiredStringProperty(env, argv[0], "region");
        const std::string bucket = requiredStringProperty(env, argv[0], "bucket");
        const std::string accessKeyId = requiredStringProperty(env, argv[0], "accessKeyId");
        const std::string secretAccessKey = requiredStringProperty(env, argv[0], "secretAccessKey");
        const std::string sessionToken = optionalStringProperty(env, argv[0], "sessionToken");

        auto endpoint = quilibrium::parse_endpoint(endpointText);
        if (!endpoint) {
            throw std::runtime_error{endpoint.error().message};
        }

        auto signer = std::make_unique<QStorageSigner>(
            std::move(*endpoint),
            region,
            bucket,
            accessKeyId,
            secretAccessKey,
            sessionToken);

        napi_value external{};
        requireStatus(
            env,
            napi_create_external(env, signer.get(), finalizeSigner, nullptr, &external),
            "Unable to create native QStorage signer handle.");
        signer.release();
        return external;
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value presignPut(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 4U;
        napi_value argv[4]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 4U) {
            throw std::runtime_error{"presignPut expects signer, key, contentType and expiresSeconds."};
        }

        auto* signer = signerFromValue(env, argv[0]);
        return presignedResult(
            env,
            signer->presignPut(
                stringValue(env, argv[1], "key"),
                stringValue(env, argv[2], "contentType"),
                std::chrono::seconds{expiryValue(env, argv[3], "expiresSeconds")}));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value presignGet(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 3U;
        napi_value argv[3]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 3U) {
            throw std::runtime_error{"presignGet expects signer, key and expiresSeconds."};
        }

        auto* signer = signerFromValue(env, argv[0]);
        return presignedResult(
            env,
            signer->presignGet(
                stringValue(env, argv[1], "key"),
                std::chrono::seconds{expiryValue(env, argv[2], "expiresSeconds")}));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value presignHead(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 3U;
        napi_value argv[3]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 3U) {
            throw std::runtime_error{"presignHead expects signer, key and expiresSeconds."};
        }

        auto* signer = signerFromValue(env, argv[0]);
        return presignedResult(
            env,
            signer->presignHead(
                stringValue(env, argv[1], "key"),
                std::chrono::seconds{expiryValue(env, argv[2], "expiresSeconds")}));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value signCreateMultipart(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 3U;
        napi_value argv[3]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 3U) {
            throw std::runtime_error{"signCreateMultipart expects signer, key and contentType."};
        }
        auto* signer = signerFromValue(env, argv[0]);
        return signedRequestResult(
            env,
            signer->signCreateMultipart(
                stringValue(env, argv[1], "key"),
                stringValue(env, argv[2], "contentType")));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value signUploadPart(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 5U;
        napi_value argv[5]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 5U) {
            throw std::runtime_error{"signUploadPart expects signer, key, uploadId, partNumber and body."};
        }
        auto* signer = signerFromValue(env, argv[0]);
        return signedRequestResult(
            env,
            signer->signUploadPart(
                stringValue(env, argv[1], "key"),
                stringValue(env, argv[2], "uploadId"),
                integerValue(env, argv[3], "partNumber", 1U, 10000U),
                bufferValue(env, argv[4], "body")));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value signCompleteMultipart(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 4U;
        napi_value argv[4]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 4U) {
            throw std::runtime_error{"signCompleteMultipart expects signer, key, uploadId and completionXml."};
        }
        auto* signer = signerFromValue(env, argv[0]);
        return signedRequestResult(
            env,
            signer->signCompleteMultipart(
                stringValue(env, argv[1], "key"),
                stringValue(env, argv[2], "uploadId"),
                bufferValue(env, argv[3], "completionXml")));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value signAbortMultipart(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 3U;
        napi_value argv[3]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 3U) {
            throw std::runtime_error{"signAbortMultipart expects signer, key and uploadId."};
        }
        auto* signer = signerFromValue(env, argv[0]);
        return signedRequestResult(
            env,
            signer->signAbortMultipart(
                stringValue(env, argv[1], "key"),
                stringValue(env, argv[2], "uploadId")));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value signHeadObject(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 2U;
        napi_value argv[2]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 2U) {
            throw std::runtime_error{"signHeadObject expects signer and key."};
        }
        auto* signer = signerFromValue(env, argv[0]);
        return signedRequestResult(
            env,
            signer->signHeadObject(stringValue(env, argv[1], "key")));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value signDeleteObject(napi_env env, napi_callback_info info)
{
    try {
        std::size_t argc = 2U;
        napi_value argv[2]{};
        requireStatus(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), "Unable to read native addon arguments.");
        if (argc != 2U) {
            throw std::runtime_error{"signDeleteObject expects signer and key."};
        }
        auto* signer = signerFromValue(env, argv[0]);
        return signedRequestResult(
            env,
            signer->signDeleteObject(stringValue(env, argv[1], "key")));
    } catch (const std::exception& error) {
        throwNapiError(env, error.what());
        return nullptr;
    }
}

[[nodiscard]] napi_value initialize(napi_env env, napi_value exports)
{
    const napi_property_descriptor properties[] = {
        {"createQStorageSigner", nullptr, createQStorageSigner, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"presignPut", nullptr, presignPut, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"presignGet", nullptr, presignGet, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"presignHead", nullptr, presignHead, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"signCreateMultipart", nullptr, signCreateMultipart, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"signUploadPart", nullptr, signUploadPart, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"signCompleteMultipart", nullptr, signCompleteMultipart, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"signAbortMultipart", nullptr, signAbortMultipart, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"signHeadObject", nullptr, signHeadObject, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"signDeleteObject", nullptr, signDeleteObject, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    if (napi_define_properties(env, exports, std::size(properties), properties) != napi_ok) {
        throwNapiError(env, "Unable to initialize the Quilibrium native addon.");
        return nullptr;
    }
    return exports;
}

} // namespace

NAPI_MODULE(genyleap_quilibrium_storage, initialize)
