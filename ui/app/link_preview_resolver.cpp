#include "link_preview_resolver.hpp"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QQueue>
#include <QRegularExpression>
#include <QRunnable>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace {

constexpr qsizetype maximumHtmlBytes = 256 * 1024;
constexpr int requestTimeoutMilliseconds = 6'000;
constexpr int absoluteRequestDeadlineMilliseconds = 9'000;
constexpr int maximumRedirects = 4;
constexpr qsizetype maximumConcurrentPreviewRequests = 2;
constexpr qsizetype maximumPendingPreviewRequests = 64;
constexpr qsizetype maximumRetainedPreviewEntries = 320;
constexpr qint64 previewFreshnessMilliseconds = 15 * 60 * 1000;
constexpr qsizetype maximumJsonBytes = 128 * 1024;

[[nodiscard]] QString normalizedUrl(const QString& rawUrl)
{
    QUrl url = QUrl::fromUserInput(rawUrl.trimmed());
    if (!url.isValid()) {
        return {};
    }
    url.setFragment({});
    return url.toString(QUrl::FullyEncoded);
}

[[nodiscard]] bool isPrivateIpv4(const quint32 address) noexcept
{
    return (address & 0xFF000000U) == 0x0A000000U       // 10/8
        || (address & 0xFF000000U) == 0x7F000000U      // 127/8
        || (address & 0xFFFF0000U) == 0xA9FE0000U      // 169.254/16
        || (address & 0xFFF00000U) == 0xAC100000U      // 172.16/12
        || (address & 0xFFFF0000U) == 0xC0A80000U      // 192.168/16
        || (address & 0xFFC00000U) == 0x64400000U      // 100.64/10
        || (address & 0xFF000000U) == 0x00000000U      // 0/8
        || (address & 0xF0000000U) == 0xE0000000U;     // multicast/reserved
}

[[nodiscard]] bool publicHttpUrl(const QUrl& url)
{
    if (!url.isValid() || url.host().trimmed().isEmpty()
        || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        return false;
    }

    const QString host = url.host().trimmed().toLower();
    if (host == QStringLiteral("localhost") || host.endsWith(QStringLiteral(".localhost"))
        || host.endsWith(QStringLiteral(".local")) || host.endsWith(QStringLiteral(".internal"))) {
        return false;
    }

    const QHostAddress address{host};
    if (address.isNull()) {
        return true;
    }
    if (address.isLoopback() || address.isLinkLocal()) {
        return false;
    }
    bool ipv4 = false;
    const quint32 value = address.toIPv4Address(&ipv4);
    if (ipv4) {
        return !isPrivateIpv4(value);
    }
    const Q_IPV6ADDR ipv6 = address.toIPv6Address();
    if ((ipv6[0] & 0xFEU) == 0xFCU) { // fc00::/7 unique-local
        return false;
    }
    return true;
}

[[nodiscard]] QString displayHost(const QUrl& url)
{
    QString host = url.host().toLower();
    if (host.startsWith(QStringLiteral("www."))) {
        host.remove(0, 4);
    }
    return host;
}

[[nodiscard]] QString remoteImageSource(const QUrl& url)
{
    if (!publicHttpUrl(url)) {
        return {};
    }
    const QByteArray encoded = url.toString(QUrl::FullyEncoded).toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("image://remote/") + QString::fromLatin1(encoded);
}

[[nodiscard]] QString youtubeVideoId(const QUrl& url)
{
    QString host = displayHost(url);
    if (host == QStringLiteral("youtu.be")) {
        return url.path().section(QLatin1Char('/'), 1, 1).trimmed();
    }
    if (host == QStringLiteral("youtube.com") || host == QStringLiteral("m.youtube.com")
        || host == QStringLiteral("music.youtube.com")) {
        const QUrlQuery query{url};
        const QString id = query.queryItemValue(QStringLiteral("v")).trimmed();
        if (!id.isEmpty()) {
            return id;
        }
        const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (segments.size() >= 2 && (segments[0] == QStringLiteral("shorts")
            || segments[0] == QStringLiteral("embed") || segments[0] == QStringLiteral("live"))) {
            return segments[1];
        }
    }
    return {};
}

[[nodiscard]] QString prettifySlug(QString value)
{
    value = QUrl::fromPercentEncoding(value.toUtf8());
    value.replace(QLatin1Char('-'), QLatin1Char(' '));
    value.replace(QLatin1Char('_'), QLatin1Char(' '));
    return value.trimmed();
}

