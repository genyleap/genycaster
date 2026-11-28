/*!
 * @file        ui/app/data/market.hpp
 * @brief       Exposes rate-limited, read-only market snapshots to Qt UI.
 *
 * @details
 * Owns transient network requests and snapshot state for the first-party
 * market aggregator without exposing transport operations through QML.
 *
 * @author      Kambiz Asadzadeh
 * @since       08 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>

class QNetworkReply;

// Read-only market snapshots served from a shared, rate-limited first-party
// aggregator. No third-party API keys or provider transport logic in QML.
class MarketController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed FINAL)
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY changed FINAL)
    Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY changed FINAL)
    Q_PROPERTY(bool partial READ partial NOTIFY changed FINAL)
    Q_PROPERTY(bool planConfigured READ planConfigured NOTIFY changed FINAL)
public:
    explicit MarketController(bool directProxy, bool networkEnabled, QObject* parent = nullptr);
    ~MarketController() override;
    QString state() const { return m_state; }
    QVariantMap metrics() const { return m_metrics; }
    QString updatedAt() const { return m_updatedAt; }
    bool partial() const { return m_partial; }
    bool planConfigured() const { return m_planConfigured; }
    Q_INVOKABLE bool refresh(bool force = false);
signals:
    void changed();
private:
    QNetworkAccessManager m_network{this};
    QPointer<QNetworkReply> m_reply{};
    QVariantMap m_metrics{};
    QString m_state{QStringLiteral("idle")};
    QString m_updatedAt{};
    bool m_partial{false};
    bool m_planConfigured{false};
    bool m_networkEnabled{false};
    qint64 m_lastRefreshMs{0};
    qint64 m_retryAtMs{0};
    quint64 m_epoch{0};
};
