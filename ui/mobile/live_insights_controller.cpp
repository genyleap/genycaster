#include "live_insights_controller.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#if GENYCASTER_HAS_PUBLIC_DATA_PROVIDER
import farcaster.capability.profile_quality;
import farcaster.domain.identity;
import farcaster.domain.market;
import farcaster.provider.public_data.config;
import farcaster.provider.public_data.market;
import farcaster.provider.public_data.holdings;
import farcaster.provider.public_data.score;
import farcaster.transport.qtnetwork;
#endif

namespace {

constexpr auto marketRefreshInterval = std::chrono::minutes{2};
constexpr auto automaticInitialRefreshDelay = std::chrono::milliseconds{650};
constexpr auto tokenCacheFreshness = std::chrono::minutes{2};
constexpr auto scoreCacheFreshness = std::chrono::hours{8};

[[nodiscard]] QString normalizedNetwork(QString network)
{
    network = network.trimmed().toLower();
    if (network == QStringLiteral("ethereum")) return QStringLiteral("eth");
    if (network == QStringLiteral("base") || network == QStringLiteral("eth")) return network;
    return {};
}

[[nodiscard]] QString normalizedAddress(QString address)
{
    address = address.trimmed();
    static const QRegularExpression evmAddress{QStringLiteral(R"(^0x[0-9a-fA-F]{40}$)")};
    return evmAddress.match(address).hasMatch() ? address.toLower() : QString{};
}

[[nodiscard]] QString remoteImageSource(const QString& rawUrl)
{
    const QString normalized = rawUrl.trimmed();
    if (normalized.isEmpty() || normalized.startsWith(QStringLiteral("image://")) ||
        normalized.startsWith(QStringLiteral("qrc:/")) || normalized.startsWith(QStringLiteral("file:/"))) {
        return normalized;
    }
    const QUrl parsed{normalized};
    if (!parsed.isValid() || parsed.host().isEmpty() ||
        (parsed.scheme() != QStringLiteral("https") && parsed.scheme() != QStringLiteral("http"))) {
        return {};
    }
    const QByteArray encoded = normalized.toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("image://remote/") + QString::fromLatin1(encoded);
}

[[nodiscard]] QVariantList toVariantList(const std::vector<double>& values)
{
    QVariantList result{};
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const double value : values) result.push_back(value);
    return result;
}

[[nodiscard]] QString shortenedContract(const QString& address)
{
    return address.size() > 13 ? address.left(7) + QStringLiteral("…") + address.right(4) : address;
}

[[nodiscard]] QVariantMap builtInAsset(
    const QString& key,
    const QString& name,
    const QString& symbol,
    const QString& network = {},
    const QString& address = {},
    const QString& imageSource = {})
{
    return {
        {QStringLiteral("key"), key},
        {QStringLiteral("name"), name},
        {QStringLiteral("symbol"), symbol},
        {QStringLiteral("network"), network},
        {QStringLiteral("address"), address},
        {QStringLiteral("imageSource"), imageSource},
        {QStringLiteral("custom"), false},
        {QStringLiteral("enabled"), true},
    };
}

} // namespace

class LiveInsightsController::Impl final {
public:
    struct CachedMap final {
        QVariantMap value{};
        QDateTime observedAt{};
    };

