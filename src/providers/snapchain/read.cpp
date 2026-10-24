module;

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_set>
#include <vector>

#include <boost/json.hpp>
#include <boost/system/error_code.hpp>

module farcaster.provider.snapchain.read;

import farcaster.application.read.model;
import farcaster.capability.catalog;
import farcaster.capability.notification;
import farcaster.capability.pagination;
import farcaster.capability.search;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.social;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.transport.http;

namespace farcaster::provider::snapchain::read {
namespace {
namespace json = boost::json;
namespace async = farcaster::application::read::async_gateway;
namespace model = farcaster::application::read::model;
namespace foundation = farcaster::foundation;
using Error = foundation::error::Error;
using ErrorCode = foundation::error::ErrorCode;

[[nodiscard]] Error protocolError(std::string detail)
{
    return Error::make(ErrorCode::ProtocolFailure, std::move(detail));
}

[[nodiscard]] Error unsupported(std::string operation)
{
    return Error::make(ErrorCode::Unsupported,
        "Raw Snapchain cannot satisfy " + std::move(operation)
        + " with its available bounded read capabilities.");
}

[[nodiscard]] const json::value* field(const json::object& source, const std::string_view key)
{
    const auto entry = source.find(key);
    return entry == source.end() ? nullptr : &entry->value();
}

[[nodiscard]] const json::object* object(const json::value* value)
{
    return value && value->is_object() ? &value->as_object() : nullptr;
}

[[nodiscard]] const json::array* array(const json::value* value)
{
    return value && value->is_array() ? &value->as_array() : nullptr;
}

[[nodiscard]] std::optional<std::string> string(const json::value* value)
{
    if (!value || !value->is_string()) return std::nullopt;
    const auto& source = value->as_string();
    return std::string{source.data(), source.size()};
}

[[nodiscard]] std::optional<std::uint64_t> unsignedValue(const json::value* value)
{
    if (!value) return std::nullopt;
    if (value->is_uint64()) return value->as_uint64();
    if (value->is_int64() && value->as_int64() >= 0)
        return static_cast<std::uint64_t>(value->as_int64());
    if (const auto text = string(value)) {
        std::uint64_t result{};
        const char* const start = text->data();
        const auto [end, error] = std::from_chars(start, start + text->size(), result);
        if (error == std::errc{} && end == start + text->size()) return result;
    }
    return std::nullopt;
}

[[nodiscard]] foundation::result::Result<json::value> parse(const std::string_view input)
{
    boost::system::error_code error{};
    json::value parsed = json::parse(input, error);
    if (error) return std::unexpected(protocolError("Raw Snapchain response is not valid JSON."));
    if (!parsed.is_object())
        return std::unexpected(protocolError("Raw Snapchain response must be an object."));
    return parsed;
}

[[nodiscard]] std::optional<std::string> cursorFrom(const json::object& value)
{
    auto cursor = string(field(value, "nextPageToken"));
    return cursor && !cursor->empty() ? cursor : std::nullopt;
}

[[nodiscard]] std::string escapeQuery(const std::string_view value)
{
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result{};
    result.reserve(value.size() * 3U);
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
            || (byte >= '0' && byte <= '9') || byte == '-' || byte == '_'
            || byte == '.' || byte == '~') {
            result.push_back(static_cast<char>(byte));
        } else {
            result.push_back('%');
            result.push_back(digits[byte >> 4U]);
            result.push_back(digits[byte & 15U]);
        }
    }
    return result;
}

[[nodiscard]] bool validHash(const std::string_view hash)
{
    if (hash.size() != 42U || !hash.starts_with("0x")) return false;
    for (std::size_t index{2}; index < hash.size(); ++index) {
        if (!std::isxdigit(static_cast<unsigned char>(hash[index]))) return false;
    }
    return true;
}

[[nodiscard]] std::optional<std::string> canonicalSignerKey(const json::object& message)
{
    // Protobuf JSON serializes bytes as base64; a subset of compatible
    // implementations also emits 0x-prefixed hex. Reject other lengths.
    const auto value = string(field(message, "signer"));
    if (!value || value->empty()) return std::nullopt;
    std::string result{"0x"};
    constexpr char hex[] = "0123456789abcdef";
    if (value->size() == 66U && value->starts_with("0x")) {
        for (std::size_t i = 2U; i < value->size(); ++i) {
            const auto c = static_cast<unsigned char>((*value)[i]);
            if (!std::isxdigit(c)) return std::nullopt;
            result.push_back(static_cast<char>(std::tolower(c)));
        }
        return result;
    }
    if (value->size() != 44U || (*value)[43] != '=') return std::nullopt;
    std::uint32_t bits = 0U;
    int available = 0;
    std::size_t decoded = 0U;
    for (std::size_t i = 0U; i < 43U; ++i) {
        const char ch = (*value)[i];
        int digit = -1;
        if (ch >= 'A' && ch <= 'Z') digit = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') digit = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') digit = ch - '0' + 52;
        else if (ch == '+') digit = 62;
        else if (ch == '/') digit = 63;
        if (digit < 0) return std::nullopt;
        bits = (bits << 6U) | static_cast<std::uint32_t>(digit);
        available += 6;
        if (available >= 8) {
            available -= 8;
            const auto byte = static_cast<unsigned>((bits >> available) & 0xffU);
            result.push_back(hex[byte >> 4U]);
            result.push_back(hex[byte & 15U]);
            ++decoded;
        }
    }
    // One '=' represents two original bytes in the final three-byte block.
    if (decoded != 32U || available != 2 || (bits & 3U) != 0U)
        return std::nullopt;
    return result;
}

[[nodiscard]] std::optional<int> userDataType(const json::value* value)
{
    if (const auto code = unsignedValue(value); code && *code <= 255U)
        return static_cast<int>(*code);
    if (const auto label = string(value)) {
        if (*label == "USER_DATA_TYPE_PFP") return 1;
        if (*label == "USER_DATA_TYPE_DISPLAY") return 2;
        if (*label == "USER_DATA_TYPE_BIO") return 3;
        if (*label == "USER_DATA_TYPE_URL") return 5;
        if (*label == "USER_DATA_TYPE_USERNAME") return 6;
        if (*label == "USER_DATA_TYPE_LOCATION") return 7;
        if (*label == "USER_DATA_TYPE_BANNER") return 10;
    }
    return std::nullopt;
}

