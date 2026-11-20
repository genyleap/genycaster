module;

#include "genycaster_deployment.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <exception>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module farcaster.provider.compatibility.auth;

import farcaster.capability.auth;
import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.transport.http;

namespace farcaster::provider::compatibility::auth {
namespace {

using Bytes32 = std::array<unsigned char, 32>;

constexpr std::string_view idRegistryAddress{"0x00000000fc6c5f01fc30151999387bb99a9f489b"};
constexpr std::string_view keyRegistryAddress{"0x00000000fc1237824fb747abde0ff18990e59b7e"};
constexpr std::string_view universalSignatureValidatorByteCode{
    "0x608060405234801561001057600080fd5b5060405161069438038061069483398101604081905261002f9161051e565b600061003c848484610048565b9050806000526001601ff35b60007f64926492649264926492649264926492649264926492649264926492649264926100748361040c565b036101e7576000606080848060200190518101906100929190610577565b60405192955090935091506000906001600160a01b038516906100b69085906105dd565b6000604051808303816000865af19150503d80600081146100f3576040519150601f19603f3d011682016040523d82523d6000602084013e6100f8565b606091505b50509050876001600160a01b03163b60000361016057806101605760405162461bcd60e51b815260206004820152601e60248201527f5369676e617475726556616c696461746f723a206465706c6f796d656e74000060448201526064015b60405180910390fd5b604051630b135d3f60e11b808252906001600160a01b038a1690631626ba7e90610190908b9087906004016105f9565b602060405180830381865afa1580156101ad573d6000803e3d6000fd5b505050506040513d601f19601f820116820180604052508101906101d19190610633565b6001600160e01b03191614945050505050610405565b6001600160a01b0384163b1561027a57604051630b135d3f60e11b808252906001600160a01b03861690631626ba7e9061022790879087906004016105f9565b602060405180830381865afa158015610244573d6000803e3d6000fd5b505050506040513d601f19601f820116820180604052508101906102689190610633565b6001600160e01b031916149050610405565b81516041146102df5760405162461bcd60e51b815260206004820152603a602482015260008051602061067483398151915260448201527f3a20696e76616c6964207369676e6174757265206c656e6774680000000000006064820152608401610157565b6102e7610425565b5060208201516040808401518451859392600091859190811061030c5761030c61065d565b016020015160f81c9050601b811480159061032b57508060ff16601c14155b1561038c5760405162461bcd60e51b815260206004820152603b602482015260008051602061067483398151915260448201527f3a20696e76616c6964207369676e617475726520762076616c756500000000006064820152608401610157565b60408051600081526020810180835289905260ff83169181019190915260608101849052608081018390526001600160a01b0389169060019060a0016020604051602081039080840390855afa1580156103ea573d6000803e3d6000fd5b505050602060405103516001600160a01b0316149450505050505b9392505050565b600060208251101561041d57600080fd5b508051015190565b60405180606001604052806003906020820280368337509192915050565b6001600160a01b038116811461045857600080fd5b50565b634e487b7160e01b600052604160045260246000fd5b60005b8381101561048c578181015183820152602001610474565b50506000910152565b600082601f8301126104a657600080fd5b81516001600160401b038111156104bf576104bf61045b565b604051601f8201601f19908116603f011681016001600160401b03811182821017156104ed576104ed61045b565b60405281815283820160200185101561050557600080fd5b610516826020830160208701610471565b949350505050565b60008060006060848603121561053357600080fd5b835161053e81610443565b6020850151604086015191945092506001600160401b0381111561056157600080fd5b61056d86828701610495565b9150509250925092565b60008060006060848603121561058c57600080fd5b835161059781610443565b60208501519093506001600160401b038111156105b357600080fd5b6105bf86828701610495565b604086015190935090506001600160401b0381111561056157600080fd5b600082516105ef818460208701610471565b9190910192915050565b828152604060208201526000825180604084015261061e816060850160208701610471565b601f01601f1916919091016060019392505050565b60006020828403121561064557600080fd5b81516001600160e01b03198116811461040557600080fd5b634e487b7160e01b600052603260045260246000fdfe5369676e617475726556616c696461746f72237265636f7665725369676e6572"};

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] std::string lower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] bool startsWithHttp(const std::string_view value)
{
    return value.starts_with("https://") || value.starts_with("http://");
}

[[nodiscard]] bool isValidHttpUri(const std::string_view value)
{
    if (!startsWithHttp(value)) {
        return false;
    }
    for (const char rawCh : value) {
        const auto ch = static_cast<unsigned char>(rawCh);
        if (std::isspace(ch) || ch == '\\' || ch == '"' || ch == '\'') {
            return false;
        }
    }

    const std::size_t authorityStart = value.find("://") + 3U;
    const std::size_t authorityEnd = value.find_first_of("/?#", authorityStart);
    const std::string_view authority = authorityEnd == std::string_view::npos
        ? value.substr(authorityStart)
        : value.substr(authorityStart, authorityEnd - authorityStart);
    if (authority.empty() || authority.front() == ':' || authority.contains('@')) {
        return false;
    }
    return true;
}

[[nodiscard]] std::string trim(std::string value)
{
    const auto notSpace = [](const unsigned char ch) { return !std::isspace(ch); };
    const auto first = std::ranges::find_if(value, notSpace);
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if(value.rbegin(), value.rend(), notSpace).base();
    return std::string{first, last};
}

[[nodiscard]] std::string normalizeConfigurationValue(std::string value)
{
    value = trim(std::move(value));
    if (value.size() >= 2U &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = trim(value.substr(1U, value.size() - 2U));
    }
    return value;
}