    Impl(LiveInsightsController& owner, QSettings& settings, const bool useDirectProxy, const bool networkEnabled)
        : q(owner), settings(settings), useDirectProxy(useDirectProxy), networkEnabled(networkEnabled)
    {
        catalog = {
            builtInAsset(
                QStringLiteral("bitcoin"), QStringLiteral("Bitcoin"), QStringLiteral("BTC"), {}, {},
                QStringLiteral("qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/crypto/bitcoin.svg")),
            builtInAsset(
                QStringLiteral("ethereum"), QStringLiteral("Ethereum"), QStringLiteral("ETH"), {}, {},
                QStringLiteral("qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/crypto/ethereum.svg")),
            builtInAsset(
                QStringLiteral("geny"), QStringLiteral("Genyleap"), QStringLiteral("GENY"),
                QStringLiteral("base"), QStringLiteral("0x2a3d6f8c1fc4acdcf3a75d19b445bae02f03676b"),
                QStringLiteral("qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/geny.svg")),
            builtInAsset(
                QStringLiteral("snap"), QStringLiteral("SNAP"), QStringLiteral("SNAP"),
                QStringLiteral("eth"), QStringLiteral("0x49b5a631f54927c0007232844f06fe18cbf69786")),
        };
        restoreSettings();
        rebuildAssets();

        refreshTimer.setInterval(static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(marketRefreshInterval).count()));
        QObject::connect(&refreshTimer, &QTimer::timeout, &q, [&] { refreshMarket(); });
        if (networkEnabled && providerAvailable()) {
            refreshTimer.start();
            QTimer::singleShot(static_cast<int>(automaticInitialRefreshDelay.count()), &q, [&] { refreshMarket(); });
        } else {
            status = providerAvailable()
                ? QStringLiteral("Live data paused in verification mode")
                : QStringLiteral("Live market provider is disabled in this build");
        }
    }

    [[nodiscard]] bool providerAvailable() const noexcept
    {
#if GENYCASTER_HAS_PUBLIC_DATA_PROVIDER
        return true;
#else
        return false;
#endif
    }

    void restoreSettings()
    {
        marketPriceDecimals = std::clamp(
            settings.value(QStringLiteral("marketPulse/priceDecimals"), 8).toInt(), 2, 12);

        const QStringList storedEnabled = settings.value(QStringLiteral("marketPulse/enabledAssets")).toStringList();
        const bool hasStoredEnabled = settings.contains(QStringLiteral("marketPulse/enabledAssets"));
        for (QVariant& value : catalog) {
            QVariantMap row = value.toMap();
            row.insert(QStringLiteral("enabled"), hasStoredEnabled
                ? storedEnabled.contains(row.value(QStringLiteral("key")).toString()) : true);
            value = row;
        }

        const QVariantList custom = settings.value(QStringLiteral("marketPulse/customTokens")).toList();
        for (const QVariant& value : custom) {
            const QVariantMap saved = value.toMap();
            const QString network = normalizedNetwork(saved.value(QStringLiteral("network")).toString());
            const QString address = normalizedAddress(saved.value(QStringLiteral("address")).toString());
            if (network.isEmpty() || address.isEmpty()) continue;
            const QString key = QStringLiteral("custom:") + network + QLatin1Char(':') + address;
            catalog.push_back(QVariantMap{
                {QStringLiteral("key"), key},
                {QStringLiteral("name"), saved.value(QStringLiteral("name")).toString()},
                {QStringLiteral("symbol"), saved.value(QStringLiteral("symbol")).toString()},
                {QStringLiteral("network"), network},
                {QStringLiteral("address"), address},
                {QStringLiteral("custom"), true},
                {QStringLiteral("enabled"), saved.value(QStringLiteral("enabled"), true).toBool()},
            });
        }
        const QStringList storedOrder = settings.value(QStringLiteral("marketPulse/order")).toStringList();
        if (!storedOrder.isEmpty()) {
            QVariantList ordered{};
            QSet<QString> used{};
            for (const QString& key : storedOrder) {
                for (const QVariant& value : catalog) {
                    const QVariantMap row = value.toMap();
                    if (row.value(QStringLiteral("key")).toString() == key && !used.contains(key)) {
                        ordered.push_back(value);
                        used.insert(key);
                        break;
                    }
                }
            }
            for (const QVariant& value : catalog) {
                const QString key = value.toMap().value(QStringLiteral("key")).toString();
                if (!used.contains(key)) ordered.push_back(value);
            }
            catalog = std::move(ordered);
        }

        const QStringList defaults{QStringLiteral("assets"), QStringLiteral("pulse"), QStringLiteral("calendar"),
                                   QStringLiteral("market"), QStringLiteral("suggested")};
        const QStringList storedWidgets = settings.value(QStringLiteral("contextRail/widgetOrder")).toStringList();
        contextWidgets.clear();
        for (const QString& key : storedWidgets) {
            if (defaults.contains(key) && !contextWidgets.contains(key)) contextWidgets.push_back(key);
        }
        for (const QString& key : defaults) {
            if (!contextWidgets.contains(key)) contextWidgets.push_back(key);
        }
    }

    void persistSettings()
    {
        QStringList enabled{};
        QVariantList custom{};
        for (const QVariant& value : catalog) {
            const QVariantMap row = value.toMap();
            if (row.value(QStringLiteral("enabled")).toBool()) enabled.push_back(row.value(QStringLiteral("key")).toString());
            if (row.value(QStringLiteral("custom")).toBool()) {
                custom.push_back(QVariantMap{
                    {QStringLiteral("network"), row.value(QStringLiteral("network"))},
                    {QStringLiteral("address"), row.value(QStringLiteral("address"))},
                    {QStringLiteral("name"), row.value(QStringLiteral("name"))},
                    {QStringLiteral("symbol"), row.value(QStringLiteral("symbol"))},
                    {QStringLiteral("enabled"), row.value(QStringLiteral("enabled"))},
                });
            }
        }
        QStringList order{};
        for (const QVariant& value : catalog) order.push_back(value.toMap().value(QStringLiteral("key")).toString());
        settings.setValue(QStringLiteral("marketPulse/enabledAssets"), enabled);
        settings.setValue(QStringLiteral("marketPulse/customTokens"), custom);
        settings.setValue(QStringLiteral("marketPulse/order"), order);
        settings.setValue(QStringLiteral("marketPulse/priceDecimals"), marketPriceDecimals);
        settings.setValue(QStringLiteral("contextRail/widgetOrder"), contextWidgets);
    }

    [[nodiscard]] QVariantMap initialPresentation(QVariantMap row) const
    {
        row.insert(QStringLiteral("available"), false);
        row.insert(QStringLiteral("loading"), networkEnabled && providerAvailable());
        row.insert(QStringLiteral("stale"), false);
        row.insert(QStringLiteral("priceUsd"), 0.0);
        row.insert(QStringLiteral("change24h"), 0.0);
        row.insert(QStringLiteral("marketCapUsd"), 0.0);
        row.insert(QStringLiteral("liquidityUsd"), 0.0);
        row.insert(QStringLiteral("volume24hUsd"), 0.0);
        if (!row.contains(QStringLiteral("imageSource"))) {
            row.insert(QStringLiteral("imageSource"), QString{});
        }
        row.insert(QStringLiteral("sparkline"), QVariantList{});
        row.insert(QStringLiteral("source"), QString{});
        row.insert(QStringLiteral("observedAtMs"), 0LL);
        return row;
    }

    void rebuildAssets()
    {
        QVariantList next{};
        for (const QVariant& value : catalog) {
            QVariantMap row = value.toMap();
            if (!row.value(QStringLiteral("enabled")).toBool()) continue;
            // Preserve saved catalog entries, but never display or fetch
            // more than the verified active entitlement permits.
            if (next.size() >= marketAssetLimit) break;
            const QString key = row.value(QStringLiteral("key")).toString();
            bool reused = false;
            for (const QVariant& oldValue : assets) {
                const QVariantMap old = oldValue.toMap();
                if (old.value(QStringLiteral("key")).toString() == key) {
                    next.push_back(old);
                    reused = true;
                    break;
                }
            }
            if (!reused) next.push_back(initialPresentation(std::move(row)));
        }
        assets = std::move(next);
        emit q.marketAssetsChanged();
    }

