/*!
 * @file        ui/mobile/node_selection_controller.hpp
 * @brief       Farcaster node selection controller.
 *
 * @details
 * Manages user-selected node profiles and connectivity diagnostics with
 * operation-aware route eligibility and explicit fallback state.
 *
 * @author      Kambiz Asadzadeh
 * @since       04 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;
class QSettings;

/**
 * @brief Owns first-run read-node discovery and the persisted provider endpoint preference.
 *
 * The controller deliberately stays at the composition/presentation boundary. It never leaks
 * provider DTOs into domain/application code: it only probes configured HTTP origins, records
 * observed reachability/latency, and persists the endpoint that the next runtime composition
 * should use.
 */
class NodeSelectionController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList nodes READ nodes NOTIFY nodesChanged)
    Q_PROPERTY(QString selectedNodeId READ selectedNodeId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedEndpoint READ selectedEndpoint NOTIFY selectionChanged)
    Q_PROPERTY(QString snapchainWriteEndpoint READ snapchainWriteEndpoint NOTIFY selectionChanged)
    Q_PROPERTY(bool writeRestartRequired READ writeRestartRequired NOTIFY selectionChanged)
    Q_PROPERTY(bool automatic READ automatic NOTIFY selectionChanged)
    Q_PROPERTY(bool probing READ probing NOTIFY nodesChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY selectionChanged)
    Q_PROPERTY(QString proxyMode READ proxyMode NOTIFY selectionChanged)
    Q_PROPERTY(bool proxyRestartRequired READ proxyRestartRequired NOTIFY selectionChanged)

public:
    NodeSelectionController(
        QSettings& settings,
        QString appliedEndpoint,
        QString appliedWriteEndpoint,
        QStringList candidateEndpoints,
        QString appliedProxyMode,
        bool probingEnabled,
        QObject* parent = nullptr);
    ~NodeSelectionController() override;

    [[nodiscard]] QVariantList nodes() const;
    [[nodiscard]] QString selectedNodeId() const;
    [[nodiscard]] QString selectedEndpoint() const;
    [[nodiscard]] QString snapchainWriteEndpoint() const;
    [[nodiscard]] bool writeRestartRequired() const;
    Q_INVOKABLE bool setSnapchainWriteEndpoint(const QString& endpoint);
    [[nodiscard]] bool automatic() const noexcept;
    [[nodiscard]] bool probing() const noexcept;
    [[nodiscard]] bool restartRequired() const;
    [[nodiscard]] QString proxyMode() const;
    [[nodiscard]] bool proxyRestartRequired() const;
    Q_INVOKABLE bool setProxyMode(const QString& proxyMode);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool selectNode(const QString& nodeId);
    Q_INVOKABLE void useAutomaticSelection();

signals:
    void nodesChanged();
    void selectionChanged();

private:
    struct Node final {
        QString id;
        QString label;
        QString endpoint;
        int latencyMs{-1};
        QString status{QStringLiteral("idle")};
    };

    struct Probe final {
        qsizetype nodeIndex{-1};
        QElapsedTimer timer;
    };

    void persistSelection();
    void finishProbe(QNetworkReply* reply);
    void chooseRecommendedNode();
    [[nodiscard]] qsizetype indexForId(const QString& nodeId) const;
    [[nodiscard]] qsizetype selectedIndex() const;
    [[nodiscard]] qsizetype recommendedIndex() const;

    QSettings& m_settings;
    QString m_appliedEndpoint;
    QString m_appliedWriteEndpoint;
    QString m_snapchainWriteEndpoint;
    QString m_appliedProxyMode;
    QString m_proxyMode;
    QList<Node> m_nodes;
    QString m_selectedNodeId;
    bool m_automatic{true};
    bool m_probingEnabled{true};
    int m_pendingProbes{0};
    QNetworkAccessManager* m_network{nullptr};
    QHash<QNetworkReply*, Probe> m_probes;
};
