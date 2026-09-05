// Implementation unit for farcaster.presentation.media.
//
// Rules here are the *only* URL/extension classification tables in the
// presentation layer. Feed rows, EmbedPreview dispatch, semantic grouping and
// composer media blocks all converge on these functions.
module;

#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

module farcaster.presentation.media;

namespace farcaster::presentation::media {
namespace {

[[nodiscard]] std::string lowerAscii(const std::string_view input)
{
    std::string lowered;
    lowered.reserve(input.size());
    for (const char character : input) {
        lowered.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))));
    }
    return lowered;
}

[[nodiscard]] bool endsWith(const std::string_view value, const std::string_view suffix)
{
    return value.size() >= suffix.size()
        && value.substr(value.size() - suffix.size()) == suffix;
}

[[nodiscard]] std::string_view normalizedPath(const std::string_view rawUrl)
{
    // Keep classification deterministic: drop query/fragment and consider only
    // the URL path. Extensionless signed URLs simply have no extension.
    const std::size_t query = rawUrl.find_first_of("?#");
    return query == std::string_view::npos ? rawUrl : rawUrl.substr(0, query);
}

[[nodiscard]] std::optional<std::string_view> extensionOf(const std::string_view rawUrl)
{
    const std::string_view path = normalizedPath(rawUrl);
    const std::size_t slash = path.find_last_of('/');
    const std::string_view leaf = slash == std::string_view::npos
        ? path : path.substr(slash + 1);
    const std::size_t dot = leaf.find_last_of('.');
    if (dot == std::string_view::npos || dot + 1 >= leaf.size()) {
        return std::nullopt;
    }
    return leaf.substr(dot + 1);
}

[[nodiscard]] bool matchesExtension(
    const std::string_view rawUrl,
    const std::array<std::string_view, 8>& extensions)
{
    const auto candidate = extensionOf(rawUrl);
    if (!candidate) {
        return false;
    }
    const std::string lowered = lowerAscii(*candidate);
    for (const std::string_view extension : extensions) {
        if (lowered == extension) {
            return true;
        }
    }
    return false;
}

constexpr std::array<std::string_view, 8> videoExtensions{
    "mp4", "webm", "mov", "m4v", "m3u8", "m4s", "ogv", "mkv"};
constexpr std::array<std::string_view, 8> audioExtensions{
    "mp3", "m4a", "aac", "wav", "wave", "ogg", "oga", "opus"};
constexpr std::array<std::string_view, 8> imageExtensions{
    "jpg", "jpeg", "png", "webp", "gif", "avif", "heic", "heif"};
constexpr std::array<std::string_view, 8> fileExtensions{
    "pdf", "zip", "rar", "7z", "tar", "gz", "bz2", "xz"};

[[nodiscard]] bool isVideoExtension(const std::string_view rawUrl)
{
    return matchesExtension(rawUrl, videoExtensions);
}

[[nodiscard]] bool isAudioExtension(const std::string_view rawUrl)
{
    return matchesExtension(rawUrl, audioExtensions);
}

[[nodiscard]] bool isImageExtension(const std::string_view rawUrl)
{
    return matchesExtension(rawUrl, imageExtensions)
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".svg");
}

[[nodiscard]] bool isFileExtension(const std::string_view rawUrl)
{
    return matchesExtension(rawUrl, fileExtensions)
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".doc")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".docx")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".xls")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".xlsx")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".ppt")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".pptx")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".txt")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".md")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".csv")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".json")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".xml")
        || endsWith(lowerAscii(normalizedPath(rawUrl)), ".epub");
}

struct UrlParts final {
    std::string scheme{};
    std::string host{};
    std::string path{};
    std::string query{};
};

