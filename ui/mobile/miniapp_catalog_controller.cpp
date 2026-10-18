#include "miniapp_catalog_controller.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

namespace {

constexpr qsizetype responseByteLimit = 8 * 1024 * 1024;
constexpr qsizetype savedMiniAppLimit = 200;
constexpr int cataloguePageSize = 100;
constexpr int networkTimeoutMs = 12'000;
constexpr auto savedMiniAppsSettingsKey = "miniapps/savedApps";

[[nodiscard]] QString trimmedString(const QJsonObject& object, const QString& key)
{
    return object.value(key).toString().trimmed();
}

[[nodiscard]] QString firstString(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = trimmedString(object, key);
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

[[nodiscard]] QJsonObject objectFromPossiblyEncodedJson(const QJsonValue& value)
{
    if (value.isObject()) {
        return value.toObject();
    }
    if (!value.isString()) {
        return {};
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(value.toString().toUtf8(), &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject()
        ? document.object() : QJsonObject{};
}

[[nodiscard]] QJsonObject miniAppManifestObject(const QJsonObject& row)
{
    QJsonObject source = objectFromPossiblyEncodedJson(row.value(QStringLiteral("manifest")));
    if (source.isEmpty()) {
        source = objectFromPossiblyEncodedJson(row.value(QStringLiteral("metadata")));
    }
    if (source.isEmpty()) {
        source = objectFromPossiblyEncodedJson(row.value(QStringLiteral("app")));
    }
    if (source.isEmpty()) {
        source = row;
    }

    if (source.value(QStringLiteral("miniapp")).isObject()) {
        return source.value(QStringLiteral("miniapp")).toObject();
    }
    if (source.value(QStringLiteral("frame")).isObject()) {
        return source.value(QStringLiteral("frame")).toObject();
    }
    return source;
}

[[nodiscard]] QStringList stringArray(const QJsonValue& value)
{
    QStringList result{};
    if (!value.isArray()) {
        return result;
    }

    for (const QJsonValue& item : value.toArray()) {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty() && !result.contains(text)) {
            result.push_back(text);
        }
    }
    return result;
}

[[nodiscard]] QString publicImageSource(const QString& rawUrl)
{
    const QUrl url{rawUrl.trimmed(), QUrl::StrictMode};
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()) {
        return {};
    }

    const QByteArray encoded = url.toString(QUrl::FullyEncoded).toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("image://remote/") + QString::fromLatin1(encoded);
}

[[nodiscard]] QString normalizedHomeUrl(QString value)
{
    value = value.trimmed();
    const QUrl url{value, QUrl::StrictMode};
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()) {
        return {};
    }

    QUrl normalized = url;
    normalized.setFragment({});
    return normalized.toString(QUrl::FullyEncoded);
}

[[nodiscard]] QJsonObject canonicalCatalogRow(const QJsonObject& row)
{
    const QJsonValue wrappedFrame = row.value(QStringLiteral("frame"));
    if (wrappedFrame.isObject()) {
        const QJsonObject frame = wrappedFrame.toObject();
        if (frame.contains(QStringLiteral("manifest"))
            || frame.contains(QStringLiteral("frames_url"))
            || frame.contains(QStringLiteral("author"))) {
            return frame;
        }
    }
    return row;
}

[[nodiscard]] QVariantMap normalizeApp(const QJsonObject& rawRow, const QString& source)
{
    const QJsonObject row = canonicalCatalogRow(rawRow);
    const QJsonObject app = miniAppManifestObject(row);
    const QJsonObject author = row.value(QStringLiteral("author")).toObject();

    QString homeUrl = firstString(app, {
        QStringLiteral("homeUrl"), QStringLiteral("home_url"),
        QStringLiteral("url"), QStringLiteral("launchUrl"), QStringLiteral("launch_url")});
    if (homeUrl.isEmpty()) {
        homeUrl = firstString(row, {
            QStringLiteral("homeUrl"), QStringLiteral("home_url"),
            QStringLiteral("url"), QStringLiteral("launchUrl"), QStringLiteral("launch_url"),
            QStringLiteral("frames_url")});
    }
    if (homeUrl.isEmpty()) {
        const QString domainCandidate = firstString(row, {QStringLiteral("domain"), QStringLiteral("host")});
        if (!domainCandidate.isEmpty()) {
            homeUrl = QStringLiteral("https://") + domainCandidate;
        }
    }
    homeUrl = normalizedHomeUrl(homeUrl);
    if (homeUrl.isEmpty()) {
        return {};
    }

    const QUrl parsedHome{homeUrl};
    QString domain = firstString(row, {QStringLiteral("domain"), QStringLiteral("host")});
    if (domain.isEmpty()) {
        domain = parsedHome.host().toLower();
    }

    QString name = firstString(app, {QStringLiteral("name"), QStringLiteral("title")});
    if (name.isEmpty()) {
        name = firstString(row, {
            QStringLiteral("name"), QStringLiteral("title"), QStringLiteral("frameName")});
    }
    if (name.isEmpty()) {
        name = domain;
    }

    QString description = firstString(app, {
        QStringLiteral("description"), QStringLiteral("tagline"), QStringLiteral("subtitle")});
    if (description.isEmpty()) {
        description = firstString(row, {
            QStringLiteral("description"), QStringLiteral("tagline"), QStringLiteral("subtitle")});
    }

    // Catalogue providers expose the Farcaster Mini App icon under several
    // conventional keys. Prefer small square logos over splash/poster artwork.
    const QStringList iconKeys{
        QStringLiteral("iconUrl"), QStringLiteral("icon_url"),
        QStringLiteral("iconSource"), QStringLiteral("icon_source"),
        QStringLiteral("icon"), QStringLiteral("logoUrl"),
        QStringLiteral("logo_url"), QStringLiteral("logo"),
        QStringLiteral("imageUrl"), QStringLiteral("image_url"),
        QStringLiteral("image"), QStringLiteral("thumbnailUrl"),
        QStringLiteral("thumbnail_url"), QStringLiteral("splashImageUrl"),
        QStringLiteral("splash_image_url")};
    QString iconUrl = firstString(app, iconKeys);
    if (iconUrl.isEmpty()) {
        iconUrl = firstString(row, iconKeys);
    }
    // A manifest can provide a root-relative icon. Resolve it only against
    // the already validated HTTPS Mini App home URL.
    if (!iconUrl.isEmpty()) {
        const QUrl candidate{iconUrl, QUrl::StrictMode};
        if (candidate.isRelative()) {
            iconUrl = parsedHome.resolved(candidate).toString(QUrl::FullyEncoded);
        }
    }

    QString authorName = firstString(author, {
        QStringLiteral("display_name"), QStringLiteral("displayName"), QStringLiteral("username")});
    if (authorName.isEmpty()) {
        authorName = firstString(row, {QStringLiteral("authorName"), QStringLiteral("author_name")});
    }

    qulonglong authorFid = 0;
    const QJsonValue fid = author.value(QStringLiteral("fid"));
    if (fid.isDouble() && fid.toDouble() > 0) {
        authorFid = static_cast<qulonglong>(fid.toDouble());
    }
    if (authorFid == 0) {
        const QJsonValue rowFid = row.value(QStringLiteral("author_fid"));
        if (rowFid.isDouble() && rowFid.toDouble() > 0) {
            authorFid = static_cast<qulonglong>(rowFid.toDouble());
        }
    }
    if (authorFid == 0) {
        const QJsonValue rowFid = row.value(QStringLiteral("fid"));
        if (rowFid.isDouble() && rowFid.toDouble() > 0) {
            authorFid = static_cast<qulonglong>(rowFid.toDouble());
        }
    }

    QStringList requiredChains = stringArray(app.value(QStringLiteral("requiredChains")));
    if (requiredChains.isEmpty()) {
        requiredChains = stringArray(app.value(QStringLiteral("required_chains")));
    }

    QStringList requiredCapabilities = stringArray(app.value(QStringLiteral("requiredCapabilities")));
    if (requiredCapabilities.isEmpty()) {
        requiredCapabilities = stringArray(app.value(QStringLiteral("required_capabilities")));
    }

    return {
        {QStringLiteral("name"), name.left(96)},
        {QStringLiteral("description"), description.left(420)},
        {QStringLiteral("domain"), domain.left(255)},
        {QStringLiteral("homeUrl"), homeUrl},
        {QStringLiteral("iconSource"), publicImageSource(iconUrl)},
        {QStringLiteral("authorName"), authorName.left(96)},
        {QStringLiteral("authorFid"), QVariant::fromValue(authorFid)},
        {QStringLiteral("requiredChains"), requiredChains},
        {QStringLiteral("requiredCapabilities"), requiredCapabilities},
        {QStringLiteral("source"), source},
        {QStringLiteral("saved"), false},
    };
}

[[nodiscard]] QJsonArray firstArray(const QJsonObject& object)
{
    for (const QString& key : {
             QStringLiteral("miniapps"), QStringLiteral("miniApps"), QStringLiteral("frames"),
             QStringLiteral("relevant_frames"), QStringLiteral("winners"),
             QStringLiteral("apps"), QStringLiteral("data"), QStringLiteral("results")}) {
        if (object.value(key).isArray()) {
            return object.value(key).toArray();
        }
    }
    return {};
}

[[nodiscard]] QJsonArray resultArray(const QJsonDocument& document)
{
    if (document.isArray()) {
        return document.array();
    }
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    QJsonArray rows = firstArray(root);
    if (!rows.isEmpty()) {
        return rows;
    }

    for (const QString& key : {QStringLiteral("result"), QStringLiteral("data")}) {
        if (!root.value(key).isObject()) {
            continue;
        }
        rows = firstArray(root.value(key).toObject());
        if (!rows.isEmpty()) {
            return rows;
        }
    }
    return {};
}

[[nodiscard]] QVariantList normalizedRows(const QJsonDocument& document, const QString& source)
{
    QVariantList rows{};
    QSet<QString> seen{};
    rows.reserve(cataloguePageSize);

    for (const QJsonValue& value : resultArray(document)) {
        if (!value.isObject()) {
            continue;
        }
        QVariantMap row = normalizeApp(value.toObject(), source);
        const QString homeUrl = row.value(QStringLiteral("homeUrl")).toString();
        if (homeUrl.isEmpty() || seen.contains(homeUrl)) {
            continue;
        }
        seen.insert(homeUrl);
        rows.push_back(std::move(row));
        if (rows.size() >= cataloguePageSize) {
            break;
        }
    }
    return rows;
}

[[nodiscard]] QVariantMap normalizeSavedRow(QVariantMap row)
{
    const QString homeUrl = normalizedHomeUrl(row.value(QStringLiteral("homeUrl")).toString());
    if (homeUrl.isEmpty()) {
        return {};
    }

    QString name = row.value(QStringLiteral("name")).toString().trimmed().left(96);
    const QString domain = QUrl{homeUrl}.host().toLower();
    if (name.isEmpty()) {
        name = domain;
    }

    row.insert(QStringLiteral("homeUrl"), homeUrl);
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("description"),
               row.value(QStringLiteral("description")).toString().trimmed().left(420));
    row.insert(QStringLiteral("domain"), domain);
    row.insert(QStringLiteral("iconSource"), row.value(QStringLiteral("iconSource")).toString());
    row.insert(QStringLiteral("saved"), true);
    return row;
}


