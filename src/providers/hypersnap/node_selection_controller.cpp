#include "../../../ui/mobile/node_selection_controller.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <limits>
#include <utility>

namespace {

constexpr auto kSelectionModeKey = "network/nodeSelectionMode";
constexpr auto kNodeIdKey = "network/hypersnapNodeId";
constexpr auto kNodeEndpointKey = "network/hypersnapBaseUrl";

[[nodiscard]] QString normalizedEndpoint(QString endpoint)
{
    endpoint = endpoint.trimmed();
    while (endpoint.endsWith(QLatin1Char('/'))) {
        endpoint.chop(1);
    }
    const QUrl url{endpoint};
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        return {};
    }
    return endpoint;
}

[[nodiscard]] QString nodeIdForIndex(const qsizetype index)
{
    return index == 0 ? QStringLiteral("hypersnap-public")
                      : QStringLiteral("hypersnap-candidate-%1").arg(index + 1);
}

} // namespace

NodeSelectionController::NodeSelectionController(
    QSettings& settings,
    QString appliedEndpoint,
    QStringList candidateEndpoints,
    QString appliedProxyMode,
    const bool probingEnabled,
    QObject* const parent)
    : QObject(parent),
      m_settings{settings},
      m_appliedEndpoint{normalizedEndpoint(std::move(appliedEndpoint))},
      m_appliedProxyMode{std::move(appliedProxyMode)},
      m_probingEnabled{probingEnabled},
      m_network{new QNetworkAccessManager(this)}
{
    m_proxyMode = m_settings.value(QStringLiteral("network/proxyMode"),
                                     m_appliedProxyMode).toString();
    if (m_proxyMode != QStringLiteral("system") &&
        m_proxyMode != QStringLiteral("direct")) {
        m_proxyMode = QStringLiteral("system");
    }
    if (m_appliedEndpoint.isEmpty()) {
        m_appliedEndpoint = QStringLiteral("https://haatz.quilibrium.com");
    }

    candidateEndpoints.prepend(QStringLiteral("https://haatz.quilibrium.com"));
    candidateEndpoints.prepend(m_appliedEndpoint);

    QStringList uniqueEndpoints{};
    for (QString endpoint : candidateEndpoints) {
        endpoint = normalizedEndpoint(std::move(endpoint));
        if (endpoint.isEmpty() || uniqueEndpoints.contains(endpoint, Qt::CaseInsensitive)) {
            continue;
        }
        uniqueEndpoints.push_back(endpoint);
    }

    for (qsizetype index = 0; index < uniqueEndpoints.size(); ++index) {
        const QString endpoint = uniqueEndpoints.at(index);
        const QUrl url{endpoint};
        const bool publicDefault = endpoint.compare(
            QStringLiteral("https://haatz.quilibrium.com"), Qt::CaseInsensitive) == 0;
        m_nodes.push_back(Node{
            .id = nodeIdForIndex(index),
            .label = publicDefault
                ? QStringLiteral("Hypersnap · Public")
                : QStringLiteral("Hypersnap · %1").arg(url.host()),
            .endpoint = endpoint,
        });
    }

    m_automatic = m_settings.value(
        QString::fromLatin1(kSelectionModeKey), QStringLiteral("auto")).toString()
        != QStringLiteral("manual");
    const QString savedId = m_settings.value(QString::fromLatin1(kNodeIdKey)).toString();
    const QString savedEndpoint = normalizedEndpoint(
        m_settings.value(QString::fromLatin1(kNodeEndpointKey)).toString());

    if (!savedEndpoint.isEmpty()) {
        const auto endpointIt = std::ranges::find_if(m_nodes, [&savedEndpoint](const Node& node) {
            return node.endpoint.compare(savedEndpoint, Qt::CaseInsensitive) == 0;
        });
        if (endpointIt != m_nodes.end()) {
            m_selectedNodeId = endpointIt->id;
        }
    }
    if (m_selectedNodeId.isEmpty() && indexForId(savedId) >= 0) {
        m_selectedNodeId = savedId;
    }
    if (m_selectedNodeId.isEmpty() && !m_nodes.isEmpty()) {
        const auto appliedIt = std::ranges::find_if(m_nodes, [this](const Node& node) {
            return node.endpoint.compare(m_appliedEndpoint, Qt::CaseInsensitive) == 0;
        });
        m_selectedNodeId = appliedIt != m_nodes.end() ? appliedIt->id : m_nodes.front().id;
    }

    persistSelection();
}

