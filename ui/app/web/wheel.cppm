// Keep Qt's header-defined templates outside exported interfaces.
module;
class QPoint;
class QPointF;
class QQuickWindow;

export module genycaster.ui.web.wheel;

export namespace genycaster::ui::browser_wheel {
    [[nodiscard]] bool scroll(QQuickWindow& window, const QPointF& position,
                              const QPoint& pixelDelta);
}

export namespace genycaster::ui::web {
    void installWheelBridge(QQuickWindow* window);
}