[[nodiscard]] foundation::result::Result<foundation::time::Timestamp> parseRfc3339(
    const std::string_view value)
{
    const auto invalid = [] {
        return makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF contains an invalid RFC3339 timestamp.");
    };
    const auto readNumber = [&](const std::size_t offset, const std::size_t count)
        -> std::optional<unsigned> {
        if (offset + count > value.size()) {
            return std::nullopt;
        }
        unsigned parsed{};
        const auto* first = value.data() + offset;
        const auto* last = first + count;
        const auto [end, error] = std::from_chars(first, last, parsed);
        if (error != std::errc{} || end != last) {
            return std::nullopt;
        }
        return parsed;
    };

    if (value.size() < 20U || value[4] != '-' || value[7] != '-' ||
        (value[10] != 'T' && value[10] != 't') || value[13] != ':' || value[16] != ':') {
        return std::unexpected(invalid());
    }
    const auto yearValue = readNumber(0, 4);
    const auto monthValue = readNumber(5, 2);
    const auto dayValue = readNumber(8, 2);
    const auto hourValue = readNumber(11, 2);
    const auto minuteValue = readNumber(14, 2);
    const auto secondValue = readNumber(17, 2);
    if (!yearValue || !monthValue || !dayValue || !hourValue || !minuteValue || !secondValue ||
        *hourValue > 23U || *minuteValue > 59U || *secondValue > 59U) {
        return std::unexpected(invalid());
    }

    const std::chrono::year_month_day date{
        std::chrono::year{static_cast<int>(*yearValue)},
        std::chrono::month{*monthValue},
        std::chrono::day{*dayValue},
    };
    if (!date.ok()) {
        return std::unexpected(invalid());
    }

    std::size_t cursor = 19U;
    unsigned fractionMilliseconds = 0U;
    if (cursor < value.size() && value[cursor] == '.') {
        ++cursor;
        const std::size_t fractionStart = cursor;
        unsigned digits = 0U;
        while (cursor < value.size() && std::isdigit(static_cast<unsigned char>(value[cursor]))) {
            if (digits < 3U) {
                fractionMilliseconds = fractionMilliseconds * 10U +
                    static_cast<unsigned>(value[cursor] - '0');
            }
            ++digits;
            ++cursor;
        }
        if (cursor == fractionStart) {
            return std::unexpected(invalid());
        }
        while (digits < 3U) {
            fractionMilliseconds *= 10U;
            ++digits;
        }
    }

    int offsetMinutes = 0;
    if (cursor < value.size() && (value[cursor] == 'Z' || value[cursor] == 'z')) {
        ++cursor;
    } else {
        if (cursor + 6U != value.size() ||
            (value[cursor] != '+' && value[cursor] != '-') || value[cursor + 3U] != ':') {
            return std::unexpected(invalid());
        }
        const char sign = value[cursor];
        const auto offsetHour = readNumber(cursor + 1U, 2U);
        const auto offsetMinute = readNumber(cursor + 4U, 2U);
        if (!offsetHour || !offsetMinute || *offsetHour > 23U || *offsetMinute > 59U) {
            return std::unexpected(invalid());
        }
        offsetMinutes = static_cast<int>(*offsetHour * 60U + *offsetMinute);
        if (sign == '-') {
            offsetMinutes = -offsetMinutes;
        }
        cursor += 6U;
    }
    if (cursor != value.size()) {
        return std::unexpected(invalid());
    }

    const auto local = std::chrono::sys_days{date} +
        std::chrono::hours{*hourValue} +
        std::chrono::minutes{*minuteValue} +
        std::chrono::seconds{*secondValue} +
        std::chrono::milliseconds{fractionMilliseconds};
    const auto utc = std::chrono::time_point_cast<std::chrono::milliseconds>(
        local - std::chrono::minutes{offsetMinutes});
    return foundation::time::Timestamp{utc.time_since_epoch()};
}

[[nodiscard]] bool isValidNonce(const std::string_view value)
{
    return value.size() >= 8U && std::ranges::all_of(value, [](const char rawCh) {
        return std::isalnum(static_cast<unsigned char>(rawCh)) != 0;
    });
}

[[nodiscard]] std::string jsonEscape(const std::string_view value)
{
    std::string escaped{};
    escaped.reserve(value.size() + 8);
    for (const char rawCh : value) {
        const auto ch = static_cast<unsigned char>(rawCh);
        switch (ch) {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (ch < 0x20U) {
                constexpr char digits[] = "0123456789abcdef";
                escaped += "\\u00";
                escaped.push_back(digits[(ch >> 4U) & 0x0fU]);
                escaped.push_back(digits[ch & 0x0fU]);
            } else {
                escaped.push_back(static_cast<char>(ch));
            }
        }
    }
    return escaped;
}

[[nodiscard]] std::optional<std::string> optionalString(
    const boost::property_tree::ptree& tree,
    const std::string_view path)
{
    const auto value = tree.get_optional<std::string>(std::string{path});
    if (!value) {
        return std::nullopt;
    }
    return *value;
}

[[nodiscard]] foundation::result::Result<boost::property_tree::ptree>
parseJson(const std::string& body)
{
    try {
        std::istringstream input{body};
        boost::property_tree::ptree tree{};
        boost::property_tree::read_json(input, tree);
        return tree;
    } catch (const std::exception& error) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            std::string{"Authentication endpoint returned invalid JSON: "} + error.what()));
    }
}

[[nodiscard]] std::optional<std::string> safeEndpointErrorDetail(const std::string& body)
{
    auto parsed = parseJson(body);
    if (!parsed) {
        return std::nullopt;
    }

    std::string detail{};
    const auto appendField = [&](const std::string_view path) {
        auto value = optionalString(*parsed, path);
        if (!value) {
            return;
        }
        *value = trim(std::move(*value));
        if (value->empty() || (!detail.empty() && detail == *value)) {
            return;
        }
        std::ranges::transform(*value, value->begin(), [](const unsigned char ch) {
            return ch < 0x20U || ch == 0x7fU ? ' ' : static_cast<char>(ch);
        });
        if (value->size() > 240U) {
            value->resize(240U);
            value->append("...");
        }
        if (!detail.empty()) {
            detail.append(": ");
        }
        detail.append(*value);
    };
    appendField("error");
    appendField("message");
    if (detail.empty()) {
        return std::nullopt;
    }
    return detail;
}

[[nodiscard]] foundation::result::Status requireSuccessStatus(
    const transport::http::HttpResponse& response,
    const std::string_view context)
{
    if (response.statusCode >= 200 && response.statusCode < 300) {
        return {};
    }
    const auto withDetail = [&](std::string message) {
        if (const auto detail = safeEndpointErrorDetail(response.body)) {
            message.append(" Detail: ");
            message.append(*detail);
        }
        return message;
    };
    if (response.statusCode == 401 || response.statusCode == 403) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            withDetail(std::string{context} + " rejected the request.")));
    }
    if (response.statusCode == 429) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::RateLimited,
            withDetail(std::string{context} + " is rate limited.")));
    }
    if (response.statusCode >= 500) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            withDetail(std::string{context} + " is temporarily unavailable.")));
    }
    return std::unexpected(makeError(
        foundation::error::ErrorCode::ProtocolFailure,
        withDetail(std::string{context} + " returned HTTP " +
            std::to_string(response.statusCode) + '.')));
}

[[nodiscard]] std::string joinUrl(
    std::string base,
    const std::string_view version,
    const std::string_view path)
{
    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    base.push_back('/');
    base.append(version);
    base.push_back('/');
    base.append(path);
    return base;
}