[[nodiscard]] std::optional<UrlParts> parseHttpUrl(const std::string_view rawUrl)
{
    const std::size_t schemeEnd = rawUrl.find("://");
    if (schemeEnd == std::string_view::npos) {
        return std::nullopt;
    }
    UrlParts parts{};
    parts.scheme = lowerAscii(rawUrl.substr(0, schemeEnd));
    if (parts.scheme != "http" && parts.scheme != "https") {
        return std::nullopt;
    }

    const std::size_t authorityStart = schemeEnd + 3U;
    const std::size_t authorityEnd = rawUrl.find_first_of("/?#", authorityStart);
    std::string_view authority = authorityEnd == std::string_view::npos
        ? rawUrl.substr(authorityStart)
        : rawUrl.substr(authorityStart, authorityEnd - authorityStart);
    const std::size_t at = authority.rfind('@');
    if (at != std::string_view::npos) {
        authority.remove_prefix(at + 1U);
    }
    const std::size_t port = authority.rfind(':');
    if (port != std::string_view::npos && authority.find(']') == std::string_view::npos) {
        authority = authority.substr(0, port);
    }
    parts.host = lowerAscii(authority);
    if (parts.host.empty()) {
        return std::nullopt;
    }

    if (authorityEnd == std::string_view::npos) {
        parts.path = "/";
        return parts;
    }
    const std::size_t queryStart = rawUrl.find_first_of("?#", authorityEnd);
    const std::size_t pathEnd = queryStart == std::string_view::npos ? rawUrl.size() : queryStart;
    parts.path = std::string{rawUrl.substr(authorityEnd, pathEnd - authorityEnd)};
    if (parts.path.empty()) {
        parts.path = "/";
    }
    if (queryStart != std::string_view::npos && rawUrl[queryStart] == '?') {
        const std::size_t fragment = rawUrl.find('#', queryStart);
        parts.query = std::string{rawUrl.substr(
            queryStart, fragment == std::string_view::npos
                ? std::string_view::npos : fragment - queryStart)};
    }
    return parts;
}

[[nodiscard]] bool isCloudflareStreamHost(const std::string_view host)
{
    return host == "videodelivery.net"
        || endsWith(host, ".videodelivery.net")
        || endsWith(host, ".cloudflarestream.com");
}

[[nodiscard]] bool isHlsUrl(const std::string_view rawUrl)
{
    return endsWith(lowerAscii(normalizedPath(rawUrl)), ".m3u8");
}

[[nodiscard]] std::string firstPathSegment(const std::string_view path)
{
    std::size_t begin = 0U;
    while (begin < path.size() && path[begin] == '/') {
        ++begin;
    }
    if (begin >= path.size()) {
        return {};
    }
    const std::size_t end = path.find('/', begin);
    return std::string{path.substr(
        begin, end == std::string_view::npos ? std::string_view::npos : end - begin)};
}

[[nodiscard]] std::string cloudflareManifestUrl(const UrlParts& parts)
{
    const std::string assetId = firstPathSegment(parts.path);
    if (assetId.empty()) {
        return {};
    }
    std::string host = parts.host;
    if (host == "iframe.videodelivery.net") {
        host = "videodelivery.net";
    }
    return parts.scheme + "://" + host + "/" + assetId
        + "/manifest/video.m3u8" + parts.query;
}

/// Hosts whose opaque URLs are reliably direct video media (no page
/// extraction). Media that requires scraping is intentionally never listed.
[[nodiscard]] bool isTrustedVideoHost(const std::string_view rawUrl)
{
    const std::string lowered = lowerAscii(rawUrl);
    const std::string_view schemeSeparator = "://";
    const std::size_t schemeEnd = lowered.find(schemeSeparator);
    if (schemeEnd == std::string::npos) {
        return false;
    }
    const std::size_t hostStart = schemeEnd + schemeSeparator.size();
    const std::size_t hostEnd = lowered.find_first_of("/?#", hostStart);
    const std::string host = lowered.substr(
        hostStart, hostEnd == std::string::npos ? std::string::npos : hostEnd - hostStart);

    if (host == "video.twimg.com" || host == "videodelivery.net"
        || host == "stream.warpcast.com" || host == "stream.farcaster.xyz") {
        return true;
    }
    return endsWith(host, ".videodelivery.net")
        || endsWith(host, ".cloudflarestream.com")
        || endsWith(host, ".fardeliver.com");
}

