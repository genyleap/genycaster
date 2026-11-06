/*!
 * @file        ui/app/live_insights_controller.hpp
 * @brief       Live market and token insights controller.
 *
 * @details
 * Connects optional market snapshots, token holdings and profile signals
 * to localized Qt widgets without affecting Farcaster writes.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

class QSettings;

/**
 * @brief Qt composition boundary for live market/token insights and profile-score fallback.
 *
 * QML only observes cached values and requests refreshes. All network I/O remains
 * in C++ provider adapters and runs away from the GUI thread.
 */
class LiveInsightsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList marketAssets READ marketAssets NOTIFY marketAssetsChanged FINAL)
    Q_PROPERTY(QVariantList marketCatalog READ marketCatalog NOTIFY marketCatalogChanged FINAL)
    Q_PROPERTY(bool marketRefreshing READ marketRefreshing NOTIFY marketRefreshingChanged FINAL)
    Q_PROPERTY(QString marketStatus READ marketStatus NOTIFY marketStatusChanged FINAL)
    Q_PROPERTY(int marketPriceDecimals READ marketPriceDecimals WRITE setMarketPriceDecimals NOTIFY marketPriceDecimalsChanged FINAL)
    Q_PROPERTY(bool providerAvailable READ providerAvailable CONSTANT FINAL)
    Q_PROPERTY(QVariantMap genyHolding READ genyHolding NOTIFY genyHoldingChanged FINAL)
    Q_PROPERTY(QStringList contextWidgetOrder READ contextWidgetOrder NOTIFY contextWidgetOrderChanged FINAL)

public:
    LiveInsightsController(QSettings& settings, bool useDirectProxy, bool networkEnabled, QObject* parent = nullptr);
    ~LiveInsightsController() override;

    [[nodiscard]] QVariantList marketAssets() const;
    [[nodiscard]] QVariantList marketCatalog() const;
    [[nodiscard]] bool marketRefreshing() const;
    [[nodiscard]] QString marketStatus() const;

    /** @brief Returns the maximum decimal precision used for sub-dollar market prices. */
    [[nodiscard]] int marketPriceDecimals() const;
    [[nodiscard]] bool providerAvailable() const;
    [[nodiscard]] QVariantMap genyHolding() const;
    [[nodiscard]] QStringList contextWidgetOrder() const;

    Q_INVOKABLE void refreshMarket();

    /** @brief Formats a USD market price without collapsing positive micro-prices to zero. */
    Q_INVOKABLE QString formatMarketPrice(double value) const;

    /**
     * @brief Sets and persists the maximum decimal precision for sub-dollar market prices.
     * @param decimals Requested precision; values are clamped to the supported range [2, 12].
     */
    Q_INVOKABLE void setMarketPriceDecimals(int decimals);
    // Server-verified entitlement controls displayed market slots. Not QML
    // invokable: caller-supplied account decorations cannot upgrade the cap.
    void setProMarketLimit(int limit);
    Q_INVOKABLE void setAssetEnabled(const QString& key, bool enabled);
    Q_INVOKABLE bool addCustomToken(const QString& network, const QString& address);
    Q_INVOKABLE void removeCustomToken(const QString& key);
    Q_INVOKABLE void moveAsset(const QString& key, int delta);
    Q_INVOKABLE void setWalletAddress(const QString& address);
    Q_INVOKABLE void setWalletAddresses(const QStringList& addresses);
    Q_INVOKABLE void refreshGenyHolding();
    Q_INVOKABLE void moveContextWidget(const QString& key, int targetIndex);

    Q_INVOKABLE QString tokenLocatorForText(const QString& text) const;
    Q_INVOKABLE QVariantMap tokenPreview(const QString& locator) const;
    Q_INVOKABLE void requestToken(const QString& locator);

    Q_INVOKABLE double profileScoreForFid(qulonglong fid) const;
    Q_INVOKABLE QString profileScoreSourceForFid(qulonglong fid) const;
    Q_INVOKABLE void requestProfileScore(qulonglong fid);

signals:
    void marketAssetsChanged();
    void marketCatalogChanged();
    void marketRefreshingChanged();
    void marketStatusChanged();
    void marketPriceDecimalsChanged();
    void genyHoldingChanged();
    void contextWidgetOrderChanged();
    void tokenPreviewChanged(const QString& locator);
    void profileScoreChanged(qulonglong fid);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