[[nodiscard]] foundation::result::Result<std::vector<unsigned char>> decodeHex(
    const std::string_view value)
{
    std::string_view text = value;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        text.remove_prefix(2);
    }
    if ((text.size() % 2U) != 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Hex value has an odd number of digits."));
    }
    const auto nibble = [](const char ch) -> int {
        if (ch >= '0' && ch <= '9') { return ch - '0'; }
        if (ch >= 'a' && ch <= 'f') { return ch - 'a' + 10; }
        if (ch >= 'A' && ch <= 'F') { return ch - 'A' + 10; }
        return -1;
    };
    std::vector<unsigned char> bytes{};
    bytes.reserve(text.size() / 2U);
    for (std::size_t index = 0; index < text.size(); index += 2U) {
        const int high = nibble(text[index]);
        const int low = nibble(text[index + 1U]);
        if (high < 0 || low < 0) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Hex value contains a non-hexadecimal character."));
        }
        bytes.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return bytes;
}

[[nodiscard]] bool isEthereumAddress(const std::string_view value)
{
    const auto bytes = decodeHex(value);
    return bytes && bytes->size() == 20U;
}

[[nodiscard]] std::string encodeHex(const std::span<const unsigned char> bytes, const bool prefix = true)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string output{};
    output.reserve(bytes.size() * 2U + (prefix ? 2U : 0U));
    if (prefix) {
        output = "0x";
    }
    for (const unsigned char byte : bytes) {
        output.push_back(digits[(byte >> 4U) & 0x0fU]);
        output.push_back(digits[byte & 0x0fU]);
    }
    return output;
}

// Ethereum uses legacy Keccak-256 padding (0x01), NOT FIPS SHA3-256 (0x06).
// OpenSSL's default providers do not expose EVP_MD_fetch("KECCAK-256").
// Implement the fixed-width Keccak-f[1600] sponge without optional providers.
[[nodiscard]] foundation::result::Result<Bytes32> keccak256(const std::span<const unsigned char> bytes)
{
    constexpr std::array<unsigned, 25> rotations{
        0, 1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43,
        25, 39, 41, 45, 15, 21, 8, 18, 2, 61, 56, 14,
    };
    constexpr std::array<std::uint64_t, 24> roundConstants{
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
    auto state = std::array<std::uint64_t, 25>{};
    const auto permute = [&] {
        for (const auto round : roundConstants) {
            std::array<std::uint64_t, 5> columns{};
            for (std::size_t x = 0; x < 5; ++x) {
                for (std::size_t y = 0; y < 5; ++y) {
                    columns[x] ^= state[x + 5 * y];
                }
            }
            for (std::size_t x = 0; x < 5; ++x) {
                const auto parity = columns[(x + 4) % 5]
                    ^ std::rotl(columns[(x + 1) % 5], 1);
                for (std::size_t y = 0; y < 5; ++y) {
                    state[x + 5 * y] ^= parity;
                }
            }
            std::array<std::uint64_t, 25> shifted{};
            for (std::size_t x = 0; x < 5; ++x) {
                for (std::size_t y = 0; y < 5; ++y) {
                    const auto from = x + 5 * y;
                    const auto to = y + 5 * ((2 * x + 3 * y) % 5);
                    shifted[to] = std::rotl(state[from], static_cast<int>(rotations[from]));
                }
            }
            for (std::size_t x = 0; x < 5; ++x) {
                for (std::size_t y = 0; y < 5; ++y) {
                    state[x + 5 * y] = shifted[x + 5 * y]
                        ^ (~shifted[(x + 1) % 5 + 5 * y]
                           & shifted[(x + 2) % 5 + 5 * y]);
                }
            }
            state[0] ^= round;
        }
    };
    constexpr std::size_t rate = 136;
    const auto absorb = [&](const std::span<const unsigned char> block) {
        for (std::size_t i = 0; i < rate; ++i) {
            state[i / 8] ^= static_cast<std::uint64_t>(block[i]) << (8 * (i % 8));
        }
        permute();
    };
    std::size_t consumed = 0;
    while (bytes.size() - consumed >= rate) {
        absorb(bytes.subspan(consumed, rate));
        consumed += rate;
    }
    std::array<unsigned char, rate> tail{};
    for (std::size_t i = consumed; i < bytes.size(); ++i) {
        tail[i - consumed] = bytes[i];
    }
    tail[bytes.size() - consumed] = 0x01;
    tail[rate - 1] |= 0x80;
    absorb(tail);
    Bytes32 result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<unsigned char>(state[i / 8] >> (8 * (i % 8)));
    }
    return result;
}

[[nodiscard]] foundation::result::Result<Bytes32> ethereumMessageHash(const std::string& message)
{
    const std::string prefix = "\x19" "Ethereum Signed Message:\n" + std::to_string(message.size());
    std::vector<unsigned char> payload{};
    payload.reserve(prefix.size() + message.size());
    payload.insert(payload.end(), prefix.begin(), prefix.end());
    payload.insert(payload.end(), message.begin(), message.end());
    return keccak256(payload);
}

[[nodiscard]] foundation::result::Result<std::string> functionSelector(const std::string_view signature)
{
    const auto bytes = std::span{
        reinterpret_cast<const unsigned char*>(signature.data()), signature.size()};
    auto hash = keccak256(bytes);
    if (!hash) {
        return std::unexpected(std::move(hash.error()));
    }
    return encodeHex(std::span{hash->data(), 4U}, false);
}

[[nodiscard]] std::string wordFromUnsigned(const std::uint64_t value)
{
    std::ostringstream stream{};
    stream << std::hex << std::setw(64) << std::setfill('0') << value;
    return stream.str();
}

[[nodiscard]] foundation::result::Result<std::string> addressWord(const std::string_view address)
{
    auto bytes = decodeHex(address);
    if (!bytes || bytes->size() != 20U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "SIWF address is not a 20-byte Ethereum address."));
    }
    return std::string(24U, '0') + encodeHex(*bytes, false);
}

[[nodiscard]] foundation::result::Result<std::uint64_t> parseAbiUnsigned(
    const std::string_view word)
{
    if (word.size() != 64U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Ethereum RPC returned an invalid ABI word."));
    }
    const auto firstNonZero = word.find_first_not_of('0');
    if (firstNonZero == std::string_view::npos) {
        return std::uint64_t{0};
    }
    const std::string_view significant = word.substr(firstNonZero);
    if (significant.size() > 16U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Ethereum ABI integer exceeds the supported client range."));
    }
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(
        significant.data(), significant.data() + significant.size(), value, 16);
    if (error != std::errc{} || end != significant.data() + significant.size()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Ethereum RPC returned an invalid ABI integer."));
    }
    return value;
}