#if GENYCASTER_HAS_PUBLIC_DATA_PROVIDER
    [[nodiscard]] std::vector<farcaster::domain::market::MarketAsset> configuredAssets() const
    {
        std::vector<farcaster::domain::market::MarketAsset> result{};
        result.reserve(static_cast<std::size_t>(assets.size()));
        for (const QVariant& value : assets) {
            const QVariantMap row = value.toMap();
            farcaster::domain::market::MarketAsset asset{};
            asset.key = row.value(QStringLiteral("key")).toString().toStdString();
            if (!row.value(QStringLiteral("custom")).toBool()) {
                asset.symbol = row.value(QStringLiteral("symbol")).toString().toStdString();
                asset.displayName = row.value(QStringLiteral("name")).toString().toStdString();
            }
            const QString network = row.value(QStringLiteral("network")).toString();
            const QString address = row.value(QStringLiteral("address")).toString();
            if (!network.isEmpty() && !address.isEmpty()) {
                asset.token = farcaster::domain::market::TokenReference{
                    .network = network.toStdString(), .address = address.toStdString()};
            }
            result.push_back(std::move(asset));
        }
        return result;
    }

    [[nodiscard]] static QVariantMap presentation(const farcaster::domain::market::MarketSnapshot& snapshot)
    {
        QVariantMap row{};
        row.insert(QStringLiteral("key"), QString::fromStdString(snapshot.asset.key));
        row.insert(QStringLiteral("name"), QString::fromStdString(snapshot.asset.displayName));
        row.insert(QStringLiteral("symbol"), QString::fromStdString(snapshot.asset.symbol));
        row.insert(QStringLiteral("available"), true);
        row.insert(QStringLiteral("loading"), false);
        row.insert(QStringLiteral("stale"), false);
        row.insert(QStringLiteral("priceUsd"), snapshot.priceUsd);
        row.insert(QStringLiteral("change24h"), snapshot.change24hPercent);
        row.insert(QStringLiteral("marketCapUsd"), snapshot.marketCapUsd.value_or(0.0));
        row.insert(QStringLiteral("fdvUsd"), snapshot.fullyDilutedValuationUsd.value_or(0.0));
        row.insert(QStringLiteral("liquidityUsd"), snapshot.liquidityUsd.value_or(0.0));
        row.insert(QStringLiteral("volume24hUsd"), snapshot.volume24hUsd.value_or(0.0));
        row.insert(QStringLiteral("buyers24h"), static_cast<qulonglong>(snapshot.buyers24h.value_or(0)));
        row.insert(QStringLiteral("sellers24h"), static_cast<qulonglong>(snapshot.sellers24h.value_or(0)));
        row.insert(QStringLiteral("imageSource"), remoteImageSource(QString::fromStdString(snapshot.imageUrl)));
        row.insert(QStringLiteral("sparkline"), toVariantList(snapshot.sparkline));
        row.insert(QStringLiteral("source"), QString::fromStdString(snapshot.source));
        row.insert(QStringLiteral("observedAtMs"), QDateTime::currentMSecsSinceEpoch());
        if (snapshot.asset.token) {
            row.insert(QStringLiteral("network"), QString::fromStdString(snapshot.asset.token->network));
            row.insert(QStringLiteral("address"), QString::fromStdString(snapshot.asset.token->address));
            row.insert(QStringLiteral("locator"), QString::fromStdString(snapshot.asset.token->network + ":" + snapshot.asset.token->address));
        }
        if (snapshot.poolCreatedAt) {
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                snapshot.poolCreatedAt->time_since_epoch()).count();
            row.insert(QStringLiteral("poolCreatedAtMs"), static_cast<qlonglong>(milliseconds));
        }
        return row;
    }