[[nodiscard]] QString decodeHtml(QString value)
{
    return value.replace(QStringLiteral("&amp;"), QStringLiteral("&"))
        .replace(QStringLiteral("&quot;"), QStringLiteral("\""))
        .replace(QStringLiteral("&#39;"), QStringLiteral("'"))
        .replace(QStringLiteral("&lt;"), QStringLiteral("<"))
        .replace(QStringLiteral("&gt;"), QStringLiteral(">"));
}

[[nodiscard]] QString titleFromUniversalLink(const QUrl& url)
{
    QString slug = url.path().section(QLatin1Char('/'), -1);
    slug = QUrl::fromPercentEncoding(slug.toUtf8()).trimmed();
    slug.replace(QLatin1Char('-'), QLatin1Char(' '));
    slug.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (slug.isEmpty()) {
        return QStringLiteral("Farcaster Mini App");
    }
    bool capitalize = true;
    for (qsizetype index = 0; index < slug.size(); ++index) {
        if (capitalize && slug.at(index).isLetter()) {
            slug[index] = slug.at(index).toUpper();
            capitalize = false;
        } else if (slug.at(index).isSpace()) {
            capitalize = true;
        }
    }
    return slug.left(96);
}

[[nodiscard]] QString firstHtmlAttribute(const QString& html,
                                         const QString& attribute,
                                         const qsizetype from,
                                         const qsizetype to)
{
    if (from < 0 || to <= from || from >= html.size()) {
        return {};
    }
    const QString segment = html.mid(from, std::min(to, html.size()) - from);
    const QRegularExpression expression{
        QStringLiteral(R"(%1\s*=\s*[\"']([^\"']+)[\"'])").arg(QRegularExpression::escape(attribute)),
        QRegularExpression::CaseInsensitiveOption};
    const auto match = expression.match(segment);
    return match.hasMatch() ? decodeHtml(match.captured(1)).trimmed() : QString{};
}

