/*!
 * @file        ui/render/reflection_coordinates.hpp
 * @brief       Native relative coordinates for shared reflection receivers.
 * @details     Observes only geometry below the common ancestor of the receiver
 *              and source; inactive receivers do not observe source geometry.
 * @author      Genyleap
 * @since       28 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
// Textual compatibility boundary for Qt MOC and QML attached registration.
#pragma once

#include <QPointer>
#include <QQuickItem>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/** @brief Maps the receiver rectangle into a shared source's local coordinates.
 *
 * The receiver owns the attached object. The source is observed, not owned.
 * A missing or disconnected source yields the receiver's local rectangle.
 */
class ReflectionCoordinates : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ReflectionCoordinates)
    QML_ATTACHED(ReflectionCoordinates)
    QML_UNCREATABLE("Attach ReflectionCoordinates to a visual item")
    Q_PROPERTY(QQuickItem* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QRectF bounds READ bounds NOTIFY boundsChanged)

public:
    explicit ReflectionCoordinates(QObject* owner = nullptr);
    [[nodiscard]] static ReflectionCoordinates* qmlAttachedProperties(QObject* owner);
    [[nodiscard]] QQuickItem* source() const noexcept;
    void setSource(QQuickItem* source);
    [[nodiscard]] QRectF bounds();

signals:
    void sourceChanged();
    void boundsChanged();

private:
    void reconnectGeometry();
    void topologyChanged();
    QPointer<QQuickItem> m_receiver{};
    QPointer<QQuickItem> m_source{};
    QMetaObject::Connection m_sourceDestroyed{};
    QVector<QMetaObject::Connection> m_geometryConnections{};
    bool m_observing{false};
};
QML_DECLARE_TYPEINFO(ReflectionCoordinates, QML_HAS_ATTACHED_PROPERTIES)
