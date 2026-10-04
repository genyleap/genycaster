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
    Q_PROPERTY(bool automatic READ automatic NOTIFY selectionChanged)
    Q_PROPERTY(bool probing READ probing NOTIFY nodesChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY selectionChanged)

public:
    NodeSelectionController(
        QSettings& settings,
        QString appliedEndpoint,
        QStringList candidateEndpoints,
        bool probingEnabled,
        QObject* parent = nullptr);
    ~NodeSelectionController() override;

    [[nodiscard]] QVariantList nodes() const;
    [[nodiscard]] QString selectedNodeId() const;
    [[nodiscard]] QString selectedEndpoint() const;
    [[nodiscard]] bool automatic() const noexcept;
    [[nodiscard]] bool probing() const noexcept;
    [[nodiscard]] bool restartRequired() const;

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
    QList<Node> m_nodes;
    QString m_selectedNodeId;
    bool m_automatic{true};
    bool m_probingEnabled{true};
    int m_pendingProbes{0};
    QNetworkAccessManager* m_network{nullptr};
    QHash<QNetworkReply*, Probe> m_probes;
};