[[nodiscard]] foundation::result::Result<domain::cast::Cast> decodeCast(
    const json::object& message, const domain::identity::Fid expectedFid)
{
    const auto hash = string(field(message, "hash"));
    const auto* data = object(field(message, "data"));
    if (!data || !hash || !validHash(*hash))
        return std::unexpected(protocolError("Raw cast is missing canonical data/hash."));
    const auto fid = unsignedValue(field(*data, "fid"));
    if (!fid || *fid != expectedFid.value)
        return std::unexpected(protocolError("Raw cast author FID mismatch."));
    const auto timestamp = unsignedValue(field(*data, "timestamp"));
    if (!timestamp || *timestamp > std::numeric_limits<std::uint32_t>::max())
        return std::unexpected(protocolError("Invalid Farcaster epoch timestamp."));
    const auto* body = object(field(*data, "castAddBody"));
    if (!body)
        return std::unexpected(protocolError("CastAdd body is missing."));
    const auto text = string(field(*body, "text"));
    if (!text) return std::unexpected(protocolError("CastAdd text must be a string."));
    // The Farcaster epoch starts at 2021-01-01T00:00:00Z.
    constexpr std::uint64_t farcasterEpochUnixSeconds = 1'609'459'200ULL;
    domain::cast::Cast result{
        .id = domain::cast::CastId{*hash},
        .author = expectedFid,
        .text = *text,
        .createdAt = foundation::time::Timestamp{
            std::chrono::milliseconds{
                static_cast<std::int64_t>((farcasterEpochUnixSeconds + *timestamp) * 1000ULL)}},
    };
    // Snapchain validates canonical message signatures before storing them;
    // the reported key is observed signer identity, not app attribution.
    result.signerPublicKeyHex = canonicalSignerKey(message);
    if (const auto* parent = object(field(*body, "parentCastId"))) {
        const auto parentHash = string(field(*parent, "hash"));
        if (parentHash && validHash(*parentHash))
            result.parent = domain::cast::CastId{*parentHash};
    } else if (const auto parentUrl = string(field(*body, "parentUrl"))) {
        if (!parentUrl->empty()) result.parentUrl = *parentUrl;
    }
    const auto appendUrl = [&result](const std::string& url) {
        if (!url.empty())
            result.embeds.push_back(domain::cast::Embed{
                .kind = domain::cast::EmbedKind::Url, .value = url});
    };
    if (const auto* embeds = array(field(*body, "embeds"))) {
        for (const auto& embed : *embeds) {
            const auto* value = object(&embed);
            if (!value) continue;
            if (const auto url = string(field(*value, "url"))) {
                appendUrl(*url);
            } else if (const auto* cast = object(field(*value, "castId"))) {
                const auto embeddedHash = string(field(*cast, "hash"));
                const auto author = unsignedValue(field(*cast, "fid"));
                if (embeddedHash && validHash(*embeddedHash) && author && *author != 0U)
                    result.embeds.push_back(domain::cast::Embed{
                        .kind = domain::cast::EmbedKind::CastReference,
                        .value = *embeddedHash,
                        .referencedAuthor = domain::identity::Fid{*author},
                    });
            }
        }
    }
    if (const auto* legacy = array(field(*body, "embedsDeprecated"))) {
        for (const auto& embed : *legacy) {
            if (const auto url = string(&embed)) appendUrl(*url);
        }
    }
    return result;
}

[[nodiscard]] bool matchesFeed(const domain::cast::Cast& cast, const model::FeedQuery::Kind kind)
{
    if (kind == model::FeedQuery::Kind::UserReplies)
        return cast.parent.has_value();
    if (cast.parent) return false;
    const bool quoted = std::any_of(cast.embeds.begin(), cast.embeds.end(),
        [](const domain::cast::Embed& embed) {
            return embed.kind == domain::cast::EmbedKind::CastReference;
        });
    if (kind == model::FeedQuery::Kind::UserQuotes) return quoted;
    return kind == model::FeedQuery::Kind::UserPosts && !quoted;
}

[[nodiscard]] capability::model::CapabilityDescriptor supported(
    capability::model::CapabilityId id)
{
    return {
        .id = std::move(id),
        .version = {1, 0},
        .origin = capability::model::CapabilityOrigin::Protocol,
        .quality = capability::model::CapabilityQuality::Native,
        .access = {capability::model::CapabilityAccess::Read},
        .isSelfHostable = true,
    };
}

[[nodiscard]] capability::model::CapabilityDescriptor boundedDerived(
    capability::model::CapabilityId id)
{
    auto descriptor = supported(std::move(id));
    descriptor.origin = capability::model::CapabilityOrigin::Derived;
    descriptor.quality = capability::model::CapabilityQuality::Derived;
    return descriptor;
}

struct InFlight final {
    std::atomic_bool cancelled{false};
};

using BodyCompletion = std::function<void(foundation::result::Result<std::string>)>;


/** One bounded, fully checked raw Following snapshot. The native projection
 * refuses oversized graphs or insufficient per-author scanning rather than
 * displaying a silently truncated "complete" live feed.
 */

/** Bounded best-effort raw signer-to-app enrichment, independent of host API
 * metadata. A failed optional lookup must not discard actual signed Casts.
 * Returning no sourceAppFid is deliberately preferable to invented identity.
 */
struct AppAttributionWork final : std::enable_shared_from_this<AppAttributionWork> {
    using Starter = std::function<foundation::result::Result<async::Request>(
        std::string, BodyCompletion)>;
    Starter request{};
    async::FeedCompletion finish{};
    capability::feed::FeedPage page{};
    std::vector<std::pair<std::uint64_t, std::string>> keys{};
    std::size_t cursor{};
    std::mutex mutex{};
    std::vector<async::Request> requests{};
    std::atomic_bool cancelled{false};
    std::atomic_bool completed{false};
    static constexpr std::size_t maximumLookups = 8U;

    AppAttributionWork(Starter starter, async::FeedCompletion callback)
        : request(std::move(starter)), finish(std::move(callback)) {}