/// Hosts whose opaque URLs are reliably direct image media.
[[nodiscard]] bool isTrustedImageHost(const std::string_view rawUrl)
{
    const std::string lowered = lowerAscii(rawUrl);
    const std::string_view schemeSeparator = "://";
    const std::size_t schemeEnd = lowered.find(schemeSeparator);
    if (schemeEnd == std::string::npos) {
        return false;
    }
    const std::size_t hostStart = schemeEnd + schemeSeparator.size();
    const std::size_t hostEnd = lowered.find_first_of("/?#", hostStart);
    const std::string host = lowered.substr(
        hostStart, hostEnd == std::string::npos ? std::string::npos : hostEnd - hostStart);

    if (host == "i.imgur.com" || host == "media.tenor.com"
        || host == "media.giphy.com" || host == "pbs.twimg.com"
        || host == "cdn.farcaster.xyz" || host == "i.seadn.io"
        || host == "lh3.googleusercontent.com" || host == "imagedelivery.net") {
        return true;
    }
    return endsWith(host, ".imagedelivery.net")
        || endsWith(host, ".imagekit.io")
        || endsWith(host, ".cloudfront.net");
}

[[nodiscard]] bool isWebHostedMediaPage(const std::string_view rawUrl)
{
    const std::string lowered = lowerAscii(rawUrl);
    // youtube.com / youtu.be pages are enriched link previews, never raw
    // streams, unless a direct media extension is present (handled upstream).
    return lowered.find("youtube.com/") != std::string::npos
        || lowered.find("youtu.be/") != std::string::npos
        || lowered.find("vimeo.com/") != std::string::npos
        || lowered.find("dailymotion.com/") != std::string::npos;
}

[[nodiscard]] bool authoritativeKind(
    const std::string_view hint,
    const MediaKind desired)
{
    if (hint.empty()) {
        return false;
    }
    const std::string lowered = lowerAscii(hint);
    switch (desired) {
    case MediaKind::Image:
        return lowered == "image";
    case MediaKind::Video:
        return lowered == "video" || lowered == "media-video";
    case MediaKind::Audio:
        return lowered == "audio" || lowered == "media-audio";
    case MediaKind::File:
        return lowered == "file" || lowered == "document";
    default:
        return false;
    }
}

[[nodiscard]] bool mimeFamily(
    const std::string_view mime,
    const MediaKind desired)
{
    const std::string lowered = lowerAscii(mime);
    const std::string_view prefix = [&desired]() -> std::string_view {
        switch (desired) {
        case MediaKind::Image:
            return "image/";
        case MediaKind::Video:
            return "video/";
        case MediaKind::Audio:
            return "audio/";
        default:
            return "";
        }
    }();
    if (prefix.empty()) {
        return false;
    }
    if (lowered.starts_with(prefix)) {
        return true;
    }
    if (desired == MediaKind::Video && lowered == "application/vnd.apple.mpegurl") {
        return true;
    }
    if (desired == MediaKind::Audio && lowered == "application/ogg") {
        return true;
    }
    return false;
}

[[nodiscard]] Classification urlFallback(const std::string_view url)
{
    Classification result{};
    const std::string scheme =
        lowerAscii(url.substr(0, url.find_first_of("://")));
    const bool httpScheme = scheme == "http" || scheme == "https";
    const bool localScheme = scheme == "file" || scheme == "qrc";
    if (url.empty() || (!httpScheme && !localScheme)) {
        return result; // Unknown; unsupported schemes are never auto-opened.
    }
    if (isWebHostedMediaPage(url)) {
        result.kind = MediaKind::WebPage;
        return result;
    }
    if (isVideoExtension(url) || isTrustedVideoHost(url)) {
        result.kind = MediaKind::Video;
        result.visual = true;
        result.directPlayable = true;
        return result;
    }
    if (isAudioExtension(url)) {
        result.kind = MediaKind::Audio;
        result.directPlayable = true;
        return result;
    }
    if (isImageExtension(url) || isTrustedImageHost(url)) {
        result.kind = MediaKind::Image;
        result.visual = true;
        result.directPlayable = true;
        return result;
    }
    if (isFileExtension(url)) {
        result.kind = MediaKind::File;
        return result;
    }
    result.kind = MediaKind::WebPage;
    return result;
}

} // namespace