struct VerifiedIdentityAuthorization final {
    domain::session::AuthenticationMethod method{domain::session::AuthenticationMethod::Custody};
    std::string custodyAddress{};
};

class EthereumVerifier final {
public:
    EthereumVerifier(
        transport::http::IHttpTransport& transport,
        std::string rpcUrl,
        const std::chrono::milliseconds timeout)
        : m_transport(transport), m_rpcUrl(std::move(rpcUrl)), m_timeout(timeout)
    {
    }

    [[nodiscard]] foundation::result::Result<VerifiedIdentityAuthorization> verify(
        const std::string& message,
        const std::string& signature,
        const std::string& expectedAddress,
        const domain::identity::Fid fid,
        const bool acceptAuthAddress)
    {
        auto hash = ethereumMessageHash(message);
        if (!hash) {
            return std::unexpected(std::move(hash.error()));
        }
        auto signatureValid = verifySignature(*hash, signature, expectedAddress);
        if (!signatureValid) {
            return std::unexpected(std::move(signatureValid.error()));
        }
        if (!*signatureValid) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "SIWF signature does not match the address declared in the message."));
        }

        std::optional<domain::session::AuthenticationMethod> method{};
        if (acceptAuthAddress) {
            auto authAddress = isActiveAuthAddress(expectedAddress, fid);
            if (!authAddress) {
                return std::unexpected(std::move(authAddress.error()));
            }
            if (*authAddress) {
                method = domain::session::AuthenticationMethod::AuthAddress;
            }
        }

        if (!method) {
            auto custodyFid = fidForCustody(expectedAddress);
            if (!custodyFid) {
                return std::unexpected(std::move(custodyFid.error()));
            }
            if (*custodyFid != fid.value) {
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::Unauthorized,
                    "SIWF signer is neither an active auth address nor the custody owner of the FID."));
            }
            method = domain::session::AuthenticationMethod::Custody;
        }

        auto custodyAddress = custodyAddressForFid(fid);
        if (!custodyAddress) {
            return std::unexpected(std::move(custodyAddress.error()));
        }
        return VerifiedIdentityAuthorization{
            .method = *method,
            .custodyAddress = std::move(*custodyAddress),
        };
    }

    [[nodiscard]] foundation::result::Result<std::string> currentCustodyAddress(
        const domain::identity::Fid fid)
    {
        return custodyAddressForFid(fid);
    }

    [[nodiscard]] foundation::result::Result<domain::identity::Fid>
    resolveRecoveryFid(const std::string& address)
    {
        // Invalid addresses cannot trigger even a chain-ID request.
        auto verifiedWord = addressWord(address);
        if (!verifiedWord) return std::unexpected(std::move(verifiedWord.error()));

        // A custom RPC endpoint could point at Ethereum or a fork. Never
        // infer Farcaster custody from registry-looking responses on chain != 10.
        auto chainId = rpc("eth_chainId", "[]");
        if (!chainId) return std::unexpected(std::move(chainId.error()));
        if (lower(*chainId) != "0xa") {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Custody recovery requires Optimism mainnet (chain ID 10)."));
        }

        auto fid = fidForCustody(address);
        if (!fid) return std::unexpected(std::move(fid.error()));
        if (*fid == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::NotFound,
                "This custody address has no registered Farcaster FID."));
        }
        const domain::identity::Fid identity{*fid};
        auto registered = custodyAddressForFid(identity);
        if (!registered) return std::unexpected(std::move(registered.error()));
        if (lower(*registered) != lower(address)) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Conflict,
                "Farcaster IdRegistry does not confirm custody for this FID."));
        }
        return identity;
    }