#endif

    void refreshMarket()
    {
        if (!networkEnabled || !providerAvailable() || refreshing || assets.isEmpty()) return;
        refreshing = true;
        status = QStringLiteral("Refreshing live markets…");
        for (QVariant& value : assets) {
            QVariantMap row = value.toMap();
            row.insert(QStringLiteral("loading"), !row.value(QStringLiteral("available")).toBool());
            value = row;
        }
        emit q.marketRefreshingChanged();
        emit q.marketStatusChanged();
        emit q.marketAssetsChanged();

#if GENYCASTER_HAS_PUBLIC_DATA_PROVIDER
        const auto requestAssets = configuredAssets();
        const bool direct = useDirectProxy;
        QPointer<LiveInsightsController> guard{&q};
        (void)QtConcurrent::run([guard, requestAssets, direct] {
            auto transport = farcaster::transport::qtnetwork::makeTransport(
                direct ? farcaster::transport::qtnetwork::ProxyPolicy::Direct
                       : farcaster::transport::qtnetwork::ProxyPolicy::System);
            if (!transport) {
                if (!guard) return;
                QMetaObject::invokeMethod(guard.data(), [guard] {
                    if (guard) guard->m_impl->finishMarketFailure(QStringLiteral("Network transport unavailable"));
                }, Qt::QueuedConnection);
                return;
            }
            farcaster::provider::public_data::market::MarketDataSource source{**transport, {}};
            auto snapshots = source.snapshots(requestAssets);
            if (!snapshots) {
                const QString error = QString::fromStdString(snapshots.error().message);
                if (!guard) return;
                QMetaObject::invokeMethod(guard.data(), [guard, error] {
                    if (guard) guard->m_impl->finishMarketFailure(error);
                }, Qt::QueuedConnection);
                return;
            }
            QVariantList rows{};
            for (const auto& snapshot : *snapshots) rows.push_back(presentation(snapshot));
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard, rows = std::move(rows)]() mutable {
                if (guard) guard->m_impl->finishMarketSuccess(std::move(rows));
            }, Qt::QueuedConnection);
        });