[[nodiscard]] QString decodeHtmlEntities(QString value)
{
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&#39;"), QStringLiteral("'"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&apos;"), QStringLiteral("'"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&lt;"), QStringLiteral("<"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&gt;"), QStringLiteral(">"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "), Qt::CaseInsensitive);

    // OpenGraph fields frequently encode punctuation as numeric HTML entities
    // (for example &#x27;). Decode both hexadecimal and decimal forms so card
    // titles never expose transport markup to the user.
    const QRegularExpression numericEntity{
        QStringLiteral(R"(&#(?:x([0-9A-Fa-f]+)|([0-9]+));)"),
        QRegularExpression::CaseInsensitiveOption};
    qsizetype offset = 0;
    while (offset < value.size()) {
        const QRegularExpressionMatch match = numericEntity.match(value, offset);
        if (!match.hasMatch()) {
            break;
        }

        const bool hexadecimal = !match.captured(1).isEmpty();
        bool valid = false;
        const uint scalar = (hexadecimal ? match.captured(1) : match.captured(2))
                                .toUInt(&valid, hexadecimal ? 16 : 10);
        if (!valid || scalar > 0x10FFFFU || (scalar >= 0xD800U && scalar <= 0xDFFFU)) {
            offset = match.capturedEnd();
            continue;
        }

        const char32_t codePoint = static_cast<char32_t>(scalar);
        const QString replacement = QString::fromUcs4(&codePoint, 1);
        value.replace(match.capturedStart(), match.capturedLength(), replacement);
        offset = match.capturedStart() + replacement.size();
    }

    return value.simplified();
}

[[nodiscard]] QString attributeValue(const QString& tag, const QString& name)
{
    const QRegularExpression expression{
        QStringLiteral(R"((?:^|\s)%1\s*=\s*([\"'])(.*?)\1)").arg(QRegularExpression::escape(name)),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption};
    const auto match = expression.match(tag);
    return match.hasMatch() ? decodeHtmlEntities(match.captured(2)) : QString{};
}

[[nodiscard]] QUrl resolvedAssetUrl(const QUrl& pageUrl, const QString& candidate)
{
    if (candidate.trimmed().isEmpty()) {
        return {};
    }
    const QUrl asset = pageUrl.resolved(QUrl{candidate.trimmed()});
    return publicHttpUrl(asset) ? asset : QUrl{};
}

[[nodiscard]] QVariantMap seedPreview(const QUrl& url)
{
    const QString host = displayHost(url);
    QVariantMap result{
        {QStringLiteral("domain"), host},
        {QStringLiteral("siteName"), host},
        {QStringLiteral("providerKind"), QStringLiteral("web")},
        {QStringLiteral("resolved"), false},
    };
    const QStringList pathSegments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!pathSegments.isEmpty()) {
        const QString fallbackTitle = prettifySlug(pathSegments.constLast());
        if (!fallbackTitle.isEmpty()) {
            result.insert(QStringLiteral("title"), fallbackTitle);
        }
    }

    if (const QString videoId = youtubeVideoId(url); !videoId.isEmpty()) {
        result.insert(QStringLiteral("siteName"), QStringLiteral("YouTube"));
        result.insert(QStringLiteral("providerKind"), QStringLiteral("youtube"));
        if (result.value(QStringLiteral("title")).toString().trimmed().isEmpty()
            || result.value(QStringLiteral("title")).toString().compare(QStringLiteral("watch"), Qt::CaseInsensitive) == 0) {
            result.insert(QStringLiteral("title"), QStringLiteral("YouTube video"));
        }
        result.insert(QStringLiteral("previewImage"), remoteImageSource(
            QUrl{QStringLiteral("https://i.ytimg.com/vi/%1/hqdefault.jpg").arg(videoId)}));
        return result;
    }

    if (host == QStringLiteral("github.com")) {
        result.insert(QStringLiteral("siteName"), QStringLiteral("GitHub"));
        result.insert(QStringLiteral("providerKind"), QStringLiteral("github"));
        const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (segments.size() >= 2) {
            const QString repository = segments[0] + QLatin1Char('/') + segments[1];
            result.insert(QStringLiteral("title"), repository);
            result.insert(QStringLiteral("previewImage"), remoteImageSource(
                QUrl{QStringLiteral("https://opengraph.githubassets.com/1/%1").arg(repository)}));
        }
        return result;
    }

    if (host == QStringLiteral("music.apple.com")) {
        result.insert(QStringLiteral("siteName"), QStringLiteral("Apple Music"));
        result.insert(QStringLiteral("providerKind"), QStringLiteral("apple-music"));
        const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (segments.size() >= 3) {
            result.insert(QStringLiteral("title"), prettifySlug(segments.at(segments.size() - 2)));
        }
        return result;
    }

    if (host == QStringLiteral("opensea.io") || host == QStringLiteral("zora.co")
        || host == QStringLiteral("foundation.app") || host == QStringLiteral("manifold.xyz")
        || host == QStringLiteral("rarible.com") || host == QStringLiteral("magiceden.io")
        || host == QStringLiteral("highlight.xyz") || host == QStringLiteral("mint.fun")
        || host == QStringLiteral("objkt.com") || host == QStringLiteral("transient.xyz")) {
        const QString siteName = host == QStringLiteral("opensea.io") ? QStringLiteral("OpenSea")
            : host == QStringLiteral("zora.co") ? QStringLiteral("Zora")
            : host == QStringLiteral("foundation.app") ? QStringLiteral("Foundation")
            : host == QStringLiteral("manifold.xyz") ? QStringLiteral("Manifold")
            : host == QStringLiteral("rarible.com") ? QStringLiteral("Rarible")
            : host == QStringLiteral("magiceden.io") ? QStringLiteral("Magic Eden")
            : host == QStringLiteral("highlight.xyz") ? QStringLiteral("Highlight")
            : host == QStringLiteral("mint.fun") ? QStringLiteral("mint.fun")
            : host == QStringLiteral("transient.xyz") ? QStringLiteral("Transient")
            : QStringLiteral("objkt");
        result.insert(QStringLiteral("siteName"), siteName);
        result.insert(QStringLiteral("providerKind"), host.section(QLatin1Char('.'), 0, 0));
        result.insert(QStringLiteral("assetKind"), QStringLiteral("Collectible"));
        return result;
    }

    if (host == QStringLiteral("superrare.com")) {
        result.insert(QStringLiteral("siteName"), QStringLiteral("SuperRare"));
        result.insert(QStringLiteral("providerKind"), QStringLiteral("superrare"));
        const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (!segments.isEmpty() && segments.front().compare(QStringLiteral("artwork"), Qt::CaseInsensitive) == 0) {
            result.insert(QStringLiteral("assetKind"), QStringLiteral("Artwork"));
            qsizetype contractIndex = 1;
            if (segments.size() > 1 && (segments[1].compare(QStringLiteral("eth"), Qt::CaseInsensitive) == 0
                || segments[1].compare(QStringLiteral("ethereum"), Qt::CaseInsensitive) == 0)) {
                result.insert(QStringLiteral("chain"), QStringLiteral("Ethereum"));
                contractIndex = 2;
            }
            if (segments.size() > contractIndex) {
                const QString contract = segments[contractIndex].trimmed();
                static const QRegularExpression evmAddress{QStringLiteral(R"(^0x[0-9a-fA-F]{40}$)")};
                if (evmAddress.match(contract).hasMatch()) {
                    result.insert(QStringLiteral("contractAddress"), contract);
                }
            }
            if (segments.size() > contractIndex + 1) {
                result.insert(QStringLiteral("tokenId"), segments[contractIndex + 1]);
            }
        }
        return result;
    }
    return result;
}