    void cancel() {
        cancelled.store(true, std::memory_order_release);
        std::scoped_lock lock{mutex};
        for (const auto& pending : requests) pending.cancel();
    }
    [[nodiscard]] foundation::result::Status issue(
        std::string url, BodyCompletion callback) {
        if (cancelled.load(std::memory_order_acquire))
            return std::unexpected(Error::make(ErrorCode::Unavailable, "Attribution cancelled."));
        auto launched = request(std::move(url), std::move(callback));
        if (!launched) return std::unexpected(std::move(launched.error()));
        std::scoped_lock lock{mutex};
        if (cancelled.load(std::memory_order_acquire)) launched->cancel();
        else requests.push_back(std::move(*launched));
        return {};
    }
    void complete(foundation::result::Result<capability::feed::FeedPage> value) {
        if (cancelled.load(std::memory_order_acquire)
            || completed.exchange(true, std::memory_order_acq_rel)) return;
        finish(std::move(value));
    }
    void start(capability::feed::FeedPage ready) {
        page = std::move(ready);
        for (const auto& cast : page.casts) {
            if (!cast.signerPublicKeyHex || cast.author.value == 0U) continue;
            std::pair<std::uint64_t, std::string> key{
                cast.author.value, *cast.signerPublicKeyHex};
            if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
                if (keys.size() >= maximumLookups) break;
                keys.push_back(std::move(key));
            }
        }
        next();
    }
    void next() {
        if (cancelled.load(std::memory_order_acquire)) return;
        if (cursor >= keys.size())
            return complete(std::move(page));
        const auto [fid, key] = keys[cursor++];
        auto issued = issue("/v1/signer?fid=" + std::to_string(fid)
            + "&signer=" + escapeQuery(key),
            [self = shared_from_this(), fid, key](
                foundation::result::Result<std::string> response) mutable {
                if (self->cancelled.load(std::memory_order_acquire)) return;
                if (response) {
                    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    auto app = decodeSignerAppFid(*response,
                        domain::identity::Fid{fid}, key,
                        static_cast<std::uint64_t>(now));
                    if (app && *app) {
                        for (auto& cast : self->page.casts) {
                            if (cast.author.value == fid && cast.signerPublicKeyHex
                                && *cast.signerPublicKeyHex == key)
                                cast.sourceAppFid = **app;
                        }
                    }
                }
                // 404, stale, revoked, malformed or unreachable signer
                // cannot justify app attribution. Cast content remains usable.
                self->next();
            });
        if (!issued) next(); // Optional enrichment may fail at admission.
    }
};

struct FollowingWork final : std::enable_shared_from_this<FollowingWork> {
    using Starter = std::function<foundation::result::Result<async::Request>(
        std::string, BodyCompletion)>;
    Starter startRead{};
    storage::continuation::ContinuationStore& continuations;
    provider::model::ProviderId providerId{};
    std::string scope{};
    domain::identity::Fid viewer{};
    std::size_t limit{20U};
    async::FeedCompletion completion{};
    std::vector<std::uint64_t> authors{};
    std::vector<domain::cast::Cast> candidates{};
    std::optional<std::pair<std::int64_t, std::string>> before{};
    static constexpr std::size_t maximumFollowees = 256U;
    static constexpr std::size_t graphPageSize = 100U;
    static constexpr std::size_t maximumGraphPages = 4U;
    static constexpr std::size_t maximumAuthorPages = 8U;
    std::size_t authorIndex{0U};
    std::size_t graphPages{0U};
    std::size_t authorPages{0U};
    std::optional<foundation::time::Timestamp> lastAuthorTimestamp{};
    std::unordered_set<std::string> graphTokens{};
    std::unordered_set<std::string> authorTokens{};
    std::mutex mutex{};
    std::vector<async::Request> operations{};
    std::shared_ptr<AppAttributionWork> appAttribution{};
    std::atomic_bool cancelled{false};
    std::atomic_bool finished{false};

    FollowingWork(Starter starter,
                  storage::continuation::ContinuationStore& cursorStore,
                  provider::model::ProviderId provider,
                  std::string cursorScope, domain::identity::Fid fid,
                  std::size_t requested, async::FeedCompletion observer)
        : startRead(std::move(starter)), continuations(cursorStore),
          providerId(std::move(provider)), scope(std::move(cursorScope)),
          viewer(fid), limit(requested), completion(std::move(observer))
    {
    }