[[nodiscard]] QVariantList farcasterDirectoryRows(const QByteArray& bytes, const QString& source)
{
    const QString html = QString::fromUtf8(bytes);
    const QRegularExpression universalLinkExpression{
        QStringLiteral(R"(href\s*=\s*[\"']((?:https://farcaster\.xyz)?/miniapps/[^\"'#?\s]+(?:\?[^\"']*)?)[\"'])"),
        QRegularExpression::CaseInsensitiveOption};
    auto matches = universalLinkExpression.globalMatch(html);
    QVariantList rows{};
    QSet<QString> seen{};
    rows.reserve(cataloguePageSize);

    while (matches.hasNext() && rows.size() < cataloguePageSize) {
        const auto match = matches.next();
        QString rawLink = decodeHtml(match.captured(1));
        if (rawLink.startsWith(QLatin1Char('/'))) {
            rawLink.prepend(QStringLiteral("https://farcaster.xyz"));
        }
        const QUrl universalLink{rawLink, QUrl::StrictMode};
        if (!universalLink.isValid() || universalLink.scheme() != QStringLiteral("https")
            || universalLink.host().compare(QStringLiteral("farcaster.xyz"), Qt::CaseInsensitive) != 0
            || !universalLink.path().startsWith(QStringLiteral("/miniapps/"))) {
            continue;
        }
        const QString homeUrl = normalizedHomeUrl(universalLink.toString(QUrl::FullyEncoded));
        if (homeUrl.isEmpty() || seen.contains(homeUrl)) {
            continue;
        }
        seen.insert(homeUrl);

        const qsizetype start = std::max<qsizetype>(0, match.capturedStart() - 1800);
        const qsizetype end = std::min<qsizetype>(html.size(), match.capturedEnd() + 2200);
        QString imageUrl = firstHtmlAttribute(html, QStringLiteral("src"), start, end);
        if (!imageUrl.startsWith(QStringLiteral("https://"))) {
            imageUrl.clear();
        }
        QString name = firstHtmlAttribute(html, QStringLiteral("alt"), start, end);
        if (name.startsWith(QStringLiteral("Image:"), Qt::CaseInsensitive)) {
            name = name.mid(6).trimmed();
        }
        if (name.isEmpty() || name.size() > 96) {
            name = titleFromUniversalLink(universalLink);
        }

        QString description{};
        const QString segment = html.mid(start, end - start);
        const QRegularExpression descriptionExpression{
            QStringLiteral(R"(Description:\s*(?:<[^>]+>\s*)*([^<]{1,420}))"),
            QRegularExpression::CaseInsensitiveOption};
        const auto descriptionMatch = descriptionExpression.match(segment);
        if (descriptionMatch.hasMatch()) {
            description = decodeHtml(descriptionMatch.captured(1)).trimmed();
        }

        rows.push_back(QVariantMap{
            {QStringLiteral("name"), name},
            {QStringLiteral("description"), description},
            {QStringLiteral("domain"), QStringLiteral("farcaster.xyz")},
            {QStringLiteral("homeUrl"), homeUrl},
            {QStringLiteral("iconSource"), publicImageSource(imageUrl)},
            {QStringLiteral("authorName"), QString{}},
            {QStringLiteral("authorFid"), QVariant::fromValue<qulonglong>(0)},
            {QStringLiteral("requiredChains"), QStringList{}},
            {QStringLiteral("requiredCapabilities"), QStringList{}},
            {QStringLiteral("source"), source},
            {QStringLiteral("saved"), false},
            {QStringLiteral("universalLink"), true},
        });
    }
    return rows;
}