Classification classify(
    const std::string_view kindHint,
    const std::string_view url,
    const std::string_view mediaType,
    const std::string_view contentType,
    const std::string_view providerKind)
{
    // 1. Explicit canonical embed kind wins when it names a media family.
    for (const MediaKind hintKind : {MediaKind::Image, MediaKind::Video,
                                     MediaKind::Audio, MediaKind::File}) {
        if (authoritativeKind(kindHint, hintKind)) {
            Classification result{};
            result.kind = hintKind;
            result.visual = hintKind == MediaKind::Image || hintKind == MediaKind::Video;
            result.directPlayable =
                hintKind != MediaKind::File && isSupportedPlaybackScheme(url);
            return result;
        }
    }

    // Direct mediaType/contentType (from rich blocks, composer media records
    // or provider metadata) is the next strongest signal. It is required to
    // recognize signed/extensionless direct media without trusting a page.
    const std::string_view contentTypes[] = {mediaType, contentType};
    for (const std::string_view mime : contentTypes) {
        if (mime.empty()) {
            continue;
        }
        for (const MediaKind mimeKind : {MediaKind::Image, MediaKind::Video,
                                         MediaKind::Audio}) {
            if (mimeFamily(mime, mimeKind)) {
                Classification result{};
                result.kind = mimeKind;
                result.visual = mimeKind == MediaKind::Image
                    || mimeKind == MediaKind::Video;
                result.directPlayable = isSupportedPlaybackScheme(url);
                return result;
            }
        }
    }

    // 2. Trusted metadata provider kind (the link resolver only emits these
    //    after a direct body was observed, never for pages).
    const std::string loweredProvider = lowerAscii(providerKind);
    if (loweredProvider == "video") {
        Classification result{};
        result.kind = MediaKind::Video;
        result.visual = true;
        result.directPlayable = isSupportedPlaybackScheme(url);
        return result;
    }
    if (loweredProvider == "image") {
        Classification result{};
        result.kind = MediaKind::Image;
        result.visual = true;
        result.directPlayable = isSupportedPlaybackScheme(url);
        return result;
    }
    if (loweredProvider == "audio") {
        Classification result{};
        result.kind = MediaKind::Audio;
        result.directPlayable = isSupportedPlaybackScheme(url);
        return result;
    }

    // 3. URL/extension fallback.
    return classifyUrl(url);
}

Classification classifyUrl(const std::string_view url)
{
    return urlFallback(url);
}

PlaybackSource playbackSourceForUrl(const std::string_view url)
{
    PlaybackSource result{};
    result.url = std::string{url};
    if (url.empty() || !isSupportedPlaybackScheme(url)) {
        return result;
    }
    if (isHlsUrl(url)) {
        result.adaptiveUrl = result.url;
        result.contentType = "application/vnd.apple.mpegurl";
        return result;
    }

    const auto parts = parseHttpUrl(url);
    if (!parts || !isCloudflareStreamHost(parts->host)) {
        return result;
    }
    result.adaptiveUrl = cloudflareManifestUrl(*parts);
    if (!result.adaptiveUrl.empty()) {
        result.contentType = "application/vnd.apple.mpegurl";
    }
    return result;
}

bool isSupportedPlaybackScheme(const std::string_view url)
{
    const std::size_t separator = url.find("://");
    if (separator == std::string_view::npos) {
        return false;
    }
    const std::string scheme = lowerAscii(url.substr(0, separator));
    return scheme == "http" || scheme == "https" || scheme == "file"
        || scheme == "qrc";
}

std::string_view kindLabel(const MediaKind kind)
{
    switch (kind) {
    case MediaKind::Unknown:
        return "";
    case MediaKind::WebPage:
        return "web";
    case MediaKind::Image:
        return "image";
    case MediaKind::Video:
        return "video";
    case MediaKind::Audio:
        return "audio";
    case MediaKind::File:
        return "file";
    }
    return "";
}

} // namespace farcaster::presentation::media