    void cancel() {
        cancelled.store(true, std::memory_order_release);
        std::shared_ptr<AppAttributionWork> child{};
        {
            std::scoped_lock lock{mutex};
            for (const auto& operation : operations) operation.cancel();
            child = appAttribution;
        }
        if (child) child->cancel();
    }
    void remember(async::Request operation) {
        std::scoped_lock lock{mutex};
        if (cancelled.load(std::memory_order_acquire)) operation.cancel();
        else operations.push_back(std::move(operation));
    }
    void finish(foundation::result::Result<capability::feed::FeedPage> result) {
        if (cancelled.load(std::memory_order_acquire)
            || finished.exchange(true, std::memory_order_acq_rel)) return;
        completion(std::move(result));
    }
    void fail(Error error) {
        finish(std::unexpected(std::move(error)));
    }
    [[nodiscard]] foundation::result::Status issue(
        std::string url, BodyCompletion callback) {
        if (cancelled.load(std::memory_order_acquire))
            return std::unexpected(Error::make(ErrorCode::Unavailable, "Following cancelled."));
        auto started = startRead(std::move(url), std::move(callback));
        if (!started) return std::unexpected(std::move(started.error()));
        remember(std::move(*started));
        return {};
    }
    [[nodiscard]] foundation::result::Status begin() {
        if (!authors.empty()) {
            nextAuthor();
            return {};
        }
        authors.push_back(viewer.value);
        return nextGraphPage(std::nullopt);
    }
    [[nodiscard]] foundation::result::Status nextGraphPage(
        std::optional<std::string> token) {
        if (graphPages >= maximumGraphPages)
            return std::unexpected(unsupported("Following graph exceeds bounded page count"));
        std::string path = "/v1/linksByFid?fid=" + std::to_string(viewer.value)
            + "&link_type=follow&pageSize=" + std::to_string(graphPageSize);
        if (token) {
            if (token->empty() || token->size() > 2048U
                || !graphTokens.insert(*token).second)
                return std::unexpected(protocolError("Repeated or invalid Following graph cursor."));
            path += "&pageToken=" + escapeQuery(*token);
        }
        ++graphPages;
        return issue(std::move(path),
            [self = shared_from_this()](
                foundation::result::Result<std::string> response) mutable {
                if (self->cancelled.load(std::memory_order_acquire)) return;
                if (!response) return self->fail(std::move(response.error()));
                auto parsed = parse(*response);
                if (!parsed) return self->fail(std::move(parsed.error()));
                const auto* rawMessages = array(field(parsed->as_object(), "messages"));
                if (!rawMessages || rawMessages->size() > graphPageSize)
                    return self->fail(protocolError(
                        "Raw Following graph page must contain <=100 messages."));
                auto graph = decodeFollowLinks(*response, self->viewer,
                    model::SocialQuery::Kind::Following);
                if (!graph) return self->fail(std::move(graph.error()));
                for (const auto& relation : graph->relations)
                    self->authors.push_back(relation.target.value);
                std::sort(self->authors.begin(), self->authors.end());
                self->authors.erase(std::unique(self->authors.begin(), self->authors.end()),
                                    self->authors.end());
                if (self->authors.size() > maximumFollowees + 1U)
                    return self->fail(unsupported("Following graph exceeds 256 followees"));
                if (const auto next = cursorFrom(parsed->as_object())) {
                    auto status = self->nextGraphPage(next);
                    if (!status) self->fail(std::move(status.error()));
                    return;
                }
                self->nextAuthor();
            });
    }
    void nextAuthor() {
        if (cancelled.load(std::memory_order_acquire)) return;
        if (authorIndex == authors.size()) {
            finishMerged();
            return;
        }
        authorPages = 0U;
        lastAuthorTimestamp.reset();
        authorTokens.clear();
        auto started = nextAuthorPage(std::nullopt);
        if (!started) fail(std::move(started.error()));
    }
    [[nodiscard]] foundation::result::Status nextAuthorPage(
        std::optional<std::string> token) {
        if (authorPages >= maximumAuthorPages)
            return std::unexpected(unsupported("Following author history exceeds 8 pages"));
        const auto fid = domain::identity::Fid{authors[authorIndex]};
        std::string path = "/v1/castsByFid?fid=" + std::to_string(fid.value)
            + "&pageSize=100&reverse=1";
        if (token) {
            if (token->empty() || token->size() > 2048U
                || !authorTokens.insert(*token).second)
                return std::unexpected(protocolError("Repeated or invalid author Cast cursor."));
            path += "&pageToken=" + escapeQuery(*token);
        }
        ++authorPages;
        return issue(std::move(path),
            [self = shared_from_this(), fid](
                foundation::result::Result<std::string> response) mutable {
                if (self->cancelled.load(std::memory_order_acquire)) return;
                if (!response) return self->fail(std::move(response.error()));
                auto parsed = parse(*response);
                if (!parsed) return self->fail(std::move(parsed.error()));
                const auto* messages = array(field(parsed->as_object(), "messages"));
                if (!messages || messages->size() > 100U)
                    return self->fail(protocolError("Raw Following page must contain <=100 messages."));
                for (const auto& item : *messages) {
                    const auto* message = object(&item);
                    const auto* data = message ? object(field(*message, "data")) : nullptr;
                    if (!data)
                        return self->fail(protocolError("Malformed Following Cast entry."));
                    if (!object(field(*data, "castAddBody"))) continue;
                    auto cast = decodeCast(*message, fid);
                    if (!cast) return self->fail(std::move(cast.error()));
                    // Check chronological order across *all* pages of an author,
                    // not only inside each HTTP response.
                    if (self->lastAuthorTimestamp
                        && cast->createdAt > *self->lastAuthorTimestamp)
                        return self->fail(protocolError("Following author Cast pages are unordered."));
                    self->lastAuthorTimestamp = cast->createdAt;
                    if (cast->parent) continue;
                    const auto ms = cast->createdAt.time_since_epoch().count();
                    if (self->before
                        && (ms > self->before->first
                            || (ms == self->before->first
                                && cast->id.value >= self->before->second)))
                        continue;
                    self->candidates.push_back(std::move(*cast));
                }
                // Global top-K+1 is sufficient to rank every processed author
                // and bounds memory even after thousands of raw Casts.
                std::sort(self->candidates.begin(), self->candidates.end(),
                    [](const domain::cast::Cast& a, const domain::cast::Cast& b) {
                        if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
                        return a.id.value > b.id.value;
                    });
                self->candidates.erase(std::unique(
                    self->candidates.begin(), self->candidates.end(),
                    [](const domain::cast::Cast& a, const domain::cast::Cast& b) {
                        return a.id.value == b.id.value;
                    }), self->candidates.end());
                if (self->candidates.size() > self->limit + 1U)
                    self->candidates.resize(self->limit + 1U);
                if (const auto next = cursorFrom(parsed->as_object())) {
                    // Additional pages can be skipped only when their newest
                    // possible timestamp is *strictly* below global rank K+1;
                    // equal-second hashes still need investigation.
                    if (self->candidates.size() <= self->limit
                        || !self->lastAuthorTimestamp
                        || *self->lastAuthorTimestamp >= self->candidates.back().createdAt) {
                        auto status = self->nextAuthorPage(next);
                        if (!status) self->fail(std::move(status.error()));
                        return;
                    }
                }
                ++self->authorIndex;
                self->nextAuthor();
            });
    }
    void finishMerged() {
        std::sort(candidates.begin(), candidates.end(),
            [](const domain::cast::Cast& a, const domain::cast::Cast& b) {
                if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
                return a.id.value > b.id.value;
            });
        candidates.erase(std::unique(candidates.begin(), candidates.end(),
            [](const domain::cast::Cast& a, const domain::cast::Cast& b) {
                return a.id.value == b.id.value;
            }), candidates.end());
        capability::feed::FeedPage page{};
        const bool hasMore = candidates.size() > limit;
        if (candidates.size() > limit) candidates.resize(limit);
        for (auto& cast : candidates) {
            page.entries.push_back(domain::feed::FeedEntry{
                .castId = cast.id, .sortTime = cast.createdAt});
            page.casts.push_back(std::move(cast));
        }
        if (hasMore) {
            const auto& last = page.casts.back();
            json::array values{};
            for (const auto fid : authors) values.emplace_back(fid);
            json::object metadata{};
            metadata["ms"] = last.createdAt.time_since_epoch().count();
            metadata["id"] = last.id.value;
            metadata["authors"] = std::move(values);
            metadata["limit"] = limit;
            auto next = continuations.bind(
                capability::catalog::feedFollowingRead(),
                providerId, scope + json::serialize(metadata));
            if (!next) return fail(std::move(next.error()));
            page.next = std::move(*next);
        }
        auto appWork = std::make_shared<AppAttributionWork>(
            startRead, [weak = weak_from_this()](
                foundation::result::Result<capability::feed::FeedPage> enriched) mutable {
                if (auto self = weak.lock())
                    self->finish(std::move(enriched));
            });
        {
            std::scoped_lock lock{mutex};
            appAttribution = appWork;
            if (cancelled.load(std::memory_order_acquire)) appWork->cancel();
        }
        appWork->start(std::move(page));
    }
};

