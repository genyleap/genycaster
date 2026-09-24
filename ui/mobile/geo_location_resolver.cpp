#include "geo_location_resolver.hpp"

#include <QByteArray>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkRequest>
#include <QQueue>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <utility>

namespace {

constexpr int requestSpacingMilliseconds = 1'100;
constexpr int requestTimeoutMilliseconds = 6'000;

struct GeoCoordinate final {
    double latitude{};
    double longitude{};
};

struct PendingRequest final {
    QString cacheKey;
    QString languageTag;
    GeoCoordinate coordinate{};
};

[[nodiscard]] std::optional<GeoCoordinate> parseGeoUri(const QString& location)
{
    const QString value = location.trimmed();
    if (!value.startsWith(QStringLiteral("geo:"), Qt::CaseInsensitive)) {
        return std::nullopt;
    }

    const QString coordinateText = value.sliced(4).section(QLatin1Char(';'), 0, 0);
    const QStringList parts = coordinateText.split(QLatin1Char(','), Qt::KeepEmptyParts);
    if (parts.size() != 2) {
        return std::nullopt;
    }

    bool latitudeOk = false;
    bool longitudeOk = false;
    const double latitude = parts.at(0).trimmed().toDouble(&latitudeOk);
    const double longitude = parts.at(1).trimmed().toDouble(&longitudeOk);
    if (!latitudeOk || !longitudeOk || !std::isfinite(latitude) || !std::isfinite(longitude)
        || latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
        return std::nullopt;
    }
    return GeoCoordinate{.latitude = latitude, .longitude = longitude};
}

[[nodiscard]] QString coordinateLabel(const GeoCoordinate coordinate, QString languageTag)
{
    languageTag.replace(QLatin1Char('-'), QLatin1Char('_'));
    const QLocale locale = languageTag.trimmed().isEmpty() ? QLocale::system() : QLocale{languageTag};
    const auto absolute = [&locale](const double value) {
        return locale.toString(std::abs(value), 'f', 2);
    };
    const QString northSouth = coordinate.latitude >= 0.0 ? QStringLiteral("N") : QStringLiteral("S");
    const QString eastWest = coordinate.longitude >= 0.0 ? QStringLiteral("E") : QStringLiteral("W");
    return QStringLiteral("%1° %2 · %3° %4")
        .arg(absolute(coordinate.latitude), northSouth, absolute(coordinate.longitude), eastWest);
}

[[nodiscard]] QString firstAddressValue(
    const QJsonObject& address,
    const std::initializer_list<const char*> keys)
{
    for (const char* const key : keys) {
        const QString value = address.value(QString::fromLatin1(key)).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

[[nodiscard]] QString placeLabelFrom(const QByteArray& payload)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonObject object = document.object();
    const QJsonObject address = object.value(QStringLiteral("address")).toObject();
    if (address.isEmpty()) {
        return object.value(QStringLiteral("display_name")).toString().trimmed();
    }

    const QString locality = firstAddressValue(address, {
        "city", "town", "village", "municipality", "borough", "suburb", "county"});
    const QString region = firstAddressValue(address, {"state", "region", "state_district"});
    const QString country = firstAddressValue(address, {"country"});

    QStringList parts{};
    const auto appendUnique = [&parts](const QString& value) {
        if (value.isEmpty()) {
            return;
        }
        const bool duplicate = std::ranges::any_of(parts, [&value](const QString& existing) {
            return existing.compare(value, Qt::CaseInsensitive) == 0;
        });
        if (!duplicate) {
            parts.push_back(value);
        }
    };

    appendUnique(locality);
    if (locality.isEmpty()) {
        appendUnique(region);
    }
    appendUnique(country);
    if (!parts.isEmpty()) {
        return parts.join(QStringLiteral(", "));
    }
    return object.value(QStringLiteral("display_name")).toString().trimmed();
}

} // namespace

class GeoLocationResolver::Impl final {
public:
    explicit Impl(GeoLocationResolver& owner, const bool useDirectProxy)
        : q(owner)
    {
        if (useDirectProxy) {
            network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
        } else {
            QNetworkProxyFactory::setUseSystemConfiguration(true);
        }
        spacingTimer.setSingleShot(true);
        QObject::connect(&spacingTimer, &QTimer::timeout, &q, [this] { startNext(); });
    }

