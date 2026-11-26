import QtQuick

// Non-visual admission contract shared by media consumers. Viewport visibility
// is distinct from Item.visible (ListView cache-buffer rows remain visible).
QtObject {
    id: root
    required property Item owner
    property bool observeViewport: true
    readonly property bool foreground: Qt.application.state !== Qt.ApplicationSuspended
        && Qt.application.state !== Qt.ApplicationHidden
        && ((Qt.platform.os !== "android" && Qt.platform.os !== "ios"
             && Qt.platform.os !== "wasm") || Qt.application.state === Qt.ApplicationActive)
    readonly property bool eligible: owner !== null && owner.visible && foreground
        && (!observeViewport || intersectsViewport())

    function intersectsViewport() {
        if (!owner || owner.width <= 0 || owner.height <= 0)
            return false
        // Walk ancestors so QML tracks position/size dependencies as well as
        // Flickable.contentX/Y. Nested scroll/clip containers all must admit.
        let item = owner
        let x = 0
        let y = 0
        while (item && item.parent) {
            x += item.x
            y += item.y
            const parent = item.parent
            if (parent.contentX !== undefined && parent.contentY !== undefined
                    && parent.contentItem !== undefined) {
                const cx = parent.contentX
                const cy = parent.contentY
                // contentItem's x/y already include the scroll translation.
                // Read the offsets explicitly to track changes on all backends.
                const mapped = owner.mapToItem(parent, 0, 0)
                if (!isFinite(cx) || !isFinite(cy)
                        || mapped.x + owner.width <= 0 || mapped.y + owner.height <= 0
                        || mapped.x >= parent.width || mapped.y >= parent.height)
                    return false
            }
            item = parent
        }
        return true
    }
}