class Gateway final : public async::IAsyncReadGateway {
public:
    Gateway(
        transport::async_http::IAsyncHttpTransport& transport,
        execution::weighted_thread_pool::WeightedThreadPoolExecutor& decoder,
        storage::continuation::ContinuationStore& continuations,
        Config config)
        : m_transport(transport), m_decoder(decoder),
          m_continuations(continuations), m_config(std::move(config))
    {
        while (!m_config.baseUrl.empty() && m_config.baseUrl.back() == '/')
            m_config.baseUrl.pop_back();
    }

    [[nodiscard]] foundation::result::Result<async::Request> resolveIdentity(
        const std::string_view username, async::IdentityCompletion completion,
        model::ReadOptions options) override
    {
        if (username.empty() || username.size() > 256U)
            return std::unexpected(Error::make(
                ErrorCode::InvalidArgument, "Username must be nonempty and bounded."));
        return request("/v1/userNameProofByName?name=" + escapeQuery(username),
            [complete = std::move(completion)](foundation::result::Result<std::string> body) mutable {
                if (!body) return complete(std::unexpected(std::move(body.error())));
                auto parsed = parse(*body);
                if (!parsed) return complete(std::unexpected(std::move(parsed.error())));
                const auto* outer = &parsed->as_object();
                const auto* proof = object(field(*outer, "proof"));
                const auto fid = unsignedValue(field(proof ? *proof : *outer, "fid"));
                if (!fid || *fid == 0U) return complete(std::unexpected(protocolError(
                    "Username proof does not contain a valid FID.")));
                complete(domain::identity::Fid{*fid});
            }, options);
    }

    [[nodiscard]] foundation::result::Result<async::Request> profile(
        const domain::identity::Fid fid,
        async::ProfileCompletion completion, model::ReadOptions options) override
    {
        if (fid.value == 0U) return std::unexpected(Error::make(
            ErrorCode::InvalidArgument, "Profile FID cannot be zero."));
        return request("/v1/userDataByFid?fid=" + std::to_string(fid.value) + "&pageSize=100",
            [fid, complete = std::move(completion)](
                foundation::result::Result<std::string> body) mutable {
                if (!body) return complete(std::unexpected(std::move(body.error())));
                complete(decodeUserData(*body, fid));
            }, options);
    }

    [[nodiscard]] foundation::result::Result<async::Request> cast(
        domain::cast::CastId, async::CastCompletion, model::ReadOptions) override
    {
        return std::unexpected(unsupported("cast lookup by hash alone"));
    }

    [[nodiscard]] foundation::result::Result<async::Request> feed(
        model::FeedQuery query, async::FeedCompletion completion,
        model::ReadOptions options) override
    {
        if (query.kind == model::FeedQuery::Kind::Following)
            return followingFeed(std::move(query), std::move(completion), options);
        if (query.kind != model::FeedQuery::Kind::UserPosts
            && query.kind != model::FeedQuery::Kind::UserQuotes
            && query.kind != model::FeedQuery::Kind::UserReplies)
            return std::unexpected(unsupported("aggregated feeds"));
        if (!query.fid || query.fid->value == 0U)
            return std::unexpected(Error::make(
                ErrorCode::InvalidArgument, "User feed must contain a valid FID."));
        const auto capabilityId = query.kind == model::FeedQuery::Kind::UserPosts
            ? capability::catalog::feedUserPostsRead()
            : query.kind == model::FeedQuery::Kind::UserQuotes
                ? capability::catalog::feedUserQuotesRead()
                : capability::catalog::feedUserRepliesRead();
        std::string url{"/v1/castsByFid?fid="};
        url += std::to_string(query.fid->value);
        url += "&pageSize=" + std::to_string(std::clamp(query.limit, std::size_t{1}, std::size_t{100}));
        // HTTP read-node API specifies 1 for reverse chronological order.
        url += "&reverse=1";
        if (query.continuation) {
            auto record = m_continuations.resolve(*query.continuation);
            if (!record) return std::unexpected(std::move(record.error()));
            if (record->capabilityId.value != capabilityId.value
                || record->providerId.value != m_config.providerId.value)
                return std::unexpected(Error::make(
                    ErrorCode::Conflict, "Read continuation does not match its capability/provider."));
            // A continuation is valid for exactly one provider *instance*.
            // Provider IDs can be reused after editing node profiles: a stale
            // cursor must never be replayed against a different HTTP origin.
            const std::string prefix = "raw-v2:" + m_config.baseUrl + "\n"
                + std::to_string(query.fid->value) + "\n";
            if (!record->providerCursor.starts_with(prefix))
                return std::unexpected(Error::make(
                    ErrorCode::Conflict,
                    "Raw-node continuation has a different endpoint or user FID; "
                    "restart this feed from its first page."));
            url += "&pageToken=" + escapeQuery(
                std::string_view{record->providerCursor}.substr(prefix.size()));
        }
        auto work = std::make_shared<AppAttributionWork>(
            [this, options](std::string path, BodyCompletion done) {
                return request(std::move(path), std::move(done), options);
            }, std::move(completion));
        auto launched = work->issue(std::move(url),
            [this, work, fid = *query.fid, kind = query.kind, capabilityId](
                foundation::result::Result<std::string> body) mutable {
                if (!body) return work->complete(std::unexpected(std::move(body.error())));
                auto page = decodeUserCasts(*body, fid, kind);
                if (!page) return work->complete(std::unexpected(std::move(page.error())));
                auto parsed = parse(*body);
                if (!parsed) return work->complete(std::unexpected(std::move(parsed.error())));
                if (const auto cursor = cursorFrom(parsed->as_object())) {
                    auto next = m_continuations.bind(
                        capabilityId, m_config.providerId,
                        "raw-v2:" + m_config.baseUrl + "\n"
                            + std::to_string(fid.value) + "\n" + *cursor);
                    if (!next) return work->complete(std::unexpected(std::move(next.error())));
                    page->next = std::move(*next);
                }
                work->start(std::move(*page));
            });
        if (!launched) return std::unexpected(std::move(launched.error()));
        return async::Request{[work] { work->cancel(); }};
    }