#endif
    }

    void finishMarketFailure(const QString& error)
    {
        refreshing = false;
        for (QVariant& value : assets) {
            QVariantMap row = value.toMap();
            row.insert(QStringLiteral("loading"), false);
            row.insert(QStringLiteral("stale"), row.value(QStringLiteral("available")).toBool());
            value = row;
        }
        status = error.isEmpty() ? QStringLiteral("Live market data unavailable")
                                 : QStringLiteral("Live data unavailable · ") + error;
        emit q.marketRefreshingChanged();
        emit q.marketStatusChanged();
        emit q.marketAssetsChanged();
    }

    void finishMarketSuccess(QVariantList refreshed)
    {
        refreshing = false;
        for (QVariant& value : assets) {
            QVariantMap row = value.toMap();
            const QString key = row.value(QStringLiteral("key")).toString();
            bool found = false;
            for (const QVariant& newValue : refreshed) {
                QVariantMap incoming = newValue.toMap();
                if (incoming.value(QStringLiteral("key")).toString() != key) continue;
                const QString localImageSource = row.value(QStringLiteral("imageSource")).toString();
                for (auto it = incoming.cbegin(); it != incoming.cend(); ++it) row.insert(it.key(), it.value());
                if (row.value(QStringLiteral("imageSource")).toString().isEmpty() && !localImageSource.isEmpty()) {
                    row.insert(QStringLiteral("imageSource"), localImageSource);
                }
                // Preserve local catalog fields when provider metadata is empty.
                if (row.value(QStringLiteral("name")).toString().isEmpty()) row.insert(QStringLiteral("name"), shortenedContract(row.value(QStringLiteral("address")).toString()));
                if (row.value(QStringLiteral("symbol")).toString().isEmpty()) row.insert(QStringLiteral("symbol"), QStringLiteral("TOKEN"));
                found = true;
                break;
            }
            if (!found) {
                row.insert(QStringLiteral("loading"), false);
                row.insert(QStringLiteral("stale"), row.value(QStringLiteral("available")).toBool());
            } else if (row.value(QStringLiteral("custom")).toBool()) {
                updateCustomCatalogMetadata(key, row.value(QStringLiteral("name")).toString(), row.value(QStringLiteral("symbol")).toString());
            }
            value = row;
        }
        status = QStringLiteral("Live · CoinGecko + GeckoTerminal");
        emit q.marketRefreshingChanged();
        emit q.marketStatusChanged();
        emit q.marketAssetsChanged();
    }

    void updateCustomCatalogMetadata(const QString& key, const QString& name, const QString& symbol)
    {
        for (QVariant& value : catalog) {
            QVariantMap row = value.toMap();
            if (row.value(QStringLiteral("key")).toString() != key) continue;
            row.insert(QStringLiteral("name"), name);
            row.insert(QStringLiteral("symbol"), symbol);
            value = row;
            persistSettings();
            emit q.marketCatalogChanged();
            return;
        }
    }

    [[nodiscard]] QString tokenLocatorForText(QString text) const
    {
        text = QUrl::fromPercentEncoding(text.toUtf8());
        // Contract addresses inside ordinary web/NFT URLs are not fungible-token
        // hints. Strip URLs before semantic token detection to avoid classifying
        // SuperRare artwork paths such as /artwork/eth/0x… as ERC-20 cards.
        static const QRegularExpression webUrl{
            QStringLiteral(R"(https?://[^\s<>]+)"),
            QRegularExpression::CaseInsensitiveOption};
        text.remove(webUrl);
        static const QRegularExpression explicitLocator{
            QStringLiteral(R"((?:^|[^A-Za-z0-9])(?:eip155:)?(base|eth|ethereum)\s*[:/|]\s*(0x[0-9a-fA-F]{40}))"),
            QRegularExpression::CaseInsensitiveOption};
        auto match = explicitLocator.match(text);
        if (match.hasMatch()) {
            const QString network = normalizedNetwork(match.captured(1));
            const QString address = normalizedAddress(match.captured(2));
            if (!network.isEmpty() && !address.isEmpty()) return network + QLatin1Char(':') + address;
        }

        // Farcaster/Clanker link-preview titles commonly expose "base:0x…" while
        // the URL itself is opaque. A bare contract is only accepted when a Base
        // or Ethereum hint appears nearby, avoiding false positives in normal prose.
        static const QRegularExpression addressPattern{QStringLiteral(R"((0x[0-9a-fA-F]{40}))")};
        match = addressPattern.match(text);
        if (match.hasMatch()) {
            const QString lowerText = text.toLower();
            const QString network = lowerText.contains(QStringLiteral("base")) ? QStringLiteral("base")
                                  : lowerText.contains(QStringLiteral("ethereum")) || lowerText.contains(QStringLiteral("eth:"))
                                      ? QStringLiteral("eth") : QString{};
            if (!network.isEmpty()) return network + QLatin1Char(':') + normalizedAddress(match.captured(1));
        }

        // Cast bodies commonly identify fungible tokens by cashtag (for
        // example `$GENY`) without repeating the contract address. Resolve only
        // symbols that are already present in the local market catalog and map
        // to exactly one canonical contract. Unknown or ambiguous cashtags stay
        // ordinary text; no provider lookup or symbol guessing occurs here.
        static const QRegularExpression cashtagPattern{
            QStringLiteral(R"((?:^|[^A-Za-z0-9_$])\$([A-Za-z][A-Za-z0-9]{1,14})(?=$|[^A-Za-z0-9_]))"),
            QRegularExpression::CaseInsensitiveOption};
        auto cashtags = cashtagPattern.globalMatch(text);
        while (cashtags.hasNext()) {
            const QString symbol = cashtags.next().captured(1).toUpper();
            QSet<QString> candidates{};
            for (const QVariant& value : catalog) {
                const QVariantMap row = value.toMap();
                if (row.value(QStringLiteral("symbol")).toString().trimmed().toUpper() != symbol)
                    continue;
                const QString network = normalizedNetwork(
                    row.value(QStringLiteral("network")).toString());
                const QString address = normalizedAddress(
                    row.value(QStringLiteral("address")).toString());
                if (!network.isEmpty() && !address.isEmpty())
                    candidates.insert(network + QLatin1Char(':') + address);
            }
            if (candidates.size() == 1)
                return *candidates.cbegin();
        }
        return {};
    }

    [[nodiscard]] std::optional<std::pair<QString, QString>> splitLocator(const QString& locator) const
    {
        const qsizetype separator = locator.indexOf(QLatin1Char(':'));
        if (separator <= 0) return std::nullopt;
        const QString network = normalizedNetwork(locator.left(separator));
        const QString address = normalizedAddress(locator.mid(separator + 1));
        if (network.isEmpty() || address.isEmpty()) return std::nullopt;
        return std::pair{network, address};
    }

    void requestToken(const QString& locator)
    {
        if (!networkEnabled || !providerAvailable()) return;
        const auto parsed = splitLocator(locator);
        if (!parsed) return;
        const QString canonical = parsed->first + QLatin1Char(':') + parsed->second;
        if (tokenInflight.contains(canonical)) return;
        // Failed upstream reads must not refetch on every QML notification.
        const auto failure = tokenFailureAt.constFind(canonical);
        if (failure != tokenFailureAt.cend()
            && failure.value().secsTo(QDateTime::currentDateTimeUtc()) < 120) return;
        const auto cached = tokenCache.constFind(canonical);
        if (cached != tokenCache.cend() && cached.value().observedAt.secsTo(QDateTime::currentDateTimeUtc()) < tokenCacheFreshness.count() * 60) {
            emit q.tokenPreviewChanged(canonical);
            return;
        }
        tokenInflight.insert(canonical);
#if GENYCASTER_HAS_PUBLIC_DATA_PROVIDER
        const bool direct = useDirectProxy;
        const QString network = parsed->first;
        const QString address = parsed->second;
        QPointer<LiveInsightsController> guard{&q};
        (void)QtConcurrent::run([guard, canonical, network, address, direct] {
            auto transport = farcaster::transport::qtnetwork::makeTransport(
                direct ? farcaster::transport::qtnetwork::ProxyPolicy::Direct
                       : farcaster::transport::qtnetwork::ProxyPolicy::System);
            if (!transport) {
                if (!guard) return;
                QMetaObject::invokeMethod(guard.data(), [guard, canonical] { if (guard) guard->m_impl->finishToken(canonical, {}); }, Qt::QueuedConnection);
                return;
            }
            farcaster::provider::public_data::market::MarketDataSource source{**transport, {}};
            auto snapshot = source.tokenSnapshot({.network = network.toStdString(), .address = address.toStdString()});
            QVariantMap row{};
            if (snapshot) row = presentation(*snapshot);
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard, canonical, row = std::move(row)]() mutable {
                if (guard) guard->m_impl->finishToken(canonical, std::move(row));
            }, Qt::QueuedConnection);
        });