private:
    [[nodiscard]] foundation::result::Result<std::string> rpc(
        const std::string_view method,
        const std::string& paramsJson)
    {
        const std::string body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"" +
            jsonEscape(method) + "\",\"params\":" + paramsJson + '}';
        auto response = m_transport.post(transport::http::HttpPostRequest{
            .url = m_rpcUrl,
            .headers = {"Content-Type: application/json"},
            .body = body,
            .timeout = m_timeout,
            .maxBodyBytes = 512U * 1024U,
        });
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (auto status = requireSuccessStatus(*response, "Optimism RPC"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        auto json = parseJson(response->body);
        if (!json) {
            return std::unexpected(std::move(json.error()));
        }
        if (const auto message = json->get_optional<std::string>("error.message")) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Optimism RPC rejected authentication verification: " + *message));
        }
        const auto result = json->get_optional<std::string>("result");
        if (!result) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Optimism RPC authentication verification returned no result."));
        }
        return *result;
    }

    [[nodiscard]] foundation::result::Result<std::string> ethCall(
        const std::string_view to,
        const std::string& data)
    {
        const std::string params = "[{\"to\":\"" + jsonEscape(to) +
            "\",\"data\":\"" + jsonEscape(data) + "\"},\"latest\"]";
        return rpc("eth_call", params);
    }

    [[nodiscard]] foundation::result::Result<std::string> ethCallDeployless(
        const std::string& data)
    {
        const std::string params = "[{\"data\":\"" + jsonEscape(data) + "\"},\"latest\"]";
        return rpc("eth_call", params);
    }

    [[nodiscard]] foundation::result::Result<bool> verifySignature(
        const Bytes32& hash,
        const std::string& signature,
        const std::string& address)
    {
        auto addressValue = addressWord(address);
        auto signatureBytes = decodeHex(signature);
        if (!addressValue || !signatureBytes) {
            return std::unexpected(addressValue
                ? std::move(signatureBytes.error()) : std::move(addressValue.error()));
        }

        // Match the deployless universal verification path used by the minimum
        // viem dependency of @farcaster/auth-client 0.7.1. The pinned validator
        // handles EOA, deployed ERC-1271 and ERC-6492 counterfactual signatures.
        std::string callData{universalSignatureValidatorByteCode};
        callData += *addressValue;
        callData += encodeHex(hash, false);
        callData += wordFromUnsigned(96U);
        callData += wordFromUnsigned(static_cast<std::uint64_t>(signatureBytes->size()));
        callData += encodeHex(*signatureBytes, false);
        const std::size_t remainder = signatureBytes->size() % 32U;
        if (remainder != 0U) {
            callData.append((32U - remainder) * 2U, '0');
        }

        auto result = ethCallDeployless(callData);
        if (!result) {
            return std::unexpected(std::move(result.error()));
        }
        std::string normalized = lower(*result);
        if (normalized.starts_with("0x")) {
            normalized.erase(0, 2);
        }
        if (normalized.empty()) {
            return false;
        }
        const auto firstNonZero = normalized.find_first_not_of('0');
        return firstNonZero != std::string::npos && normalized.substr(firstNonZero) == "1";
    }

    [[nodiscard]] foundation::result::Result<bool> isActiveAuthAddress(
        const std::string& address,
        const domain::identity::Fid fid)
    {
        auto selector = functionSelector("keyDataOf(uint256,bytes)");
        auto addressValue = addressWord(address);
        if (!selector || !addressValue) {
            return std::unexpected(selector
                ? std::move(addressValue.error()) : std::move(selector.error()));
        }
        const std::string data = "0x" + *selector + wordFromUnsigned(fid.value) +
            wordFromUnsigned(64U) + wordFromUnsigned(32U) + *addressValue;
        auto result = ethCall(keyRegistryAddress, data);
        if (!result) {
            return std::unexpected(std::move(result.error()));
        }
        std::string_view encoded = *result;
        if (encoded.starts_with("0x")) {
            encoded.remove_prefix(2);
        }
        if (encoded.size() < 128U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "KeyRegistry returned incomplete key data."));
        }
        auto state = parseAbiUnsigned(encoded.substr(0, 64));
        auto keyType = parseAbiUnsigned(encoded.substr(64, 64));
        if (!state || !keyType) {
            return std::unexpected(state
                ? std::move(keyType.error()) : std::move(state.error()));
        }
        return *state == 1U && *keyType == 2U;
    }

    [[nodiscard]] foundation::result::Result<std::string> custodyAddressForFid(
        const domain::identity::Fid fid)
    {
        auto selector = functionSelector("custodyOf(uint256)");
        if (!selector) {
            return std::unexpected(std::move(selector.error()));
        }
        auto result = ethCall(
            idRegistryAddress,
            "0x" + *selector + wordFromUnsigned(fid.value));
        if (!result) {
            return std::unexpected(std::move(result.error()));
        }
        std::string encoded = lower(*result);
        if (encoded.starts_with("0x")) {
            encoded.erase(0U, 2U);
        }
        constexpr std::size_t addressWordHexLength{64U};
        constexpr std::size_t addressHexLength{40U};
        constexpr std::size_t addressPaddingHexLength{addressWordHexLength - addressHexLength};
        if (encoded.size() != addressWordHexLength ||
            !std::ranges::all_of(encoded.substr(0U, addressPaddingHexLength),
                [](const char ch) { return ch == '0'; })) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "IdRegistry returned an invalid custody-address ABI value."));
        }
        const std::string address = encoded.substr(addressPaddingHexLength);
        if (std::ranges::all_of(address, [](const char ch) { return ch == '0'; })) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "Farcaster FID has no active custody address in IdRegistry."));
        }
        return "0x" + address;
    }

    [[nodiscard]] foundation::result::Result<std::uint64_t> fidForCustody(
        const std::string& address)
    {
        auto selector = functionSelector("idOf(address)");
        auto addressValue = addressWord(address);
        if (!selector || !addressValue) {
            return std::unexpected(selector
                ? std::move(addressValue.error()) : std::move(selector.error()));
        }
        auto result = ethCall(idRegistryAddress, "0x" + *selector + *addressValue);
        if (!result) {
            return std::unexpected(std::move(result.error()));
        }
        std::string_view encoded = *result;
        if (encoded.starts_with("0x")) {
            encoded.remove_prefix(2);
        }
        return parseAbiUnsigned(encoded);
    }

    transport::http::IHttpTransport& m_transport;
    std::string m_rpcUrl;
    std::chrono::milliseconds m_timeout;
};

struct ParsedSiwe final {
    std::string domain{};
    std::string address{};
    std::string statement{};
    std::string uri{};
    std::string nonce{};
    std::string version{};
    std::string issuedAt{};
    std::optional<foundation::time::Timestamp> expirationTime{};
    std::optional<foundation::time::Timestamp> notBefore{};
    std::uint64_t chainId{};
    std::vector<std::string> resources{};
};

[[nodiscard]] foundation::result::Result<ParsedSiwe> parseSiwe(const std::string& message)
{
    std::istringstream stream{message};
    std::vector<std::string> lines{};
    for (std::string line{}; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    if (lines.size() < 8U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF message is incomplete."));
    }
    constexpr std::string_view suffix{" wants you to sign in with your Ethereum account:"};
    if (!std::string_view{lines.front()}.ends_with(suffix)) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF message has an invalid domain declaration."));
    }
    ParsedSiwe parsed{};
    parsed.domain = lines.front().substr(0, lines.front().size() - suffix.size());
    parsed.address = trim(lines[1]);

    std::size_t index = 2U;
    while (index < lines.size() && lines[index].empty()) {
        ++index;
    }
    if (index < lines.size() && lines[index].find(':') == std::string::npos) {
        parsed.statement = trim(lines[index]);
        ++index;
    }

    bool resources = false;
    for (; index < lines.size(); ++index) {
        const std::string& line = lines[index];
        if (line == "Resources:") {
            resources = true;
            continue;
        }
        if (resources) {
            if (std::string_view{line}.starts_with("- ")) {
                parsed.resources.push_back(trim(line.substr(2)));
            }
            continue;
        }
        const auto readField = [&](const std::string_view prefix) -> std::optional<std::string> {
            if (std::string_view{line}.starts_with(prefix)) {
                return trim(line.substr(prefix.size()));
            }
            return std::nullopt;
        };
        if (auto uriValue = readField("URI: ")) {
            parsed.uri = std::move(*uriValue);
        } else if (auto nonceValue = readField("Nonce: ")) {
            parsed.nonce = std::move(*nonceValue);
        } else if (auto versionValue = readField("Version: ")) {
            parsed.version = std::move(*versionValue);
        } else if (auto issuedAtValue = readField("Issued At: ")) {
            parsed.issuedAt = std::move(*issuedAtValue);
        } else if (auto expirationValue = readField("Expiration Time: ")) {
            auto timestamp = parseRfc3339(*expirationValue);
            if (!timestamp) {
                return std::unexpected(std::move(timestamp.error()));
            }
            parsed.expirationTime = *timestamp;
        } else if (auto notBeforeValue = readField("Not Before: ")) {
            auto timestamp = parseRfc3339(*notBeforeValue);
            if (!timestamp) {
                return std::unexpected(std::move(timestamp.error()));
            }
            parsed.notBefore = *timestamp;
        } else if (auto chainIdValue = readField("Chain ID: ")) {
            std::uint64_t chain{};
            const auto [end, error] = std::from_chars(
                chainIdValue->data(), chainIdValue->data() + chainIdValue->size(), chain);
            if (error != std::errc{} || end != chainIdValue->data() + chainIdValue->size()) {
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::Unauthorized,
                    "SIWF Chain ID is invalid."));
            }
            parsed.chainId = chain;
        }
    }
    return parsed;
}