    [[nodiscard]] foundation::result::Result<async::Request> search(
        model::SearchQuery, async::SearchCompletion, model::ReadOptions) override
    {
        return std::unexpected(unsupported("full-text search"));
    }

    [[nodiscard]] foundation::result::Result<async::Request> notifications(
        model::NotificationQuery, async::NotificationCompletion, model::ReadOptions) override
    {
        return std::unexpected(unsupported("notifications"));
    }

    [[nodiscard]] foundation::result::Result<async::Request> social(
        model::SocialQuery query, async::SocialCompletion completion,
        model::ReadOptions options) override
    {
        if (query.fid.value == 0U || query.limit == 0U || query.limit > 100U)
            return std::unexpected(Error::make(
                ErrorCode::InvalidArgument, "Raw graph query needs a FID and 1..100 rows."));
        const bool following = query.kind == model::SocialQuery::Kind::Following;
        const auto capabilityId = following
            ? capability::catalog::graphFollowingRead()
            : capability::catalog::graphFollowersRead();
        const std::string scope = "raw-social-v1:" + m_config.baseUrl + "\n"
            + std::to_string(query.fid.value) + "\n";
        std::string url = following
            ? "/v1/linksByFid?fid=" + std::to_string(query.fid.value)
            : "/v1/linksByTargetFid?target_fid=" + std::to_string(query.fid.value);
        url += "&link_type=follow&pageSize=" + std::to_string(query.limit);
        if (query.continuation) {
            auto record = m_continuations.resolve(*query.continuation);
            if (!record) return std::unexpected(std::move(record.error()));
            if (record->capabilityId.value != capabilityId.value
                || record->providerId.value != m_config.providerId.value
                || !record->providerCursor.starts_with(scope))
                return std::unexpected(Error::make(
                    ErrorCode::Conflict, "Graph cursor has another source, FID, or direction."));
            url += "&pageToken=" + escapeQuery(
                std::string_view{record->providerCursor}.substr(scope.size()));
        }
        return request(std::move(url),
            [this, fid = query.fid, kind = query.kind, capabilityId,
             scope, complete = std::move(completion)](
                foundation::result::Result<std::string> body) mutable {
                if (!body) return complete(std::unexpected(std::move(body.error())));
                auto result = decodeFollowLinks(*body, fid, kind);
                if (!result) return complete(std::unexpected(std::move(result.error())));
                auto parsed = parse(*body);
                if (!parsed) return complete(std::unexpected(std::move(parsed.error())));
                if (const auto cursor = cursorFrom(parsed->as_object())) {
                    auto next = m_continuations.bind(
                        capabilityId, m_config.providerId, scope + *cursor);
                    if (!next) return complete(std::unexpected(std::move(next.error())));
                    result->next = std::move(*next);
                }
                complete(std::move(result));
            }, options);
    }

private:

    [[nodiscard]] foundation::result::Result<async::Request> followingFeed(
        model::FeedQuery query, async::FeedCompletion completion,
        model::ReadOptions options)
    {
        if (!query.fid || query.fid->value == 0U
            || query.limit == 0U || query.limit > 30U)
            return std::unexpected(Error::make(ErrorCode::InvalidArgument,
                "Raw Following requires viewer FID and page size 1..30."));
        const std::string scope = "raw-follow-v1:" + m_config.baseUrl + "\n"
            + std::to_string(query.fid->value) + "\n";
        auto work = std::make_shared<FollowingWork>(
            [this, options](std::string path, BodyCompletion onRead) {
                return request(std::move(path), std::move(onRead), options);
            }, m_continuations, m_config.providerId, scope, *query.fid,
            query.limit, std::move(completion));
        if (query.continuation) {
            auto record = m_continuations.resolve(*query.continuation);
            if (!record) return std::unexpected(std::move(record.error()));
            if (record->providerId.value != m_config.providerId.value
                || record->capabilityId.value !=
                    capability::catalog::feedFollowingRead().value
                || !record->providerCursor.starts_with(scope))
                return std::unexpected(Error::make(ErrorCode::Conflict,
                    "Following cursor has another provider, source, or viewer."));
            const auto serialized = std::string_view{record->providerCursor}
                .substr(scope.size());
            auto decoded = parse(serialized);
            if (!decoded) return std::unexpected(Error::make(ErrorCode::Conflict,
                "Following cursor metadata is invalid."));
            const auto& meta = decoded->as_object();
            const auto cursorTime = unsignedValue(field(meta, "ms"));
            const auto id = string(field(meta, "id"));
            const auto pageLimit = unsignedValue(field(meta, "limit"));
            const auto* ids = array(field(meta, "authors"));
            if (!cursorTime || *cursorTime > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max())
                || !id || !validHash(*id) || !pageLimit
                || *pageLimit != query.limit || !ids || ids->empty()
                || ids->size() > FollowingWork::maximumFollowees + 1U)
                return std::unexpected(Error::make(ErrorCode::Conflict,
                    "Following cursor lacks a valid bounded author snapshot."));
            work->before = std::pair{static_cast<std::int64_t>(*cursorTime), *id};
            for (const auto& author : *ids) {
                const auto fid = unsignedValue(&author);
                if (!fid || *fid == 0U)
                    return std::unexpected(Error::make(ErrorCode::Conflict,
                        "Following cursor has invalid author FID."));
                work->authors.push_back(*fid);
            }
            if (!std::is_sorted(work->authors.begin(), work->authors.end())
                || std::adjacent_find(work->authors.begin(), work->authors.end())
                    != work->authors.end()
                || !std::binary_search(work->authors.begin(),
                                       work->authors.end(), query.fid->value))
                return std::unexpected(Error::make(ErrorCode::Conflict,
                    "Following cursor author snapshot failed validation."));
        }
        auto started = work->begin();
        if (!started) return std::unexpected(std::move(started.error()));
        return async::Request{[work] { work->cancel(); }};
    }

    [[nodiscard]] foundation::result::Result<async::Request> request(
        std::string path,
        BodyCompletion complete,
        const model::ReadOptions options)
    {
        transport::async_http::Request outbound{
            .method = transport::async_http::Method::Get,
            .url = m_config.baseUrl + std::move(path),
            .headers = {"Accept: application/json"},
            .timeout = m_config.timeout,
            .maxBodyBytes = m_config.maximumResponseBytes,
            .priority = options.intent == model::ReadIntent::Prefetch
                ? transport::async_http::Priority::Prefetch
                : transport::async_http::Priority::Visible,
            .retryPolicy = transport::async_http::RetryPolicy::TransientRead,
            .maximumAttempts = 2U,
        };
        if (!m_config.authorizationHeaderName.empty()) {
            outbound.headers.push_back(
                m_config.authorizationHeaderName + ": " + m_config.authorizationHeaderValue);
        }
        auto inflight = std::make_shared<InFlight>();
        auto started = m_transport.start(std::move(outbound),
            [inflight, &decoder = m_decoder,
             complete = std::move(complete)](
                foundation::result::Result<transport::http::HttpResponse> response) mutable {
                if (inflight->cancelled.load(std::memory_order_acquire)) return;
                if (!response) {
                    complete(std::unexpected(std::move(response.error())));
                    return;
                }
                if (response->statusCode != 200U) {
                    const auto status = response->statusCode;
                    const ErrorCode code = status == 404U ? ErrorCode::NotFound
                        : status == 401U || status == 403U ? ErrorCode::Unauthorized
                        : status == 429U ? ErrorCode::RateLimited
                        : status >= 500U ? ErrorCode::Unavailable
                        : ErrorCode::ProtocolFailure;
                    complete(std::unexpected(Error::make(code,
                        "Raw-node HTTP read returned status " + std::to_string(status))));
                    return;
                }
                const auto bytes = response->body.size();
                auto body = std::make_shared<std::string>(std::move(response->body));
                auto completion = std::make_shared<BodyCompletion>(std::move(complete));
                auto posted = decoder.post(bytes,
                    [inflight, body = std::move(body), completion]() mutable {
                        if (!inflight->cancelled.load(std::memory_order_acquire))
                            (*completion)(std::move(*body));
                    });
                if (!posted && !inflight->cancelled.load(std::memory_order_acquire))
                    (*completion)(std::unexpected(std::move(posted.error())));
            });
        if (!started) return std::unexpected(std::move(started.error()));
        return async::Request{[inflight, operation = std::move(*started)]() mutable {
            inflight->cancelled.store(true, std::memory_order_release);
            operation.cancel();
        }};
    }

    transport::async_http::IAsyncHttpTransport& m_transport;
    execution::weighted_thread_pool::WeightedThreadPoolExecutor& m_decoder;
    storage::continuation::ContinuationStore& m_continuations;
    Config m_config{};
};

} // namespace

std::vector<capability::model::CapabilityDescriptor> capabilities()
{
    return {
        supported(capability::catalog::identityFidResolve()),
        supported(capability::catalog::profileRead()),
        supported(capability::catalog::feedUserPostsRead()),
        supported(capability::catalog::feedUserQuotesRead()),
        supported(capability::catalog::feedUserRepliesRead()),
        supported(capability::catalog::graphFollowersRead()),
        supported(capability::catalog::graphFollowingRead()),
        boundedDerived(capability::catalog::feedFollowingRead()),
    };
}

foundation::result::Result<domain::profile::Profile> decodeUserData(
    const std::string_view jsonBody, const domain::identity::Fid expectedFid)
{
    if (expectedFid.value == 0U)
        return std::unexpected(protocolError("Profile FID must be positive."));
    auto parsed = parse(jsonBody);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    const auto* messages = array(field(parsed->as_object(), "messages"));
    if (!messages)
        return std::unexpected(protocolError("UserData response has no messages array."));
    domain::profile::Profile profile{.fid = expectedFid};
    for (const auto& item : *messages) {
        const auto* message = object(&item);
        if (!message) return std::unexpected(protocolError("Malformed UserData message."));
        const auto* data = object(field(*message, "data"));
        if (!data) return std::unexpected(protocolError("UserData message missing data."));
        const auto fid = unsignedValue(field(*data, "fid"));
        if (!fid || *fid != expectedFid.value)
            return std::unexpected(protocolError("UserData response contains a different FID."));
        const auto* body = object(field(*data, "userDataBody"));
        if (!body) continue;
        const auto type = userDataType(field(*body, "type"));
        const auto value = string(field(*body, "value"));
        if (!type || !value) continue;
        switch (*type) {
        case 1: profile.avatarUrl = *value; break;
        case 2: profile.displayName = domain::identity::DisplayName{*value}; break;
        case 3: profile.bio = *value; break;
        case 5: profile.profileUrl = *value; break;
        case 6: profile.username = domain::identity::Username{*value}; break;
        case 7: profile.location = *value; break;
        case 10: profile.bannerUrl = *value; break;
        default: break;
        }
    }
    return profile;
}

