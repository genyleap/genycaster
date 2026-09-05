module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <charconv>
#include <cmath>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.provider.emoji.emoji_asset_validator;

import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::provider::emoji::validator {
namespace {

[[nodiscard]] foundation::error::Error error(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] constexpr std::uint32_t rotateRight(
    const std::uint32_t value,
    const std::uint32_t bits) noexcept
{
    return (value >> bits) | (value << (32U - bits));
}

[[nodiscard]] bool looksLikeUrl(const std::string_view value) noexcept
{
    for (const std::string_view prefix : {"http://", "https://", "data:", "file:", "ftp:"}) {
        if (value.size() >= prefix.size() &&
            value.substr(0, prefix.size()) == prefix) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool parseDoubleValue(const std::string_view text, double& out) noexcept
{
    if (text.empty()) {
        return false;
    }
    double value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value)) {
        return false;
    }
    out = value;
    return true;
}

} // namespace

Sha256 sha256(const std::span<const std::byte> bytes) noexcept
{
    // FIPS 180-4 SHA-256. Self-contained so core validation never depends on
    // a platform crypto API.
    static constexpr std::array<std::uint32_t, 64> K{{
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
        0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
        0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
        0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
    }};

    const std::size_t size = bytes.size();
    const std::size_t paddedLength = ((size + 8U) / 64U + 1U) * 64U;
    std::vector<std::byte> padded(paddedLength, std::byte{0});
    if (size > 0U) {
        std::memcpy(padded.data(), bytes.data(), size);
    }
    padded[size] = std::byte{0x80U};
    const std::uint64_t bitLength = static_cast<std::uint64_t>(size) * 8U;
    for (std::size_t index = 0; index < 8U; ++index) {
        padded[paddedLength - 1U - index] =
            static_cast<std::byte>((bitLength >> (index * 8U)) & 0xFFU);
    }

    std::array<std::uint32_t, 8> hash{{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    }};

    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t chunk = 0; chunk < paddedLength; chunk += 64U) {
        for (std::size_t index = 0; index < 16U; ++index) {
            const std::size_t offset = chunk + index * 4U;
            schedule[index] =
                (static_cast<std::uint32_t>(std::to_integer<unsigned char>(padded[offset])) << 24U) |
                (static_cast<std::uint32_t>(std::to_integer<unsigned char>(padded[offset + 1U])) << 16U) |
                (static_cast<std::uint32_t>(std::to_integer<unsigned char>(padded[offset + 2U])) << 8U) |
                static_cast<std::uint32_t>(std::to_integer<unsigned char>(padded[offset + 3U]));
        }
        for (std::size_t index = 16U; index < 64U; ++index) {
            const auto s0 = rotateRight(schedule[index - 15U], 7U) ^
                rotateRight(schedule[index - 15U], 18U) ^ (schedule[index - 15U] >> 3U);
            const auto s1 = rotateRight(schedule[index - 2U], 17U) ^
                rotateRight(schedule[index - 2U], 19U) ^ (schedule[index - 2U] >> 10U);
            schedule[index] = schedule[index - 16U] + s0 + schedule[index - 7U] + s1;
        }

        auto a = hash[0];
        auto b = hash[1];
        auto c = hash[2];
        auto d = hash[3];
        auto e = hash[4];
        auto f = hash[5];
        auto g = hash[6];
        auto h = hash[7];
        for (std::size_t index = 0; index < 64U; ++index) {
            const auto s1 = rotateRight(e, 6U) ^ rotateRight(e, 11U) ^ rotateRight(e, 25U);
            const auto ch = (e & f) ^ ((~e) & g);
            const auto temp1 = h + s1 + ch + K[index] + schedule[index];
            const auto s0 = rotateRight(a, 2U) ^ rotateRight(a, 13U) ^ rotateRight(a, 22U);
            const auto maj = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    Sha256 digest{};
    for (std::size_t index = 0; index < 8U; ++index) {
        digest[index * 4U] = static_cast<std::byte>((hash[index] >> 24U) & 0xFFU);
        digest[index * 4U + 1U] = static_cast<std::byte>((hash[index] >> 16U) & 0xFFU);
        digest[index * 4U + 2U] = static_cast<std::byte>((hash[index] >> 8U) & 0xFFU);
        digest[index * 4U + 3U] = static_cast<std::byte>(hash[index] & 0xFFU);
    }
    return digest;
}

namespace {

/**
 * @brief Single-pass strict JSON scanner for Lottie validation.
 *
 * Validates RFC 8259 syntax with hard depth/token budgets, counts layers and
 * assets, captures the top-level numeric metadata and rejects unsupported
 * Lottie features (3D layers, image/text layers, external resource references,
 * unexpected URLs, font data). No DOM is materialized.
 */
class LottieJsonScanner final {
public:
    LottieJsonScanner(
        std::string_view input,
        ValidationLimits limits,
        std::uint32_t& layers,
        std::uint32_t& assets,
        double& width,
        double& height,
        double& frameRate,
        double& inPoint,
        double& outPoint,
        std::string& version)
        : m_input(input),
          m_limits(limits),
          m_layers(layers),
          m_assets(assets),
          m_width(width),
          m_height(height),
          m_frameRate(frameRate),
          m_inPoint(inPoint),
          m_outPoint(outPoint),
          m_version(version)
    {
    }

    [[nodiscard]] bool scan() noexcept
    {
        skipWhitespace();
        const Kind root = parseValue(0U);
        if (root != Kind::Object) {
            (void)fail("emoji asset root must be a JSON object");
            return false;
        }
        skipWhitespace();
        return m_cursor == m_input.size();
    }

    [[nodiscard]] std::string_view failure() const noexcept { return m_failure; }

private:
    enum class Kind : std::uint8_t {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    [[nodiscard]] bool fail(const std::string_view reason) noexcept
    {
        if (m_failure.empty()) {
            m_failure = reason;
        }
        return false;
    }

    void skipWhitespace() noexcept
    {
        while (m_cursor < m_input.size()) {
            const char ch = m_input[m_cursor];
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                ++m_cursor;
            } else {
                break;
            }
        }
    }

    [[nodiscard]] bool consume(const char expected) noexcept
    {
        if (m_cursor < m_input.size() && m_input[m_cursor] == expected) {
            ++m_cursor;
            return true;
        }
        return false;
    }

    [[nodiscard]] Kind parseValue(const std::size_t depth) noexcept
    {
        if (depth > m_limits.maxJsonDepth) {
            (void)fail("JSON nesting exceeds the validation depth limit");
            return Kind::Null;
        }
        if (++m_tokenCount > m_limits.maxJsonTokens) {
            (void)fail("JSON token budget exceeded");
            return Kind::Null;
        }
        skipWhitespace();
        if (m_cursor >= m_input.size()) {
            (void)fail("unexpected end of JSON");
            return Kind::Null;
        }
        const char ch = m_input[m_cursor];
        if (ch == '{') {
            return parseObject(depth);
        }
        if (ch == '[') {
            return parseArray(depth);
        }
        if (ch == '"') {
            if (!parseString()) {
                return Kind::Null;
            }
            return Kind::String;
        }
        if (ch == 't') {
            return consumeLiteral("true") ? Kind::Bool : Kind::Null;
        }
        if (ch == 'f') {
            return consumeLiteral("false") ? Kind::Bool : Kind::Null;
        }
        if (ch == 'n') {
            return consumeLiteral("null") ? Kind::Null : Kind::Null;
        }
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            return parseNumber() ? Kind::Number : Kind::Null;
        }
        (void)fail("unexpected JSON token");
        return Kind::Null;
    }

    [[nodiscard]] bool consumeLiteral(const std::string_view literal) noexcept
    {
        if (m_input.substr(m_cursor, literal.size()) != literal) {
            (void)fail("invalid JSON literal");
            return false;
        }
        m_cursor += literal.size();
        return true;
    }

    [[nodiscard]] bool parseNumber() noexcept
    {
        const std::size_t start = m_cursor;
        if (m_cursor < m_input.size() && m_input[m_cursor] == '-') {
            ++m_cursor;
        }
        if (m_cursor >= m_input.size()) {
            (void)fail("incomplete JSON number");
            return false;
        }
        if (m_input[m_cursor] == '0') {
            ++m_cursor;
        } else if (m_input[m_cursor] >= '1' && m_input[m_cursor] <= '9') {
            while (m_cursor < m_input.size() && m_input[m_cursor] >= '0' && m_input[m_cursor] <= '9') {
                ++m_cursor;
            }
        } else {
            (void)fail("invalid JSON number");
            return false;
        }
        if (m_cursor < m_input.size() && m_input[m_cursor] == '.') {
            ++m_cursor;
            if (m_cursor >= m_input.size() || m_input[m_cursor] < '0' || m_input[m_cursor] > '9') {
                (void)fail("invalid JSON fraction");
                return false;
            }
            while (m_cursor < m_input.size() && m_input[m_cursor] >= '0' && m_input[m_cursor] <= '9') {
                ++m_cursor;
            }
        }
        if (m_cursor < m_input.size() && (m_input[m_cursor] == 'e' || m_input[m_cursor] == 'E')) {
            ++m_cursor;
            if (m_cursor < m_input.size() && (m_input[m_cursor] == '+' || m_input[m_cursor] == '-')) {
                ++m_cursor;
            }
            if (m_cursor >= m_input.size() || m_input[m_cursor] < '0' || m_input[m_cursor] > '9') {
                (void)fail("invalid JSON exponent");
                return false;
            }
            while (m_cursor < m_input.size() && m_input[m_cursor] >= '0' && m_input[m_cursor] <= '9') {
                ++m_cursor;
            }
        }
        m_lastNumber = std::string_view{m_input}.substr(start, m_cursor - start);
        return true;
    }

    [[nodiscard]] bool parseString() noexcept
    {
        ++m_cursor; // opening quote
        std::string value{};
        value.reserve(32);
        while (m_cursor < m_input.size()) {
            const char ch = m_input[m_cursor];
            if (ch == '"') {
                ++m_cursor;
                m_lastString = std::move(value);
                return true;
            }
            if (static_cast<unsigned char>(ch) < 0x20U) {
                (void)fail("unescaped control character in JSON string");
                return false;
            }
            if (ch == '\\') {
                ++m_cursor;
                if (m_cursor >= m_input.size()) {
                    (void)fail("incomplete JSON escape");
                    return false;
                }
                const char escaped = m_input[m_cursor];
                switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    value.push_back(escaped);
                    break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case 'u':
                    if (m_cursor + 4U >= m_input.size()) {
                        (void)fail("incomplete unicode escape");
                        return false;
                    }
                    m_cursor += 4U;
                    break;
                default:
                    (void)fail("invalid JSON escape");
                    return false;
                }
                ++m_cursor;
                continue;
            }
            value.push_back(ch);
            ++m_cursor;
        }
        (void)fail("unterminated JSON string");
        return false;
    }

    /** @brief Validates one object member after its value has been parsed. */
    [[nodiscard]] bool finalizeMember(
        const std::string_view key,
        const Kind kind,
        const std::size_t depth) noexcept
    {
        // These invariants apply at every nesting level.
        if (key == "ddd") {
            if (kind != Kind::Number) {
                return fail("Lottie 3D flag must be a number");
            }
            double value{};
            if (!parseDoubleValue(m_lastNumber, value)) {
                return fail("Lottie 3D flag is out of range");
            }
            if (value != 0.0) {
                return fail("3D layers are unsupported in emoji assets");
            }
            return true;
        }
        if (key == "chars") {
            return fail("embedded font/text data is unsupported in emoji assets");
        }
        if (kind == Kind::String && looksLikeUrl(m_lastString)) {
            return fail("unexpected URL string inside emoji asset");
        }

        if (depth == 0U) {
            if (key == "w" || key == "h" || key == "fr" || key == "ip" || key == "op") {
                if (kind != Kind::Number) {
                    return fail("top-level Lottie metadata field must be a number");
                }
                double value{};
                if (!parseDoubleValue(m_lastNumber, value)) {
                    return fail("top-level Lottie metadata number is out of range");
                }
                if (key == "w") m_width = value;
                else if (key == "h") m_height = value;
                else if (key == "fr") m_frameRate = value;
                else if (key == "ip") m_inPoint = value;
                else m_outPoint = value;
                return true;
            }
            if (key == "v") {
                if (kind != Kind::String) {
                    return fail("Lottie version field must be a string");
                }
                m_version = m_lastString;
                return true;
            }
            if (key == "layers" && kind != Kind::Array) {
                return fail("Lottie layers field must be an array");
            }
            if (key == "assets" && kind != Kind::Array) {
                return fail("Lottie assets field must be an array");
            }
            return true;
        }

        // Nested layer/asset validation.
        if (key == "ty") {
            // Shape operators use string types; only layer types are numeric.
            if (kind == Kind::String) {
                for (const std::string_view type : {"gr", "sh", "fl", "gf", "st", "gs", "tr", "el", "rc", "sr", "tm", "rd", "rp", "mm", "pb", "zz", "op"}) {
                    if (m_lastString == type) {
                        return true;
                    }
                }
                return fail("unknown Lottie shape type");
            }
            if (kind != Kind::Number) {
                return fail("Lottie layer type must be a number");
            }
            double value{};
            if (!parseDoubleValue(m_lastNumber, value)) {
                return fail("Lottie layer type is out of range");
            }
            if (value != std::floor(value) || value < 0.0 || value > 5.0 || value == 2.0 || value == 5.0) {
                return fail("image and text layers are unsupported in emoji assets");
            }
            return true;
        }
        if (key == "u" || key == "p" || key == "e") {
            if (kind == Kind::String && !m_lastString.empty()) {
                return fail("external or embedded resource references are unsupported");
            }
            return true;
        }
        return true;
    }

    [[nodiscard]] Kind parseObject(const std::size_t depth) noexcept
    {
        ++m_cursor; // '{'
        skipWhitespace();
        if (m_cursor < m_input.size() && m_input[m_cursor] == '}') {
            ++m_cursor;
            return Kind::Object;
        }
        bool firstMember = true;
        while (m_cursor < m_input.size()) {
            if (!firstMember) {
                skipWhitespace();
                if (!consume(',')) {
                    (void)fail("expected ',' in JSON object");
                    return Kind::Null;
                }
            }
            firstMember = false;
            skipWhitespace();
            if (m_cursor >= m_input.size() || m_input[m_cursor] != '"') {
                (void)fail("expected string key in JSON object");
                return Kind::Null;
            }
            if (!parseString()) {
                return Kind::Null;
            }
            const std::string key = std::move(m_lastString);
            // Arm array counting before the value is parsed so nested arrays
            // inside the counted array are not miscounted.
            if (key == "layers") {
                m_layersKeyPending = true;
            } else if (key == "assets") {
                m_assetsKeyPending = true;
            }
            skipWhitespace();
            if (!consume(':')) {
                (void)fail("expected ':' in JSON object");
                return Kind::Null;
            }
            const Kind kind = parseValue(depth + 1U);
            if (kind == Kind::Null && !m_failure.empty()) {
                return Kind::Null;
            }
            if (!finalizeMember(key, kind, depth)) {
                return Kind::Null;
            }
            skipWhitespace();
            if (m_cursor < m_input.size() && m_input[m_cursor] == '}') {
                ++m_cursor;
                return Kind::Object;
            }
        }
        (void)fail("unterminated JSON object");
        return Kind::Null;
    }

    [[nodiscard]] Kind parseArray(const std::size_t depth) noexcept
    {
        // Decide before consuming so nested arrays are not miscounted.
        const bool countingLayers = m_layersKeyPending;
        const bool countingAssets = m_assetsKeyPending;
        m_layersKeyPending = false;
        m_assetsKeyPending = false;

        ++m_cursor; // '['
        skipWhitespace();
        if (m_cursor < m_input.size() && m_input[m_cursor] == ']') {
            ++m_cursor;
            return Kind::Array;
        }
        std::size_t elementCount{};
        bool firstElement = true;
        while (m_cursor < m_input.size()) {
            if (!firstElement) {
                skipWhitespace();
                if (!consume(',')) {
                    (void)fail("expected ',' in JSON array");
                    return Kind::Null;
                }
            }
            firstElement = false;
            if (countingLayers && ++elementCount > m_limits.maxLayers) {
                (void)fail("layer count exceeds the validation limit");
                return Kind::Null;
            }
            if (countingAssets && ++elementCount > m_limits.maxAssets) {
                (void)fail("asset count exceeds the validation limit");
                return Kind::Null;
            }
            const Kind kind = parseValue(depth + 1U);
            if (kind == Kind::Null && !m_failure.empty()) {
                return Kind::Null;
            }
            skipWhitespace();
            if (m_cursor < m_input.size() && m_input[m_cursor] == ']') {
                ++m_cursor;
                if (countingLayers) m_layers = static_cast<std::uint32_t>(elementCount);
                if (countingAssets) m_assets = static_cast<std::uint32_t>(elementCount);
                return Kind::Array;
            }
        }
        (void)fail("unterminated JSON array");
        return Kind::Null;
    }

    std::string_view m_input{};
    ValidationLimits m_limits{};
    std::size_t m_cursor{};
    std::size_t m_tokenCount{};
    std::string m_lastString{};
    std::string_view m_lastNumber{};
    std::string_view m_failure{};
    std::uint32_t& m_layers;
    std::uint32_t& m_assets;
    double& m_width;
    double& m_height;
    double& m_frameRate;
    double& m_inPoint;
    double& m_outPoint;
    std::string& m_version;
    bool m_layersKeyPending{};
    bool m_assetsKeyPending{};
};

} // namespace