[[nodiscard]] foundation::result::Status validateSiwe(
    const ParsedSiwe& parsed,
    const std::string_view expectedDomain,
    const std::string_view expectedUri,
    const std::string_view expectedNonce,
    const std::uint64_t expectedFid)
{
    if (parsed.domain != expectedDomain || parsed.nonce != expectedNonce ||
        parsed.uri != expectedUri) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF challenge domain, URI or nonce does not match the request."));
    }
    if (parsed.version != "1" || parsed.issuedAt.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF message must use SIWE version 1 and include an issued-at timestamp."));
    }
    if (parsed.statement != "Farcaster Auth" && parsed.statement != "Farcaster Connect") {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF statement is not a Farcaster authentication statement."));
    }
    if (parsed.chainId != 10U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF Chain ID must be Optimism mainnet (10)."));
    }
    if (!isEthereumAddress(parsed.address) || !isValidNonce(parsed.nonce)) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF address or nonce is structurally invalid."));
    }
    const auto now = foundation::time::now();
    if (parsed.expirationTime && now >= *parsed.expirationTime) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF authentication proof has expired."));
    }
    if (parsed.notBefore && now < *parsed.notBefore) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF authentication proof is not valid yet."));
    }

    std::optional<std::uint64_t> resourceFid{};
    std::size_t fidResources = 0U;
    for (std::string resource : parsed.resources) {
        if (resource.ends_with('/')) {
            resource.pop_back();
        }
        constexpr std::string_view prefix{"farcaster://fid/"};
        if (!std::string_view{resource}.starts_with(prefix)) {
            continue;
        }
        ++fidResources;
        const std::string_view number{resource.data() + prefix.size(), resource.size() - prefix.size()};
        if (number.empty() || number.front() == '0') {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "SIWF FID resource is invalid."));
        }
        std::uint64_t fid{};
        const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), fid);
        if (error != std::errc{} || end != number.data() + number.size() || fid == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "SIWF FID resource is invalid."));
        }
        resourceFid = fid;
    }
    if (fidResources != 1U || !resourceFid || *resourceFid != expectedFid) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "SIWF message must contain exactly one FID resource matching the authenticated account."));
    }
    return {};
}

class Attempt final : public capability::auth::IAuthenticationAttempt {
public:
    Attempt(
        transport::http::IHttpTransport& transport,
        Config config,
        std::string channelToken,
        std::string nonce,
        std::string domain,
        std::string siweUri)
        : m_transport(transport),
          m_config(std::move(config)),
          m_channelToken(std::move(channelToken)),
          m_nonce(std::move(nonce)),
          m_domain(std::move(domain)),
          m_siweUri(std::move(siweUri)),
          m_deadline(std::chrono::steady_clock::now() + m_config.attemptLifetime)
    {
    }

    [[nodiscard]] foundation::result::Result<capability::auth::PollResult> poll() override
    {
        if (std::chrono::steady_clock::now() >= m_deadline) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Timeout,
                "Farcaster authentication approval window expired."));
        }
        auto response = m_transport.get(transport::http::HttpRequest{
            .url = joinUrl(m_config.relayBaseUrl, m_config.relayVersion, "channel/status"),
            .headers = {"Authorization: Bearer " + m_channelToken},
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 1024U * 1024U,
        });
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (auto status = requireSuccessStatus(*response, "Farcaster authentication relay"); !status) {
            return std::unexpected(std::move(status.error()));
        }
        auto json = parseJson(response->body);
        if (!json) {
            return std::unexpected(std::move(json.error()));
        }
        const std::string state = json->get("state", std::string{});
        if (state == "pending") {
            return capability::auth::PollResult{.state = capability::auth::PollState::Pending};
        }
        if (state != "completed") {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Farcaster authentication relay returned an unknown channel state."));
        }

        const auto message = json->get_optional<std::string>("message");
        const auto signature = json->get_optional<std::string>("signature");
        const auto fidValue = json->get_optional<std::uint64_t>("fid");
        if (!message || !signature || !fidValue || *fidValue == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Completed Farcaster authentication is missing its signed identity proof."));
        }
        const std::string responseNonce = json->get("nonce", std::string{});
        const std::string signatureDomain = json->get("signatureParams.domain", std::string{});
        const std::string signatureUri = json->get("signatureParams.siweUri", std::string{});
        if (responseNonce != m_nonce || signatureDomain != m_domain || signatureUri != m_siweUri) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "Farcaster relay challenge metadata does not match the created channel."));
        }

        auto parsed = parseSiwe(*message);
        if (!parsed) {
            return std::unexpected(std::move(parsed.error()));
        }
        if (auto valid = validateSiwe(*parsed, m_domain, m_siweUri, m_nonce, *fidValue); !valid) {
            return std::unexpected(std::move(valid.error()));
        }

        EthereumVerifier verifier{m_transport, m_config.optimismRpcUrl, m_config.requestTimeout};
        const bool acceptAuthAddress = json->get("acceptAuthAddress", true);
        auto method = verifier.verify(
            *message,
            *signature,
            parsed->address,
            domain::identity::Fid{*fidValue},
            acceptAuthAddress);
        if (!method) {
            return std::unexpected(std::move(method.error()));
        }

        const auto authenticatedAt = foundation::time::now();
        auto expiresAt = authenticatedAt +
            std::chrono::duration_cast<std::chrono::milliseconds>(m_config.sessionLifetime);
        if (parsed->expirationTime && *parsed->expirationTime < expiresAt) {
            expiresAt = *parsed->expirationTime;
        }
        domain::session::ClientSession session{
            .fid = domain::identity::Fid{*fidValue},
            .username = optionalString(*json, "username"),
            .displayName = optionalString(*json, "displayName"),
            .avatarUrl = optionalString(*json, "pfpUrl"),
            .custodyAddress = method->custodyAddress,
            .method = method->method,
            .authenticatedAt = authenticatedAt,
            .expiresAt = expiresAt,
        };
        return capability::auth::PollResult{
            .state = capability::auth::PollState::Completed,
            .session = std::move(session),
        };
    }

private:
    transport::http::IHttpTransport& m_transport;
    Config m_config;
    std::string m_channelToken;
    std::string m_nonce;
    std::string m_domain;
    std::string m_siweUri;
    std::chrono::steady_clock::time_point m_deadline{};
};