struct CatalogNetworkResult final {
    QVariantList rows{};
    QString error{};
    bool success{};
};

class CatalogNetworkWorker final : public QObject {
public:
    void initialize(const bool useDirectProxy)
    {
        if (m_network != nullptr) {
            return;
        }
        m_network = new QNetworkAccessManager(this);
        if (useDirectProxy) {
            m_network->setProxy(QNetworkProxy::NoProxy);
        }
    }

    void cancel()
    {
        if (m_reply == nullptr) {
            return;
        }
        QObject::disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    void issue(QUrl url,
               QList<QPair<QByteArray, QByteArray>> headers,
               QString source,
               const MiniAppCatalogFormat format,
               std::function<void(CatalogNetworkResult)> completion)
    {
        cancel();
        if (m_network == nullptr) {
            completion(CatalogNetworkResult{.error = QStringLiteral("Network worker is unavailable.")});
            return;
        }
        if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()) {
            completion(CatalogNetworkResult{.error = QStringLiteral("Discovery endpoint is invalid.")});
            return;
        }

        QNetworkRequest request{url};
        request.setTransferTimeout(networkTimeoutMs);
        request.setRawHeader("Accept", format == MiniAppCatalogFormat::Json ? "application/json" : "text/html,application/xhtml+xml");
        for (const auto& [name, value] : headers) {
            if (!name.isEmpty() && !value.isEmpty()) {
                request.setRawHeader(name, value);
            }
        }

        m_reply = m_network->get(request);
        m_reply->setReadBufferSize(responseByteLimit + 1);
        QNetworkReply* const activeReply = m_reply;
        auto bytes = std::make_shared<QByteArray>();
        auto oversized = std::make_shared<bool>(false);

        const auto appendAvailable = [activeReply, bytes, oversized] {
            if (*oversized || activeReply->error() != QNetworkReply::NoError
                || !activeReply->isOpen()) {
                return;
            }
            const qsizetype remaining = (responseByteLimit + 1) - bytes->size();
            if (remaining <= 0) {
                *oversized = true;
                activeReply->abort();
                return;
            }
            bytes->append(activeReply->read(remaining));
            if (bytes->size() > responseByteLimit || activeReply->bytesAvailable() > 0) {
                *oversized = true;
                activeReply->abort();
            }
        };

        QObject::connect(activeReply, &QIODevice::readyRead, this, appendAvailable);
        QObject::connect(activeReply, &QNetworkReply::finished, this,
            [this, activeReply, bytes, oversized, source = std::move(source),
             format, completion = std::move(completion), appendAvailable]() mutable {
                if (activeReply == m_reply) {
                    m_reply = nullptr;
                }
                appendAvailable();

                const auto networkError = activeReply->error();
                const int status = activeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QString networkErrorText = activeReply->errorString();
                activeReply->deleteLater();

                if (*oversized) {
                    completion(CatalogNetworkResult{
                        .error = QStringLiteral("Discovery response exceeded the safety limit.")});
                    return;
                }
                if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
                    completion(CatalogNetworkResult{
                        .error = QStringLiteral("HTTP %1: %2").arg(status).arg(networkErrorText)});
                    return;
                }

                QVariantList rows{};
                if (format == MiniAppCatalogFormat::Json) {
                    QJsonParseError parseError{};
                    const QJsonDocument document = QJsonDocument::fromJson(*bytes, &parseError);
                    if (parseError.error != QJsonParseError::NoError) {
                        completion(CatalogNetworkResult{
                            .error = QStringLiteral("Discovery returned invalid JSON.")});
                        return;
                    }
                    rows = normalizedRows(document, source);
                } else {
                    rows = farcasterDirectoryRows(*bytes, source);
                }

                completion(CatalogNetworkResult{
                    .rows = std::move(rows),
                    .success = true,
                });
            });
    }