#endif
    }

    void finishToken(const QString& locator, QVariantMap value)
    {
        tokenInflight.remove(locator);
        if (!value.isEmpty()) {
            tokenFailureAt.remove(locator);
            tokenCache.insert(locator, CachedMap{std::move(value), QDateTime::currentDateTimeUtc()});
        } else {
            tokenFailureAt.insert(locator, QDateTime::currentDateTimeUtc());
        }
        emit q.tokenPreviewChanged(locator);
    }

    void requestProfileScore(const qulonglong fid)
    {
        if (!networkEnabled || !providerAvailable() || fid == 0 || scoreInflight.contains(fid)) return;
        const auto cached = scoreCache.constFind(fid);
        if (cached != scoreCache.cend() && cached.value().observedAt.secsTo(QDateTime::currentDateTimeUtc()) < scoreCacheFreshness.count() * 3600) {
            emit q.profileScoreChanged(fid);
            return;
        }
        scoreInflight.insert(fid);
#if GENYCASTER_HAS_PUBLIC_DATA_PROVIDER
        const bool direct = useDirectProxy;
        QPointer<LiveInsightsController> guard{&q};
        (void)QtConcurrent::run([guard, fid, direct] {
            auto transport = farcaster::transport::qtnetwork::makeTransport(
                direct ? farcaster::transport::qtnetwork::ProxyPolicy::Direct
                       : farcaster::transport::qtnetwork::ProxyPolicy::System);
            QVariantMap value{};
            if (transport) {
                farcaster::provider::public_data::score::ProfileQualitySource source{**transport, {}};
                auto score = source.get(farcaster::domain::identity::Fid{static_cast<std::uint64_t>(fid)});
                if (score) {
                    value.insert(QStringLiteral("score"), score->value);
                    value.insert(QStringLiteral("source"), QString::fromStdString(score->source));
                }
            }
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard, fid, value = std::move(value)]() mutable {
                if (guard) guard->m_impl->finishProfileScore(fid, std::move(value));
            }, Qt::QueuedConnection);
        });
#endif
    }

    void finishProfileScore(const qulonglong fid, QVariantMap value)
    {
        scoreInflight.remove(fid);
        if (!value.isEmpty()) scoreCache.insert(fid, CachedMap{std::move(value), QDateTime::currentDateTimeUtc()});
        emit q.profileScoreChanged(fid);
    }

    LiveInsightsController& q;
    QSettings& settings;
    bool useDirectProxy{};
    bool networkEnabled{};
    QVariantList catalog{};
    QVariantList assets{};
    bool refreshing{};
    QString status{};
    int marketPriceDecimals{8};
    int marketAssetLimit{4};
    QStringList walletAddresses{};
    QVariantMap holding{};
    bool holdingRefreshing{};
    QStringList contextWidgets{};
    QTimer refreshTimer{};
    QHash<QString, CachedMap> tokenCache{};
    QHash<QString, QDateTime> tokenFailureAt{};
    QSet<QString> tokenInflight{};
    QHash<qulonglong, CachedMap> scoreCache{};
    QSet<qulonglong> scoreInflight{};
};

LiveInsightsController::LiveInsightsController(
    QSettings& settings,
    const bool useDirectProxy,
    const bool networkEnabled,
    QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(*this, settings, useDirectProxy, networkEnabled))
{
}

LiveInsightsController::~LiveInsightsController() = default;

QVariantList LiveInsightsController::marketAssets() const { return m_impl->assets; }
QVariantList LiveInsightsController::marketCatalog() const { return m_impl->catalog; }
bool LiveInsightsController::marketRefreshing() const { return m_impl->refreshing; }
QString LiveInsightsController::marketStatus() const { return m_impl->status; }
int LiveInsightsController::marketPriceDecimals() const { return m_impl->marketPriceDecimals; }
bool LiveInsightsController::providerAvailable() const { return m_impl->providerAvailable(); }
QVariantMap LiveInsightsController::genyHolding() const { return m_impl->holding; }
QStringList LiveInsightsController::contextWidgetOrder() const { return m_impl->contextWidgets; }
void LiveInsightsController::refreshMarket() { m_impl->refreshMarket(); }

QString LiveInsightsController::formatMarketPrice(const double value) const
{
    if (!std::isfinite(value) || value <= 0.0) {
        return QStringLiteral("—");
    }

    const int maximumDecimals = std::clamp(m_impl->marketPriceDecimals, 2, 12);
    if (value >= 1.0) {
        return QStringLiteral("$") + QString::number(value, 'f', 2);
    }

    const int leadingZeros = std::max(0, static_cast<int>(std::ceil(-std::log10(value))) - 1);
    const int decimals = std::min(maximumDecimals, std::max(2, leadingZeros + 4));
    const double rounded = std::round(value * std::pow(10.0, decimals)) / std::pow(10.0, decimals);
    if (rounded <= 0.0) {
        return QStringLiteral("$") + QString::number(value, 'e', std::min(6, std::max(2, maximumDecimals - 1)));
    }

    QString numeric = QString::number(value, 'f', decimals);
    while (numeric.contains(QLatin1Char('.')) && numeric.endsWith(QLatin1Char('0')) &&
           numeric.indexOf(QLatin1Char('.')) <= numeric.size() - 3) {
        numeric.chop(1);
    }
    return QStringLiteral("$") + numeric;
}

void LiveInsightsController::setMarketPriceDecimals(const int decimals)
{
    const int canonical = std::clamp(decimals, 2, 12);
    if (m_impl->marketPriceDecimals == canonical) return;
    m_impl->marketPriceDecimals = canonical;
    m_impl->persistSettings();
    emit marketPriceDecimalsChanged();
}