foundation::result::Result<capability::feed::FeedPage> decodeUserCasts(
    const std::string_view jsonBody, const domain::identity::Fid expectedFid,
    const model::FeedQuery::Kind kind)
{
    if (kind != model::FeedQuery::Kind::UserPosts
        && kind != model::FeedQuery::Kind::UserQuotes
        && kind != model::FeedQuery::Kind::UserReplies)
        return std::unexpected(unsupported("aggregated feeds"));
    auto parsed = parse(jsonBody);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    const auto* messages = array(field(parsed->as_object(), "messages"));
    if (!messages)
        return std::unexpected(protocolError("Casts response has no messages array."));
    capability::feed::FeedPage page{};
    for (const auto& item : *messages) {
        const auto* message = object(&item);
        if (!message) return std::unexpected(protocolError("Malformed CastAdd message."));
        const auto* data = object(field(*message, "data"));
        if (!data) return std::unexpected(protocolError("Cast message missing data."));
        // /v1/castsByFid returns effective CastAdd messages, not tombstones,
        // but don't interpret other message kinds as casts if an endpoint
        // includes historical entries.
        if (!object(field(*data, "castAddBody"))) continue;
        auto cast = decodeCast(*message, expectedFid);
        if (!cast) return std::unexpected(std::move(cast.error()));
        if (!matchesFeed(*cast, kind)) continue;
        page.entries.push_back(domain::feed::FeedEntry{
            .castId = cast->id,
            .sortTime = cast->createdAt,
        });
        page.casts.push_back(std::move(*cast));
    }
    return page;
}


foundation::result::Result<std::optional<domain::identity::Fid>> decodeSignerAppFid(
    const std::string_view jsonBody,
    const domain::identity::Fid author,
    const std::string_view signerKeyHex,
    const std::uint64_t currentUnixSeconds)
{
    // The selected Snapchain node has already validated the signed custody
    // request bound to requestFid. We independently check this read response
    // for accidental/corrupt mismatches; we do not claim locally verified EIP-712.
    if (author.value == 0U || signerKeyHex.size() != 66U
        || !signerKeyHex.starts_with("0x"))
        return std::unexpected(protocolError("Signer lookup needs a canonical key and author."));
    for (std::size_t i = 2; i < signerKeyHex.size(); ++i)
        if (!std::isxdigit(static_cast<unsigned char>(signerKeyHex[i])))
            return std::unexpected(protocolError("Signer lookup key is malformed."));
    auto parsed = parse(jsonBody);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    const auto* signer = object(field(parsed->as_object(), "signer"));
    if (!signer) return std::optional<domain::identity::Fid>{};
    const auto key = string(field(*signer, "key"));
    const auto fid = unsignedValue(field(*signer, "fid"));
    const auto type = unsignedValue(field(*signer, "keyType"));
    if (!key || !fid || !type
        || *key != signerKeyHex || *fid != author.value || *type != 1U)
        return std::unexpected(protocolError("Unified signer record mismatches signed cast."));
    if (string(field(*signer, "source")) != "SIGNER_SOURCE_OFFCHAIN")
        return std::optional<domain::identity::Fid>{};
    const auto requester = unsignedValue(field(*signer, "requestFid"));
    const auto ttl = unsignedValue(field(*signer, "ttl"));
    const auto used = unsignedValue(field(*signer, "lastUsedAt"));
    const auto expiration = unsignedValue(field(*signer, "expiresAt"));
    const auto* scopes = array(field(*signer, "scopes"));
    if (!requester || *requester == 0U
        || !ttl || *ttl == 0U || *ttl > 90ULL * 24U * 60U * 60U
        || !used || !expiration || !scopes || scopes->empty())
        return std::optional<domain::identity::Fid>{};
    if (*used > std::numeric_limits<std::uint64_t>::max() - *ttl
        || *expiration != *used + *ttl || *expiration <= currentUnixSeconds)
        return std::optional<domain::identity::Fid>{};
    bool castsAllowed = false;
    for (const auto& scope : *scopes)
        if (const auto value = string(&scope); value && *value == "MESSAGE_TYPE_CAST_ADD")
            castsAllowed = true;
    if (!castsAllowed) return std::optional<domain::identity::Fid>{};
    return domain::identity::Fid{*requester};
}

foundation::result::Result<capability::social::SocialGraphPage> decodeFollowLinks(
    const std::string_view jsonBody, const domain::identity::Fid expectedFid,
    const model::SocialQuery::Kind kind)
{
    if (expectedFid.value == 0U)
        return std::unexpected(protocolError("Follow query needs a positive FID."));
    auto parsed = parse(jsonBody);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    const auto* messages = array(field(parsed->as_object(), "messages"));
    if (!messages)
        return std::unexpected(protocolError("Follow query has no messages array."));
    capability::social::SocialGraphPage page{};
    std::unordered_set<std::uint64_t> seen{};
    for (const auto& item : *messages) {
        const auto* message = object(&item);
        if (!message)
            return std::unexpected(protocolError("Invalid link entry."));
        const auto* data = object(field(*message, "data"));
        if (!data)
            return std::unexpected(protocolError("Link entry missing data."));
        const auto* link = object(field(*data, "linkBody"));
        if (!link) continue; // Not an effective LinkAdd.
        const auto type = string(field(*link, "type"));
        if (!type || *type != "follow") continue;
        const auto from = unsignedValue(field(*data, "fid"));
        const auto to = unsignedValue(field(*link, "targetFid"));
        if (!from || !to || *from == 0U || *to == 0U)
            return std::unexpected(protocolError("Link message missing a canonical FID."));
        if ((kind == model::SocialQuery::Kind::Following && *from != expectedFid.value)
            || (kind == model::SocialQuery::Kind::Followers && *to != expectedFid.value))
            return std::unexpected(protocolError("Link endpoint returned a mismatched FID."));
        const auto other = kind == model::SocialQuery::Kind::Following ? *to : *from;
        if (!seen.insert(other).second) continue;
        page.relations.push_back(domain::social::SocialRelation{
            .source = domain::identity::Fid{*from},
            .target = domain::identity::Fid{*to},
            .kind = domain::social::SocialRelationKind::Follows,
        });
        // The raw node does not return profile summaries with link pages.
        // Present only canonical FIDs; no usernames/avatars are invented.
        page.profiles.push_back(domain::profile::Profile{
            .fid = domain::identity::Fid{other},
        });
    }
    return page;
}

std::unique_ptr<async::IAsyncReadGateway> makeGateway(
    transport::async_http::IAsyncHttpTransport& transport,
    execution::weighted_thread_pool::WeightedThreadPoolExecutor& decodeExecutor,
    storage::continuation::ContinuationStore& continuations,
    Config config)
{
    return std::make_unique<Gateway>(transport, decodeExecutor, continuations, std::move(config));
}

} // namespace farcaster::provider::snapchain::read
