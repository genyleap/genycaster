/*!
 * @file        ui/app/miniapp/catalog.hpp
 * @brief       Mini App catalog and discovery controller.
 *
 * @details
 * Exposes app metadata and saved catalog state to Qt while keeping remote
 * catalog parsing and permission policies outside QML.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC requires a QObject declaration.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

class QSettings;

/** @brief Wire representation returned by a Mini App catalogue endpoint. */
enum class MiniAppCatalogFormat {
    Json,
    FarcasterDirectoryHtml,
};

/** @brief Provider-neutral HTTP endpoint configuration supplied by the composition root. */
struct MiniAppCatalogEndpoint final {
    QString catalogueUrl{};
    QString searchUrl{};
    QString relevantUrl{};
    QByteArray authorizationHeaderName{};
    QByteArray authorizationHeaderValue{};
    QString providerLabel{};
    MiniAppCatalogFormat format{MiniAppCatalogFormat::Json};

    /** @brief Returns true when the endpoint has a catalogue URL and any required credential. */
    [[nodiscard]] bool available() const noexcept
    {
        return !catalogueUrl.trimmed().isEmpty()
            && (authorizationHeaderName.isEmpty() || !authorizationHeaderValue.isEmpty());
    }

    /** @brief Returns true when remote search is configured and any required credential is present. */
    [[nodiscard]] bool searchAvailable() const noexcept
    {
        return !searchUrl.trimmed().isEmpty()
            && (authorizationHeaderName.isEmpty() || !authorizationHeaderValue.isEmpty());
    }

    /** @brief Returns true when personalized discovery is configured and authorized. */
    [[nodiscard]] bool relevantAvailable() const noexcept
    {
        return !relevantUrl.trimmed().isEmpty()
            && (authorizationHeaderName.isEmpty() || !authorizationHeaderValue.isEmpty());
    }
};

/**
 * @brief Composition-owned Mini App discovery and local collection controller.
 *
 * Discovery uses provider-neutral endpoint descriptors supplied by the executable
 * composition root. Secrets and network replies never cross into QML.
 * "My MiniApps" is Genycaster-local state. It backs the host-mediated
 * add/save consent path but does not claim remote notification or webhook sync.
 */
class MiniAppCatalogController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList explorerApps READ explorerApps NOTIFY explorerAppsChanged FINAL)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged FINAL)
    Q_PROPERTY(QVariantList myApps READ myApps NOTIFY myAppsChanged FINAL)
    Q_PROPERTY(QVariantList recentApps READ recentApps NOTIFY recentAppsChanged FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged FINAL)
    Q_PROPERTY(QString providerLabel READ providerLabel NOTIFY providerLabelChanged FINAL)
    Q_PROPERTY(QString query READ query NOTIFY queryChanged FINAL)
    Q_PROPERTY(bool remoteSearchAvailable READ remoteSearchAvailable CONSTANT FINAL)
    Q_PROPERTY(bool personalizedDiscoveryAvailable READ personalizedDiscoveryAvailable CONSTANT FINAL)

public:
    MiniAppCatalogController(QSettings& settings,
                             bool useDirectProxy,
                             MiniAppCatalogEndpoint primaryEndpoint,
                             MiniAppCatalogEndpoint fallbackEndpoint,
                             bool networkEnabled,
                             QObject* parent = nullptr);
    ~MiniAppCatalogController() override;

    [[nodiscard]] QVariantList explorerApps() const;
    [[nodiscard]] bool hasMore() const noexcept;
    [[nodiscard]] QVariantList myApps() const;
    [[nodiscard]] QVariantList recentApps() const;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QString providerLabel() const;
    [[nodiscard]] QString query() const;
    [[nodiscard]] bool remoteSearchAvailable() const noexcept;
    [[nodiscard]] bool personalizedDiscoveryAvailable() const noexcept;

    /** @brief Refreshes the protocol catalogue using the configured provider chain. */
    Q_INVOKABLE void refresh();

    /** @brief Loads the next bounded provider catalogue page, preserving prior results. */
    Q_INVOKABLE void loadMore();

    /** @brief Searches remotely when supported, otherwise filters the cached catalogue locally. */
    Q_INVOKABLE void search(const QString& query);

    /** @brief Fetches Mini Apps personalized for a Farcaster viewer FID when the provider supports it. */
    Q_INVOKABLE void refreshRelevant(qulonglong viewerFid);

    /** @brief Filters the current catalogue to apps authored by the supplied FID. */
    Q_INVOKABLE void searchByFid(qulonglong authorFid);

    /** @brief Saves a normalized app row into Genycaster's local Mini App collection. */
    Q_INVOKABLE void addToMyMiniApps(const QVariantMap& app);

    /** @brief Removes a saved app by its stable home URL. */
    Q_INVOKABLE void removeFromMyMiniApps(const QString& homeUrl);

    /** @brief Records only an actual user-triggered local Mini App launch. */
    Q_INVOKABLE void recordRecentUse(const QVariantMap& app);

signals:
    void explorerAppsChanged();
    void hasMoreChanged();
    void myAppsChanged();
    void recentAppsChanged();
    void busyChanged();
    void errorTextChanged();
    void providerLabelChanged();
    void queryChanged();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