void LiveInsightsController::setProMarketLimit(const int limit)
{
    const int normalized = std::clamp(limit, 4, 16);
    if (m_impl->marketAssetLimit == normalized) return;
    m_impl->marketAssetLimit = normalized;
    m_impl->rebuildAssets();
    emit marketCatalogChanged();
    m_impl->refreshMarket();
}

void LiveInsightsController::setAssetEnabled(const QString& key, const bool enabled)
{
    if (enabled) {
        int current = 0;
        for (const QVariant& item : m_impl->catalog)
            if (item.toMap().value(QStringLiteral("enabled")).toBool()) ++current;
        if (current >= m_impl->marketAssetLimit) return;
    }
    bool changed = false;
    for (QVariant& value : m_impl->catalog) {
        QVariantMap row = value.toMap();
        if (row.value(QStringLiteral("key")).toString() != key) continue;
        if (row.value(QStringLiteral("enabled")).toBool() == enabled) return;
        row.insert(QStringLiteral("enabled"), enabled);
        value = row;
        changed = true;
        break;
    }
    if (!changed) return;
    m_impl->persistSettings();
    m_impl->rebuildAssets();
    emit marketCatalogChanged();
    m_impl->refreshMarket();
}

bool LiveInsightsController::addCustomToken(const QString& network, const QString& address)
{
    const QString normalizedChain = normalizedNetwork(network);
    const QString normalizedContract = normalizedAddress(address);
    if (normalizedChain.isEmpty() || normalizedContract.isEmpty()) return false;
    int enabledCount = 0;
    for (const QVariant& item : m_impl->catalog)
        if (item.toMap().value(QStringLiteral("enabled")).toBool()) ++enabledCount;
    if (enabledCount >= m_impl->marketAssetLimit) return false;
    const QString key = QStringLiteral("custom:") + normalizedChain + QLatin1Char(':') + normalizedContract;
    for (const QVariant& value : m_impl->catalog) {
        if (value.toMap().value(QStringLiteral("key")).toString() == key) return false;
    }
    m_impl->catalog.push_back(QVariantMap{
        {QStringLiteral("key"), key},
        {QStringLiteral("name"), shortenedContract(normalizedContract)},
        {QStringLiteral("symbol"), QStringLiteral("TOKEN")},
        {QStringLiteral("network"), normalizedChain},
        {QStringLiteral("address"), normalizedContract},
        {QStringLiteral("custom"), true},
        {QStringLiteral("enabled"), true},
    });
    m_impl->persistSettings();
    m_impl->rebuildAssets();
    emit marketCatalogChanged();
    m_impl->refreshMarket();
    return true;
}

void LiveInsightsController::removeCustomToken(const QString& key)
{
    for (qsizetype index = 0; index < m_impl->catalog.size(); ++index) {
        const QVariantMap row = m_impl->catalog[index].toMap();
        if (row.value(QStringLiteral("key")).toString() != key || !row.value(QStringLiteral("custom")).toBool()) continue;
        m_impl->catalog.removeAt(index);
        m_impl->persistSettings();
        m_impl->rebuildAssets();
        emit marketCatalogChanged();
        return;
    }
}

void LiveInsightsController::moveAsset(const QString& key, const int delta)
{
    if (delta == 0) return;
    qsizetype from = -1;
    for (qsizetype index = 0; index < m_impl->catalog.size(); ++index) {
        if (m_impl->catalog[index].toMap().value(QStringLiteral("key")).toString() == key) { from = index; break; }
    }
    if (from < 0) return;
    const qsizetype to = std::clamp<qsizetype>(from + delta, 0, m_impl->catalog.size() - 1);
    if (from == to) return;
    m_impl->catalog.move(from, to);
    m_impl->persistSettings();
    m_impl->rebuildAssets();
    emit marketCatalogChanged();
}

void LiveInsightsController::setWalletAddress(const QString& address)
{
    setWalletAddresses(QStringList{address});
}

void LiveInsightsController::setWalletAddresses(const QStringList& addresses)
{
    QStringList canonical{};
    for (const QString& address : addresses) {
        const QString normalized = normalizedAddress(address);
        if (!normalized.isEmpty() && !canonical.contains(normalized)) {
            canonical.push_back(normalized);
        }
    }
    if (m_impl->walletAddresses == canonical) return;
    m_impl->walletAddresses = canonical;
    m_impl->holding = QVariantMap{
        {QStringLiteral("wallet"), canonical.isEmpty() ? QString{} : canonical.front()},
        {QStringLiteral("wallets"), canonical},
        {QStringLiteral("addresses"), QVariantList{}},
        {QStringLiteral("available"), false},
        {QStringLiteral("partial"), false},
        {QStringLiteral("loading"), !canonical.isEmpty()},
        {QStringLiteral("balance"), 0.0},
        {QStringLiteral("holder"), false},
        {QStringLiteral("error"), QString{}},
    };
    emit genyHoldingChanged();
    if (!canonical.isEmpty()) refreshGenyHolding();
}