private:
    QNetworkAccessManager* m_network{};
    QNetworkReply* m_reply{};
};

[[nodiscard]] QList<QPair<QByteArray, QByteArray>> endpointHeaders(const MiniAppCatalogEndpoint& endpoint)
{
    QList<QPair<QByteArray, QByteArray>> headers{};
    if (!endpoint.authorizationHeaderName.isEmpty() && !endpoint.authorizationHeaderValue.isEmpty()) {
        headers.push_back({endpoint.authorizationHeaderName, endpoint.authorizationHeaderValue});
    }
    return headers;
}

[[nodiscard]] QUrl endpointUrl(const QString& rawUrl)
{
    return QUrl{rawUrl.trimmed(), QUrl::StrictMode};
}

[[nodiscard]] QString safeProviderLabel(const MiniAppCatalogEndpoint& endpoint)
{
    const QString label = endpoint.providerLabel.trimmed();
    return label.isEmpty() ? QStringLiteral("Discovery provider") : label.left(64);
}

} // namespace

class MiniAppCatalogController::Impl final {
public:
    Impl(MiniAppCatalogController& owner,
         QSettings& settingsRef,
         const bool useDirectProxy,
         MiniAppCatalogEndpoint primary,
         MiniAppCatalogEndpoint fallback,
         const bool allowNetwork)
        : q(owner),
          settings(settingsRef),
          primaryEndpoint(std::move(primary)),
          fallbackEndpoint(std::move(fallback)),
          networkEnabled(allowNetwork)
    {
        loadSaved();
        if (!networkEnabled || (!primaryEndpoint.available() && !fallbackEndpoint.available())) {
            return;
        }

#if defined(__EMSCRIPTEN__)
        // Browser networking is asynchronous but has no native QThread.
        // The QObject and QNetworkAccessManager live on Qt's GUI event loop.
        networkWorker = new CatalogNetworkWorker{};
        networkWorker->initialize(useDirectProxy);
#else
        networkWorker = new CatalogNetworkWorker{};
        networkWorker->moveToThread(&networkThread);
        QObject::connect(&networkThread, &QThread::finished, networkWorker, &QObject::deleteLater);
        networkThread.setObjectName(QStringLiteral("miniapp-discovery-network"));
        networkThread.start();
        QMetaObject::invokeMethod(networkWorker,
            [worker = networkWorker, useDirectProxy] { worker->initialize(useDirectProxy); },
            Qt::QueuedConnection);
#endif
    }