class Authentication final : public capability::auth::ICompatibilityAuthentication {
public:
    Authentication(transport::http::IHttpTransport& transport, Config config)
        : m_transport(transport), m_config(std::move(config))
    {
    }

    [[nodiscard]] foundation::result::Result<capability::auth::StartedAuthentication> begin(
        const capability::auth::SignInRequest& request) override
    {
        if (request.route != capability::auth::SignInRoute::Farcaster) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unsupported,
                "This compatibility adapter only supports direct Farcaster sign-in."));
        }
        const std::string domain = normalizeConfigurationValue(request.domain);
        const std::string siweUri = normalizeConfigurationValue(request.siweUri);
        if (domain.empty() || siweUri.empty() || !isValidHttpUri(siweUri)) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "SIWF requires an app-owned domain and an http/https relying-party URI."));
        }
        const std::string body = "{\"siweUri\":\"" + jsonEscape(siweUri) +
            "\",\"domain\":\"" + jsonEscape(domain) +
            "\",\"acceptAuthAddress\":true}";
        auto response = m_transport.post(transport::http::HttpPostRequest{
            .url = joinUrl(m_config.relayBaseUrl, m_config.relayVersion, "channel"),
            .headers = {"Content-Type: application/json"},
            .body = body,
            .timeout = m_config.requestTimeout,
            .maxBodyBytes = 512U * 1024U,
        });
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (auto status = requireSuccessStatus(*response, "Farcaster authentication relay"); !status) {
            auto error = std::move(status.error());
            if (response->statusCode == 400U || response->statusCode == 422U) {
                error.code = foundation::error::ErrorCode::InvalidArgument;
            }
            error.message.append(" Configured SIWF URI: \"");
            error.message.append(jsonEscape(siweUri));
            error.message.append("\".");
            return std::unexpected(std::move(error));
        }
        auto json = parseJson(response->body);
        if (!json) {
            return std::unexpected(std::move(json.error()));
        }
        const std::string token = json->get("channelToken", std::string{});
        const std::string approvalUrl = json->get("url", std::string{});
        const std::string nonce = json->get("nonce", std::string{});
        if (token.empty() || approvalUrl.empty() || nonce.empty() || !startsWithHttp(approvalUrl)) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Farcaster authentication relay returned an incomplete channel."));
        }
        capability::auth::StartedAuthentication started{
            .challenge = capability::auth::AuthenticationChallenge{
                .approvalUrl = approvalUrl,
            },
            .attempt = std::make_unique<Attempt>(
                m_transport,
                m_config,
                token,
                nonce,
                domain,
                siweUri),
        };
        return started;
    }

    [[nodiscard]] foundation::result::Result<std::string> resolveCustodyAddress(
        const domain::identity::Fid fid) override
    {
        if (fid.value == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Custody resolution requires a non-zero FID."));
        }
        EthereumVerifier verifier{m_transport, m_config.optimismRpcUrl, m_config.requestTimeout};
        return verifier.currentCustodyAddress(fid);
    }

private:
    transport::http::IHttpTransport& m_transport;
    Config m_config;
};

}

foundation::result::Result<std::unique_ptr<capability::auth::ICompatibilityAuthentication>>
makeAuthentication(transport::http::IHttpTransport& transport, Config config)
{
    config.relayBaseUrl = normalizeConfigurationValue(std::move(config.relayBaseUrl));
    config.relayVersion = normalizeConfigurationValue(std::move(config.relayVersion));
    config.optimismRpcUrl = normalizeConfigurationValue(std::move(config.optimismRpcUrl));

    if (!isValidHttpUri(config.relayBaseUrl) || !isValidHttpUri(config.optimismRpcUrl) ||
        config.relayBaseUrl.find_first_of("?#") != std::string::npos ||
        config.relayVersion.empty() || config.relayVersion.find_first_of("/?#\\") != std::string::npos ||
        config.requestTimeout <= std::chrono::milliseconds::zero() ||
        config.attemptLifetime <= std::chrono::minutes::zero() ||
        config.sessionLifetime <= std::chrono::hours::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Farcaster authentication configuration contains an invalid relay, RPC or timeout."));
    }
    std::unique_ptr<capability::auth::ICompatibilityAuthentication> authentication =
        std::make_unique<Authentication>(transport, std::move(config));
    return authentication;
}

namespace {

/** Never permit an identity gateway URL to contain credentials or extra paths. */
[[nodiscard]] bool validRecoveryOrigin(const std::string_view origin)
{
    if (!origin.starts_with("https://") || origin.size() > 253U
        || origin.find_first_of("?#@\\ \t\r\n") != std::string_view::npos)
        return false;
    const auto authority = origin.substr(8);
    if (authority.empty() || authority.starts_with('.')
        || authority.ends_with('.') || authority.contains(".."))
        return false;
    const auto slash = authority.find('/');
    if (slash != std::string_view::npos && slash != authority.size() - 1U)
        return false;
    const auto hostPort = authority.substr(0, slash);
    if (hostPort.empty() || hostPort.front() == '[' || hostPort.contains(':')
        || hostPort.find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.")
               != std::string_view::npos)
        return false;
    return hostPort.contains('.');
}

[[nodiscard]] bool validRecoveryTicket(const std::string_view value)
{
    return value.size() == 43U
        && std::ranges::all_of(value, [](const unsigned char ch) {
               return (ch >= 'A' && ch <= 'Z')
                   || (ch >= 'a' && ch <= 'z')
                   || (ch >= '0' && ch <= '9')
                   || ch == '_' || ch == '-';
           });
}

[[nodiscard]] bool validRecoverySignature(const std::string_view value)
{
    return value.size() == 132U && value.starts_with("0x")
        && std::ranges::all_of(value.substr(2), [](const unsigned char ch) {
               return std::isxdigit(ch);
           })
        && (value.substr(130) == "1b" || value.substr(130) == "1c"
            || value.substr(130) == "1B" || value.substr(130) == "1C");
}

[[nodiscard]] foundation::result::Result<boost::property_tree::ptree>
recoveryPost(transport::http::IHttpTransport& transport,
             std::string gatewayOrigin, const std::string_view path,
             const std::string& body)
{
    if (!validRecoveryOrigin(gatewayOrigin)) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "A configured HTTPS OpenProof recovery origin is required."));
    }
    if (gatewayOrigin.ends_with('/')) gatewayOrigin.pop_back();
    auto response = transport.post(transport::http::HttpPostRequest{
        .url = gatewayOrigin + std::string{path},
        .headers = {"Content-Type: application/json", "Accept: application/json"},
        .body = body,
        .timeout = std::chrono::milliseconds{12'000},
        .maxBodyBytes = 12U * 1024U,
    });
    if (!response) return std::unexpected(std::move(response.error()));
    if (response->statusCode != 200 || response->body.size() > 12U * 1024U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "OpenProof custody recovery refused or returned an oversized result."));
    }
    const bool isJson = std::ranges::any_of(response->headers,
        [](const std::string& header) {
            return lower(header).starts_with("content-type: application/json");
        });
    if (!isJson) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "OpenProof custody recovery response is not JSON."));
    }
    return parseJson(response->body);
}

} // namespace

