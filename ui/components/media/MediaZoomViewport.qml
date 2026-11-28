import QtQuick

// Gesture state is independent of the Image's decode size and layout. The
// caller scales its fitted image around the viewport center using these values.
Item {
    id: root
    clip: true

    property bool interactionEnabled: true
    property real fittedWidth: 0
    property real fittedHeight: 0
    readonly property real maximumZoom: 5
    property real zoomFactor: 1
    property real offsetX: 0
    property real offsetY: 0
    readonly property bool zoomed: zoomFactor > 1.001
    readonly property bool gestureActive: pinch.active || pan.active
    readonly property bool canInteract: interactionEnabled && fittedWidth > 0 && fittedHeight > 0

    function boundOffset(value, extent, viewport) {
        const bound = Math.max(0, (extent * zoomFactor - viewport) / 2)
        return Math.max(-bound, Math.min(bound, value))
    }
    function constrain() {
        offsetX = boundOffset(offsetX, fittedWidth, width)
        offsetY = boundOffset(offsetY, fittedHeight, height)
    }
    function reset() {
        zoomFactor = 1
        offsetX = 0
        offsetY = 0
    }
    function zoomAt(value, x, y) {
        if (!canInteract || !isFinite(value) || !isFinite(x) || !isFinite(y))
            return
        const next = Math.max(1, Math.min(maximumZoom, value))
        if (next <= 1.001) {
            reset()
            return
        }
        // Keep the source point beneath the mouse/fingers stationary unless
        // the image reaches its pan boundary. Never modify implicit geometry.
        const ratio = next / zoomFactor
        offsetX = x - width / 2 - (x - width / 2 - offsetX) * ratio
        offsetY = y - height / 2 - (y - height / 2 - offsetY) * ratio
        zoomFactor = next
        constrain()
    }
    function zoomBy(factor) {
        zoomAt(zoomFactor * factor, width / 2, height / 2)
    }
    function panBy(dx, dy) {
        if (!canInteract || !zoomed || !isFinite(dx) || !isFinite(dy)) return
        offsetX += dx
        offsetY += dy
        constrain()
    }
    function toggleZoom(x, y) {
        if (zoomed) reset()
        else zoomAt(2.5, x, y)
    }
    onWidthChanged: constrain()
    onHeightChanged: constrain()
    onFittedWidthChanged: constrain()
    onFittedHeightChanged: constrain()
    onInteractionEnabledChanged: if (!interactionEnabled) reset()

    PinchHandler {
        id: pinch
        objectName: "mediaPinchHandler"
        enabled: root.canInteract
        target: null
        minimumPointCount: 2
        maximumPointCount: 2
        rotationAxis.enabled: false
        onScaleChanged: delta => {
            if (active)
                root.zoomAt(root.zoomFactor * delta, centroid.position.x, centroid.position.y)
        }
        onTranslationChanged: delta => {
            if (active) root.panBy(delta.x, delta.y)
        }
    }
    DragHandler {
        id: pan
        objectName: "mediaPanHandler"
        enabled: root.canInteract && root.zoomed && !pinch.active
        target: null
        minimumPointCount: 1
        maximumPointCount: 1
        property point lastTranslation: Qt.point(0, 0)
        onActiveChanged: lastTranslation = Qt.point(activeTranslation.x, activeTranslation.y)
        onActiveTranslationChanged: {
            const next = activeTranslation
            if (active) root.panBy(next.x - lastTranslation.x, next.y - lastTranslation.y)
            lastTranslation = Qt.point(next.x, next.y)
        }
    }
    WheelHandler {
        id: wheelZoom
        objectName: "mediaWheelHandler"
        enabled: root.canInteract && !pinch.active
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            const amount = event.pixelDelta.y !== 0 ? event.pixelDelta.y / 80
                                                    : event.angleDelta.y / 120
            if (amount === 0) {
                event.accepted = false
                return
            }
            root.zoomAt(root.zoomFactor * Math.pow(1.22, amount),
                        point.position.x, point.position.y)
            event.accepted = true
        }
    }
    TapHandler {
        objectName: "mediaDoubleTapHandler"
        enabled: root.canInteract
        gesturePolicy: TapHandler.DragThreshold
        onDoubleTapped: (eventPoint, button) =>
            root.toggleZoom(eventPoint.position.x, eventPoint.position.y)
    }
}