[[nodiscard]] QVariantMap parsedHtmlPreview(const QByteArray& payload, const QUrl& pageUrl)
{
    const QString html = QString::fromUtf8(payload);
    if (html.trimmed().isEmpty()) {
        return {};
    }

    QHash<QString, QString> metadata{};
    const QRegularExpression metaExpression{
        QStringLiteral(R"(<meta\b[^>]*>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption};
    auto metaMatches = metaExpression.globalMatch(html);
    while (metaMatches.hasNext()) {
        const QString tag = metaMatches.next().captured(0);
        QString key = attributeValue(tag, QStringLiteral("property")).toLower();
        if (key.isEmpty()) {
            key = attributeValue(tag, QStringLiteral("name")).toLower();
        }
        const QString content = attributeValue(tag, QStringLiteral("content"));
        if (!key.isEmpty() && !content.isEmpty() && !metadata.contains(key)) {
            metadata.insert(key, content);
        }
    }

    const auto firstMeta = [&metadata](std::initializer_list<const char*> keys) {
        for (const char* key : keys) {
            const QString value = metadata.value(QString::fromLatin1(key)).trimmed();
            if (!value.isEmpty()) {
                return value;
            }
        }
        return QString{};
    };

    QString title = firstMeta({"og:title", "twitter:title"});
    if (title.isEmpty()) {
        const QRegularExpression titleExpression{
            QStringLiteral(R"(<title\b[^>]*>(.*?)</title>)"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption};
        const auto titleMatch = titleExpression.match(html);
        if (titleMatch.hasMatch()) {
            title = decodeHtmlEntities(titleMatch.captured(1));
        }
    }

    const QString summary = firstMeta({"og:description", "twitter:description", "description"});
    const QString siteName = firstMeta({"og:site_name", "application-name"});
    const QString imageCandidate = firstMeta({"og:image:secure_url", "og:image", "twitter:image"});
    const QString imageAlt = firstMeta({"og:image:alt", "twitter:image:alt"});
    const QString author = firstMeta({"article:author", "author", "twitter:creator"});
    const QString publicationDate = firstMeta({"article:published_time", "date", "datepublished"});
    const QString pageType = firstMeta({"og:type"});
    const int imageWidth = firstMeta({"og:image:width", "twitter:image:width"}).toInt();
    const int imageHeight = firstMeta({"og:image:height", "twitter:image:height"}).toInt();

    QString favicon{};
    const QRegularExpression linkExpression{
        QStringLiteral(R"(<link\b[^>]*>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption};
    auto linkMatches = linkExpression.globalMatch(html);
    while (linkMatches.hasNext()) {
        const QString tag = linkMatches.next().captured(0);
        const QString rel = attributeValue(tag, QStringLiteral("rel")).toLower();
        if (!rel.contains(QStringLiteral("icon"))) {
            continue;
        }
        favicon = attributeValue(tag, QStringLiteral("href"));
        if (!favicon.isEmpty()) {
            break;
        }
    }

    QVariantMap result{};
    if (!title.isEmpty()) result.insert(QStringLiteral("title"), title);
    if (!summary.isEmpty()) result.insert(QStringLiteral("summary"), summary);
    if (!siteName.isEmpty()) result.insert(QStringLiteral("siteName"), siteName);
    if (!imageAlt.isEmpty()) result.insert(QStringLiteral("imageAlt"), imageAlt);
    if (!author.isEmpty()) result.insert(QStringLiteral("author"), author);
    if (!publicationDate.isEmpty()) result.insert(QStringLiteral("publicationDate"), publicationDate);
    if (!pageType.isEmpty()) result.insert(QStringLiteral("pageType"), pageType);
    if (const QUrl image = resolvedAssetUrl(pageUrl, imageCandidate); image.isValid()) {
        result.insert(QStringLiteral("previewImage"), remoteImageSource(image));
        if (imageWidth > 0 && imageHeight > 0) {
            result.insert(QStringLiteral("imageWidth"), imageWidth);
            result.insert(QStringLiteral("imageHeight"), imageHeight);
        }
    }
    if (const QUrl icon = resolvedAssetUrl(pageUrl, favicon); icon.isValid()) {
        result.insert(QStringLiteral("favicon"), remoteImageSource(icon));
    }
    // Mini App discovery is inert metadata, never script execution or a grant
    // of host/wallet capabilities. Legacy v1 frame button POSTs are not executed.
    // Existing Mini Apps still publish the Frames v2 "next" embed version.
    // This is distinct from legacy POST frames ("vNext") and from the domain
    // manifest version. Try both tags: a malformed preferred tag must not hide
    // a valid compatibility embed.
    for (const char* metaKey : {"fc:miniapp", "fc:frame"}) {
        const QString miniAppJson = metadata.value(QString::fromLatin1(metaKey)).trimmed();
        if (miniAppJson.isEmpty() || miniAppJson.size() > 16 * 1024) {
            continue;
        }
        const QJsonObject app = QJsonDocument::fromJson(miniAppJson.toUtf8()).object();
        const QJsonObject button = app.value(QStringLiteral("button")).toObject();
        const QJsonObject action = button.value(QStringLiteral("action")).toObject();
        const QString buttonTitle = button.value(QStringLiteral("title")).toString().trimmed();
        const QString imageValue = app.value(QStringLiteral("imageUrl")).toString();
        const QString launchValue = action.value(QStringLiteral("url")).toString();
        const QString actionType = action.value(QStringLiteral("type")).toString();
        const QUrl imageUrl{imageValue};
        QUrl launchUrl = launchValue.isEmpty() ? pageUrl : QUrl{launchValue};
        const QString embedVersion = app.value(QStringLiteral("version")).toString();
        const bool imageIsPage = normalizedUrl(imageUrl.toString(QUrl::FullyEncoded))
            == normalizedUrl(pageUrl.toString(QUrl::FullyEncoded));
        if ((embedVersion == QStringLiteral("1") || embedVersion == QStringLiteral("next"))
            && (actionType == QStringLiteral("launch_frame") || actionType == QStringLiteral("launch_miniapp"))
            && !buttonTitle.isEmpty() && buttonTitle.size() <= 32
            && imageValue.size() <= 1024 && launchValue.size() <= 1024
            && publicHttpUrl(imageUrl) && imageUrl.scheme() == QStringLiteral("https")
            && publicHttpUrl(launchUrl) && launchUrl.scheme() == QStringLiteral("https")
            && launchUrl.userInfo().isEmpty() && imageUrl.userInfo().isEmpty()) {
            QUrlQuery launchQuery{launchUrl};
            const QUrlQuery pageQuery{pageUrl};
            for (const auto& item : pageQuery.queryItems()) {
                if (!launchQuery.hasQueryItem(item.first)) launchQuery.addQueryItem(item.first, item.second);
            }
            launchUrl.setQuery(launchQuery);
            result.insert(QStringLiteral("providerKind"), QStringLiteral("miniapp"));
            result.insert(QStringLiteral("miniAppButtonTitle"), buttonTitle);
            result.insert(QStringLiteral("miniAppLaunchUrl"), launchUrl.toString(QUrl::FullyEncoded));
            result.insert(QStringLiteral("miniAppLaunchDomain"), displayHost(launchUrl));
            if (!imageIsPage) {
                result.insert(QStringLiteral("previewImage"), remoteImageSource(imageUrl));
                result.insert(QStringLiteral("imageWidth"), 3);
                result.insert(QStringLiteral("imageHeight"), 2);
            }
            break;
        }
    }
    return result;
}

void mergeNonEmpty(QVariantMap& target, const QVariantMap& incoming)
{
    for (auto iterator = incoming.cbegin(); iterator != incoming.cend(); ++iterator) {
        const QVariant value = iterator.value();
        if (!value.isValid()) {
            continue;
        }
        if (value.metaType().id() == QMetaType::QString && value.toString().trimmed().isEmpty()) {
            continue;
        }
        target.insert(iterator.key(), value);
    }
}

enum class PreviewFetchKind : std::uint8_t {
    Html,
    Json,
};

struct PreviewFetchResult final {
    QUrl finalUrl{};
    QByteArray payload{};
    QString contentType{};
    bool usable{};
};

/**
 * Dedicated low-priority Qt Network reactor for link-preview discovery.
 *
 * QNetworkAccessManager and every readyRead/finished handler live on this worker
 * thread. The GUI thread sees only one bounded completion per URL, so scrolling
 * cannot be interrupted by TLS/socket callbacks or HTML chunk accumulation.
 */
class LinkPreviewNetworkWorker final : public QObject {
public:
    using Completion = std::function<void(PreviewFetchResult)>;

    void initialize(const bool useDirectProxy)
    {
        if (m_network != nullptr) {
            return;
        }
        m_network = new QNetworkAccessManager{this};
        if (useDirectProxy) {
            m_network->setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
        } else {
            QNetworkProxyFactory::setUseSystemConfiguration(true);
        }
    }

    void shutdown()
    {
        m_shuttingDown = true;
        if (m_network == nullptr) {
            return;
        }
        const auto replies = m_network->findChildren<QNetworkReply*>();
        for (QNetworkReply* const reply : replies) {
            if (reply == nullptr) {
                continue;
            }
            // No completion callback is useful once the owner is tearing down,
            // and a late readAll() after abort is exactly what produces the
            // QSslSocket "device not open" diagnostics on affected Qt backends.
            QObject::disconnect(reply, nullptr, this, nullptr);
            if (reply->isRunning()) {
                reply->abort();
            }
        }
    }

    void fetch(
        QUrl url,
        const PreviewFetchKind kind,
        const qint64 deadlineMs,
        Completion completion,
        const int redirectCount = 0)
    {
        if (m_shuttingDown || m_network == nullptr || !completion) {
            return;
        }
        if (!publicHttpUrl(url) || redirectCount > maximumRedirects) {
            completion(PreviewFetchResult{.finalUrl = std::move(url)});
            return;
        }

        const qint64 remainingMs = deadlineMs - QDateTime::currentMSecsSinceEpoch();
        if (remainingMs <= 0) {
            completion(PreviewFetchResult{.finalUrl = std::move(url)});
            return;
        }

        QNetworkRequest request{url};
        request.setHeader(
            QNetworkRequest::UserAgentHeader,
            QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X) AppleWebKit/537.36 "
                           "(KHTML, like Gecko) Genycaster/1.0 LinkPreview Safari/537.36"));
        request.setRawHeader(
            "Accept",
            kind == PreviewFetchKind::Json
                ? QByteArrayLiteral("application/json")
                : QByteArrayLiteral("text/html,application/xhtml+xml;q=0.9,*/*;q=0.1"));
        request.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute,
            QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(static_cast<int>(
            std::min<qint64>(remainingMs, requestTimeoutMilliseconds)));

        QNetworkReply* const reply = m_network->get(request);
        if (reply == nullptr) {
            completion(PreviewFetchResult{.finalUrl = std::move(url)});
            return;
        }

        const qsizetype maximumBytes =
            kind == PreviewFetchKind::Json ? maximumJsonBytes : maximumHtmlBytes;
        auto payload = std::make_shared<QByteArray>();
        auto truncated = std::make_shared<bool>(false);
        auto stopped = std::make_shared<bool>(false);

        QTimer::singleShot(
            static_cast<int>(std::min<qint64>(
                remainingMs, absoluteRequestDeadlineMilliseconds)),
            reply,
            [reply, stopped] {
                if (reply->isRunning()) {
                    *stopped = true;
                    reply->abort();
                }
            });

        QObject::connect(
            reply,
            &QNetworkReply::readyRead,
            this,
            [reply, payload, truncated, stopped, maximumBytes] {
                if (*stopped || reply->error() != QNetworkReply::NoError || !reply->isOpen()) {
                    return;
                }
                const QByteArray chunk = reply->readAll();
                if (chunk.isEmpty()) {
                    return;
                }
                if (payload->size() < maximumBytes) {
                    const qsizetype remaining = maximumBytes - payload->size();
                    payload->append(chunk.constData(), std::min(chunk.size(), remaining));
                    if (chunk.size() > remaining) {
                        *truncated = true;
                    }
                } else {
                    *truncated = true;
                }
                // Link metadata is expected in the bounded prefix. Stop socket
                // delivery as soon as it is full; this callback runs off-GUI.
                if (*truncated && reply->isRunning()) {
                    *stopped = true;
                    reply->abort();
                }
            });

        QObject::connect(
            reply,
            &QNetworkReply::finished,
            this,
            [this, reply, url = std::move(url), kind, deadlineMs,
             redirectCount, payload, truncated, stopped, maximumBytes,
             completion = std::move(completion)]() mutable {
                const QVariant redirectValue =
                    reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
                if (redirectValue.isValid() && redirectCount < maximumRedirects) {
                    const QUrl target = url.resolved(redirectValue.toUrl());
                    reply->deleteLater();
                    if (publicHttpUrl(target)) {
                        fetch(
                            target,
                            kind,
                            deadlineMs,
                            std::move(completion),
                            redirectCount + 1);
                        return;
                    }
                    completion(PreviewFetchResult{.finalUrl = std::move(url)});
                    return;
                }

                if (!*stopped && reply->error() == QNetworkReply::NoError
                    && reply->isOpen() && payload->size() < maximumBytes) {
                    const QByteArray tail = reply->readAll();
                    const qsizetype remaining = maximumBytes - payload->size();
                    payload->append(tail.constData(), std::min(tail.size(), remaining));
                    if (tail.size() > remaining) {
                        *truncated = true;
                    }
                }

                const QString contentType =
                    reply->header(QNetworkRequest::ContentTypeHeader)
                        .toString()
                        .toLower();
                const bool usable =
                    reply->error() == QNetworkReply::NoError
                    || (*truncated
                        && reply->error() == QNetworkReply::OperationCanceledError);
                reply->deleteLater();
                completion(PreviewFetchResult{
                    .finalUrl = std::move(url),
                    .payload = std::move(*payload),
                    .contentType = contentType,
                    .usable = usable,
                });
            });
    }

private:
    QNetworkAccessManager* m_network{};
    bool m_shuttingDown{};
};

} // namespace

class LinkPreviewResolver::Impl final {
public:
    struct PendingPreview final {
        QString key{};
        QUrl url{};
    };

    Impl(LinkPreviewResolver& owner, const bool useDirectProxy)
        : q(owner)
    {
#if defined(__EMSCRIPTEN__)
        // Preview fetching uses a QThread and blocking cross-thread setup on
        // native platforms. Browser WASM keeps safe URL-only preview seeds.
        (void)useDirectProxy;
#else
        networkWorker = new LinkPreviewNetworkWorker{};
        networkWorker->moveToThread(&networkThread);
        QObject::connect(
            &networkThread, &QThread::finished,
            networkWorker, &QObject::deleteLater);
        networkThread.setObjectName(QStringLiteral("link-preview-network"));
        networkThread.start(QThread::LowPriority);
        QMetaObject::invokeMethod(
            networkWorker,
            [worker = networkWorker, useDirectProxy] {
                worker->initialize(useDirectProxy);
            },
            Qt::BlockingQueuedConnection);

        // HTML/OpenGraph parsing is regex-heavy and must never consume the GUI
        // thread's frame budget. One low-priority worker is sufficient because
        // previews are opportunistic and serial parsing avoids CPU bursts while
        // the user is scrolling through several links at once.
        parsePool.setMaxThreadCount(1);
        parsePool.setThreadPriority(QThread::LowPriority);
        parsePool.setExpiryTimeout(10'000);
#endif
    }

    ~Impl()
    {
        // Prevent queued parse work from extending application shutdown. Running
        // tasks are bounded to the 256 KiB metadata prefix and finish promptly.
        parsePool.clear();
        parsePool.waitForDone();

        if (networkWorker != nullptr && networkThread.isRunning()) {
            // Never block the GUI thread waiting for a network-thread callback
            // during application teardown. Run shutdown and quit in-order on the
            // worker event loop, then wait with a hard bound below.
            QMetaObject::invokeMethod(
                networkWorker,
                [worker = networkWorker] {
                    worker->shutdown();
                    QThread::currentThread()->quit();
                },
                Qt::QueuedConnection);
            // The worker owns no blocking operation: network replies were
            // aborted above and all callbacks were disconnected. Keep a bounded
            // exit anyway so a broken platform socket cannot strand the GUI
            // process after its last window has closed.
            if (!networkThread.wait(2'000)) {
                networkThread.requestInterruption();
                networkThread.quit();
                if (!networkThread.wait(1'000)) {
                    networkThread.terminate();
                    networkThread.wait();
                }
            }
        }
        // QThread::finished -> deleteLater destroys the worker and QNAM in their
        // own affinity thread. Never move live sockets back onto the GUI thread.
        networkWorker = nullptr;
    }

    // FIFO eviction never discards a running or queued URL. A scroll across
    // thousands of links therefore retains at most 320 metadata snapshots and
    // 64 pending operations instead of growing with historical feed length.
    [[nodiscard]] bool rememberPreview(const QString& key, QVariantMap value)
    {
        if (!cache.contains(key)) {
            qsizetype attempts = cacheOrder.size();
            while (cache.size() >= maximumRetainedPreviewEntries && attempts-- > 0) {
                const QString victim = cacheOrder.dequeue();
                if (inFlight.contains(victim) || queued.contains(victim)) {
                    cacheOrder.enqueue(victim);
                    continue;
                }
                cache.remove(victim);
                observedAt.remove(victim);
            }
            if (cache.size() >= maximumRetainedPreviewEntries) {
                return false;
            }
            cacheOrder.enqueue(key);
        }
        cache.insert(key, std::move(value));
        return true;
    }

    [[nodiscard]] QVariantMap previewFor(const QString& rawUrl) const
    {
        const QString key = normalizedUrl(rawUrl);
        if (key.isEmpty()) {
            return {};
        }
        const auto found = cache.constFind(key);
        if (found != cache.cend()) {
            return found.value();
        }
        const QUrl url{key};
        return publicHttpUrl(url) ? seedPreview(url) : QVariantMap{};
    }

    void requestPreview(const QString& rawUrl)
    {
        const QString key = normalizedUrl(rawUrl);
        if (key.isEmpty()) {
            return;
        }
        const QUrl url{key};
        if (!publicHttpUrl(url) || inFlight.contains(key) || queued.contains(key)) {
            return;
        }
        const auto cached = cache.constFind(key);
        if (cached != cache.cend() && cached.value().value(QStringLiteral("resolved")).toBool()
            && QDateTime::currentMSecsSinceEpoch() - observedAt.value(key, 0)
                   < previewFreshnessMilliseconds) {
            return;
        }
        if (!cache.contains(key) && !rememberPreview(key, seedPreview(url))) {
            return;
        }
#if defined(__EMSCRIPTEN__)
        // Metadata parsing requires a native worker. Avoid queuing requests
        // against a non-existent browser thread; keep the clickable URL seed.
        QVariantMap fallback = cache.value(key);
        fallback.insert(QStringLiteral("resolved"), true);
        if (!rememberPreview(key, fallback)) {
            return;
        }
        observedAt.insert(key, QDateTime::currentMSecsSinceEpoch());
        ++revisionValue;
        emit q.revisionChanged();
        emit q.previewReady(key, fallback);
        return;
#endif
        if (pending.size() >= maximumPendingPreviewRequests) {
            // New viewport interests supersede the oldest unresolved preview.
            // That URL remains clickable and can be re-admitted if visible.
            queued.remove(pending.dequeue().key);
        }
        queued.insert(key);
        pending.enqueue(PendingPreview{.key = key, .url = url});
        drainPending();
    }

    void drainPending()
    {
        while (activePreviewRequests < maximumConcurrentPreviewRequests && !pending.isEmpty()) {
            PendingPreview request = pending.dequeue();
            queued.remove(request.key);
            if (cache.value(request.key).value(QStringLiteral("resolved")).toBool()) {
                continue;
            }
            inFlight.insert(request.key);
            ++activePreviewRequests;
            const qint64 deadlineMs =
                QDateTime::currentMSecsSinceEpoch() + absoluteRequestDeadlineMilliseconds;
            if (!youtubeVideoId(request.url).isEmpty()) {
                startYoutubeOEmbed(request.key, request.url, deadlineMs);
            } else {
                startFetch(request.key, request.url, deadlineMs);
            }
        }
    }

    void parseHtmlAsync(
        QString key,
        QUrl pageUrl,
        QByteArray payload,
        QVariantMap result)
    {
        QPointer<LinkPreviewResolver> owner{&q};
        auto* const task = QRunnable::create(
            [owner, key = std::move(key), pageUrl = std::move(pageUrl),
             payload = std::move(payload), result = std::move(result)]() mutable {
                mergeNonEmpty(result, parsedHtmlPreview(payload, pageUrl));
                if (owner.isNull()) {
                    return;
                }
                QMetaObject::invokeMethod(
                    owner.data(),
                    [owner, key = std::move(key), result = std::move(result)]() mutable {
                        if (owner.isNull()) {
                            return;
                        }
                        owner->m_impl->finish(key, std::move(result));
                    },
                    Qt::QueuedConnection);
            });
        parsePool.start(task);
    }

    void startYoutubeOEmbed(const QString& key, const QUrl& videoUrl, const qint64 deadlineMs)
    {
        QUrl endpoint{QStringLiteral("https://www.youtube.com/oembed")};
        QUrlQuery query{};
        query.addQueryItem(QStringLiteral("url"), videoUrl.toString(QUrl::FullyEncoded));
        query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
        endpoint.setQuery(query);

        LinkPreviewResolver* const owner = &q;
        const bool queuedRequest = QMetaObject::invokeMethod(
            networkWorker,
            [worker = networkWorker, owner, key, endpoint = std::move(endpoint), deadlineMs]() mutable {
                worker->fetch(
                    std::move(endpoint),
                    PreviewFetchKind::Json,
                    deadlineMs,
                    [owner, key](PreviewFetchResult fetched) mutable {
                        QVariantMap delta{};
                        if (fetched.usable) {
                            const QJsonObject object =
                                QJsonDocument::fromJson(fetched.payload).object();
                            const QString title =
                                object.value(QStringLiteral("title")).toString().trimmed();
                            const QString author =
                                object.value(QStringLiteral("author_name")).toString().trimmed();
                            const QUrl thumbnail{
                                object.value(QStringLiteral("thumbnail_url")).toString()};
                            if (!title.isEmpty()) {
                                delta.insert(QStringLiteral("title"), title);
                            }
                            if (!author.isEmpty()) {
                                delta.insert(QStringLiteral("author"), author);
                            }
                            if (publicHttpUrl(thumbnail)) {
                                delta.insert(
                                    QStringLiteral("previewImage"),
                                    remoteImageSource(thumbnail));
                            }
                            const int width =
                                object.value(QStringLiteral("thumbnail_width")).toInt();
                            const int height =
                                object.value(QStringLiteral("thumbnail_height")).toInt();
                            if (width > 0 && height > 0) {
                                delta.insert(QStringLiteral("imageWidth"), width);
                                delta.insert(QStringLiteral("imageHeight"), height);
                            }
                            delta.insert(
                                QStringLiteral("siteName"),
                                QStringLiteral("YouTube"));
                            delta.insert(
                                QStringLiteral("providerKind"),
                                QStringLiteral("youtube"));
                        }
                        QMetaObject::invokeMethod(
                            owner,
                            [owner, key, delta = std::move(delta)]() mutable {
                                QVariantMap result = owner->m_impl->cache.value(key);
                                mergeNonEmpty(result, delta);
                                owner->m_impl->finish(key, std::move(result));
                            },
                            Qt::QueuedConnection);
                    });
            },
            Qt::QueuedConnection);
        if (!queuedRequest) {
            finish(key, cache.value(key));
        }
    }

    void startFetch(const QString& key, const QUrl& url, const qint64 deadlineMs)
    {
        if (!publicHttpUrl(url)) {
            finish(key, cache.value(key));
            return;
        }

        LinkPreviewResolver* const owner = &q;
        const bool queuedRequest = QMetaObject::invokeMethod(
            networkWorker,
            [worker = networkWorker, owner, key, url, deadlineMs] {
                worker->fetch(
                    url,
                    PreviewFetchKind::Html,
                    deadlineMs,
                    [owner, key](PreviewFetchResult fetched) mutable {
                        QMetaObject::invokeMethod(
                            owner,
                            [owner, key, fetched = std::move(fetched)]() mutable {
                                QVariantMap result = owner->m_impl->cache.value(key);
                                if (!fetched.usable) {
                                    owner->m_impl->finish(key, std::move(result));
                                    return;
                                }

                                const QString contentType =
                                    fetched.contentType.section(QLatin1Char(';'), 0, 0)
                                        .trimmed()
                                        .toLower();
                                if (contentType.isEmpty()
                                    || contentType.contains(QStringLiteral("html"))) {
                                    owner->m_impl->parseHtmlAsync(
                                        key,
                                        std::move(fetched.finalUrl),
                                        std::move(fetched.payload),
                                        std::move(result));
                                    return;
                                }

                                if (contentType.startsWith(QStringLiteral("image/"))) {
                                    result.insert(
                                        QStringLiteral("providerKind"),
                                        QStringLiteral("image"));
                                    result.insert(
                                        QStringLiteral("previewImage"),
                                        remoteImageSource(fetched.finalUrl));
                                } else if (contentType.startsWith(QStringLiteral("video/"))) {
                                    result.insert(
                                        QStringLiteral("providerKind"),
                                        QStringLiteral("video"));
                                } else if (contentType.startsWith(QStringLiteral("audio/"))) {
                                    result.insert(
                                        QStringLiteral("providerKind"),
                                        QStringLiteral("audio"));
                                }

                                if ((contentType.startsWith(QStringLiteral("image/"))
                                     || contentType.startsWith(QStringLiteral("video/"))
                                     || contentType.startsWith(QStringLiteral("audio/")))
                                    && result.value(QStringLiteral("title"))
                                           .toString()
                                           .trimmed()
                                           .isEmpty()) {
                                    result.insert(
                                        QStringLiteral("title"),
                                        prettifySlug(fetched.finalUrl.fileName()));
                                }
                                owner->m_impl->finish(key, std::move(result));
                            },
                            Qt::QueuedConnection);
                    });
            },
            Qt::QueuedConnection);
        if (!queuedRequest) {
            finish(key, cache.value(key));
        }
    }

    void finish(const QString& key, QVariantMap result)
    {
        if (inFlight.remove(key) > 0 && activePreviewRequests > 0) {
            --activePreviewRequests;
        }
        result.insert(QStringLiteral("resolved"), true);
        const QVariantMap previous = cache.value(key);
        if (!rememberPreview(key, result)) {
            drainPending();
            return;
        }
        observedAt.insert(key, QDateTime::currentMSecsSinceEpoch());
        if (previous != result) {
            ++revisionValue;
            emit q.revisionChanged();
        }
        emit q.previewReady(key, result);
        drainPending();
    }

    LinkPreviewResolver& q;
    QThread networkThread{};
    LinkPreviewNetworkWorker* networkWorker{};
    QThreadPool parsePool{};
    QHash<QString, QVariantMap> cache{};
    QQueue<QString> cacheOrder{};
    QHash<QString, qint64> observedAt{};
    QQueue<PendingPreview> pending{};
    QSet<QString> queued{};
    QSet<QString> inFlight{};
    qsizetype activePreviewRequests{};
    qulonglong revisionValue{};
};

LinkPreviewResolver::LinkPreviewResolver(const bool useDirectProxy, QObject* const parent)
    : QObject(parent),
      m_impl(std::make_unique<Impl>(*this, useDirectProxy))
{
}

LinkPreviewResolver::~LinkPreviewResolver() = default;

qulonglong LinkPreviewResolver::revision() const noexcept
{
    return m_impl->revisionValue;
}

QVariantMap LinkPreviewResolver::previewFor(const QString& rawUrl) const
{
    return m_impl->previewFor(rawUrl);
}

QString LinkPreviewResolver::imageSourceForUrl(const QString& rawUrl) const
{
    const QString trimmed = rawUrl.trimmed();
    if (trimmed.startsWith(QStringLiteral("image://"))
        || trimmed.startsWith(QStringLiteral("qrc:/"))
        || trimmed.startsWith(QStringLiteral("file:/"))) {
        return trimmed;
    }
    const QString key = normalizedUrl(trimmed);
    if (key.isEmpty()) {
        return {};
    }
    return remoteImageSource(QUrl{key});
}

void LinkPreviewResolver::requestPreview(const QString& rawUrl)
{
    m_impl->requestPreview(rawUrl);
}