foundation::result::Result<RecoveryChallenge>
startRecoveryChallenge(transport::http::IHttpTransport& transport,
                       std::string gatewayOrigin,
                       const domain::identity::Fid fid,
                       std::string custodyAddress)
{
    if (fid.value == 0U || !validRecoveryOrigin(gatewayOrigin)) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Invalid native recovery gateway or Farcaster identity."));
    }
    // A public address is sent to the gateway. The caller MUST already have
    // verified the exact same address/FID pair on Optimism IdRegistry.
    const auto checked = addressWord(custodyAddress);
    if (!checked) return std::unexpected(std::move(checked.error()));
    custodyAddress = lower(std::move(custodyAddress));
    const std::string fidText = std::to_string(fid.value);
    auto json = recoveryPost(transport, std::move(gatewayOrigin),
        "/v1/browser/farcaster/recovery/start",
        "{\"fid\":\"" + fidText + "\",\"address\":\"" + custodyAddress + "\"}");
    if (!json) return std::unexpected(std::move(json.error()));
    const auto ticket = json->get<std::string>("ticket", "");
    const auto responseFid = json->get<std::string>("fid", "");
    const auto message = json->get<std::string>("message", "");
    if (!validRecoveryTicket(ticket) || responseFid != fidText
        || message.size() < 80U || message.size() > 8192U
        || message.find("\n- farcaster://fid/" + fidText) == std::string::npos) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "OpenProof returned an invalid or mismatched recovery challenge."));
    }
    return RecoveryChallenge{.fid = fid, .ticket = ticket, .message = message};
}

foundation::result::Result<RecoveryIdentity>
completeRecoveryChallenge(transport::http::IHttpTransport& transport,
                          std::string gatewayOrigin,
                          const RecoveryChallenge& challenge,
                          std::string signature)
{
    if (challenge.fid.value == 0U
        || !validRecoveryTicket(challenge.ticket)
        || !validRecoverySignature(signature)
        || !validRecoveryOrigin(gatewayOrigin)) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Malformed or expired native recovery proof."));
    }
    auto json = recoveryPost(transport, std::move(gatewayOrigin),
        "/v1/browser/farcaster/recovery/complete",
        "{\"ticket\":\"" + challenge.ticket + "\",\"signature\":\"" + signature + "\"}");
    if (!json) return std::unexpected(std::move(json.error()));
    const auto responseFid = json->get<std::string>("fid", "");
    const auto subject = json->get<std::string>("subject", "");
    const auto issuer = json->get<std::string>("issuer", "");
    const auto accountTicket = json->get<std::string>("account_ticket", "");
    const auto expires = json->get<int>("expires_in", 0);
    const auto authenticated = json->get<bool>("authenticated", false);
    if (!authenticated || responseFid != std::to_string(challenge.fid.value)
        || subject.empty() || subject.size() > 512U
        || issuer != genycaster::deployment::openProofIssuer
        || !accountTicket.starts_with("recovery.")
        || !validRecoveryTicket(std::string_view{accountTicket}.substr(9))
        || expires < 60 || expires > 3600) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "OpenProof identity did not match the verified Farcaster recovery."));
    }
    return RecoveryIdentity{
        .fid = challenge.fid,
        .issuer = issuer,
        .subject = subject,
        .accountTicket = accountTicket,
        .expiresSeconds = expires,
    };
}

foundation::result::Result<MiniAppSignInProof>
verifyMiniAppSignIn(
    transport::http::IHttpTransport& transport,
    std::string message,
    std::string signature,
    std::string expectedDomain,
    std::string expectedUri,
    std::string expectedNonce,
    const domain::identity::Fid fid,
    const bool acceptAuthAddress,
    Config config)
{
    if (fid.value == 0U || message.size() < 80U || message.size() > 4096U
        || signature.size() < 130U || signature.size() > 2048U
        || expectedNonce.size() < 8U || expectedNonce.size() > 128U
        || !config.optimismRpcUrl.starts_with("https://")
        || !isValidHttpUri(config.optimismRpcUrl)
        || config.requestTimeout <= std::chrono::milliseconds::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Invalid Mini App SIWF proof, nonce, or verification endpoint."));
    }
    auto parsed = parseSiwe(message);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    if (auto checked = validateSiwe(*parsed, expectedDomain, expectedUri,
                                    expectedNonce, fid.value); !checked) {
        return std::unexpected(std::move(checked.error()));
    }
    // A server-generated nonce and a short, origin-specific expiry are
    // essential for replay resistance. The Mini App relying party MUST also
    // verify these fields on its own server.
    if (!parsed->expirationTime) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "Mini App SIWF proof requires a short expiration time."));
    }
    const auto now = foundation::time::now();
    if (*parsed->expirationTime > now + std::chrono::minutes{10}) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "Mini App SIWF proof has an excessive lifetime."));
    }
    EthereumVerifier verifier{transport, std::move(config.optimismRpcUrl),
                              config.requestTimeout};
    auto result = verifier.verify(message, signature, parsed->address, fid,
                                  acceptAuthAddress);
    if (!result) return std::unexpected(std::move(result.error()));
    return MiniAppSignInProof{
        .authAddress = result->method == domain::session::AuthenticationMethod::AuthAddress,
    };
}

foundation::result::Result<domain::identity::Fid>
resolveRecoveryCustodyFid(
    transport::http::IHttpTransport& transport,
    std::string custodyAddress,
    Config config)
{
    config.optimismRpcUrl = normalizeConfigurationValue(std::move(config.optimismRpcUrl));
    // Custody verification is an authentication operation; reject HTTP
    // endpoints instead of accepting the more permissive SIWF test transport.
    if (!config.optimismRpcUrl.starts_with("https://") ||
        !isValidHttpUri(config.optimismRpcUrl) ||
        config.optimismRpcUrl.find_first_of("?#") != std::string::npos ||
        config.requestTimeout <= std::chrono::milliseconds::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Custody recovery requires a trusted HTTPS Optimism RPC endpoint."));
    }
    EthereumVerifier verifier{transport, std::move(config.optimismRpcUrl),
                              config.requestTimeout};
    return verifier.resolveRecoveryFid(custodyAddress);
}

}
