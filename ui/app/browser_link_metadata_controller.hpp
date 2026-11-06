/*!
 * @file        ui/app/browser_link_metadata_controller.hpp
 * @brief       Browser link preview metadata controller.
 *
 * @details
 * Performs bounded asynchronous link metadata retrieval for QML-facing
 * previews without granting web content arbitrary network authority.
 *
 * @author      Kambiz Asadzadeh
 * @since       20 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class QNetworkReply;

/**
 * @brief Optional browser-side link and Mini App relay adapter.
 *
 * All HTTP/JSON and provider-specific endpoint behavior stays outside QML,
 * application and domain. Requests with identical canonical URL and kind
 * share work and cached metadata. Failed lookups deliver empty maps and retain
 * the ability to retry after the bounded unavailable cooldown.
 */
class BrowserLinkMetadataController final : public QObject {
    Q_OBJECT
public:
    explicit BrowserLinkMetadataController(bool useDirectProxy, bool enabled,
        QObject* parent = nullptr, QUrl relayBase = {});
    ~BrowserLinkMetadataController() override;

    /** @brief Enqueue link or miniapp metadata, with bounded admission. */
    Q_INVOKABLE void requestPreview(const QString& pageUrl, const QString& kind);
    /** @brief Read fresh verified metadata for the exact URL and kind. */
    Q_INVOKABLE [[nodiscard]] QVariantMap previewFor(
        const QString& pageUrl, const QString& kind) const;

signals:
    void previewReady(const QString& pageUrl, const QString& kind,
                      const QVariantMap& preview);

private:
    struct Request final {
        QString url{};
        QString kind{};
        QString key{};
    };
    struct CacheEntry final {
        QVariantMap data{};
        qint64 freshUntil{};
    };
    [[nodiscard]] static Request normalized(const QString& pageUrl, const QString& kind);
    void dispatch();
    void complete(QNetworkReply* reply, Request request);
    void store(const QString& key, QVariantMap data, qint64 ttl);

    QNetworkAccessManager m_network{};
    QUrl m_relayBase{};
    QQueue<Request> m_pending{};
    QHash<QString, CacheEntry> m_cache{};
    QHash<QString, QNetworkReply*> m_active{};
    QElapsedTimer m_clock{};
    bool m_enabled{};
};