foundation::result::Result<ValidatedEmojiAssetInfo> validateEmojiAsset(
    const std::string_view provider,
    const std::string_view assetKey,
    const std::span<const std::byte> bytes,
    ValidationLimits limits,
    const std::optional<Sha256> expectedSha256) noexcept
{
    if (provider.empty() || assetKey.empty()) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset validation requires a provider and asset key."));
    }
    if (bytes.empty()) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset is empty."));
    }
    if (bytes.size() > limits.maxFileBytes) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset exceeds the maximum validated file size."));
    }
    const Sha256 digest = sha256(bytes);
    if (expectedSha256 && *expectedSha256 != digest) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset checksum does not match the pinned manifest."));
    }

    const auto input = std::string_view{
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size(),
    };
    if (input.find('\0') != std::string_view::npos) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset contains embedded NUL bytes."));
    }

    std::uint32_t layers{};
    std::uint32_t assets{};
    double width{};
    double height{};
    double frameRate{};
    double inPoint{};
    double outPoint{};
    std::string version{};
    LottieJsonScanner scanner(
        input, limits, layers, assets,
        width, height, frameRate, inPoint, outPoint, version);
    if (!scanner.scan()) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset is not valid bounded JSON: " + std::string{scanner.failure()}));
    }

    if (width < static_cast<double>(limits.minDimension) ||
        height < static_cast<double>(limits.minDimension) ||
        width > static_cast<double>(limits.maxDimension) ||
        height > static_cast<double>(limits.maxDimension)) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset dimensions are outside the validated bounds."));
    }
    if (frameRate < limits.minFrameRate || frameRate > limits.maxFrameRate) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset frame rate is outside the validated bounds."));
    }
    if (outPoint <= inPoint || (outPoint - inPoint) > static_cast<double>(limits.maxFrameCount)) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset duration is outside the validated bounds."));
    }
    if (layers == 0U || layers > limits.maxLayers) {
        return std::unexpected(error(
            foundation::error::ErrorCode::InvalidArgument,
            "Emoji asset layer count is outside the validated bounds."));
    }

    const std::uint32_t frameCount = static_cast<std::uint32_t>(outPoint - inPoint);
    const double durationSeconds =
        frameRate > 0.0 ? static_cast<double>(frameCount) / frameRate : 0.0;

    return ValidatedEmojiAssetInfo{
        .provider = std::string{provider},
        .assetKey = std::string{assetKey},
        .revision = 0U,
        .width = static_cast<std::uint32_t>(width),
        .height = static_cast<std::uint32_t>(height),
        .frameRate = frameRate,
        .durationSeconds = durationSeconds,
        .frameCount = frameCount,
        .layerCount = layers,
        .assetSha256 = digest,
    };
}

} // namespace farcaster::provider::emoji::validator