void LiveInsightsController::refreshGenyHolding()
{
    if (!m_impl->networkEnabled || !m_impl->providerAvailable() || m_impl->walletAddresses.isEmpty() || m_impl->holdingRefreshing) return;
    m_impl->holdingRefreshing = true;
    QVariantMap next = m_impl->holding;
    next.insert(QStringLiteral("loading"), true);
    next.insert(QStringLiteral("error"), QString{});
    m_impl->holding = next;
    emit genyHoldingChanged();
#if GENYCASTER_HAS_PUBLIC_DATA_PROVIDER
    const bool direct = m_impl->useDirectProxy;
    const QStringList wallets = m_impl->walletAddresses;
    QPointer<LiveInsightsController> guard{this};
    (void)QtConcurrent::run([guard, direct, wallets] {
        auto transport = farcaster::transport::qtnetwork::makeTransport(
            direct ? farcaster::transport::qtnetwork::ProxyPolicy::Direct
                   : farcaster::transport::qtnetwork::ProxyPolicy::System);
        QVariantMap value{
            {QStringLiteral("wallet"), wallets.isEmpty() ? QString{} : wallets.front()},
            {QStringLiteral("wallets"), wallets},
            {QStringLiteral("loading"), false},
        };
        QVariantList addressRows{};
        addressRows.reserve(wallets.size());
        double totalBalance = 0.0;
        qsizetype successCount = 0;
        QString firstError{};

        if (!transport) {
            value.insert(QStringLiteral("available"), false);
            value.insert(QStringLiteral("partial"), false);
            value.insert(QStringLiteral("addresses"), addressRows);
            value.insert(QStringLiteral("error"), QStringLiteral("Network transport unavailable"));
        } else {
            farcaster::provider::public_data::holdings::WalletAssetSource source{**transport};
            for (const QString& wallet : wallets) {
                QVariantMap row{
                    {QStringLiteral("wallet"), wallet},
                    {QStringLiteral("available"), false},
                    {QStringLiteral("balance"), 0.0},
                    {QStringLiteral("holder"), false},
                    {QStringLiteral("error"), QString{}},
                };
                auto holding = source.erc20Balance(
                    wallet.toStdString(), "0x2a3d6f8c1fc4acdcf3a75d19b445bae02f03676b");
                if (holding) {
                    ++successCount;
                    totalBalance += holding->balance;
                    row.insert(QStringLiteral("available"), true);
                    row.insert(QStringLiteral("balance"), holding->balance);
                    row.insert(QStringLiteral("decimals"), static_cast<int>(holding->decimals));
                    row.insert(QStringLiteral("holder"), holding->balance > 0.0);
                } else {
                    const QString error = QString::fromStdString(holding.error().message);
                    row.insert(QStringLiteral("error"), error);
                    if (firstError.isEmpty()) firstError = error;
                }
                addressRows.push_back(row);
            }
            value.insert(QStringLiteral("addresses"), addressRows);
            value.insert(QStringLiteral("available"), successCount > 0);
            value.insert(QStringLiteral("partial"), successCount > 0 && successCount < wallets.size());
            value.insert(QStringLiteral("balance"), totalBalance);
            value.insert(QStringLiteral("holder"), totalBalance > 0.0);
            value.insert(QStringLiteral("source"), QStringLiteral("Base RPC"));
            value.insert(QStringLiteral("error"), successCount == 0 ? firstError : QString{});
        }
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, value = std::move(value), wallets]() mutable {
            if (!guard || guard->m_impl->walletAddresses != wallets) return;
            guard->m_impl->holdingRefreshing = false;
            guard->m_impl->holding = std::move(value);
            emit guard->genyHoldingChanged();
        }, Qt::QueuedConnection);
    });
#else
    m_impl->holdingRefreshing = false;
    next.insert(QStringLiteral("loading"), false);
    next.insert(QStringLiteral("available"), false);
    next.insert(QStringLiteral("error"), QStringLiteral("Public data provider unavailable"));
    m_impl->holding = std::move(next);
    emit genyHoldingChanged();
#endif
}

void LiveInsightsController::moveContextWidget(const QString& key, const int targetIndex)
{
    const qsizetype from = m_impl->contextWidgets.indexOf(key);
    if (from < 0 || m_impl->contextWidgets.isEmpty()) return;
    const qsizetype to = std::clamp<qsizetype>(targetIndex, 0, m_impl->contextWidgets.size() - 1);
    if (from == to) return;
    m_impl->contextWidgets.move(from, to);
    m_impl->persistSettings();
    emit contextWidgetOrderChanged();
}

QString LiveInsightsController::tokenLocatorForText(const QString& text) const
{
    return m_impl->tokenLocatorForText(text);
}

QVariantMap LiveInsightsController::tokenPreview(const QString& locator) const
{
    const auto parsed = m_impl->splitLocator(locator);
    if (!parsed) return {};
    const QString canonical = parsed->first + QLatin1Char(':') + parsed->second;
    const auto found = m_impl->tokenCache.constFind(canonical);
    return found == m_impl->tokenCache.cend() ? QVariantMap{} : found.value().value;
}

void LiveInsightsController::requestToken(const QString& locator) { m_impl->requestToken(locator); }

double LiveInsightsController::profileScoreForFid(const qulonglong fid) const
{
    const auto found = m_impl->scoreCache.constFind(fid);
    return found == m_impl->scoreCache.cend() ? -1.0 : found.value().value.value(QStringLiteral("score"), -1.0).toDouble();
}

QString LiveInsightsController::profileScoreSourceForFid(const qulonglong fid) const
{
    const auto found = m_impl->scoreCache.constFind(fid);
    return found == m_impl->scoreCache.cend() ? QString{} : found.value().value.value(QStringLiteral("source")).toString();
}

void LiveInsightsController::requestProfileScore(const qulonglong fid) { m_impl->requestProfileScore(fid); }
