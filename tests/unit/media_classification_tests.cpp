// Deterministic unit tests for the canonical embed-media classification.
//
// These cover the full precedence contract (kind hints, mediaType,
// contentType, metadata provider kind, URL/extension fallback) and the
// direct-video vs video-webpage boundary. No Qt Multimedia runtime is needed:
// decoder behavior is a platform integration concern tested separately.
#include <cassert>
#include <string>
#include <string_view>

import farcaster.presentation.media;

namespace {

using farcaster::presentation::media::Classification;
using farcaster::presentation::media::MediaKind;
using farcaster::presentation::media::classify;
using farcaster::presentation::media::classifyUrl;
using farcaster::presentation::media::kindLabel;
using farcaster::presentation::media::isSupportedPlaybackScheme;
using farcaster::presentation::media::playbackSourceForUrl;

void expectKind(const MediaKind expected, const Classification& result, const char* message)
{
    assert(result.kind == expected && message);
}

} // namespace

int main()
{
    // -- Explicit video kind + HTTPS URL -------------------------------------
    expectKind(MediaKind::Video, classify("video", "https://cdn.example.com/clip.mp4", {}, {}, {}),
        "explicit video kind stays video");
    assert(classify("video", "https://cdn.example.com/clip.mp4", {}, {}, {}).visual);
    assert(classify("video", "https://cdn.example.com/clip.mp4", {}, {}, {}).directPlayable);

    // -- MIME classification (no extension required) --------------------------
    expectKind(MediaKind::Video,
        classify("url", "https://signed.example.com/asset/9f2c1", "video/mp4", {}, {}),
        "video/mp4 mediaType classifies extensionless URL as video");
    expectKind(MediaKind::Video,
        classify("url", "https://signed.example.com/asset/9f2c1", "video/webm", {}, {}),
        "video/webm mediaType classifies as video");
    expectKind(MediaKind::Video,
        classify("url", "https://signed.example.com/asset/9f2c1", {}, "video/quicktime", {}),
        "video/quicktime contentType classifies as video");
    expectKind(MediaKind::Video,
        classify("url", "https://signed.example.com/asset/9f2c1", {}, "application/vnd.apple.mpegurl", {}),
        "HLS content type classifies as video");
    expectKind(MediaKind::Video,
        classify("url", "https://signed.example.com/asset/9f2c1", {}, "video/mp4; codecs=avc1", {}),
        "content type with parameters still classifies as video");
    expectKind(MediaKind::Image,
        classify("url", "https://signed.example.com/asset/9f2c1", "image/jpeg", {}, {}),
        "image MIME classifies as image");

    // -- Extension fallback ---------------------------------------------------
    expectKind(MediaKind::Video, classifyUrl("https://cdn.example.com/a.mp4"), "mp4 extension");
    expectKind(MediaKind::Video, classifyUrl("https://cdn.example.com/a.webm"), "webm extension");
    expectKind(MediaKind::Video, classifyUrl("https://cdn.example.com/a.mov"), "mov extension");
    expectKind(MediaKind::Video, classifyUrl("https://cdn.example.com/a.m4v"), "m4v extension");
    expectKind(MediaKind::Video, classifyUrl("https://cdn.example.com/stream/a.m3u8"), "m3u8 extension");
    expectKind(MediaKind::Audio, classifyUrl("https://cdn.example.com/a.mp3"), "audio extension");
    expectKind(MediaKind::File, classifyUrl("https://cdn.example.com/a.pdf"), "file extension");

    // Signed CDN URLs without extensions resolve through metadata only.
    expectKind(MediaKind::WebPage, classifyUrl("https://signed.example.com/asset/9f2c1"),
        "extensionless URL without metadata stays a web page");

    // -- Metadata provider kind (observed direct body) ------------------------
    expectKind(MediaKind::Video,
        classify("url", "https://signed.example.com/asset/9f2c1", {}, {}, "video"),
        "trusted providerKind video classifies as direct video");
    expectKind(MediaKind::Video,
        classify("url", "https://signed.example.com/asset/9f2c1", {}, {}, "Video"),
        "provider kind comparison is case-insensitive");

    // -- Trusted media hosts --------------------------------------------------
    expectKind(MediaKind::Video, classifyUrl("https://videodelivery.net/abc"),
        "videodelivery opaque URL is video");
    expectKind(MediaKind::Video, classifyUrl("https://customer.cloudflarestream.com/abc"),
        "cloudflarestream opaque URL is video");
    expectKind(MediaKind::Video, classifyUrl("https://video.twimg.com/ext_tw_video/abc"),
        "twitter video host is video");
    expectKind(MediaKind::Video, classifyUrl("https://stream.warpcast.com/abc"),
        "warpcast stream host is video");

    // -- Decoder-facing adaptive source hints ---------------------------------
    {
        const auto source = playbackSourceForUrl(
            "https://stream.warpcast.com/v1/video/abc123.m3u8?token=x");
        assert(source.url == "https://stream.warpcast.com/v1/video/abc123.m3u8?token=x");
        assert(source.adaptiveUrl == source.url);
        assert(source.contentType == "application/vnd.apple.mpegurl");
    }
    {
        const auto source = playbackSourceForUrl(
            "https://customer-abc.cloudflarestream.com/0123456789abcdef/watch?token=signed");
        assert(source.url ==
            "https://customer-abc.cloudflarestream.com/0123456789abcdef/watch?token=signed");
        assert(source.adaptiveUrl ==
            "https://customer-abc.cloudflarestream.com/0123456789abcdef/manifest/video.m3u8?token=signed");
        assert(source.contentType == "application/vnd.apple.mpegurl");
    }
    {
        const auto source = playbackSourceForUrl("https://iframe.videodelivery.net/0123456789abcdef");
        assert(source.adaptiveUrl ==
            "https://videodelivery.net/0123456789abcdef/manifest/video.m3u8");
    }
    {
        const auto source = playbackSourceForUrl("https://cdn.example.com/video.mp4");
        assert(source.url == "https://cdn.example.com/video.mp4");
        assert(source.adaptiveUrl.empty());
        assert(source.contentType.empty());
    }

    // -- Direct-video vs video-webpage boundary -------------------------------
    expectKind(MediaKind::WebPage, classifyUrl("https://www.youtube.com/watch?v=abc123"),
        "YouTube watch page is a web page, not a raw stream");
    expectKind(MediaKind::WebPage, classifyUrl("https://youtu.be/abc123"),
        "youtu.be short link is a web page, not a raw stream");
    expectKind(MediaKind::WebPage, classifyUrl("https://vimeo.com/12345"),
        "Vimeo page is a web page, not a raw stream");
    expectKind(MediaKind::WebPage, classifyUrl("https://news.example.com/article/about-video"),
        "generic article page is not direct video");
    expectKind(MediaKind::Video, classifyUrl("https://cdn.example.com/direct.webm"),
        "direct media file URL stays video regardless of page-like host");

    // -- Explicit kind takes precedence over URL shape ------------------------
    expectKind(MediaKind::Image,
        classify("image", "https://cdn.example.com/photo.mp4", {}, {}, {}),
        "explicit image kind wins over a confusing extension");
    expectKind(MediaKind::Video,
        classify("video", "https://example.com/article", {}, {}, {}),
        "explicit video kind wins over a page-like URL");

    // -- Locally playable sources ---------------------------------------------
    expectKind(MediaKind::Video, classifyUrl("file:///Users/me/movie.mp4"), "local video file");
    expectKind(MediaKind::Video, classifyUrl("qrc:/bundled/movie.mp4"), "bundled video resource");
    assert(isSupportedPlaybackScheme("https://cdn.example.com/a.mp4"));
    assert(isSupportedPlaybackScheme("file:///a.mp4"));
    assert(!isSupportedPlaybackScheme("javascript:alert(1)"));
    assert(!isSupportedPlaybackScheme("custom-media://x"));
    assert(!classifyUrl("javascript:alert(1)").directPlayable);

    // -- Provider neutrality / no scraping assumptions -------------------------
    assert(!classifyUrl("https://www.youtube.com/watch?v=abc123").directPlayable);
    expectKind(MediaKind::Unknown, classifyUrl(""), "empty source stays unknown");

    assert(kindLabel(MediaKind::Video) == "video");
    assert(kindLabel(MediaKind::Image) == "image");
    assert(kindLabel(MediaKind::WebPage) == "web");
    return 0;
}