    [[nodiscard]] QString displayLabel(const QString& location, QString languageTag)
    {
        const auto coordinate = parseGeoUri(location);
        if (!coordinate) {
            return location.trimmed();
        }

        languageTag = languageTag.trimmed().toLower();
        if (languageTag.isEmpty() || languageTag == QStringLiteral("system")) {
            languageTag = QLocale::system().bcp47Name();
        }
        const QString normalizedLocation = QStringLiteral("geo:%1,%2")
            .arg(QString::number(coordinate->latitude, 'f', 2),
                 QString::number(coordinate->longitude, 'f', 2));
        const QString cacheKey = normalizedLocation + QLatin1Char('|') + languageTag;
        const auto cached = labels.constFind(cacheKey);
        if (cached != labels.cend()) {
            return cached.value();
        }

        const QString fallback = coordinateLabel(*coordinate, languageTag);
        labels.insert(cacheKey, fallback);
        enqueue(PendingRequest{
            .cacheKey = cacheKey,
            .languageTag = languageTag,
            .coordinate = *coordinate,
        });
        return fallback;
    }

    void enqueue(PendingRequest request)
    {
        if (queuedKeys.contains(request.cacheKey) || activeKey == request.cacheKey) {
            return;
        }
        queuedKeys.insert(request.cacheKey);
        queue.enqueue(std::move(request));
        if (activeKey.isEmpty() && !spacingTimer.isActive()) {
            spacingTimer.start(0);
        }
    }

    void startNext()
    {
        if (!activeKey.isEmpty() || queue.isEmpty()) {
            return;
        }

        PendingRequest request = queue.dequeue();
        queuedKeys.remove(request.cacheKey);
        activeKey = request.cacheKey;

        QUrl url{QStringLiteral("https://nominatim.openstreetmap.org/reverse")};
        QUrlQuery query{};
        query.addQueryItem(QStringLiteral("format"), QStringLiteral("jsonv2"));
        query.addQueryItem(QStringLiteral("lat"), QString::number(request.coordinate.latitude, 'f', 2));
        query.addQueryItem(QStringLiteral("lon"), QString::number(request.coordinate.longitude, 'f', 2));
        query.addQueryItem(QStringLiteral("zoom"), QStringLiteral("10"));
        query.addQueryItem(QStringLiteral("addressdetails"), QStringLiteral("1"));
        query.addQueryItem(QStringLiteral("accept-language"), request.languageTag);
        url.setQuery(query);

        QNetworkRequest networkRequest{url};
        networkRequest.setHeader(
            QNetworkRequest::UserAgentHeader,
            QStringLiteral("Genycaster/alpha (https://github.com/genyleap)"));
        networkRequest.setRawHeader("Accept", "application/json");
        networkRequest.setTransferTimeout(requestTimeoutMilliseconds);

        QNetworkReply* const reply = network.get(networkRequest);
        QObject::connect(reply, &QNetworkReply::finished, &q, [this, reply, request = std::move(request)] {
            QString resolved{};
            if (reply->error() == QNetworkReply::NoError) {
                resolved = placeLabelFrom(reply->readAll());
            }
            reply->deleteLater();

            if (!resolved.isEmpty()) {
                const QString previous = labels.value(request.cacheKey);
                if (previous != resolved) {
                    labels.insert(request.cacheKey, std::move(resolved));
                    ++revision;
                    emit q.revisionChanged();
                }
            }

            activeKey.clear();
            if (!queue.isEmpty()) {
                spacingTimer.start(requestSpacingMilliseconds);
            }
        });
    }

    GeoLocationResolver& q;
    QNetworkAccessManager network{};
    QTimer spacingTimer{};
    QQueue<PendingRequest> queue{};
    QSet<QString> queuedKeys{};
    QHash<QString, QString> labels{};
    QString activeKey{};
    qulonglong revision{};
};

GeoLocationResolver::GeoLocationResolver(const bool useDirectProxy, QObject* const parent)
    : QObject(parent),
      m_impl(std::make_unique<Impl>(*this, useDirectProxy))
{
}

GeoLocationResolver::~GeoLocationResolver() = default;

qulonglong GeoLocationResolver::revision() const noexcept
{
    return m_impl->revision;
}

QString GeoLocationResolver::displayLabel(const QString& location, const QString& languageTag)
{
    return m_impl->displayLabel(location, languageTag);
}