    ~Impl()
    {
        ++generation;
#if defined(__EMSCRIPTEN__)
        if (networkWorker != nullptr) {
            networkWorker->cancel();
            delete networkWorker;
            networkWorker = nullptr;
        }
#else
        if (networkWorker != nullptr && networkThread.isRunning()) {
            // Teardown must not block the GUI thread on a queued network callback.
            // Cancel the reply and stop the event loop in-order on its own thread.
            QMetaObject::invokeMethod(networkWorker,
                [worker = networkWorker] {
                    worker->cancel();
                    QThread::currentThread()->quit();
                }, Qt::QueuedConnection);
            if (!networkThread.wait(2'000)) {
                networkThread.requestInterruption();
                networkThread.quit();
                if (!networkThread.wait(1'000)) {
                    networkThread.terminate();
                    networkThread.wait();
                }
            }
        }
        networkWorker = nullptr;
#endif
    }

    void refresh()
    {
        const quint64 requestGeneration = beginRequest();
        if (!networkEnabled) {
            setBusy(false);
            setError(QStringLiteral("Mini App discovery is unavailable in this runtime profile."));
            return;
        }

        if (primaryEndpoint.available()) {
            setProvider(safeProviderLabel(primaryEndpoint));
            issue(endpointUrl(primaryEndpoint.catalogueUrl), endpointHeaders(primaryEndpoint), primaryEndpoint.format,
                  requestGeneration, safeProviderLabel(primaryEndpoint), true);
            return;
        }
        requestFallbackCatalogue(requestGeneration);
    }

    void search(QString text)
    {
        text = text.trimmed();
        const bool changed = text != query;
        query = std::move(text);
        if (changed) {
            emit q.queryChanged();
        }

        const quint64 requestGeneration = beginRequest();
        if (query.isEmpty()) {
            if (!catalogueSnapshot.isEmpty()) {
                setExplorer(catalogueSnapshot, false);
                setProvider(catalogueProviderLabel);
                setBusy(false);
            } else {
                refreshWithGeneration(requestGeneration);
            }
            return;
        }

        if (query.startsWith(QStringLiteral("fid:"), Qt::CaseInsensitive)) {
            localFilter();
            return;
        }

        const MiniAppCatalogEndpoint* const endpoint = remoteSearchEndpoint();
        if (!networkEnabled || endpoint == nullptr) {
            localFilter();
            return;
        }

        requestSearch(*endpoint, query.left(32), requestGeneration);
    }

    void refreshRelevant(const qulonglong viewerFid)
    {
        const quint64 requestGeneration = beginRequest();
        if (!networkEnabled) {
            setBusy(false);
            setError(QStringLiteral("Mini App discovery is unavailable in this runtime profile."));
            return;
        }
        if (viewerFid == 0) {
            setBusy(false);
            setError(QStringLiteral("Sign in with Farcaster to load Mini Apps relevant to your FID."));
            return;
        }

        const MiniAppCatalogEndpoint* const endpoint = relevantEndpoint();
        if (endpoint == nullptr) {
            setBusy(false);
            setProvider(QStringLiteral("Farcaster public index"));
            setError(QStringLiteral("Personalized Mini App discovery requires a configured structured discovery provider."));
            return;
        }

        QUrl url = endpointUrl(endpoint->relevantUrl);
        QUrlQuery parameters{url};
        parameters.removeAllQueryItems(QStringLiteral("viewer_fid"));
        parameters.addQueryItem(QStringLiteral("viewer_fid"), QString::number(viewerFid));
        url.setQuery(parameters);
        const QString source = safeProviderLabel(*endpoint) + QStringLiteral(" · For You");
        setProvider(source);
        issue(url, endpointHeaders(*endpoint), endpoint->format, requestGeneration, source, false);
    }

    void searchByFid(const qulonglong authorFid)
    {
        if (authorFid == 0) {
            return;
        }
        const QString value = QStringLiteral("fid:%1").arg(authorFid);
        const bool changed = query != value;
        query = value;
        if (changed) {
            emit q.queryChanged();
        }
        [[maybe_unused]] const quint64 requestGeneration = beginRequest();
        localFilter();
    }

    void add(const QVariantMap& app)
    {
        QVariantMap row = normalizeSavedRow(app);
        const QString homeUrl = row.value(QStringLiteral("homeUrl")).toString();
        if (homeUrl.isEmpty() || isSaved(homeUrl)) {
            return;
        }

        if (myApps.size() >= savedMiniAppLimit) {
            myApps.removeLast();
        }
        myApps.prepend(std::move(row));
        persistSaved();
        decorateSaved(explorerApps);
        decorateSaved(catalogueSnapshot);
        emit q.myAppsChanged();
        emit q.explorerAppsChanged();
    }

    void remove(const QString& rawHomeUrl)
    {
        const QString homeUrl = normalizedHomeUrl(rawHomeUrl);
        if (homeUrl.isEmpty()) {
            return;
        }

        bool removed = false;
        for (qsizetype index = myApps.size(); index > 0; --index) {
            const qsizetype candidate = index - 1;
            if (myApps.at(candidate).toMap().value(QStringLiteral("homeUrl")).toString() == homeUrl) {
                myApps.removeAt(candidate);
                removed = true;
            }
        }
        if (!removed) {
            return;
        }

        persistSaved();
        decorateSaved(explorerApps);
        decorateSaved(catalogueSnapshot);
        emit q.myAppsChanged();
        emit q.explorerAppsChanged();
    }

    [[nodiscard]] bool hasRemoteSearch() const noexcept
    {
        return networkEnabled && remoteSearchEndpoint() != nullptr;
    }

    [[nodiscard]] bool hasRelevantDiscovery() const noexcept
    {
        return networkEnabled && relevantEndpoint() != nullptr;
    }

    MiniAppCatalogController& q;
    QSettings& settings;
    QThread networkThread{};
    CatalogNetworkWorker* networkWorker{};
    MiniAppCatalogEndpoint primaryEndpoint{};
    MiniAppCatalogEndpoint fallbackEndpoint{};
    bool networkEnabled{};
    bool busy{};
    QString errorText{};
    QString providerLabel{};
    QString catalogueProviderLabel{};
    QString query{};
    QVariantList explorerApps{};
    QVariantList catalogueSnapshot{};
    QVariantList myApps{};
    quint64 generation{};

private:
    [[nodiscard]] const MiniAppCatalogEndpoint* remoteSearchEndpoint() const noexcept
    {
        if (primaryEndpoint.searchAvailable()) {
            return &primaryEndpoint;
        }
        if (fallbackEndpoint.searchAvailable()) {
            return &fallbackEndpoint;
        }
        return nullptr;
    }

    [[nodiscard]] const MiniAppCatalogEndpoint* relevantEndpoint() const noexcept
    {
        if (primaryEndpoint.relevantAvailable()) {
            return &primaryEndpoint;
        }
        if (fallbackEndpoint.relevantAvailable()) {
            return &fallbackEndpoint;
        }
        return nullptr;
    }

    [[nodiscard]] quint64 beginRequest()
    {
        ++generation;
        cancelWorkerReply();
        setError({});
        setBusy(true);
        return generation;
    }

    void refreshWithGeneration(const quint64 requestGeneration)
    {
        if (!networkEnabled) {
            setBusy(false);
            setError(QStringLiteral("Mini App discovery is unavailable in this runtime profile."));
            return;
        }
        if (primaryEndpoint.available()) {
            setProvider(safeProviderLabel(primaryEndpoint));
            issue(endpointUrl(primaryEndpoint.catalogueUrl), endpointHeaders(primaryEndpoint), primaryEndpoint.format,
                  requestGeneration, safeProviderLabel(primaryEndpoint), true);
            return;
        }
        requestFallbackCatalogue(requestGeneration);
    }

    void cancelWorkerReply()
    {
#if defined(__EMSCRIPTEN__)
        if (networkWorker != nullptr) {
            networkWorker->cancel();
        }
#else
        if (networkWorker == nullptr || !networkThread.isRunning()) {
            return;
        }
        QMetaObject::invokeMethod(networkWorker,
            [worker = networkWorker] { worker->cancel(); }, Qt::QueuedConnection);
#endif
    }

    void issue(const QUrl& url,
               const QList<QPair<QByteArray, QByteArray>>& headers,
               const MiniAppCatalogFormat format,
               const quint64 requestGeneration,
               const QString& source,
               const bool allowFallback)
    {
#if defined(__EMSCRIPTEN__)
        if (networkWorker == nullptr) {
#else
        if (networkWorker == nullptr || !networkThread.isRunning()) {
#endif
            handleNetworkResult(CatalogNetworkResult{.error = QStringLiteral("Discovery worker is unavailable.")},
                                requestGeneration, source, allowFallback);
            return;
        }

        const QPointer<MiniAppCatalogController> owner{&q};
        auto completion = [owner, requestGeneration, source, allowFallback](CatalogNetworkResult result) mutable {
            if (!owner) {
                return;
            }
            QMetaObject::invokeMethod(owner.data(),
                [owner, requestGeneration, source, allowFallback, result = std::move(result)]() mutable {
                    if (!owner || !owner->m_impl) {
                        return;
                    }
                    owner->m_impl->handleNetworkResult(
                        std::move(result), requestGeneration, source, allowFallback);
                }, Qt::QueuedConnection);
        };

#if defined(__EMSCRIPTEN__)
        networkWorker->issue(url, headers, source, format, std::move(completion));
#else
        QMetaObject::invokeMethod(networkWorker,
            [worker = networkWorker, url, headers, source, format, completion = std::move(completion)]() mutable {
                worker->issue(url, headers, source, format, std::move(completion));
            }, Qt::QueuedConnection);
#endif
    }

    void handleNetworkResult(CatalogNetworkResult result,
                             const quint64 requestGeneration,
                             const QString& source,
                             const bool allowFallback)
    {
        if (requestGeneration != generation) {
            return;
        }

        if (!result.success || result.rows.isEmpty()) {
            if (allowFallback && fallbackEndpoint.available()) {
                requestFallbackCatalogue(requestGeneration);
                return;
            }
            setProvider(source);
            setBusy(false);
            if (result.success && result.rows.isEmpty()) {
                setError(QStringLiteral("%1 returned no discoverable Mini Apps.").arg(source));
            } else if (!result.error.isEmpty()) {
                setError(QStringLiteral("Mini App discovery failed via %1: %2").arg(source, result.error));
            } else {
                setError(QStringLiteral("Mini App discovery could not be refreshed from %1.").arg(source));
            }
            return;
        }

        setExplorer(std::move(result.rows), query.isEmpty(), source);
        setProvider(source);
        setError({});
        setBusy(false);
    }

    void requestFallbackCatalogue(const quint64 requestGeneration)
    {
        if (!fallbackEndpoint.available()) {
            setBusy(false);
            const QString source = primaryEndpoint.available()
                ? safeProviderLabel(primaryEndpoint) : QStringLiteral("configured providers");
            setProvider(source);
            setError(QStringLiteral("No available Mini App discovery fallback is configured."));
            return;
        }

        const QString source = safeProviderLabel(fallbackEndpoint);
        setProvider(source);
        issue(endpointUrl(fallbackEndpoint.catalogueUrl), endpointHeaders(fallbackEndpoint), fallbackEndpoint.format,
              requestGeneration, source, false);
    }

    void requestSearch(const MiniAppCatalogEndpoint& endpoint,
                       const QString& searchText,
                       const quint64 requestGeneration)
    {
        QUrl url = endpointUrl(endpoint.searchUrl);
        QUrlQuery parameters{url};
        parameters.removeAllQueryItems(QStringLiteral("q"));
        parameters.removeAllQueryItems(QStringLiteral("limit"));
        parameters.addQueryItem(QStringLiteral("q"), searchText);
        parameters.addQueryItem(QStringLiteral("limit"), QStringLiteral("20"));
        url.setQuery(parameters);

        const QString source = safeProviderLabel(endpoint) + QStringLiteral(" · Search");
        setProvider(source);
        issue(url, endpointHeaders(endpoint), endpoint.format, requestGeneration, source, false);
    }

    void localFilter()
    {
        QVariantList filtered{};
        const QString needle = query.trimmed();
        if (needle.isEmpty()) {
            setExplorer(catalogueSnapshot, false);
        } else if (needle.startsWith(QStringLiteral("fid:"), Qt::CaseInsensitive)) {
            bool ok = false;
            const qulonglong authorFid = needle.mid(4).trimmed().toULongLong(&ok);
            if (ok && authorFid > 0) {
                for (const QVariant& value : catalogueSnapshot) {
                    const QVariantMap row = value.toMap();
                    if (row.value(QStringLiteral("authorFid")).toULongLong() == authorFid) {
                        filtered.push_back(row);
                    }
                }
            }
            setExplorer(std::move(filtered), false);
        } else {
            for (const QVariant& value : catalogueSnapshot) {
                const QVariantMap row = value.toMap();
                const QString haystack = row.value(QStringLiteral("name")).toString() + QLatin1Char(' ')
                    + row.value(QStringLiteral("domain")).toString() + QLatin1Char(' ')
                    + row.value(QStringLiteral("description")).toString() + QLatin1Char(' ')
                    + row.value(QStringLiteral("authorName")).toString();
                if (haystack.contains(needle, Qt::CaseInsensitive)) {
                    filtered.push_back(row);
                }
            }
            setExplorer(std::move(filtered), false);
        }

        const QString base = catalogueProviderLabel.isEmpty()
            ? QStringLiteral("Cached catalogue") : catalogueProviderLabel;
        setProvider(base + QStringLiteral(" · local filter"));
        setBusy(false);
    }

    void setExplorer(QVariantList rows, const bool updateSnapshot, const QString& source = {})
    {
        decorateSaved(rows);
        if (updateSnapshot) {
            catalogueSnapshot = rows;
            catalogueProviderLabel = source;
        }
        if (rows == explorerApps) {
            return;
        }
        explorerApps = std::move(rows);
        emit q.explorerAppsChanged();
    }

    void decorateSaved(QVariantList& rows) const
    {
        for (QVariant& value : rows) {
            QVariantMap row = value.toMap();
            row.insert(QStringLiteral("saved"), isSaved(row.value(QStringLiteral("homeUrl")).toString()));
            value = std::move(row);
        }
    }

    [[nodiscard]] bool isSaved(const QString& rawHomeUrl) const
    {
        const QString homeUrl = normalizedHomeUrl(rawHomeUrl);
        if (homeUrl.isEmpty()) {
            return false;
        }
        return std::ranges::any_of(myApps, [&homeUrl](const QVariant& value) {
            return value.toMap().value(QStringLiteral("homeUrl")).toString() == homeUrl;
        });
    }

    void loadSaved()
    {
        QVariantList loaded{};
        QSet<QString> seen{};
        const QVariantList stored = settings.value(QString::fromLatin1(savedMiniAppsSettingsKey)).toList();
        loaded.reserve(std::min(stored.size(), savedMiniAppLimit));

        for (const QVariant& value : stored) {
            QVariantMap row = normalizeSavedRow(value.toMap());
            const QString homeUrl = row.value(QStringLiteral("homeUrl")).toString();
            if (homeUrl.isEmpty() || seen.contains(homeUrl)) {
                continue;
            }
            seen.insert(homeUrl);
            loaded.push_back(std::move(row));
            if (loaded.size() >= savedMiniAppLimit) {
                break;
            }
        }
        myApps = std::move(loaded);
    }

    void persistSaved()
    {
        settings.setValue(QString::fromLatin1(savedMiniAppsSettingsKey), myApps);
    }

    void setBusy(const bool value)
    {
        if (busy == value) {
            return;
        }
        busy = value;
        emit q.busyChanged();
    }

    void setError(QString value)
    {
        if (errorText == value) {
            return;
        }
        errorText = std::move(value);
        emit q.errorTextChanged();
    }

    void setProvider(QString value)
    {
        if (providerLabel == value) {
            return;
        }
        providerLabel = std::move(value);
        emit q.providerLabelChanged();
    }
};

MiniAppCatalogController::MiniAppCatalogController(QSettings& settings,
                                                   const bool useDirectProxy,
                                                   MiniAppCatalogEndpoint primaryEndpoint,
                                                   MiniAppCatalogEndpoint fallbackEndpoint,
                                                   const bool networkEnabled,
                                                   QObject* parent)
    : QObject(parent),
      m_impl(std::make_unique<Impl>(*this, settings, useDirectProxy,
                                    std::move(primaryEndpoint), std::move(fallbackEndpoint), networkEnabled))
{}

MiniAppCatalogController::~MiniAppCatalogController() = default;

QVariantList MiniAppCatalogController::explorerApps() const { return m_impl->explorerApps; }
QVariantList MiniAppCatalogController::myApps() const { return m_impl->myApps; }
bool MiniAppCatalogController::busy() const noexcept { return m_impl->busy; }
QString MiniAppCatalogController::errorText() const { return m_impl->errorText; }
QString MiniAppCatalogController::providerLabel() const { return m_impl->providerLabel; }
QString MiniAppCatalogController::query() const { return m_impl->query; }
bool MiniAppCatalogController::remoteSearchAvailable() const noexcept { return m_impl->hasRemoteSearch(); }
bool MiniAppCatalogController::personalizedDiscoveryAvailable() const noexcept { return m_impl->hasRelevantDiscovery(); }
void MiniAppCatalogController::refresh() { m_impl->refresh(); }
void MiniAppCatalogController::search(const QString& value) { m_impl->search(value); }
void MiniAppCatalogController::refreshRelevant(const qulonglong viewerFid) { m_impl->refreshRelevant(viewerFid); }
void MiniAppCatalogController::searchByFid(const qulonglong authorFid) { m_impl->searchByFid(authorFid); }
void MiniAppCatalogController::addToMyMiniApps(const QVariantMap& app) { m_impl->add(app); }
void MiniAppCatalogController::removeFromMyMiniApps(const QString& homeUrl) { m_impl->remove(homeUrl); }