NodeSelectionController::~NodeSelectionController() = default;

QVariantList NodeSelectionController::nodes() const
{
    const qsizetype recommended = recommendedIndex();
    const qsizetype selected = selectedIndex();
    QVariantList result{};
    result.reserve(m_nodes.size());
    for (qsizetype index = 0; index < m_nodes.size(); ++index) {
        const Node& node = m_nodes.at(index);
        result.push_back(QVariantMap{
            {QStringLiteral("id"), node.id},
            {QStringLiteral("label"), node.label},
            {QStringLiteral("endpoint"), node.endpoint},
            {QStringLiteral("provider"), QStringLiteral("Hypersnap v2")},
            {QStringLiteral("latencyMs"), node.latencyMs},
            {QStringLiteral("status"), node.status},
            {QStringLiteral("online"), node.status == QStringLiteral("online")},
            {QStringLiteral("selected"), index == selected},
            {QStringLiteral("recommended"), index == recommended},
            {QStringLiteral("experimental"), index != 0},
        });
    }
    return result;
}

QString NodeSelectionController::selectedNodeId() const
{
    return m_selectedNodeId;
}

QString NodeSelectionController::selectedEndpoint() const
{
    const qsizetype index = selectedIndex();
    return index >= 0 ? m_nodes.at(index).endpoint : m_appliedEndpoint;
}

bool NodeSelectionController::automatic() const noexcept
{
    return m_automatic;
}

bool NodeSelectionController::probing() const noexcept
{
    return m_pendingProbes > 0;
}

bool NodeSelectionController::restartRequired() const
{
    return selectedEndpoint().compare(m_appliedEndpoint, Qt::CaseInsensitive) != 0;
}

QString NodeSelectionController::proxyMode() const
{
    return m_proxyMode;
}

bool NodeSelectionController::proxyRestartRequired() const
{
    return m_proxyMode != m_appliedProxyMode;
}

bool NodeSelectionController::setProxyMode(const QString& mode)
{
    if (mode != QStringLiteral("system") && mode != QStringLiteral("direct")) return false;
    if (m_proxyMode == mode) return true;
    m_proxyMode = mode;
    m_settings.setValue(QStringLiteral("network/proxyMode"), m_proxyMode);
    m_settings.sync();
    emit selectionChanged();
    return true;
}

void NodeSelectionController::refresh()
{
    for (auto it = m_probes.begin(); it != m_probes.end(); ++it) {
        if (it.key() != nullptr) {
            it.key()->abort();
        }
    }
    m_probes.clear();
    m_pendingProbes = 0;

    for (Node& node : m_nodes) {
        node.latencyMs = -1;
        node.status = m_probingEnabled ? QStringLiteral("checking") : QStringLiteral("idle");
    }
    emit nodesChanged();

    if (!m_probingEnabled) {
        return;
    }

    for (qsizetype index = 0; index < m_nodes.size(); ++index) {
        // The network selector is restricted to real Hypersnap nodes.
        // A HEAD on any host (including a Farcaster Hub or auth gateway)
        // is not proof that it supports the Hypersnap v2 read protocol.
        QNetworkRequest request{
            QUrl{m_nodes.at(index).endpoint + QStringLiteral("/v1/info")}};
        request.setTransferTimeout(2500);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* const reply = m_network->get(request);
        Probe probe{};
        probe.nodeIndex = index;
        probe.timer.start();
        m_probes.insert(reply, std::move(probe));
        ++m_pendingProbes;
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            finishProbe(reply);
        });
    }
    emit nodesChanged();
}

