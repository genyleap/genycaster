#include "reflection_coordinates.hpp"

#include <QQuickWindow>

#include <utility>

namespace {
// Relative mapping cancels transforms above the shared ancestor. Find that
// ancestor without allocating temporary containers, following the Neon kit's
// native coordinate policy (genyleap/neon, fd35c89c).
QQuickItem* commonAncestor(QQuickItem* first, QQuickItem* second)
{
    const auto depth = [](const QQuickItem* item) {
        int count = 0;
        for (; item; item = item->parentItem()) ++count;
        return count;
    };
    int left = depth(first);
    int right = depth(second);
    for (; left > right; --left) first = first->parentItem();
    for (; right > left; --right) second = second->parentItem();
    while (first != second) {
        first = first->parentItem();
        second = second->parentItem();
    }
    return first;
}
}

ReflectionCoordinates::ReflectionCoordinates(QObject* owner)
    : QObject(owner), m_receiver(qobject_cast<QQuickItem*>(owner))
{
}

ReflectionCoordinates* ReflectionCoordinates::qmlAttachedProperties(QObject* owner)
{
    return new ReflectionCoordinates(owner);
}

QQuickItem* ReflectionCoordinates::source() const noexcept
{
    return m_source.data();
}

void ReflectionCoordinates::setSource(QQuickItem* source)
{
    if (m_source == source) return;
    disconnect(m_sourceDestroyed);
    m_source = source;
    if (source) {
        m_sourceDestroyed = connect(source, &QObject::destroyed, this, [this] {
            m_source.clear();
            topologyChanged();
            emit sourceChanged();
        });
    }
    topologyChanged();
    emit sourceChanged();
}

QRectF ReflectionCoordinates::bounds()
{
    if (!m_observing) {
        m_observing = true;
        reconnectGeometry();
    }
    if (!m_receiver) return {};
    const QRectF local{0, 0, m_receiver->width(), m_receiver->height()};
    if (!m_source || m_receiver->window() != m_source->window()
        || !commonAncestor(m_receiver, m_source)) return local;
    return m_receiver->mapRectToItem(m_source, local);
}

void ReflectionCoordinates::topologyChanged()
{
    reconnectGeometry();
    emit boundsChanged();
}

void ReflectionCoordinates::reconnectGeometry()
{
    for (const auto& connection : std::as_const(m_geometryConnections))
        disconnect(connection);
    m_geometryConnections.clear();
    if (!m_observing || !m_receiver) return;
    // The local rectangle always follows the receiver size, even with no
    // active source or when a caller has an explicit source-local override.
    const auto sizeSignals = [this](QQuickItem* item) {
        m_geometryConnections.append(connect(item, &QQuickItem::widthChanged,
                                             this, &ReflectionCoordinates::boundsChanged));
        m_geometryConnections.append(connect(item, &QQuickItem::heightChanged,
                                             this, &ReflectionCoordinates::boundsChanged));
    };
    if (!m_source) {
        sizeSignals(m_receiver);
        return;
    }
    auto* const common = commonAncestor(m_receiver, m_source);
    const auto observe = [this, common, &sizeSignals](QQuickItem* item) {
        for (auto* ancestor = item; ancestor && ancestor != common;
             ancestor = ancestor->parentItem()) {
            m_geometryConnections.append(connect(ancestor, &QQuickItem::parentChanged,
                                                 this, &ReflectionCoordinates::topologyChanged));
            m_geometryConnections.append(connect(ancestor, &QQuickItem::xChanged,
                                                 this, &ReflectionCoordinates::boundsChanged));
            m_geometryConnections.append(connect(ancestor, &QQuickItem::yChanged,
                                                 this, &ReflectionCoordinates::boundsChanged));
            sizeSignals(ancestor);
            m_geometryConnections.append(connect(ancestor, &QQuickItem::scaleChanged,
                                                 this, &ReflectionCoordinates::boundsChanged));
            m_geometryConnections.append(connect(ancestor, &QQuickItem::rotationChanged,
                                                 this, &ReflectionCoordinates::boundsChanged));
            m_geometryConnections.append(connect(ancestor, &QQuickItem::transformOriginChanged,
                                                 this, &ReflectionCoordinates::boundsChanged));
        }
    };
    observe(m_receiver);
    if (m_source != m_receiver) observe(m_source);
    if (common == m_receiver) sizeSignals(m_receiver);
    m_geometryConnections.append(connect(m_receiver, &QQuickItem::windowChanged,
                                         this, &ReflectionCoordinates::topologyChanged));
    if (m_source != m_receiver)
        m_geometryConnections.append(connect(m_source, &QQuickItem::windowChanged,
                                             this, &ReflectionCoordinates::topologyChanged));
}
