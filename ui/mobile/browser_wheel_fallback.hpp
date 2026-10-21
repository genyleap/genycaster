#pragma once

#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <cmath>

// Qt's WebAssembly screen can drop synthesized wheel events even when pointer
// dragging works. Scroll the nearest interactive QQuickFlickable instead of
// depending on accepted() from the synthetic wheel event.
//
// Qt Quick overlays are often full-window QQuickItems. A single childAt()
// would hit that *empty* overlay and never reach the actual feed. Walk items
// in visual stacking order, treating visible popup/modal surfaces as input
// boundaries while skipping noninteractive, empty overlays.
namespace genycaster::ui::browser_wheel {

inline bool moveAxis(QQuickItem* item, const char* positionName,
                     const char* contentName, const double delta)
{
    if (std::abs(delta) < 0.1 || !std::isfinite(delta)) return false;

    const auto currentValue = item->property(positionName);
    const auto contentValue = item->property(contentName);
    const auto interactiveValue = item->property("interactive");
    if (!currentValue.isValid() || !contentValue.isValid()
        || (interactiveValue.isValid() && !interactiveValue.toBool())) return false;

    const bool vertical = positionName[7] == 'Y'; // contentY / contentX
    const double viewport = vertical ? item->height() : item->width();
    const double content = contentValue.toDouble();
    const double previous = currentValue.toDouble();
    if (!std::isfinite(viewport) || !std::isfinite(content)
        || !std::isfinite(previous) || viewport <= 0) return false;

    // ListView/Flickable can have a non-zero (including negative) origin
    // after reusing and remeasuring delegates. Clamping to absolute zero
    // makes wheel ticks appear stuck at the wrong beginning/end.
    const auto originValue = item->property(vertical ? "originY" : "originX");
    const double minimum = originValue.isValid() ? originValue.toDouble() : 0.0;
    if (!std::isfinite(minimum)) return false;
    const double maximum = minimum + std::max(0.0, content - viewport);
    if (maximum - minimum < 1) return false;
    const double next = std::clamp(previous + delta, minimum, maximum);
    if (std::abs(previous - next) < 0.1) return false;

    if (!item->setProperty(positionName, next)) return false;
    return std::abs(item->property(positionName).toDouble() - previous) > 0.1;
}

enum class Result { NotHandled, Scrolled, Blocked };

inline Result scrollBranch(QQuickItem* item, const QPointF& scenePosition,
                           const QPoint& pixelDelta)
{
    if (!item || !item->isVisible() || !item->isEnabled()) return Result::NotHandled;
    const QPointF local = item->mapFromScene(scenePosition);
    if (!std::isfinite(local.x()) || !std::isfinite(local.y())) return Result::NotHandled;
    const bool contains = item->contains(local);
    if (!contains && item->clip()) return Result::NotHandled;

    // Stacking: larger z appears first; equal z paints later children on top.
    // A full-window overlay without an open popup must be transparent to this
    // *scroll lookup* (not to Qt's actual mouse event routing).
    auto children = item->childItems();
    std::reverse(children.begin(), children.end());
    std::stable_sort(children.begin(), children.end(),
        [](const QQuickItem* a, const QQuickItem* b) { return a->z() > b->z(); });
    for (auto* child : children) {
        const auto result = scrollBranch(child, scenePosition, pixelDelta);
        if (result != Result::NotHandled) return result;
    }

    if (contains) {
        if (moveAxis(item, "contentY", "contentHeight",
                     -static_cast<double>(pixelDelta.y()))
            || moveAxis(item, "contentX", "contentWidth",
                        -static_cast<double>(pixelDelta.x()))) {
            return Result::Scrolled;
        }
        const QString className = QString::fromLatin1(item->metaObject()->className());
        if (className.contains(QStringLiteral("PopupItem"))
            || item->objectName() == QStringLiteral("modalInputGuard")) {
            return Result::Blocked;
        }
    }
    return Result::NotHandled;
}

inline bool scroll(QQuickWindow& window, const QPointF& position,
                   const QPoint& qtPixelDelta)
{
    auto* root = window.contentItem();
    if (!root || position.x() < 0 || position.y() < 0
        || position.x() >= window.width() || position.y() >= window.height()) {
        return false;
    }
    // Modal popup QObjects can be owned by the screen instead of the visual
    // overlay. Looking them up via findChildren() on *every wheel tick* was a
    // linear scan of the entire feed/delegate QObject tree, especially costly
    // in Chrome and on screens with many casts. Cache only a currently open
    // modal and invalidate it when it closes, is destroyed or focus enters
    // another popup. This does not retain raw pointers across route changes.
    static QPointer<QQuickWindow> cachedWindow{};
    static QPointer<QObject> cachedModal{};
    if (cachedWindow == &window && cachedModal
        && cachedModal->property("modal").toBool()
        && cachedModal->property("visible").toBool()) {
        auto* content = cachedModal->property("contentItem").value<QQuickItem*>();
        auto* focus = window.activeFocusItem();
        if (content && (!focus || focus == content || content->isAncestorOf(focus)))
            return scrollBranch(content, position, qtPixelDelta) == Result::Scrolled;
    }
    cachedWindow = &window;
    cachedModal.clear();
    // Qt Quick's modal Popup/Drawer content lives in an overlay, with the
    // Popup QObject parented to the originating control rather than always
    // to the visual overlay Item. Before walking the feed, route the wheel
    // strictly into the topmost open modal's content item. Otherwise scrolling
    // outside the modal would incorrectly move the feed behind its scrim.
    for (auto* branch : root->childItems()) {
        const QVariant overlayState = branch->property("overlayOpen");
        if (overlayState.isValid() && !overlayState.toBool()) {
            // The main AppShell has no open modal; skip an expensive search
            // through thousands of feed delegates for every wheel tick.
            continue;
        }
        const auto objects = branch->findChildren<QObject*>();
        for (auto it = objects.crbegin(); it != objects.crend(); ++it) {
            QObject* object = *it;
            if (!object->property("modal").toBool()
                || !object->property("visible").toBool()) continue;
            const auto content = object->property("contentItem");
            if (!content.isValid()) continue;
            auto* item = content.value<QQuickItem*>();
            if (!item) continue;
            cachedModal = object;
            return scrollBranch(item, position, qtPixelDelta) == Result::Scrolled;
        }
    }
    return scrollBranch(root, position, qtPixelDelta) == Result::Scrolled;
}

} // namespace genycaster::ui::browser_wheel