bool NodeSelectionController::selectNode(const QString& nodeId)
{
    if (indexForId(nodeId) < 0) {
        return false;
    }
    const bool changed = m_selectedNodeId != nodeId || m_automatic;
    m_selectedNodeId = nodeId;
    m_automatic = false;
    persistSelection();
    if (changed) {
        emit selectionChanged();
        emit nodesChanged();
    }
    return true;
}

void NodeSelectionController::useAutomaticSelection()
{
    const bool modeChanged = !m_automatic;
    m_automatic = true;
    chooseRecommendedNode();
    persistSelection();
    if (modeChanged) {
        emit selectionChanged();
        emit nodesChanged();
    }
}

void NodeSelectionController::persistSelection()
{
    m_settings.setValue(
        QString::fromLatin1(kSelectionModeKey),
        m_automatic ? QStringLiteral("auto") : QStringLiteral("manual"));
    m_settings.setValue(QString::fromLatin1(kNodeIdKey), m_selectedNodeId);
    m_settings.setValue(QString::fromLatin1(kNodeEndpointKey), selectedEndpoint());
    m_settings.sync();
}

void NodeSelectionController::finishProbe(QNetworkReply* const reply)
{
    const auto probeIt = m_probes.find(reply);
    if (probeIt == m_probes.end()) {
        reply->deleteLater();
        return;
    }

    const Probe probe = probeIt.value();
    m_probes.erase(probeIt);
    m_pendingProbes = std::max(0, m_pendingProbes - 1);

    if (probe.nodeIndex >= 0 && probe.nodeIndex < m_nodes.size()) {
        Node& node = m_nodes[probe.nodeIndex];
        const QVariant httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int statusCode = httpStatus.toInt();
        const auto body = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject info = body.isObject() ? body.object() : QJsonObject{};
        const bool hypersnapCompatible = reply->error() == QNetworkReply::NoError &&
            statusCode >= 200 && statusCode < 300 &&
            info.value(QStringLiteral("version")).isString() &&
            info.value(QStringLiteral("shardInfos")).isArray();
        node.status = hypersnapCompatible ? QStringLiteral("online")
                     : (statusCode == 401 || statusCode == 402 || statusCode == 403)
                         ? QStringLiteral("auth-gated")
                         : statusCode > 0 ? QStringLiteral("incompatible")
                                          : QStringLiteral("offline");
        node.latencyMs = hypersnapCompatible
            ? static_cast<int>(std::clamp<qint64>(probe.timer.elapsed(), 0, 9999))
            : -1;
    }

    reply->deleteLater();

    if (m_pendingProbes == 0 && m_automatic) {
        const QString previous = m_selectedNodeId;
        chooseRecommendedNode();
        persistSelection();
        if (previous != m_selectedNodeId) {
            emit selectionChanged();
        }
    }
    emit nodesChanged();
}

void NodeSelectionController::chooseRecommendedNode()
{
    const qsizetype recommended = recommendedIndex();
    if (recommended >= 0) {
        m_selectedNodeId = m_nodes.at(recommended).id;
    }
}

qsizetype NodeSelectionController::indexForId(const QString& nodeId) const
{
    for (qsizetype index = 0; index < m_nodes.size(); ++index) {
        if (m_nodes.at(index).id == nodeId) {
            return index;
        }
    }
    return -1;
}

qsizetype NodeSelectionController::selectedIndex() const
{
    return indexForId(m_selectedNodeId);
}

qsizetype NodeSelectionController::recommendedIndex() const
{
    qsizetype best = -1;
    int bestLatency = std::numeric_limits<int>::max();
    for (qsizetype index = 0; index < m_nodes.size(); ++index) {
        const Node& node = m_nodes.at(index);
        if (node.status != QStringLiteral("online") || node.latencyMs < 0) {
            continue;
        }
        if (node.latencyMs < bestLatency) {
            bestLatency = node.latencyMs;
            best = index;
        }
    }
    if (best >= 0) {
        return best;
    }
    return selectedIndex() >= 0 ? selectedIndex() : (m_nodes.isEmpty() ? -1 : 0);
}
